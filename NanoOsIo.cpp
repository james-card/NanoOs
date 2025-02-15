////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2025 James Card                     //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included    //
// in all copies or substantial portions of the Software.                     //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//                                 James Card                                 //
//                          http://www.jamescard.org                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// Doxygen marker
/// @file

// Custom includes
#include "NanoOsIo.h"
#include "SdCard.h"
#include "Fat16Filesystem.h"
#include "Scheduler.h"

// Basic SD card communication using SPI
#include <SPI.h>

// SD card commands
#define CMD0    0x40  // GO_IDLE_STATE
#define CMD8    0x48  // SEND_IF_COND
#define CMD9    0x49  // SEND_CSD
#define CMD16   0x50  // SET_BLOCKLEN
#define CMD17   0x51  // READ_SINGLE_BLOCK
#define CMD24   0x58  // WRITE_BLOCK
#define CMD58   0x7A  // READ_OCR
#define CMD55   0x77  // APP_CMD
#define ACMD41  0x69  // SD_SEND_OP_COND

// R1 Response bit flags
#define R1_IDLE_STATE  0x01
#define R1_ERASE_RESET 0x02
#define R1_ILLEGAL_CMD 0x04
#define R1_CRC_ERROR   0x08
#define R1_ERASE_SEQ   0x10
#define R1_ADDR_ERROR  0x20
#define R1_PARAM_ERROR 0x40

/// @struct SdCardState
///
/// @brief State required to interact with the SD card.
///
/// @param chipSelect The I/O pin connected to the SD card's chip select line.
/// @param blockSize The number of bytes per block on the SD card as presented
///   to the host.
/// @param numBlocks The total number of blocks available on the SD card.
/// @param sdCardVersion The version of the card (1 or 2).
typedef struct SdCardState {
  uint8_t chipSelect;
  uint16_t blockSize;
  uint32_t numBlocks;
  int sdCardVersion;
} SdCardState;

/// @struct NanoOsIoState
///
/// @brief Full state required for NanoOs I/O operations.  Includes an
/// SdCardState object and a FilesystemState object.
///
/// @param sdCardState The SdCardState object required for communication with
///   the SD card.
/// @param filesystemState The FilesystemState object that tracks the metadata
///   required to manage files.
/// @param consoleState The ConsoleState object that manages I/O on the
///   consoles (serial ports).
typedef struct NanoOsIoState {
  SdCardState sdCardState;
  FilesystemState filesystemState;
  ConsoleState consoleState;
} NanoOsIoState;

/// @typedf NanoOsIoCommandHandler
///
/// @brief Definition for the format of a command handler for NanoOs I/O inter-
/// process communication.
typedef int (*NanoOsIoCommandHandler)(NanoOsIoState*, ProcessMessage*);

/// @var nanoOsIoStdin
///
/// @brief Implementation of nanoOsIoStdin which is the define value for stdin.
FILE *nanoOsIoStdin  = (FILE*) ((intptr_t) 0x1);

/// @var nanoOsIoStdout
///
/// @brief Implementation of nanoOsIoStdout which is the define value for
/// stdout.
FILE *nanoOsIoStdout = (FILE*) ((intptr_t) 0x2);

/// @var nanoOsIoStderr
///
/// @brief Implementation of nanoOsIoStderr which is the define value for
/// stderr.
FILE *nanoOsIoStderr = (FILE*) ((intptr_t) 0x3);

/// @fn void sdSpiEnd(int chipSelect)
///
/// @brief End communication with the SD card.
///
/// @param chipSelect The I/O pin connected to the SD card's chip select line.
///
/// @return This function returns no value.
__attribute__((noinline)) void sdSpiEnd(uint8_t chipSelect) {
  // Deselect the SD chip select pin.
  digitalWrite(chipSelect, HIGH);
  for (int ii = 0; ii < 8; ii++) {
    SPI.transfer(0xFF); // 8 clock pulses
  }

  return;
}

/// @fn uint8_t sdSpiSendCommand(uint8_t chipSelect, uint8_t cmd, uint32_t arg)
///
/// @brief Send a command and its argument to the SD card over the SPI
/// interface.
///
/// @param chipSelect The pin tht the SD card's chip select line is connected
///   to.
/// @param cmd The 8-bit SD command to send to the SD card.
/// @param arg The 32-bit arguent to send for the SD command.
///
/// @return Returns the 8-bit command response from the SD card.
uint8_t sdSpiSendCommand(uint8_t chipSelect, uint8_t cmd, uint32_t arg) {
  digitalWrite(chipSelect, LOW);
  
  // Command byte
  SPI.transfer(cmd | 0x40);
  
  // Argument
  SPI.transfer((arg >> 24) & 0xff);
  SPI.transfer((arg >> 16) & 0xff);
  SPI.transfer((arg >>  8) & 0xff);
  SPI.transfer((arg >>  0) & 0xff);
  
  // CRC - only needed for CMD0 and CMD8
  uint8_t crc = 0xFF;
  if (cmd == CMD0) {
    crc = 0x95; // Valid CRC for CMD0
  } else if (cmd == CMD8) {
    crc = 0x87; // Valid CRC for CMD8 (0x1AA)
  }
  SPI.transfer(crc);
  
  // Wait for response
  uint8_t response;
  for (int ii = 0; ii < 10; ii++) {
    response = SPI.transfer(0xFF);
    if ((response & 0x80) == 0) {
      break; // Exit if valid response
    }
  }
  
  return response;
}

/// @fn int sdSpiCardInit(uint8_t chipSelect)
///
/// @brief Initialize the SD card for communication with the OS.
///
/// @param chipSelect The pin tht the SD card's chip select line is connected
///   to.
///
/// @return Returns the version of the connected card on success (1 or 2),
/// 0 on error.
int sdSpiCardInit(uint8_t chipSelect) {
  uint8_t response;
  uint16_t timeoutCount;
  bool isSDv2 = false;
  
  // Set up chip select pin
  pinMode(chipSelect, OUTPUT);
  digitalWrite(chipSelect, HIGH);
  
  // Set up SPI at low speed
  SPI.begin();
  
  // Extended power up sequence - Send more clock cycles
  for (int ii = 0; ii < 32; ii++) {
    SPI.transfer(0xFF);
  }
  
  // Send CMD0 to enter SPI mode
  timeoutCount = 200;  // Extended timeout
  do {
    for (int ii = 0; ii < 8; ii++) {  // More dummy clocks
      SPI.transfer(0xFF);
    }
    response = sdSpiSendCommand(chipSelect, CMD0, 0);
    if (--timeoutCount == 0) {
      sdSpiEnd(chipSelect);
      return -1;
    }
  } while (response != R1_IDLE_STATE);
  
  // Send CMD8 to check version
  for (int ii = 0; ii < 8; ii++) {
    SPI.transfer(0xFF);
  }
  response = sdSpiSendCommand(chipSelect, CMD8, 0x000001AA);
  if (response == R1_IDLE_STATE) {
    isSDv2 = true;
    for (int ii = 0; ii < 4; ii++) {
      response = SPI.transfer(0xFF);
    }
  }
  sdSpiEnd(chipSelect);
  
  // Initialize card with ACMD41
  timeoutCount = 20000;  // Much longer timeout
  do {
    response = sdSpiSendCommand(chipSelect, CMD55, 0);
    sdSpiEnd(chipSelect);
    
    for (int ii = 0; ii < 8; ii++) {
      SPI.transfer(0xFF);
    }
    
    // Try both with and without HCS bit based on card version
    uint32_t acmd41Arg = isSDv2 ? 0x40000000 : 0;
    response = sdSpiSendCommand(chipSelect, ACMD41, acmd41Arg);
    sdSpiEnd(chipSelect);
    
    if (--timeoutCount == 0) {
      sdSpiEnd(chipSelect);
      return -5;
    }
  } while (response != 0);
  
  // If we get here, card is initialized
  for (int ii = 0; ii < 8; ii++) {
    SPI.transfer(0xFF);
  }
  
  sdSpiEnd(chipSelect);

  // Now that card is initialized, increase SPI speed
  // The Nano Every can handle up to 8MHz reliably with most SD cards
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

  return isSDv2 ? 2 : 1;
}

/// @fn bool sdSpiReadBlock(SdCardState *sdCardState,
///   uint32_t blockNumber, uint8_t *buffer)
///
/// @brief Read a 512-byte block from the SD card.
///
/// @param sdCardState A pointer to the SdCardState object maintained by the
///   runSdCard process.
/// @param blockNumber The logical block number to read from the card.
/// @param buffer A pointer to a character buffer to read the block into.
///
/// @return Returns 0 on success, error code on failure.
int sdSpiReadBlock(SdCardState *sdCardState,
  uint32_t blockNumber, uint8_t *buffer
) {
  // Check that buffer is not null
  if (buffer == NULL) {
    return EINVAL;
  }
  
  uint32_t address = blockNumber;
  if (sdCardState->sdCardVersion == 1) {
    address *= sdCardState->blockSize; // Convert to byte address
  }
  
  
  // Send READ_SINGLE_BLOCK command
  uint8_t response = sdSpiSendCommand(sdCardState->chipSelect, CMD17, address);
  if (response != 0x00) {
    sdSpiEnd(sdCardState->chipSelect);
    return EIO; // Command failed
  }
  
  // Wait for data token (0xFE)
  uint16_t timeout = 10000;
  while (timeout--) {
    response = SPI.transfer(0xFF);
    if (response == 0xFE) {
      break;
    }
    if (timeout == 0) {
      sdSpiEnd(sdCardState->chipSelect);
      return EIO;  // Timeout waiting for data
    }
  }
  
  // Read 512 byte block
  for (int ii = 0; ii < 512; ii++) {
    buffer[ii] = SPI.transfer(0xFF);
  }
  
  // Read CRC (2 bytes, ignored)
  SPI.transfer(0xFF);
  SPI.transfer(0xFF);
  
  sdSpiEnd(sdCardState->chipSelect);
  return 0;
}

/// @fn int sdSpiWriteBlock(SdCardState *sdCardState,
///   uint32_t blockNumber, const uint8_t *buffer)
/// 
/// @brief Write a 512-byte block to the SD card.
///
/// @param sdCardState A pointer to the SdCardState object maintained by the
///   runSdCard process.
/// @param blockNumber The logical block number to write from the card.
/// @param buffer A pointer to a character buffer to write the block from.
///
/// @return Returns 0 on success, error code on failure.
int sdSpiWriteBlock(SdCardState *sdCardState,
  uint32_t blockNumber, const uint8_t *buffer
) {
  if (buffer == NULL) {
    return EINVAL;
  }
  
  // Check if card is responsive
  digitalWrite(sdCardState->chipSelect, LOW);
  uint8_t response = SPI.transfer(0xFF);
  if (response != 0xFF) {
    digitalWrite(sdCardState->chipSelect, HIGH);
    return EIO;
  }
  
  uint32_t address = blockNumber;
  if (sdCardState->sdCardVersion == 1) {
    address *= sdCardState->blockSize; // Convert to byte address
  }
  
  // Send WRITE_BLOCK command
  response = sdSpiSendCommand(sdCardState->chipSelect, CMD24, address);
  if (response != 0x00) {
    sdSpiEnd(sdCardState->chipSelect);
    return EIO; // Command failed
  }
  
  // Wait for card to be ready before sending data
  uint16_t timeout = 10000;
  do {
    response = SPI.transfer(0xFF);
    if (--timeout == 0) {
      sdSpiEnd(sdCardState->chipSelect);
      return EIO;
    }
  } while (response != 0xFF);
  
  // Send start token
  SPI.transfer(0xFE);
  
  // Write data
  for (int ii = 0; ii < 512; ii++) {
    SPI.transfer(buffer[ii]);
  }
  
  // Send dummy CRC
  SPI.transfer(0xFF);
  SPI.transfer(0xFF);
  
  // Get data response
  response = SPI.transfer(0xFF);
  if ((response & 0x1F) != 0x05) {
    sdSpiEnd(sdCardState->chipSelect);
    return EIO; // Bad response
  }
  
  // Wait for write to complete
  timeout = 10000;
  while (timeout--) {
    if (SPI.transfer(0xFF) != 0x00) {
      break;
    }
    if (timeout == 0) {
      sdSpiEnd(sdCardState->chipSelect);
      return EIO; // Write timeout
    }
  }
  
  sdSpiEnd(sdCardState->chipSelect);
  return 0;
}

/// @fn int16_t sdSpiGetBlockSize(uint8_t chipSelect)
///
/// @brief Get the size, in bytes, of blocks on the SD card as presented to the
/// host.
///
/// @param chipSelect The pin tht the SD card's chip select line is connected
///   to.
///
/// @return Returns the number of bytes per block on success, negative error
/// code on failure.
int16_t sdSpiGetBlockSize(uint8_t chipSelect) {
  uint8_t response = sdSpiSendCommand(chipSelect, CMD9, 0);
  if (response != 0x00) {
    sdSpiEnd(chipSelect);
    //// printString(__func__);
    //// printString(": ERROR! CMD9 returned ");
    //// printInt(response);
    //// printString("\n");
    return -1;
  }

  for(int i = 0; i < 100; i++) {
    response = SPI.transfer(0xFF);
    if (response == 0xFE) {
      break;  // Data token
    }
  }
  
  // Read 16-byte CSD register
  uint8_t csd[16];
  for(int i = 0; i < 16; i++) {
    csd[i] = SPI.transfer(0xFF);
  }
  
  // Read 2 CRC bytes
  SPI.transfer(0xFF);
  SPI.transfer(0xFF);
  sdSpiEnd(chipSelect);

  // For CSD Version 1.0 and 2.0, READ_BL_LEN is at the same location
  uint8_t readBlockLength = (csd[5] & 0x0F);
  return (int16_t) (((uint16_t) 1) << readBlockLength);
}

/// @fn int sdSpiGetBlockCount(uint8_t chipSelect)
///
/// @brief Get the total number of available blocks on an SD card.
///
/// @param chipSelect The pin tht the SD card's chip select line is connected
///   to.
///
/// @return Returns the number of blocks available on success, negative error
/// code on failure.
int32_t sdSpiGetBlockCount(uint8_t chipSelect) {
  uint8_t cardSpecificData[16];
  uint32_t blockCount = 0;
  
  // Send SEND_CSD command
  uint8_t response = sdSpiSendCommand(chipSelect, CMD9, 0);
  if (response != 0x00) {
    sdSpiEnd(chipSelect);
    //// printString(__func__);
    //// printString(": ERROR! CMD9 returned ");
    //// printInt(response);
    //// printString("\n");
    return -1;
  }
  
  // Wait for data token
  uint16_t timeoutCount = 10000;
  while (timeoutCount--) {
    response = SPI.transfer(0xFF);
    if (response == 0xFE) {
      break;
    }
    if (timeoutCount == 0) {
      sdSpiEnd(chipSelect);
      return -2;
    }
  }
  
  // Read CSD register
  for (int ii = 0; ii < 16; ii++) {
    cardSpecificData[ii] = SPI.transfer(0xFF);
  }
  
  sdSpiEnd(chipSelect);
  
  // Calculate capacity based on CSD version
  if ((cardSpecificData[0] >> 6) == 0x01) {  // CSD version 2.0
    // C_SIZE is bits [69:48] in CSD
    uint32_t capacity = ((uint32_t) cardSpecificData[7] & 0x3F) << 16;
    capacity |= (uint32_t) cardSpecificData[8] << 8;
    capacity |= (uint32_t) cardSpecificData[9];
    blockCount = (capacity + 1) << 10; // Multiply by 1024 blocks
  } else {  // CSD version 1.0
    // Calculate from C_SIZE, C_SIZE_MULT, and READ_BL_LEN
    uint32_t capacity = ((uint32_t) (cardSpecificData[6] & 0x03) << 10);
    capacity |= (uint32_t) cardSpecificData[7] << 2;
    capacity |= (uint32_t) (cardSpecificData[8] >> 6);
    
    uint8_t capacityMultiplier = ((cardSpecificData[9] & 0x03) << 1);
    capacityMultiplier |= ((cardSpecificData[10] & 0x80) >> 7);
    
    uint8_t readBlockLength = cardSpecificData[5] & 0x0F;
    
    blockCount = (capacity + 1) << (capacityMultiplier + 2);
    blockCount <<= (readBlockLength - 9);  // Adjust for 512-byte blocks
  }
  
  return (int32_t) blockCount;
}

/// @fn void fat16FormatFilename(const char *pathname, char *formattedName)
///
/// @brief Format a user-supplied pathname into a name formatted for easy
/// comparison in a directory search.
///
/// @param pathname A pointer to the user-supplied path to format.
/// @param formattedName A pointer to the character buffer that is to be
///   populated by this function.
///
/// @return This function returns no value.
static void fat16FormatFilename(const char *pathname, char *formattedName) {
  memset(formattedName, ' ', FAT16_FULL_NAME_LENGTH);
  const char *dot = strrchr(pathname, '.');
  size_t nameLen = dot ? (dot - pathname) : strlen(pathname);
  
  for (uint8_t ii = 0; ii < FAT16_FILENAME_LENGTH && ii < nameLen; ii++) {
    formattedName[ii] = toupper(pathname[ii]);
  }
  
  if (dot) {
    for (uint8_t ii = 0; ii < FAT16_EXTENSION_LENGTH && dot[ii + 1]; ii++) {
      formattedName[FAT16_FILENAME_LENGTH + ii] = toupper(dot[ii + 1]);
    }
  }
}

/// @brief Search the root directory for a file entry
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param file Pointer to the FAT16 file structure containing common values
/// @param pathname The pathname to search for
/// @param entry Pointer to where the directory entry buffer should be stored
/// @param block Pointer to where the block number containing the entry should
///   be stored
/// @param entryIndex Pointer to where the entry index within the block should
///   be stored
///
/// @return FAT16_DIR_SEARCH_ERROR on error, FAT16_DIR_SEARCH_FOUND if entry
///   found, FAT16_DIR_SEARCH_DELETED if a deleted entry with matching name
///   found, FAT16_DIR_SEARCH_NOT_FOUND if no matching entry found
static int fat16FindDirectoryEntry(NanoOsIoState *nanoOsIoState,
    Fat16File *file, const char *pathname, uint8_t **entry, uint32_t *block,
    uint16_t *entryIndex)
{
  char upperName[FAT16_FULL_NAME_LENGTH + 1];
  fat16FormatFilename(pathname, upperName);
  upperName[FAT16_FULL_NAME_LENGTH] = '\0';
  
  for (uint16_t ii = 0; ii < file->rootEntries; ii++) {
    *block = file->rootStart + (ii / (file->bytesPerSector >>
      FAT16_DIR_ENTRIES_PER_SECTOR_SHIFT));
    if (sdSpiReadBlock(&nanoOsIoState->sdCardState, *block, nanoOsIoState->filesystemState.blockBuffer)) {
      return FAT16_DIR_SEARCH_ERROR;
    }
    
    *entry = nanoOsIoState->filesystemState.blockBuffer + ((ii % (file->bytesPerSector >>
      FAT16_DIR_ENTRIES_PER_SECTOR_SHIFT)) * FAT16_BYTES_PER_DIRECTORY_ENTRY);
    uint8_t firstChar = (*entry)[FAT16_DIR_FILENAME];
    
    if (memcmp(*entry + FAT16_DIR_FILENAME, upperName,
        FAT16_FULL_NAME_LENGTH) == 0) {
      if (entryIndex) {
        *entryIndex = ii;
      }
      if (firstChar == FAT16_DELETED_MARKER) {
        return FAT16_DIR_SEARCH_DELETED;
      } else if (firstChar != FAT16_EMPTY_ENTRY) {
        return FAT16_DIR_SEARCH_FOUND;
      }
    } else if (firstChar == FAT16_EMPTY_ENTRY) {
      // Once we hit an empty entry, there are no more entries to check
      break;
    }
  }
  
  return FAT16_DIR_SEARCH_NOT_FOUND;
}

/// @fn Fat16File* fat16Fopen(NanoOsIoState *nanoOsIoState, const char *pathname,
///   const char *mode)
///
/// @brief Open a file in the FAT16 filesystem
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param pathname The name of the file to open
/// @param mode The mode to open the file in ("r" for read, "w" for write,
///   "a" for append)
///
/// @return Pointer to a newly allocated Fat16File structure on success,
///   NULL on failure. The caller is responsible for freeing the returned
///   structure when finished with it.
Fat16File* fat16Fopen(NanoOsIoState *nanoOsIoState, const char *pathname,
    const char *mode
) {
  bool createFile = (mode[0] & 1); // 'w' and 'a' have LSB set
  bool append = (mode[0] == 'a');
  uint8_t *buffer = NULL;
  uint8_t *entry = NULL;
  uint32_t block = 0;
  uint16_t entryIndex = 0;
  Fat16File *file = NULL;
  int result = 0;
  char upperName[FAT16_FULL_NAME_LENGTH + 1];
  
  if (nanoOsIoState->filesystemState.numOpenFiles == 0) {
    nanoOsIoState->filesystemState.blockBuffer = (uint8_t*) malloc(
      nanoOsIoState->filesystemState.blockSize);
    if (nanoOsIoState->filesystemState.blockBuffer == NULL) {
      goto exit;
    }
  }
  buffer = nanoOsIoState->filesystemState.blockBuffer;

  // Read boot sector
  if (sdSpiReadBlock(&nanoOsIoState->sdCardState,
      nanoOsIoState->filesystemState.startLba, buffer)) {
    goto exit;
  }
  
  // Create file structure to hold common values
  file = (Fat16File*) malloc(sizeof(Fat16File));
  if (!file) {
    goto exit;
  }
  
  // Store common boot sector values
  file->bytesPerSector = *((uint16_t*) &buffer[FAT16_BOOT_BYTES_PER_SECTOR]);
  file->sectorsPerCluster = buffer[FAT16_BOOT_SECTORS_PER_CLUSTER];
  file->reservedSectors =
    *((uint16_t*) &buffer[FAT16_BOOT_RESERVED_SECTORS]);
  file->numberOfFats = buffer[FAT16_BOOT_NUMBER_OF_FATS];
  file->rootEntries = *((uint16_t*) &buffer[FAT16_BOOT_ROOT_ENTRIES]);
  file->sectorsPerFat = *((uint16_t*) &buffer[FAT16_BOOT_SECTORS_PER_FAT]);
  file->bytesPerCluster = (uint32_t) file->bytesPerSector *
    (uint32_t) file->sectorsPerCluster;
  file->fatStart = ((uint32_t) nanoOsIoState->filesystemState.startLba) +
    ((uint32_t) file->reservedSectors);
  file->rootStart = ((uint32_t) file->fatStart) +
    (((uint32_t) file->numberOfFats) * ((uint32_t) file->sectorsPerFat));
  file->dataStart = ((uint32_t) file->rootStart) +
    (((((uint32_t) file->rootEntries) * FAT16_BYTES_PER_DIRECTORY_ENTRY) +
    ((uint32_t) file->bytesPerSector) - 1) /
    ((uint32_t) file->bytesPerSector));
  
  result = fat16FindDirectoryEntry(nanoOsIoState, file, pathname, &entry,
    &block, &entryIndex);
  
  if (result == FAT16_DIR_SEARCH_FOUND) {
    if (createFile && !append) {
      // File exists but we're in write mode - truncate it
      file->currentCluster =
        *((uint16_t*) &entry[FAT16_DIR_FIRST_CLUSTER_LOW]);
      file->firstCluster = file->currentCluster;
      file->fileSize = 0;
      file->currentPosition = 0;
    } else {
      // Opening existing file for read
      file->currentCluster =
        *((uint16_t*) &entry[FAT16_DIR_FIRST_CLUSTER_LOW]);
      file->fileSize = *((uint32_t*) &entry[FAT16_DIR_FILE_SIZE]);
      file->firstCluster = file->currentCluster;
      file->currentPosition = append ? file->fileSize : 0;
    }
    // Store directory entry location
    file->directoryBlock = block;
    file->directoryOffset = (entryIndex % (file->bytesPerSector >>
      FAT16_DIR_ENTRIES_PER_SECTOR_SHIFT)) * FAT16_BYTES_PER_DIRECTORY_ENTRY;
    nanoOsIoState->filesystemState.numOpenFiles++;
  } else if (createFile && 
      (result == FAT16_DIR_SEARCH_DELETED || 
       result == FAT16_DIR_SEARCH_NOT_FOUND)) {
    // Create new file using the entry location we found
    fat16FormatFilename(pathname, upperName);
    memcpy(entry + FAT16_DIR_FILENAME, upperName, FAT16_FULL_NAME_LENGTH);
    entry[FAT16_DIR_ATTRIBUTES] = FAT16_ATTR_NORMAL_FILE;
    memset(entry + FAT16_DIR_ATTRIBUTES + 1, FAT16_EMPTY_ENTRY,
      FAT16_BYTES_PER_DIRECTORY_ENTRY - (FAT16_DIR_ATTRIBUTES + 1));
    
    if (sdSpiWriteBlock(&nanoOsIoState->sdCardState, block, buffer)) {
      free(file);
      file = NULL;
      goto exit;
    }
    
    file->currentCluster = FAT16_EMPTY_ENTRY;
    file->fileSize = 0;
    file->firstCluster = FAT16_EMPTY_ENTRY;
    file->currentPosition = 0;
    // Store directory entry location
    file->directoryBlock = block;
    file->directoryOffset = (entryIndex % (file->bytesPerSector >>
      FAT16_DIR_ENTRIES_PER_SECTOR_SHIFT)) * FAT16_BYTES_PER_DIRECTORY_ENTRY;
    nanoOsIoState->filesystemState.numOpenFiles++;
  } else {
    free(file);
    file = NULL;
  }
  
exit:
  if (nanoOsIoState->filesystemState.numOpenFiles == 0) {
    free(nanoOsIoState->filesystemState.blockBuffer);
    nanoOsIoState->filesystemState.blockBuffer = NULL;
  }
  return file;
}

/// @fn int fat16GetNextCluster(NanoOsIoState *nanoOsIoState, Fat16File *file, 
///   uint16_t *nextCluster)
///
/// @brief Get the next cluster in the FAT chain for a given file
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param file A pointer to the FAT16 file structure
/// @param nextCluster Pointer to store the next cluster number
///
/// @return Returns 0 on success, -1 on error
static int fat16GetNextCluster(NanoOsIoState *nanoOsIoState, Fat16File *file,
    uint16_t *nextCluster
) {
  uint32_t fatBlock = file->fatStart + 
    ((file->currentCluster * sizeof(uint16_t)) / file->bytesPerSector);

  if (sdSpiReadBlock(&nanoOsIoState->sdCardState, fatBlock, nanoOsIoState->filesystemState.blockBuffer)) {
    return -1;
  }

  *nextCluster = *((uint16_t*) &nanoOsIoState->filesystemState.blockBuffer
    [(file->currentCluster * sizeof(uint16_t)) % file->bytesPerSector]);
  return 0;
}

/// @fn int fat16HandleClusterTransition(NanoOsIoState *nanoOsIoState, Fat16File *file,
///   bool allocateIfNeeded)
///
/// @brief Handle transitioning to the next cluster, optionally allocating a new
/// one if needed
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param file A pointer to the FAT16 file structure
/// @param allocateIfNeeded Whether to allocate a new cluster if at chain end
///
/// @return Returns 0 on success, -1 on error
static int fat16HandleClusterTransition(NanoOsIoState *nanoOsIoState,
  Fat16File *file, bool allocateIfNeeded
) {
  if ((file->currentPosition % (file->bytesPerSector * 
      file->sectorsPerCluster)) != 0) {
    return 0;
  }

  uint16_t nextCluster;
  if (fat16GetNextCluster(nanoOsIoState, file, &nextCluster)) {
    return -1;
  }

  if (nextCluster >= FAT16_CLUSTER_CHAIN_END) {
    if (!allocateIfNeeded) {
      return -1;
    }

    // Find free cluster
    if (sdSpiReadBlock(&nanoOsIoState->sdCardState, file->fatStart, nanoOsIoState->filesystemState.blockBuffer)) {
      return -1;
    }
    
    uint16_t *fat = (uint16_t*) nanoOsIoState->filesystemState.blockBuffer;
    nextCluster = 0;
    for (uint16_t ii = FAT16_MIN_DATA_CLUSTER;
        ii < FAT16_MAX_CLUSTER_NUMBER; ii++) {
      if (fat[ii] == FAT16_EMPTY_ENTRY) {
        nextCluster = ii;
        break;
      }
    }
    
    if (!nextCluster) {
      return -1;
    }

    // Update FAT entries
    fat[file->currentCluster] = nextCluster;
    fat[nextCluster] = FAT16_CLUSTER_CHAIN_END;
    
    // Write FAT copies
    for (uint8_t ii = 0; ii < file->numberOfFats; ii++) {
      if (sdSpiWriteBlock(&nanoOsIoState->sdCardState, file->fatStart +
          (ii * file->sectorsPerFat), nanoOsIoState->filesystemState.blockBuffer)) {
        return -1;
      }
    }
  }

  file->currentCluster = nextCluster;
  return 0;
}

/// @fn int fat16UpdateDirectoryEntry(NanoOsIoState *nanoOsIoState,
///   Fat16File *file)
///
/// @brief Update the directory entry for a file using stored location
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param file A pointer to the FAT16 file structure
///
/// @return Returns 0 on success, -1 on error
static int fat16UpdateDirectoryEntry(NanoOsIoState *nanoOsIoState,
    Fat16File *file
) {
  if (sdSpiReadBlock(&nanoOsIoState->sdCardState, file->directoryBlock,
      nanoOsIoState->filesystemState.blockBuffer)) {
    return -1;
  }

  uint8_t *entry = nanoOsIoState->filesystemState.blockBuffer +
    file->directoryOffset;

  *((uint32_t*) &entry[FAT16_DIR_FILE_SIZE]) = file->fileSize;
  if (!*((uint16_t*) &entry[FAT16_DIR_FIRST_CLUSTER_LOW])) {
    *((uint16_t*) &entry[FAT16_DIR_FIRST_CLUSTER_LOW]) = file->firstCluster;
  }
  
  return sdSpiWriteBlock(&nanoOsIoState->sdCardState, file->directoryBlock,
    nanoOsIoState->filesystemState.blockBuffer);
}

/// @fn int fat16Read(NanoOsIoState *nanoOsIoState, Fat16File *file, void *buffer,
///   uint32_t length)
///
/// @brief Read an opened FAT file into a provided buffer up to the a specified
/// number of bytes.
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param file A pointer to a previously-opened and initialized Fat16File
///   object.
/// @param buffer A pointer to the memory to read the file contents into.
/// @param length The maximum number of bytes to read from the file.
///
/// @return Returns the number of bytes read from the file.
int fat16Read(NanoOsIoState *nanoOsIoState, Fat16File *file, void *buffer,
    uint32_t length
) {
  if (!length || file->currentPosition >= file->fileSize) {
    return 0;
  }
  
  uint32_t bytesRead = 0;
  uint32_t startByte
    = file->currentPosition & (((uint32_t) file->bytesPerSector) - 1);
  while (bytesRead < length) {
    uint32_t sectorInCluster = (((uint32_t) file->currentPosition) / 
      ((uint32_t) file->bytesPerSector))
      % ((uint32_t) file->sectorsPerCluster);
    uint32_t block = ((uint32_t) file->dataStart) + 
      ((((uint32_t) file->currentCluster) - FAT16_MIN_DATA_CLUSTER) * 
      ((uint32_t) file->sectorsPerCluster)) + ((uint32_t) sectorInCluster);
      
    if (sdSpiReadBlock(&nanoOsIoState->sdCardState, block, nanoOsIoState->filesystemState.blockBuffer)) {
      break;
    }
    
    uint16_t toCopy = min(file->bytesPerSector - startByte, length - bytesRead);
    memcpy((uint8_t*) buffer + bytesRead, &nanoOsIoState->filesystemState.blockBuffer[startByte], toCopy);
    bytesRead += toCopy;
    file->currentPosition += toCopy;
    
    if (fat16HandleClusterTransition(nanoOsIoState, file, false)) {
      break;
    }
    startByte = 0;
  }
  
  return bytesRead;
}

/// @fn int fat16Write(NanoOsIoState *nanoOsIoState, Fat16File *file,
///   const uint8_t *buffer, uint32_t length)
///
/// @brief Write data to a FAT16 file
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param file Pointer to the FAT16 file structure containing file state
/// @param buffer Pointer to the data to write
/// @param length Number of bytes to write from the buffer
///
/// @return Number of bytes written on success, -1 on failure
int fat16Write(NanoOsIoState *nanoOsIoState, Fat16File *file, const uint8_t *buffer,
    uint32_t length
) {
  uint32_t bytesWritten = 0;
  
  while (bytesWritten < length) {
    if (!file->currentCluster) {
      if (fat16HandleClusterTransition(nanoOsIoState, file, true)) {
        return -1;
      }
      if (!file->firstCluster) {
        file->firstCluster = file->currentCluster;
      }
    }

    uint32_t sectorInCluster = (((uint32_t) file->currentPosition) / 
      ((uint32_t) file->bytesPerSector))
      % ((uint32_t) file->sectorsPerCluster);
    uint32_t block = ((uint32_t) file->dataStart) + 
      ((((uint32_t) file->currentCluster) - FAT16_MIN_DATA_CLUSTER) * 
      ((uint32_t) file->sectorsPerCluster)) + ((uint32_t) sectorInCluster);
    uint32_t sectorOffset = file->currentPosition % file->bytesPerSector;
    
    if ((sectorOffset != 0) || 
        (length - bytesWritten < file->bytesPerSector)) {
      if (sdSpiReadBlock(&nanoOsIoState->sdCardState, block, nanoOsIoState->filesystemState.blockBuffer)) {
        return -1;
      }
    }
    
    uint32_t bytesToWrite = file->bytesPerSector - sectorOffset;
    if (bytesToWrite > (length - bytesWritten)) {
      bytesToWrite = length - bytesWritten;
    }
    
    memcpy(nanoOsIoState->filesystemState.blockBuffer + sectorOffset, buffer + bytesWritten,
      bytesToWrite);
    
    if (sdSpiWriteBlock(&nanoOsIoState->sdCardState, block, nanoOsIoState->filesystemState.blockBuffer)) {
      return -1;
    }

    bytesWritten += bytesToWrite;
    file->currentPosition += bytesToWrite;
    if (file->currentPosition > file->fileSize) {
      file->fileSize = file->currentPosition;
    }

    if (fat16HandleClusterTransition(nanoOsIoState, file, true)) {
      return -1;
    }
  }

  return fat16UpdateDirectoryEntry(nanoOsIoState, file) == 0 ? bytesWritten : -1;
}

/// @fn int fat16Seek(NanoOsIoState *nanoOsIoState, Fat16File *file, int32_t offset,
///   uint8_t whence)
///
/// @brief Move the current position of the file to the specified position
/// using optimized cluster traversal.
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param file A pointer to a previously-opened and initialized Fat16File
///   object.
/// @param offset A signed integer value that will be added to the specified
///   position.
/// @param whence The location within the file to apply the offset to. Valid
///   values are SEEK_SET (beginning), SEEK_CUR (current), SEEK_END (end).
///
/// @return Returns 0 on success, -1 on failure.
int fat16Seek(NanoOsIoState *nanoOsIoState, Fat16File *file, int32_t offset,
    uint8_t whence
) {
  // Calculate target position
  uint32_t newPosition = file->currentPosition;
  switch (whence) {
    case SEEK_SET:
      newPosition = offset;
      break;
    case SEEK_CUR:
      newPosition += offset;
      break;
    case SEEK_END:
      newPosition = file->fileSize + offset;
      break;
    default:
      return -1;
  }
  
  // Check bounds
  if (newPosition > file->fileSize) {
    return -1;
  }
  
  // If no movement needed, return early
  if (newPosition == file->currentPosition) {
    return 0;
  }
  
  // Calculate cluster positions
  uint32_t currentClusterIndex = file->currentPosition / file->bytesPerCluster;
  uint32_t targetClusterIndex = newPosition / file->bytesPerCluster;
  uint32_t clustersToTraverse;
  
  if (targetClusterIndex >= currentClusterIndex) {
    // Seeking forward or to the same cluster
    clustersToTraverse = targetClusterIndex - currentClusterIndex;
  } else {
    // Reset to start if seeking backwards
    file->currentPosition = 0;
    file->currentCluster = file->firstCluster;
    clustersToTraverse = targetClusterIndex;
  }
  
  // Fast path: no cluster traversal needed
  if (clustersToTraverse == 0) {
    file->currentPosition = newPosition;
    return 0;
  }
  
  // Read multiple FAT entries at once
  uint32_t currentFatBlock = (uint32_t) -1;
  uint16_t *fatEntries = (uint16_t*) nanoOsIoState->filesystemState.blockBuffer;
  
  while (clustersToTraverse > 0) {
    uint32_t fatBlock = file->fatStart + 
      ((file->currentCluster * sizeof(uint16_t)) / file->bytesPerSector);
    
    // Only read FAT block if different from current
    if (fatBlock != currentFatBlock) {
      if (sdSpiReadBlock(&nanoOsIoState->sdCardState, fatBlock, nanoOsIoState->filesystemState.blockBuffer)) {
        return -1;
      }
      currentFatBlock = fatBlock;
    }
    
    uint16_t nextCluster = fatEntries[(file->currentCluster * sizeof(uint16_t))
      % file->bytesPerSector / sizeof(uint16_t)];
      
    if (nextCluster >= FAT16_CLUSTER_CHAIN_END) {
      return -1;
    }
    
    file->currentCluster = nextCluster;
    file->currentPosition += file->bytesPerCluster;
    clustersToTraverse--;
  }
  
  // Final position adjustment within cluster
  file->currentPosition = newPosition;
  return 0;
}

/// @fn size_t fat16Copy(NanoOsIoState *nanoOsIoState, Fat16File *srcFile,
///   off_t srcStart, Fat16File *dstFile, off_t dstStart, size_t length)
///
/// @brief Copy a specified number of bytes from one file to another at the
/// specified offsets. If the destination file's size is less than dstStart,
/// the file will be padded with zeros up to dstStart before copying begins.
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param srcFile A pointer to the source Fat16File to copy from.
/// @param srcStart The offset within the source file to start copying from.
/// @param dstFile A pointer to the destination Fat16File to copy to.
/// @param dstStart The offset within the destination file to start copying to.
/// @param length The number of bytes to copy.
///
/// @return Returns the number of bytes successfully copied.
size_t fat16Copy(NanoOsIoState *nanoOsIoState,
  Fat16File *srcFile, off_t srcStart,
  Fat16File *dstFile, off_t dstStart, size_t length
) {
  // Verify assumptions
  if (dstFile == NULL) {
    // Nothing to copy to
    return 0;
  } else if ((srcFile != NULL)
    && ((srcFile->bytesPerSector != dstFile->bytesPerSector)
      || ((srcStart & (srcFile->bytesPerSector - 1)) != 0)
    )
  ) {
    // Can't work with this
    return 0;
  } else if (
      ((dstStart & (dstFile->bytesPerSector - 1)) != 0)
      || ((length & (dstFile->bytesPerSector - 1)) != 0)
  ) {
    return 0;
  }

  // Handle padding the destination file if needed
  if (dstFile->fileSize < dstStart) {
    if (fat16Seek(nanoOsIoState, dstFile, dstFile->fileSize, SEEK_SET) != 0) {
      return 0;
    }
    
    memset(nanoOsIoState->filesystemState.blockBuffer, 0, dstFile->bytesPerSector);
    while (dstFile->fileSize < dstStart) {
      uint32_t block = dstFile->dataStart + ((dstFile->currentCluster -
        FAT16_MIN_DATA_CLUSTER) * dstFile->sectorsPerCluster);
      if (sdSpiWriteBlock(&nanoOsIoState->sdCardState, block, nanoOsIoState->filesystemState.blockBuffer)) {
        return 0;
      }

      dstFile->currentPosition += dstFile->bytesPerSector;
      dstFile->fileSize = dstFile->currentPosition;

      if (fat16HandleClusterTransition(nanoOsIoState, dstFile, true)) {
        return 0;
      }
    }
  }

  // Position both files
  if (srcFile != NULL) {
    if (fat16Seek(nanoOsIoState, srcFile, srcStart, SEEK_SET) != 0) {
      return 0;
    }
  } else {
    memset(nanoOsIoState->filesystemState.blockBuffer, 0, dstFile->bytesPerSector);
  }
  if (fat16Seek(nanoOsIoState, dstFile, dstStart, SEEK_SET) != 0) {
    return 0;
  }

  size_t remainingBytes = length;
  uint32_t sectorInCluster = 0;
  while (remainingBytes > 0) {
    if (srcFile != NULL) {
      // Read source block
      sectorInCluster = (((uint32_t) srcFile->currentPosition) / 
        ((uint32_t) srcFile->bytesPerSector))
        % ((uint32_t) srcFile->sectorsPerCluster);
      uint32_t srcBlock = ((uint32_t) srcFile->dataStart) + 
        ((((uint32_t) srcFile->currentCluster) - FAT16_MIN_DATA_CLUSTER) * 
        ((uint32_t) srcFile->sectorsPerCluster)) + ((uint32_t) sectorInCluster);
      if (sdSpiReadBlock(&nanoOsIoState->sdCardState, srcBlock, nanoOsIoState->filesystemState.blockBuffer)) {
        return length - remainingBytes;
      }
    }

    // Write to destination
    sectorInCluster = (((uint32_t) dstFile->currentPosition) / 
      ((uint32_t) dstFile->bytesPerSector))
      % ((uint32_t) dstFile->sectorsPerCluster);
    uint32_t dstBlock = ((uint32_t) dstFile->dataStart) + 
      ((((uint32_t) dstFile->currentCluster) - FAT16_MIN_DATA_CLUSTER) * 
      ((uint32_t) dstFile->sectorsPerCluster)) + ((uint32_t) sectorInCluster);
    if (sdSpiWriteBlock(&nanoOsIoState->sdCardState, dstBlock, nanoOsIoState->filesystemState.blockBuffer)) {
      return length - remainingBytes;
    }

    // Update positions
    dstFile->currentPosition += dstFile->bytesPerSector;
    remainingBytes -= dstFile->bytesPerSector;
    if (dstFile->currentPosition > dstFile->fileSize) {
      dstFile->fileSize = dstFile->currentPosition;
    }

    // Handle cluster transitions
    if (srcFile != NULL) {
      srcFile->currentPosition += srcFile->bytesPerSector;
      if (fat16HandleClusterTransition(nanoOsIoState, srcFile, false) != 0) {
        return length - remainingBytes;
      }
    }
    if (fat16HandleClusterTransition(nanoOsIoState, dstFile, true) != 0) {
      return length - remainingBytes;
    }
  }

  fat16UpdateDirectoryEntry(nanoOsIoState, dstFile);

  return length - remainingBytes;
}

/// @fn int fat16Remove(NanoOsIoState *nanoOsIoState, const char *pathname)
///
/// @brief Remove (delete) a file from the FAT16 filesystem
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param pathname The name of the file to remove
///
/// @return 0 on success, -1 on failure
int fat16Remove(NanoOsIoState *nanoOsIoState, const char *pathname) {
  // We need a file handle to access the cached values
  Fat16File *file = fat16Fopen(nanoOsIoState, pathname, "r");
  if (!file) {
    return -1;
  }
  
  uint8_t *entry;
  uint32_t block;
  int result = fat16FindDirectoryEntry(nanoOsIoState, file, pathname, &entry, &block,
    NULL);
    
  if (result != FAT16_DIR_SEARCH_FOUND) {
    free(file);
    return -1;
  }
  
  // Get the first cluster of the file's chain
  uint16_t currentCluster = 
    *((uint16_t*) &entry[FAT16_DIR_FIRST_CLUSTER_LOW]);

  // Mark the directory entry as deleted
  entry[FAT16_DIR_FILENAME] = FAT16_DELETED_MARKER;
  result = sdSpiWriteBlock(&nanoOsIoState->sdCardState, block, nanoOsIoState->filesystemState.blockBuffer);
  if (result != 0) {
    free(file);
    return -1;
  }

  // Follow cluster chain to mark all clusters as free
  while (currentCluster != 0 && currentCluster < FAT16_CLUSTER_CHAIN_END) {
    // Calculate FAT sector containing current cluster entry
    uint32_t fatBlock = file->fatStart + 
      ((currentCluster * sizeof(uint16_t)) / file->bytesPerSector);

    // Read the FAT block
    if (sdSpiReadBlock(&nanoOsIoState->sdCardState, fatBlock, nanoOsIoState->filesystemState.blockBuffer)) {
      free(file);
      return -1;
    }

    // Get next cluster in chain before marking current as free
    uint16_t nextCluster = *((uint16_t*) &nanoOsIoState->filesystemState.blockBuffer
      [(currentCluster * sizeof(uint16_t)) % file->bytesPerSector]);

    // Mark current cluster as free (0x0000)
    *((uint16_t*) &nanoOsIoState->filesystemState.blockBuffer
      [(currentCluster * sizeof(uint16_t)) % file->bytesPerSector]) = 0;

    // Write updated FAT block
    for (uint8_t ii = 0; ii < file->numberOfFats; ii++) {
      if (sdSpiWriteBlock(&nanoOsIoState->sdCardState, fatBlock + (ii * file->sectorsPerFat),
          nanoOsIoState->filesystemState.blockBuffer)) {
        free(file);
        return -1;
      }
    }

    currentCluster = nextCluster;
  }
  
  free(file);
  return result;
}

/// @fn int getPartitionInfo(NanoOsIoState *nanoOsIoState)
///
/// @brief Get information about the partition for the provided filesystem.
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
///
/// @return Returns 0 on success, negative error code on failure.
int getPartitionInfo(NanoOsIoState *nanoOsIoState) {
  if (nanoOsIoState->filesystemState.partitionNumber == 0) {
    return -1;
  }

  if (sdSpiReadBlock(&nanoOsIoState->sdCardState, 0, nanoOsIoState->filesystemState.blockBuffer) != 0) {
    return -2;
  }

  uint8_t *partitionTable = nanoOsIoState->filesystemState.blockBuffer + FAT16_PARTITION_TABLE_OFFSET;
  uint8_t *entry = partitionTable +
    ((nanoOsIoState->filesystemState.partitionNumber - 1) * FAT16_PARTITION_ENTRY_SIZE);
  uint8_t type = entry[4];
  
  if ((type == FAT16_PARTITION_TYPE_FAT16_LBA) || 
      (type == FAT16_PARTITION_TYPE_FAT16_LBA_EXTENDED)) {
    nanoOsIoState->filesystemState.startLba = (((uint32_t) entry[FAT16_PARTITION_LBA_OFFSET + 3]) << 24) |
      (((uint32_t) entry[FAT16_PARTITION_LBA_OFFSET + 2]) << 16) |
      (((uint32_t) entry[FAT16_PARTITION_LBA_OFFSET + 1]) << 8) |
      ((uint32_t) entry[FAT16_PARTITION_LBA_OFFSET]);
      
    uint32_t numSectors = 
      (((uint32_t) entry[FAT16_PARTITION_SECTORS_OFFSET + 3]) << 24) |
      (((uint32_t) entry[FAT16_PARTITION_SECTORS_OFFSET + 2]) << 16) |
      (((uint32_t) entry[FAT16_PARTITION_SECTORS_OFFSET + 1]) << 8) |
      ((uint32_t) entry[FAT16_PARTITION_SECTORS_OFFSET]);
      
    nanoOsIoState->filesystemState.endLba = nanoOsIoState->filesystemState.startLba + numSectors - 1;
    return 0;
  }
  
  return -3;
}

/// @fn int fat16FilesystemOpenFileCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage)
///
/// @brief Command handler for NANO_OS_IO_OPEN_FILE command.
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemOpenFileCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage
) {
  const char *pathname = nanoOsMessageDataPointer(processMessage, char*);
  const char *mode = nanoOsMessageFuncPointer(processMessage, char*);
  Fat16File *fat16File = fat16Fopen(nanoOsIoState, pathname, mode);
  NanoOsFile *nanoOsFile = NULL;
  if (fat16File != NULL) {
    nanoOsFile = (NanoOsFile*) malloc(sizeof(NanoOsFile));
    nanoOsFile->file = fat16File;
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = (intptr_t) nanoOsFile;
  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int fat16FilesystemCloseFileCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage)
///
/// @brief Command handler for NANO_OS_IO_CLOSE_FILE command.
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemCloseFileCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage
) {
  NanoOsFile *nanoOsFile
    = nanoOsMessageDataPointer(processMessage, NanoOsFile*);
  Fat16File *fat16File = (Fat16File*) nanoOsFile->file;
  free(fat16File);
  free(nanoOsFile);
  if (nanoOsIoState->filesystemState.numOpenFiles > 0) {
    nanoOsIoState->filesystemState.numOpenFiles--;
    if (nanoOsIoState->filesystemState.numOpenFiles == 0) {
      free(nanoOsIoState->filesystemState.blockBuffer); nanoOsIoState->filesystemState.blockBuffer = NULL;
    }
  }

  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int fat16FilesystemReadFileCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage)
///
/// @brief Command handler for NANO_OS_IO_READ_FILE command.
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemReadFileCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage
) {
  NanoOsIoCommandParameters *nanoOsIoIoCommandParameters
    = nanoOsMessageDataPointer(processMessage, NanoOsIoCommandParameters*);
  int returnValue = fat16Read(nanoOsIoState,
    (Fat16File*) nanoOsIoIoCommandParameters->file->file,
    nanoOsIoIoCommandParameters->buffer,
    nanoOsIoIoCommandParameters->length);
  if (returnValue >= 0) {
    // Return value is the number of bytes read.  Set the length variable to it
    // and set it to 0 to indicate good status.
    nanoOsIoIoCommandParameters->length = returnValue;
    returnValue = 0;
  } else {
    // Return value is a negative error code.  Negate it.
    returnValue = -returnValue;
    // Tell the caller that we read nothing.
    nanoOsIoIoCommandParameters->length = 0;
  }

  processMessageSetDone(processMessage);
  return returnValue;
}

/// @fn int fat16FilesystemWriteFileCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage)
///
/// @brief Command handler for NANO_OS_IO_WRITE_FILE command.
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemWriteFileCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage
) {
  NanoOsIoCommandParameters *nanoOsIoIoCommandParameters
    = nanoOsMessageDataPointer(processMessage, NanoOsIoCommandParameters*);
  int returnValue = fat16Write(nanoOsIoState,
    (Fat16File*) nanoOsIoIoCommandParameters->file->file,
    (uint8_t*) nanoOsIoIoCommandParameters->buffer,
    nanoOsIoIoCommandParameters->length);
  if (returnValue >= 0) {
    // Return value is the number of bytes written.  Set the length variable to
    // it and set it to 0 to indicate good status.
    nanoOsIoIoCommandParameters->length = returnValue;
    returnValue = 0;
  } else {
    // Return value is a negative error code.  Negate it.
    returnValue = -returnValue;
    // Tell the caller that we wrote nothing.
    nanoOsIoIoCommandParameters->length = 0;
  }

  processMessageSetDone(processMessage);
  return returnValue;
}

/// @fn int fat16FilesystemRemoveFileCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage)
///
/// @brief Command handler for NANO_OS_IO_REMOVE_FILE command.
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemRemoveFileCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage
) {
  const char *pathname = nanoOsMessageDataPointer(processMessage, char*);
  int returnValue = fat16Remove(nanoOsIoState, pathname);

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = returnValue;
  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int fat16FilesystemSeekFileCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage)
///
/// @brief Command handler for NANO_OS_IO_SEEK_FILE command.
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemSeekFileCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage
) {
  NanoOsIoSeekParameters *nanoOsIoSeekParameters
    = nanoOsMessageDataPointer(processMessage, NanoOsIoSeekParameters*);
  int returnValue = fat16Seek(nanoOsIoState,
    (Fat16File*) nanoOsIoSeekParameters->stream->file,
    nanoOsIoSeekParameters->offset,
    nanoOsIoSeekParameters->whence);

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = returnValue;
  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int fat16FilesystemCopyFileCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage)
///
/// @brief Command handler for NANO_OS_IO_COPY_FILE command.
///
/// @param nanoOsIoState A pointer to the NanoOsIoState in the I/O process
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemCopyFileCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *processMessage
) {
  FcopyArgs *fcopyArgs = nanoOsMessageDataPointer(processMessage, FcopyArgs*);
  size_t returnValue
    = fat16Copy(nanoOsIoState,
      (Fat16File*) fcopyArgs->srcFile->file,
      fcopyArgs->srcStart,
      (Fat16File*) fcopyArgs->dstFile->file,
      fcopyArgs->dstStart,
      fcopyArgs->length);

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = returnValue;
  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int consolePrintMessage(ConsoleState *consoleState,
///   ProcessMessage *inputMessage, const char *message)
///
/// @brief Print a message to all console ports that are owned by a process.
///
/// @param consoleState The ConsoleState being maintained by the runConsole
///   function.
/// @param inputMessage The message received from the process printing the
///   message.
/// @param message The formatted string message to print.
///
/// @return Returns processSuccess on success, processError on failure.
int consolePrintMessage(
  ConsoleState *consoleState, ProcessMessage *inputMessage, const char *message
) {
  int returnValue = processSuccess;
  ProcessId owner = processId(processMessageFrom(inputMessage));
  ConsolePort *consolePorts = consoleState->consolePorts;

  bool portFound = false;
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (consolePorts[ii].outputOwner == owner) {
      consolePorts[ii].printString(message);
      portFound = true;
    }
  }

  if (portFound == false) {
    printString("WARNING:  Request to print message \"");
    printString(message);
    printString("\" from non-owning process ");
    printInt(owner);
    printString("\n");
    returnValue = processError;
  }

  return returnValue;
}

/// @fn void consoleMessageCleanup(ProcessMessage *inputMessage)
///
/// @brief Release an input ProcessMessage if there are no waiters for the message.
///
/// @param inputMessage A pointer to the ProcessMessage to cleanup.
///
/// @return This function returns no value.
void consoleMessageCleanup(ProcessMessage *inputMessage) {
  if (processMessageWaiting(inputMessage) == false) {
    if (processMessageRelease(inputMessage) != processSuccess) {
      Serial.print("ERROR!!!  Could not release inputMessage from ");
      Serial.print(__func__);
      Serial.print("\n");
    }
  }
}

/// @fn ConsoleBuffer* getAvailableConsoleBuffer(
///   ConsoleState *consoleState, ProcessId processId)
///
/// @brief Get an available console buffer and mark it as being in use.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  The buffers in this state will be searched for
/// @param processId The numerical ID of the process (PID) making the request
///   for a buffer (if any).
///
/// @return Returns a pointer to the available ConsoleBuffer on success, NULL
/// on failure.
ConsoleBuffer* getAvailableConsoleBuffer(
  ConsoleState *consoleState, ProcessId processId
) {
  ConsoleBuffer *consoleBuffers = consoleState->consoleBuffers;
  ConsoleBuffer *returnValue = NULL;

  // Check to see if the requesting process owns one of the ports for output.
  // Use the buffer for that port if so.
  ConsolePort *consolePorts = consoleState->consolePorts;
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if ((consolePorts[ii].outputOwner == processId)
      || (consolePorts[ii].inputOwner == processId)
    ) {
      returnValue = &consoleBuffers[ii];
      // returnValue->inUse is already set to true, so no need to set it.
      break;
    }
  }

  if (returnValue == NULL) {
    returnValue = (ConsoleBuffer*) malloc(sizeof(ConsoleBuffer));
  }

  return returnValue;
}

/// @fn int consoleWriteValueCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Command handler for the NANO_OS_IO_WRITE_VALUE command.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure that manages the consoles.
/// @param inputMessage A pointer to the ProcessMessage that was received from
///   the process that sent the command.
///
/// @return Always returns 0 and sets the inputMessage to done so that the
/// calling process knows that we've handled the message.
int consoleWriteValueCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage
) {
  char staticBuffer[19]; // max length of a 64-bit value is 18 digits plus NULL.
  ConsoleValueType valueType
    = nanoOsMessageFuncValue(inputMessage, ConsoleValueType);
  const char *message = NULL;

  switch (valueType) {
    case CONSOLE_VALUE_CHAR:
      {
        char value = nanoOsMessageDataValue(inputMessage, char);
        sprintf(staticBuffer, "%c", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_UCHAR:
      {
        unsigned char value
          = nanoOsMessageDataValue(inputMessage, unsigned char);
        sprintf(staticBuffer, "%u", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_INT:
      {
        int value = nanoOsMessageDataValue(inputMessage, int);
        sprintf(staticBuffer, "%d", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_UINT:
      {
        unsigned int value
          = nanoOsMessageDataValue(inputMessage, unsigned int);
        sprintf(staticBuffer, "%u", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_LONG_INT:
      {
        long int value = nanoOsMessageDataValue(inputMessage, long int);
        sprintf(staticBuffer, "%ld", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_LONG_UINT:
      {
        long unsigned int value
          = nanoOsMessageDataValue(inputMessage, long unsigned int);
        sprintf(staticBuffer, "%lu", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_FLOAT:
      {
        float value = nanoOsMessageDataValue(inputMessage, float);
        sprintf(staticBuffer, "%f", (double) value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_DOUBLE:
      {
        double value = nanoOsMessageDataValue(inputMessage, double);
        sprintf(staticBuffer, "%lf", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_STRING:
      {
        message = nanoOsMessageDataPointer(inputMessage, const char*);
      }
      break;

    default:
      // Do nothing.
      break;

  }

  // It's possible we were passed a bad type that didn't result in the value of
  // message being set, so only attempt to print it if it was set.
  if (message != NULL) {
    consolePrintMessage(&nanoOsIoState->consoleState, inputMessage, message);
  }

  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return 0;
}

/// @fn int consoleGetBufferCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Command handler for the NANO_OS_IO_GET_BUFFER command.  Gets a free
/// buffer from the provided ConsoleState and replies to the message sender with
/// a pointer to the buffer structure.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure held by the runConsole process.  The buffers in this state will
///   be searched for something available to return to the message sender.
/// @param inputMessage A pointer to the ProcessMessage that was received from
///   the process that sent the command.
///
/// @return This function always returns zero and sets the inputMessage to
/// done so that the calling process knows that we've handled the message.
/// Since this is a synchronous call, it also pushes a message onto the message
/// sender's queue with the free buffer on success.  On failure, the
/// inputMessage is marked as done but no response is sent.
int consoleGetBufferCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage
) {
  // We're going to reuse the input message as the return message.
  ProcessMessage *returnMessage = inputMessage;
  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(returnMessage);
  nanoOsMessage->func = 0;
  nanoOsMessage->data = (intptr_t) NULL;
  ProcessId callingPid = processId(processMessageFrom(inputMessage));

  ConsoleBuffer *returnValue
    = getAvailableConsoleBuffer(&nanoOsIoState->consoleState, callingPid);
  if (returnValue != NULL) {
    // Send the buffer back to the caller via the message we allocated earlier.
    nanoOsMessage->data = (intptr_t) returnValue;
    processMessageInit(returnMessage, NANO_OS_IO_RETURNING_BUFFER,
      nanoOsMessage, sizeof(*nanoOsMessage), true);
    if (processMessageQueuePush(processMessageFrom(inputMessage), returnMessage)
      != processSuccess
    ) {
      returnValue->inUse = false;
    }
  }

  // Whether we were able to grab a buffer or not, we're now done with this
  // call, so mark the input message handled.  This is a synchronous call and
  // the caller is waiting on our response, so *DO NOT* release it.  The caller
  // is responsible for releasing it when they've received the response.
  processMessageSetDone(inputMessage);
  return 0;
}

/// @fn int consoleWriteBufferCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Command handler for the NANO_OS_IO_WRITE_BUFFER command.  Writes the
/// contents of the sent buffer to the console and then clears its inUse flag.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure held by the runConsole process.
/// @param inputMessage A pointer to the ProcessMessage that was received from
///   the process that sent the command.
///
/// @return This function always returns zero and sets the inputMessage to
/// done so that the calling process knows that we've handled the message.
int consoleWriteBufferCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage
) {
  ConsoleBuffer *consoleBuffer
    = nanoOsMessageDataPointer(inputMessage, ConsoleBuffer*);
  if (consoleBuffer != NULL) {
    const char *message = consoleBuffer->buffer;
    if (message != NULL) {
      consolePrintMessage(&nanoOsIoState->consoleState, inputMessage, message);
    }
  }
  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return 0;
}

/// @fn int consoleSetPortShellCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Set the designated shell process ID for a port.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure held by the runConsole process.
/// @param inputMessage A pointer to the ProcessMessage with the received
///   command.  This contains a NanoOsMessage that contains a
///   ConsolePortPidAssociation that will associate the port with the process
///   if this function succeeds.
///
/// @return This function always returns zero and it marks the inputMessage as
/// being 'done' on success and does *NOT* mark it on failure.
int consoleSetPortShellCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage
) {
  ConsolePortPidUnion consolePortPidUnion;
  consolePortPidUnion.nanoOsMessageData
    = nanoOsMessageDataValue(inputMessage, NanoOsMessageData);
  ConsolePortPidAssociation *consolePortPidAssociation
    = &consolePortPidUnion.consolePortPidAssociation;

  uint8_t consolePort = consolePortPidAssociation->consolePort;
  ProcessId processId = consolePortPidAssociation->processId;

  if (consolePort < CONSOLE_NUM_PORTS) {
    nanoOsIoState->consoleState.consolePorts[consolePort].shell = processId;
    processMessageSetDone(inputMessage);
    consoleMessageCleanup(inputMessage);
  } else {
    printString("ERROR:  Request to assign ownership of non-existent port ");
    printInt(consolePort);
    printString("\n");
    // *DON'T* call processMessageRelease or processMessageSetDone here.  The lack of the
    // message being done will indicate to the caller that there was a problem
    // servicing the command.
  }

  return 0;
}

/// @fn int consoleAssignPortHelper(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Assign a console port's input and possibly output to a running
/// process.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure held by the runConsole process.
/// @param inputMessage A pointer to the ProcessMessage with the received
///   command.  This contains a NanoOsMessage that contains a
///   ConsolePortPidAssociation that will associate the port's input with the
///   process if this function succeeds.
/// @param assignOutput Whether or not to assign the output as well as the
///   input.
///
/// @return This function always returns zero and it marks the inputMessage as
/// being 'done' on success and does *NOT* mark it on failure.
int consoleAssignPortHelper(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage, bool assignOutput
) {
  ConsolePortPidUnion consolePortPidUnion;
  consolePortPidUnion.nanoOsMessageData
    = nanoOsMessageDataValue(inputMessage, NanoOsMessageData);
  ConsolePortPidAssociation *consolePortPidAssociation
    = &consolePortPidUnion.consolePortPidAssociation;

  uint8_t consolePort = consolePortPidAssociation->consolePort;
  ProcessId processId = consolePortPidAssociation->processId;

  if (consolePort < CONSOLE_NUM_PORTS) {
    if (assignOutput == true) {
      nanoOsIoState->consoleState.consolePorts[consolePort].outputOwner
        = processId;
    }
    nanoOsIoState->consoleState.consolePorts[consolePort].inputOwner
      = processId;
    processMessageSetDone(inputMessage);
    consoleMessageCleanup(inputMessage);
  } else {
    printString("ERROR:  Request to assign ownership of non-existent port ");
    printInt(consolePort);
    printString("\n");
    // *DON'T* call processMessageRelease or processMessageSetDone here.  The
    // lack of the message being done will indicate to the caller that there
    // was a problem servicing the command.
  }

  return 0;
}

/// @fn int consoleAssignPortCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Assign a console port's input and output to a running process.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure held by the runConsole process.
/// @param inputMessage A pointer to the ProcessMessage with the received
///   command.  This contains a NanoOsMessage that contains a
///   ConsolePortPidAssociation that will associate the port's input and output
///   with the process if this function succeeds.
///
/// @return This function always returns zero and it marks the inputMessage as
/// being 'done' on success and does *NOT* mark it on failure.
int consoleAssignPortCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage
) {
  consoleAssignPortHelper(&nanoOsIoState->consoleState, inputMessage, true);

  return 0;
}

/// @fn int consoleAssignPortInputCommandHandler(
///   NanoOsIoState *NanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Assign a console port's input to a running process.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure held by the runConsole process.
/// @param inputMessage A pointer to the ProcessMessage with the received
///   command.  This contains a NanoOsMessage that contains a
///   ConsolePortPidAssociation that will associate the port's input with the
///   process if this function succeeds.
///
/// @return This function always returns zero and it marks the inputMessage as
/// being 'done' on success and does *NOT* mark it on failure.
int consoleAssignPortInputCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage
) {
  consoleAssignPortHelper(&nanoOsIoState->consoleState, inputMessage, false);

  return 0;
}

/// @fn int consoleReleasePortCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Release all the ports currently owned by a process.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure held by the runConsole process.
/// @param inputMessage A pointer to the ProcessMessage with the received
///   command.
///
/// @return This function always returns zero and it marks the input message as
/// done.
int consoleReleasePortCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage
) {
  ProcessId owner = processId(processMessageFrom(inputMessage));
  ConsolePort *consolePorts = nanoOsIoState->consoleState.consolePorts;

  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (consolePorts[ii].outputOwner == owner) {
      consolePorts[ii].outputOwner = consolePorts[ii].shell;
    }
    if (consolePorts[ii].inputOwner == owner) {
      consolePorts[ii].inputOwner = consolePorts[ii].shell;
    }
  }

  // Since piped commands still attempt to release a port on completion, we
  // will not print a warning if we didn't successfully release anything.

  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return 0;
}

/// @fn int consoleGetOwnedPortCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Get the first port currently owned by a process.
///
/// @note While it is technically possible for a single process to own multiple
/// ports, the expectation here is that this call is made by a process that is
/// only expecting to own one.  This is mostly for the purposes of transferring
/// ownership of the port from one process to another.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure held by the runConsole process.
/// @param inputMessage A pointer to the ProcessMessage with the received
///   command.
///
/// @return This function always returns zero and it marks the input message as
/// done.
int consoleGetOwnedPortCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage
) {
  ProcessId owner = processId(processMessageFrom(inputMessage));
  ConsolePort *consolePorts = nanoOsIoState->consoleState.consolePorts;
  ProcessMessage *returnMessage = inputMessage;

  int ownedPort = -1;
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    // inputOwner is assigned at the same as outputOwner, but inputOwner can be
    // set separately later if the commands are being piped together.
    // Therefore, checking inputOwner checks both of them.
    if (consolePorts[ii].inputOwner == owner) {
      ownedPort = ii;
      break;
    }
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(returnMessage);
  nanoOsMessage->func = 0;
  nanoOsMessage->data = (intptr_t) ownedPort;
  processMessageInit(returnMessage, NANO_OS_IO_RETURNING_PORT,
    nanoOsMessage, sizeof(*nanoOsMessage), true);
  sendProcessMessageToPid(owner, inputMessage);

  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return 0;
}

/// @fn int consoleSetEchoCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Set whether or not input is echoed back to all console ports owned
/// by a process.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure held by the runConsole process.
/// @param inputMessage A pointer to the ProcessMessage with the received
///   command.
///
/// @return This function always returns 0 and marks the input message as done.
int consoleSetEchoCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage
) {
  ProcessId owner = processId(processMessageFrom(inputMessage));
  ConsolePort *consolePorts = nanoOsIoState->consoleState.consolePorts;
  ProcessMessage *returnMessage = inputMessage;
  bool desiredEchoState = nanoOsMessageDataValue(inputMessage, bool);
  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(returnMessage);
  nanoOsMessage->func = 0;
  nanoOsMessage->data = 0;

  bool portFound = false;
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (consolePorts[ii].outputOwner == owner) {
      consolePorts[ii].echo = desiredEchoState;
      portFound = true;
    }
  }

  if (portFound == false) {
    printString("WARNING:  Request to set echo from non-owning process ");
    printInt(owner);
    printString("\n");
    nanoOsMessage->data = (intptr_t) -1;
  }

  processMessageInit(returnMessage, NANO_OS_IO_RETURNING_PORT,
    nanoOsMessage, sizeof(*nanoOsMessage), true);
  sendProcessMessageToPid(owner, inputMessage);
  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return 0;
}

/// @fn int consoleWaitForInputCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Wait for input from any of the console ports owned by a process.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure held by the runConsole process.
/// @param inputMessage A pointer to the ProcessMessage with the received
///   command.
///
/// @return This function always returns zero and marks the input message as
/// done.
int consoleWaitForInputCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage
) {
  ProcessId owner = processId(processMessageFrom(inputMessage));
  ConsolePort *consolePorts = nanoOsIoState->consoleState.consolePorts;

  bool portFound = false;
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (consolePorts[ii].inputOwner == owner) {
      consolePorts[ii].waitingForInput = true;
      portFound = true;
    }
  }

  if (portFound == false) {
    printString("WARNING:  Request to wait for input from non-owning process ");
    printInt(owner);
    printString("\n");
  }

  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return 0;
}

/// @fn int consoleReleasePidPortCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Release all the ports currently owned by a process.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure held by the runConsole process.
/// @param inputMessage A pointer to the ProcessMessage with the received
///   command.
///
/// @return This function always returns 0 and marks the input message as done.
int consoleReleasePidPortCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage
) {
  ProcessId sender = processId(processMessageFrom(inputMessage));
  if (sender != NANO_OS_SCHEDULER_PROCESS_ID) {
    // Sender is not the scheduler.  We will ignore this.
    processMessageSetDone(inputMessage);
    consoleMessageCleanup(inputMessage);
    return;
  }

  ProcessId owner
    = nanoOsMessageDataValue(inputMessage, ProcessId);
  ConsolePort *consolePorts = nanoOsIoState->consoleState.consolePorts;
  ProcessMessage *processMessage
    = nanoOsMessageFuncPointer(inputMessage, ProcessMessage*);
  bool releaseMessage = false;

  bool portFound = false;
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (consolePorts[ii].inputOwner == owner) {
      consolePorts[ii].inputOwner = consolePorts[ii].shell;
      // NOTE:  By calling sendProcessMessageToPid from within the for loop, we
      // run the risk of sending the same message to multiple shells.  That's
      // irrelevant in this case since nothing is waiting for the message and
      // all the shells will release the message.  In reality, one process
      // almost never owns multiple ports.  The only exception is during boot.
      if (owner != consolePorts[ii].shell) {
        sendProcessMessageToPid(consolePorts[ii].shell, processMessage);
      } else {
        // The shell is being restarted.  It won't be able to receive the
        // message if we send it, so we need to go ahead and release it.
        releaseMessage = true;
      }
      portFound = true;
    }
    if (consolePorts[ii].outputOwner == owner) {
      consolePorts[ii].outputOwner = consolePorts[ii].shell;
      if (owner == consolePorts[ii].shell) {
        // The shell is being restarted.  It won't be able to receive the
        // message if we send it, so we need to go ahead and release it.
        releaseMessage = true;
      }
      portFound = true;
    }
  }

  if ((releaseMessage == true) || (portFound == false)) {
    processMessageRelease(processMessage);
  }

  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return 0;
}

/// @fn int consoleReleaseBufferCommandHandler(
///   NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage)
///
/// @brief Command handler for the NANO_OS_IO_RELEASE_BUFFER command.  Releases
/// a buffer that was previously returned by the NANO_OS_IO_GET_BUFFER command.
/// a pointer to the buffer structure.
///
/// @param nanoOsIoState A pointer to the state that contains the ConsoleState
///   structure held by the runConsole process.
///   member set to false.
/// @param inputMessage A pointer to the ProcessMessage that was received from
///   the process that sent the command.
///
/// @return This function always returns zero and sets the inputMessage to
/// done so that the calling process knows that we've handled the message.
/// Since this is a synchronous call, it also pushes a message onto the message
/// sender's queue with the free buffer on success.  On failure, the
/// inputMessage is marked as done but no response is sent.
int consoleReleaseBufferCommandHandler(
  NanoOsIoState *nanoOsIoState, ProcessMessage *inputMessage
) {
  ConsoleBuffer *consoleBuffers = nanoOsIoState->consoleState.consoleBuffers;
  ConsoleBuffer *consoleBuffer
    = nanoOsMessageDataPointer(inputMessage, ConsoleBuffer*);
  if (consoleBuffer != NULL) {
    for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
      if (consoleBuffer == &consoleBuffers[ii]) {
        // The buffer being released is one of the buffers dedicated to a port.
        // *DO NOT* mark it as not being in use because it is always in use.
        // Just release the message and return.
        processMessageRelease(inputMessage);
        return;
      }
    }

    free(consoleBuffer); consoleBuffer = NULL;
  }

  // Whether we were able to grab a buffer or not, we're now done with this
  // call, so mark the input message handled.  This is a synchronous call and
  // the caller is waiting on our response, so *DO NOT* release it.  The caller
  // is responsible for releasing it when they've received the response.
  processMessageRelease(inputMessage);
  return 0;
}

/// @fn int readSerialByte(ConsolePort *consolePort, UartClass &serialPort)
///
/// @brief Do a non-blocking read of a serial port.
///
/// @param ConsolePort A pointer to the ConsolePort data structure that contains
///   the buffer information to use.
/// @param serialPort A reference to the UartClass object (Serial or Serial1)
///   to read a byte from.
///
/// @return Returns the byte read, cast to an int, on success, -1 on failure.
int readSerialByte(ConsolePort *consolePort, UartClass &serialPort) {
  int serialData = -1;
  serialData = serialPort.read();
  if (serialData > -1) {
    ConsoleBuffer *consoleBuffer = consolePort->consoleBuffer;
    char *buffer = consoleBuffer->buffer;
    buffer[consolePort->consoleIndex] = (char) serialData;
    if (consolePort->echo == true) {
      if (((char) serialData != '\r')
        && ((char) serialData != '\n')
      ) {
        serialPort.print((char) serialData);
      } else {
        serialPort.print("\r\n");
      }
    }
    consolePort->consoleIndex++;
    consolePort->consoleIndex %= CONSOLE_BUFFER_SIZE;
  }

  return serialData;
}

/// @fn int readUsbSerialByte(ConsolePort *consolePort)
///
/// @brief Do a non-blocking read of the USB serial port.
///
/// @param ConsolePort A pointer to the ConsolePort data structure that contains
///   the buffer information to use.
///
/// @return Returns the byte read, cast to an int, on success, -1 on failure.
int readUsbSerialByte(ConsolePort *consolePort) {
  return readSerialByte(consolePort, Serial);
}

/// @fn int readGpioSerialByte(ConsolePort *consolePort)
///
/// @brief Do a non-blocking read of the GPIO serial port.
///
/// @param ConsolePort A pointer to the ConsolePort data structure that contains
///   the buffer information to use.
///
/// @return Returns the byte read, cast to an int, on success, -1 on failure.
int readGpioSerialByte(ConsolePort *consolePort) {
  return readSerialByte(consolePort, Serial1);
}

/// @fn int printSerialString(UartClass &serialPort, const char *string)
///
/// @brief Print a string to the default serial port.
///
/// @param serialPort A reference to the UartClass object (Serial or Serial1)
///   to read a byte from.
/// @param string A pointer to the string to print.
///
/// @return Returns the number of bytes written to the serial port.
int printSerialString(UartClass &serialPort, const char *string) {
  int returnValue = 0;
  size_t numBytes = 0;

  char *newlineAt = strchr(string, '\n');
  newlineAt = strchr(string, '\n');
  if (newlineAt == NULL) {
    numBytes = strlen(string);
  } else {
    numBytes = (size_t) (((uintptr_t) newlineAt) - ((uintptr_t) string));
  }
  while (newlineAt != NULL) {
    returnValue += (int) serialPort.write(string, numBytes);
    returnValue += (int) serialPort.write("\r\n");
    string = newlineAt + 1;
    newlineAt = strchr(string, '\n');
    if (newlineAt == NULL) {
      numBytes = strlen(string);
    } else {
      numBytes = (size_t) (((uintptr_t) newlineAt) - ((uintptr_t) string));
    }
  }
  returnValue += (int) serialPort.write(string, numBytes);

  return returnValue;
}

/// @fn int printUsbSerialString(const char *string)
///
/// @brief Print a string to the USB serial port.
///
/// @param string A pointer to the string to print.
///
/// @return Returns the number of bytes written to the serial port.
int printUsbSerialString(const char *string) {
  return printSerialString(Serial, string);
}

/// @fn int printGpioSerialString(const char *string)
///
/// @brief Print a string to the GPIO serial port.
///
/// @param string A pointer to the string to print.
///
/// @return Returns the number of bytes written to the serial port.
int printGpioSerialString(const char *string) {
  return printSerialString(Serial1, string);
}

/// @var nanoOsIoCommandHandlers
///
/// @brief Array of NanoOsIoCommandHandler function pointers.
const NanoOsIoCommandHandler nanoOsIoCommandHandlers[] = {
  fat16FilesystemOpenFileCommandHandler,   // NANO_OS_IO_OPEN_FILE
  fat16FilesystemCloseFileCommandHandler,  // NANO_OS_IO_CLOSE_FILE
  fat16FilesystemReadFileCommandHandler,   // NANO_OS_IO_READ_FILE
  fat16FilesystemWriteFileCommandHandler,  // NANO_OS_IO_WRITE_FILE
  fat16FilesystemRemoveFileCommandHandler, // NANO_OS_IO_REMOVE_FILE
  fat16FilesystemSeekFileCommandHandler,   // NANO_OS_IO_SEEK_FILE
  fat16FilesystemCopyFileCommandHandler,   // NANO_OS_IO_COPY_FILE
  consoleWriteValueCommandHandler,         // NANO_OS_IO_WRITE_VALUE
  consoleGetBufferCommandHandler,          // NANO_OS_IO_GET_BUFFER
  consoleWriteBufferCommandHandler,        // NANO_OS_IO_WRITE_BUFFER
  consoleSetPortShellCommandHandler,       // NANO_OS_IO_SET_PORT_SHELL
  consoleAssignPortCommandHandler,         // NANO_OS_IO_ASSIGN_PORT
  consoleAssignPortInputCommandHandler,    // NANO_OS_IO_ASSIGN_PORT_INPUT
  consoleReleasePortCommandHandler,        // NANO_OS_IO_RELEASE_PORT
  consoleGetOwnedPortCommandHandler,       // NANO_OS_IO_GET_OWNED_PORT
  consoleSetEchoCommandHandler,            // NANO_OS_IO_SET_ECHO_PORT
  consoleWaitForInputCommandHandler,       // NANO_OS_IO_WAIT_FOR_INPUT
  consoleReleasePidPortCommandHandler,     // NANO_OS_IO_RELEASE_PID_PORT
  consoleReleaseBufferCommandHandler,      // NANO_OS_IO_RELEASE_BUFFER
};


/// @fn void handleNanoOsIoMessages(NanoOsIoState *nanoOsIoState)
///
/// @brief Command handler for NanoOsIoCommandResponse messages.
void handleNanoOsIoMessages(NanoOsIoState *nanoOsIoState) {
  ProcessMessage *message = processMessageQueuePop();
  while (message != NULL) {
    NanoOsIoCommandResponse type = 
      (NanoOsIoCommandResponse) processMessageType(message);
    if (type < NUM_NANO_OS_IO_COMMANDS) {
      nanoOsIoCommandHandlers[type](nanoOsIoState, message);
    }
    message = processMessageQueuePop();
  }

  return;
}

/// @fn void* runNanoOsIo(void *args)
///
/// @brief Process entry-point for input/output system.  Sets up and configures
/// access to the SD card reader and then enters an infinite loop for
/// processing commands.
///
/// @param args The chip select pin to use for communication with the SD card,
///   cast to a void*.
///
/// @return Never returns on success, returns NULL on failure.
void* runNanoOsIo(void *args) {
  int byteRead = -1;
  NanoOsIoState nanoOsIoState = {};
  nanoOsIoState.sdCardState.chipSelect = (uint8_t) ((intptr_t) args);
  printDebug("sizeof(NanoOsIoState) = ");
  printDebug(sizeof(NanoOsIoState));
  printDebug("\n");

  // For each console port, use the console buffer at the corresponding index.
  for (uint8_t ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    nanoOsIoState.consoleState.consolePorts[ii].consoleBuffer
      = &nanoOsIoState.consoleState.consoleBuffers[ii];
    nanoOsIoState.consoleState.consolePorts[ii].consoleBuffer->inUse = true;
  }

  // Set the port-specific data.
  nanoOsIoState.consoleState.consolePorts[USB_SERIAL_PORT].consoleIndex
    = 0;
  nanoOsIoState.consoleState.consolePorts[USB_SERIAL_PORT].inputOwner
    = PROCESS_ID_NOT_SET;
  nanoOsIoState.consoleState.consolePorts[USB_SERIAL_PORT].outputOwner
    = PROCESS_ID_NOT_SET;
  nanoOsIoState.consoleState.consolePorts[USB_SERIAL_PORT].shell
    = PROCESS_ID_NOT_SET;
  nanoOsIoState.consoleState.consolePorts[USB_SERIAL_PORT].waitingForInput
    = false;
  nanoOsIoState.consoleState.consolePorts[USB_SERIAL_PORT].readByte
    = readUsbSerialByte;
  nanoOsIoState.consoleState.consolePorts[USB_SERIAL_PORT].echo
    = true;
  nanoOsIoState.consoleState.consolePorts[USB_SERIAL_PORT].printString
    = printUsbSerialString;

  nanoOsIoState.consoleState.consolePorts[GPIO_SERIAL_PORT].consoleIndex
    = 0;
  nanoOsIoState.consoleState.consolePorts[GPIO_SERIAL_PORT].inputOwner
    = PROCESS_ID_NOT_SET;
  nanoOsIoState.consoleState.consolePorts[GPIO_SERIAL_PORT].outputOwner
    = PROCESS_ID_NOT_SET;
  nanoOsIoState.consoleState.consolePorts[GPIO_SERIAL_PORT].shell
    = PROCESS_ID_NOT_SET;
  nanoOsIoState.consoleState.consolePorts[GPIO_SERIAL_PORT].waitingForInput
    = false;
  nanoOsIoState.consoleState.consolePorts[GPIO_SERIAL_PORT].readByte
    = readGpioSerialByte;
  nanoOsIoState.consoleState.consolePorts[GPIO_SERIAL_PORT].echo
    = true;
  nanoOsIoState.consoleState.consolePorts[GPIO_SERIAL_PORT].printString
    = printGpioSerialString;

  // Initialize the SD card
  nanoOsIoState.sdCardState.sdCardVersion
    = sdSpiCardInit(nanoOsIoState.sdCardState.chipSelect);
  if (nanoOsIoState.sdCardState.sdCardVersion > 0) {
    nanoOsIoState.sdCardState.blockSize
      = sdSpiGetBlockSize(nanoOsIoState.sdCardState.chipSelect);
    nanoOsIoState.sdCardState.numBlocks
      = sdSpiGetBlockCount(nanoOsIoState.sdCardState.chipSelect);
#ifdef SD_CARD_DEBUG
    //// printString("Card is ");
    //// printString((nanoOsIoState.sdCardState.sdCardVersion == 1) ? "SDSC" : "SDHC/SDXC");
    //// printString("\n");

    //// printString("Card block size = ");
    //// printInt(nanoOsIoState.sdCardState.blockSize);
    //// printString("\n");
    printLong(nanoOsIoState.sdCardState.numBlocks);
    //// printString(" total blocks (");
    printLongLong(((int64_t) nanoOsIoState.sdCardState.numBlocks)
      * ((int64_t) nanoOsIoState.sdCardState.blockSize));
    //// printString(" total bytes)\n");
#endif // SD_CARD_DEBUG
    coroutineYield(&nanoOsIoState.filesystemState);
  } else {
    //// printString("ERROR! sdSpiCardInit returned status ");
    //// printInt(nanoOsIoState.sdCardState.sdCardVersion);
    //// printString("\n");
    // We can't proceed with the rest of this library if the SD card can't be
    // initialized, so return NULL here instead of yielding.  This will
    // indicate to the caller that we're dead.
    return NULL;
  }
  //// printDebug("SD card initialized.  Using partition ");
  //// printDebug(nanoOsIoState.filesystemState.partitionNumber);
  //// printDebug("\n");

  nanoOsIoState.filesystemState.blockSize
    = nanoOsIoState.sdCardState.blockSize;
  
  nanoOsIoState.filesystemState.blockBuffer
    = (uint8_t*) malloc(nanoOsIoState.filesystemState.blockSize);
  getPartitionInfo(&nanoOsIoState);
  free(nanoOsIoState.filesystemState.blockBuffer);
  nanoOsIoState.filesystemState.blockBuffer = NULL;
  
  ProcessMessage *schedulerMessage = NULL;
  while (1) {
    schedulerMessage = (ProcessMessage*) coroutineYield(NULL);
    if (schedulerMessage != NULL) {
      // We have a message from the scheduler that we need to process.  This
      // is not the expected case, but it's the priority case, so we need to
      // list it first.
      NanoOsIoCommandResponse messageType
        = (NanoOsIoCommandResponse) processMessageType(schedulerMessage);
      if (messageType < NUM_NANO_OS_IO_COMMANDS) {
        nanoOsIoCommandHandlers[messageType](&nanoOsIoState, schedulerMessage);
      }
    } else {
      handleNanoOsIoMessages(&nanoOsIoState);
    }

    // Poll the consoles
    for (uint8_t ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
      ConsolePort *consolePort = &nanoOsIoState.consoleState.consolePorts[ii];
      byteRead = consolePort->readByte(consolePort);
      if ((byteRead == ((int) '\n')) || (byteRead == ((int) '\r'))) {
        if (consolePort->waitingForInput == true) {
          // NULL-terminate the buffer.
          consolePort->consoleBuffer->buffer[consolePort->consoleIndex] = '\0';
          consolePort->consoleIndex = 0;
          sendNanoOsMessageToPid(
            consolePort->inputOwner, CONSOLE_RETURNING_INPUT,
            /* func= */ 0, (intptr_t) consolePort->consoleBuffer, false);
          consolePort->waitingForInput = false;
        } else {
          // Console port is owned but owning process is not waiting for input.
          // Reset our buffer and do nothing.
          consolePort->consoleIndex = 0;
        }
      }
    }
  }

  return NULL;
}

/// @fn int printConsoleValue(
///   ConsoleValueType valueType, void *value, size_t length)
///
/// @brief Send a command to print a value to the console.
///
/// @param command The ConsoleCommand of the message to send, which will
///   determine the handler to use and therefore the type of the value that is
///   displayed.
/// @param value A pointer to the value to send as data.
/// @param length The length of the data at the address provided by the value
///   pointer.  Will be truncated to the size of NanoOsMessageData if needed.
///
/// @return This function is non-blocking, always succeeds, and always returns
/// 0.
int printConsoleValue(ConsoleValueType valueType, void *value, size_t length) {
  NanoOsMessageData message = 0;
  length = (length <= sizeof(message)) ? length : sizeof(message);
  memcpy(&message, value, length);

  sendNanoOsMessageToPid(NANO_OS_NANO_OS_IO_PROCESS_ID, NANO_OS_IO_WRITE_VALUE,
    valueType, message, false);

  return 0;
}

/// @fn printConsole(<type> message)
///
/// @brief Print a message of an arbitrary type to the console.
///
/// @details
/// This is basically just a switch statement where the type is the switch
/// value.  They all call printConsole with the appropriate console command, a
/// pointer to the provided message, and the size of the message.
///
/// @param message The message to send to the console.
///
/// @return Returns the value returned by printConsoleValue.
int printConsole(char message) {
  return printConsoleValue(NANO_OS_IO_VALUE_CHAR, &message, sizeof(message));
}
int printConsole(unsigned char message) {
  return printConsoleValue(NANO_OS_IO_VALUE_UCHAR, &message, sizeof(message));
}
int printConsole(int message) {
  return printConsoleValue(NANO_OS_IO_VALUE_INT, &message, sizeof(message));
}
int printConsole(unsigned int message) {
  return printConsoleValue(NANO_OS_IO_VALUE_UINT, &message, sizeof(message));
}
int printConsole(long int message) {
  return printConsoleValue(NANO_OS_IO_VALUE_LONG_INT, &message, sizeof(message));
}
int printConsole(long unsigned int message) {
  return printConsoleValue(NANO_OS_IO_VALUE_LONG_UINT, &message, sizeof(message));
}
int printConsole(float message) {
  return printConsoleValue(NANO_OS_IO_VALUE_FLOAT, &message, sizeof(message));
}
int printConsole(double message) {
  return printConsoleValue(NANO_OS_IO_VALUE_DOUBLE, &message, sizeof(message));
}
int printConsole(const char *message) {
  return printConsoleValue(NANO_OS_IO_VALUE_STRING, &message, sizeof(message));
}

// Console port support functions.

/// @fn void releaseConsole(void)
///
/// @brief Release the console and display the command prompt to the user again.
///
/// @return This function returns no value.
void releaseConsole(void) {
  // releaseConsole is sometimes called from within handleCommand, which runs
  // from within the console process.  That means we can't do blocking prints
  // from this function.  i.e. We can't use printf here.  Use printConsole
  // instead.
  sendNanoOsMessageToPid(NANO_OS_NANO_OS_IO_PROCESS_ID, NANO_OS_IO_RELEASE_PORT,
    /* func= */ 0, /* data= */ 0, false);
  processYield();
}

/// @fn int getOwnedConsolePort(void)
///
/// @brief Get the first port owned by the running process.
///
/// @return Returns the numerical index of the console port the process owns on
/// success, -1 on failure.
int getOwnedConsolePort(void) {
  ProcessMessage *sent = sendNanoOsMessageToPid(
    NANO_OS_NANO_OS_IO_PROCESS_ID, NANO_OS_IO_GET_OWNED_PORT,
    /* func= */ 0, /* data= */ 0, /* waiting= */ true);

  // The console will reuse the message we sent, so don't release the message
  // in processMessageWaitForReplyWithType.
  ProcessMessage *reply = processMessageWaitForReplyWithType(
    sent, /* releaseAfterDone= */ false,
    NANO_OS_IO_RETURNING_PORT, NULL);

  int returnValue = nanoOsMessageDataValue(reply, int);
  processMessageRelease(reply);

  return returnValue;
}

/// @fn int setConsoleEcho(bool desiredEchoState)
///
/// @brief Get the echo state for all ports owned by the current process.
///
/// @return Returns 0 if the echo state was set for the current process's
/// ports, -1 on failure.
int setConsoleEcho(bool desiredEchoState) {
  ProcessMessage *sent = sendNanoOsMessageToPid(
    NANO_OS_NANO_OS_IO_PROCESS_ID, NANO_OS_IO_SET_ECHO_PORT,
    /* func= */ 0, /* data= */ desiredEchoState, /* waiting= */ true);

  // The console will reuse the message we sent, so don't release the message
  // in processMessageWaitForReplyWithType.
  ProcessMessage *reply = processMessageWaitForReplyWithType(
    sent, /* releaseAfterDone= */ false,
    NANO_OS_IO_RETURNING_PORT, NULL);

  int returnValue = nanoOsMessageDataValue(reply, int);
  processMessageRelease(reply);

  return returnValue;
}

