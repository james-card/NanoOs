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
typedef struct NanoOsIoState {
  SdCardState sdCardState;
  FilesystemState filesystemState;
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
  NanoOsIoState nanoOsIoState = {};
  nanoOsIoState.sdCardState.chipSelect = (uint8_t) ((intptr_t) args);
  printDebug("sizeof(NanoOsIoState) = ");
  printDebug(sizeof(NanoOsIoState));
  printDebug("\n");

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
      } else {
        //// printString("ERROR!!!  Received unknown IO command ");
        //// printInt(messageType);
        //// printString(" from scheduler.\n");
      }
    } else {
      handleNanoOsIoMessages(&nanoOsIoState);
    }
  }

  return NULL;
}

/// @fn FILE* nanoOsIoFOpen(const char *pathname, const char *mode)
///
/// @brief Implementation of the standard C fopen call.
///
/// @param pathname The full pathname to the file.  NOTE:  This implementation
///   can only open files in the root directory.  Subdirectories are NOT
///   supported.
/// @param mode The standard C file mode to open the file as.
///
/// @return Returns a pointer to an initialized FILE object on success, NULL on
/// failure.
FILE* nanoOsIoFOpen(const char *pathname, const char *mode) {
  if ((pathname == NULL) || (*pathname == '\0')
    || (mode == NULL) || (*mode == '\0')
  ) {
    return NULL;
  }

  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, NANO_OS_IO_OPEN_FILE,
    (intptr_t) mode, (intptr_t) pathname, true);
  processMessageWaitForDone(msg, NULL);
  FILE *file = nanoOsMessageDataPointer(msg, FILE*);
  processMessageRelease(msg);
  return file;
}

/// @fn int nanoOsIoFClose(FILE *stream)
///
/// @brief Implementation of the standard C fclose call.
///
/// @param stream A pointer to a previously-opened FILE object.
///
/// @return This function always succeeds and always returns 0.
int nanoOsIoFClose(FILE *stream) {
  if (stream != NULL) {
    ProcessMessage *msg = sendNanoOsMessageToPid(
      NANO_OS_FILESYSTEM_PROCESS_ID, NANO_OS_IO_CLOSE_FILE,
      0, (intptr_t) stream, true);
    processMessageWaitForDone(msg, NULL);
    processMessageRelease(msg);
  }
  return 0;
}

/// @fn int nanoOsIoRemove(const char *pathname)
///
/// @brief Implementation of the standard C remove call.
///
/// @param pathname The full pathname to the file.  NOTE:  This implementation
///   can only open files in the root directory.  Subdirectories are NOT
///   supported.
///
/// @return Returns 0 on success, -1 on failure.
int nanoOsIoRemove(const char *pathname) {
  int returnValue = 0;
  if ((pathname != NULL) && (*pathname != '\0')) {
    ProcessMessage *msg = sendNanoOsMessageToPid(
      NANO_OS_FILESYSTEM_PROCESS_ID, NANO_OS_IO_REMOVE_FILE,
      /* func= */ 0, (intptr_t) pathname, true);
    processMessageWaitForDone(msg, NULL);
    returnValue = nanoOsMessageDataValue(msg, int);
    processMessageRelease(msg);
  }
  return returnValue;
}

/// @fn int nanoOsIoFSeek(FILE *stream, long offset, int whence)
///
/// @brief Implementation of the standard C fseek call.
///
/// @param stream A pointer to a previously-opened FILE object.
/// @param offset A signed integer value that will be added to the specified
///   position.
/// @param whence The location within the file to apply the offset to.  Valid
///   values are SEEK_SET (the beginning of the file), SEEK_CUR (the current
///   file positon), and SEEK_END (the end of the file).
///
/// @return Returns 0 on success, -1 on failure.
int nanoOsIoFSeek(FILE *stream, long offset, int whence) {
  if (stream == NULL) {
    return -1;
  }

  NanoOsIoSeekParameters nanoOsIoSeekParameters = {
    .stream = stream,
    .offset = offset,
    .whence = whence,
  };
  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, NANO_OS_IO_SEEK_FILE,
    /* func= */ 0, (intptr_t) &nanoOsIoSeekParameters, true);
  processMessageWaitForDone(msg, NULL);
  int returnValue = nanoOsMessageDataValue(msg, int);
  processMessageRelease(msg);
  return returnValue;
}

/// @fn size_t nanoOsIoFRead(
///   void *ptr, size_t size, size_t nmemb, FILE *stream)
///
/// @brief Read data from a previously-opened file.
///
/// @param ptr A pointer to the memory to read data into.
/// @param size The size, in bytes, of each element that is to be read from the
///   file.
/// @param nmemb The number of elements that are to be read from the file.
/// @param stream A pointer to the previously-opened file.
///
/// @return Returns the total number of objects successfully read from the
/// file.
size_t nanoOsIoFRead(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  size_t returnValue = 0;
  if ((ptr == NULL) || (size == 0) || (nmemb == 0) || (stream == NULL)) {
    // Nothing to do.
    return returnValue; // 0
  }

  NanoOsIoCommandParameters nanoOsIoIoCommandParameters = {
    .file = stream,
    .buffer = ptr,
    .length = (uint32_t) (size * nmemb)
  };
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID,
    NANO_OS_IO_READ_FILE,
    /* func= */ 0,
    /* data= */ (intptr_t) &nanoOsIoIoCommandParameters,
    true);
  processMessageWaitForDone(processMessage, NULL);
  returnValue = (nanoOsIoIoCommandParameters.length / size);
  processMessageRelease(processMessage);

  return returnValue;
}

/// @fn size_t nanoOsIoFWrite(
///   const void *ptr, size_t size, size_t nmemb, FILE *stream)
///
/// @brief Write data to a previously-opened file.
///
/// @param ptr A pointer to the memory to write data from.
/// @param size The size, in bytes, of each element that is to be written to
///   the file.
/// @param nmemb The number of elements that are to be written to the file.
/// @param stream A pointer to the previously-opened file.
///
/// @return Returns the total number of objects successfully written to the
/// file.
size_t nanoOsIoFWrite(
  const void *ptr, size_t size, size_t nmemb, FILE *stream
) {
  size_t returnValue = 0;
  if ((ptr == NULL) || (size == 0) || (nmemb == 0) || (stream == NULL)) {
    // Nothing to do.
    return returnValue; // 0
  }

  NanoOsIoCommandParameters nanoOsIoIoCommandParameters = {
    .file = stream,
    .buffer = (void*) ptr,
    .length = (uint32_t) (size * nmemb)
  };
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID,
    NANO_OS_IO_WRITE_FILE,
    /* func= */ 0,
    /* data= */ (intptr_t) &nanoOsIoIoCommandParameters,
    true);
  processMessageWaitForDone(processMessage, NULL);
  returnValue = (nanoOsIoIoCommandParameters.length / size);
  processMessageRelease(processMessage);

  return returnValue;
}

/// @fn long nanoOsIoFTell(FILE *stream)
///
/// @brief Get the current value of the position indicator of a
/// previously-opened file.
///
/// @param stream A pointer to a previously-opened file.
///
/// @return Returns the current position of the file on success, -1 on failure.
long nanoOsIoFTell(FILE *stream) {
  if (stream == NULL) {
    return -1;
  }

  return (long) ((Fat16File*) stream->file)->currentPosition;
}

/// @fn size_t nanoOsIoFCopy(FILE *srcFile, off_t srcStart,
///   FILE *dstFile, off_t dstStart, size_t length)
///
/// @brief Copy a specified number of bytes from one position in a source file
///   to another position in a destination file.
///
/// @param srcFile A pointer to the FILE to copy from.  The file must be at
///   least srcStart + length bytes long.
/// @param srcStart The starting position, in bytes, to copy data from in the
///   source file.
/// @param dstFile A pointer to the FILE to copy to.  If the file is not
///   already dstStart + length bytes long, it will be padded with 0s until the
///   dstStart position is reached.
/// @param dstStart The starting position, in bytes, to copy data to in the
///   destination file.
/// @param length The number of bytes to copy from the source file to the
///   destination file.
///
/// @return Returns the number of bytes successfully copied.
size_t nanoOsIoFCopy(FILE *srcFile, off_t srcStart,
  FILE *dstFile, off_t dstStart, size_t length
) {
  if ((dstFile == NULL) || (length == 0)) {
    // Can't proceed or nothing to do.
    return 0;
  }

  FcopyArgs fcopyArgs = {
    .srcFile = srcFile,
    .srcStart = srcStart,
    .dstFile = dstFile,
    .dstStart  = dstStart,
    .length = length,
  };

  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID,
    NANO_OS_IO_COPY_FILE,
    /* func= */ 0,
    /* data= */ (intptr_t) &fcopyArgs,
    true);
  processMessageWaitForDone(processMessage, NULL);
  size_t returnValue = nanoOsMessageDataValue(processMessage, size_t);
  processMessageRelease(processMessage);

  return returnValue;
}

// Input support functions.

/// @fn ConsoleBuffer* nanoOsIoWaitForInput(void)
///
/// @brief Wait for input from the nanoOs port owned by the current process.
///
/// @return Returns a pointer to the input retrieved on success, NULL on
/// failure.
ConsoleBuffer* nanoOsIoWaitForInput(void) {
  ConsoleBuffer *nanoOsIoBuffer = NULL;
  FileDescriptor *inputFd = schedulerGetFileDescriptor(stdin);
  if (inputFd == NULL) {
    printString("ERROR!!!  Could not get input file descriptor for process ");
    printInt(getRunningProcessId());
    printString(" and stream ");
    printInt((intptr_t) stdin);
    printString(".\n");

    // We can't proceed, so bail.
    return nanoOsIoBuffer; // NULL
  }
  IoPipe *inputPipe = &inputFd->inputPipe;

  if (inputPipe->processId == NANO_OS_CONSOLE_PROCESS_ID) {
    sendNanoOsMessageToPid(inputPipe->processId, inputPipe->messageType,
      /* func= */ 0, /* data= */ 0, false);
  }

  if (inputPipe->processId != PROCESS_ID_NOT_SET) {
    ProcessMessage *response
      = processMessageQueueWaitForType(CONSOLE_RETURNING_INPUT, NULL);
    nanoOsIoBuffer = nanoOsMessageDataPointer(response, ConsoleBuffer*);

    if (processMessageWaiting(response) == false) {
      // The usual case.
      processMessageRelease(response);
    } else {
      // Just tell the sender that we're done.
      processMessageSetDone(response);
    }
  }

  return nanoOsIoBuffer;
}

/// @fn char *nanoOsIoFGets(char *buffer, int size, FILE *stream)
///
/// @brief Custom implementation of fgets for this library.
///
/// @param buffer The character buffer to write the captured input into.
/// @param size The maximum number of bytes to write into the buffer.
/// @param stream A pointer to the FILE stream to read from.  Currently, only
///   stdin is supported.
///
/// @return Returns the buffer pointer provided on success, NULL on failure.
char *nanoOsIoFGets(char *buffer, int size, FILE *stream) {
  char *returnValue = NULL;
  ConsoleBuffer *nanoOsIoBuffer
    = (ConsoleBuffer*) getProcessStorage(FGETS_CONSOLE_BUFFER_KEY);
  int numBytesReceived = 0;
  char *newlineAt = NULL;
  int numBytesToCopy = 0;
  int nanoOsIoInputLength = 0;
  int bufferIndex = 0;

  if (stream == stdin) {
    // There are three stop conditions:
    // 1. nanoOsIoWaitForInput returns NULL, signalling the end of the input
    //    from the stream.
    // 2. We read a newline.
    // 3. We reach size - 1 bytes received from the stream.
    if (nanoOsIoBuffer == NULL) {
      nanoOsIoBuffer = nanoOsIoWaitForInput();
      setProcessStorage(FGETS_CONSOLE_BUFFER_KEY, nanoOsIoBuffer);
    } else {
      newlineAt = strchr(nanoOsIoBuffer->buffer, '\n');
      if (newlineAt == NULL) {
        newlineAt = strchr(nanoOsIoBuffer->buffer, '\r');
      }
      if (newlineAt != NULL) {
        bufferIndex = (((uintptr_t) newlineAt)
          - ((uintptr_t) nanoOsIoBuffer->buffer)) + 1;
      } else {
        // This should be impossible given the algorithm below, but assume
        // nothing.
      }
    }

    while (
      (nanoOsIoBuffer != NULL)
      && (newlineAt == NULL)
      && (numBytesReceived < (size - 1))
    ) {
      returnValue = buffer;
      newlineAt = strchr(&nanoOsIoBuffer->buffer[bufferIndex], '\n');
      if (newlineAt == NULL) {
        newlineAt = strchr(nanoOsIoBuffer->buffer, '\r');
      }

      if ((newlineAt == NULL) || (newlineAt[1] == '\0')) {
        // The usual case.
        nanoOsIoInputLength
          = (int) strlen(&nanoOsIoBuffer->buffer[bufferIndex]);
      } else {
        // We've received a buffer that contains a newline plus something after
        // it.  Copy everything up to and including the newline.  Return what
        // we copy and leave the pointer alone so that it's picked up on the
        // next call.
        nanoOsIoInputLength = (int) (((uintptr_t) newlineAt)
          - ((uintptr_t) &nanoOsIoBuffer->buffer[bufferIndex]));
      }

      numBytesToCopy
        = MIN((size - 1 - numBytesReceived), nanoOsIoInputLength);
      memcpy(&buffer[numBytesReceived], &nanoOsIoBuffer->buffer[bufferIndex],
        numBytesToCopy);
      numBytesReceived += numBytesToCopy;
      buffer[numBytesReceived] = '\0';
      // Release the buffer.
      sendNanoOsMessageToPid(
        NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
        /* func= */ 0, /* data= */ (uintptr_t) nanoOsIoBuffer, false);

      if (newlineAt != NULL) {
        // We've reached one of the stop cases, so we're not going to attempt
        // to receive any more data from the file descriptor.
        nanoOsIoBuffer = NULL;
      } else {
        // There was no newline in this message.  We need to get another one.
        nanoOsIoBuffer = nanoOsIoWaitForInput();
        bufferIndex = 0;
      }

      setProcessStorage(FGETS_CONSOLE_BUFFER_KEY, nanoOsIoBuffer);
    }
  } else {
    // stream is a regular FILE.
    NanoOsIoCommandParameters nanoOsIoCommandParameters = {
      .file = stream,
      .buffer = buffer,
      .length = (uint32_t) size - 1
    };
    ProcessMessage *processMessage = sendNanoOsMessageToPid(
      NANO_OS_FILESYSTEM_PROCESS_ID,
      NANO_OS_IO_READ_FILE,
      /* func= */ 0,
      /* data= */ (intptr_t) &nanoOsIoCommandParameters,
      true);
    processMessageWaitForDone(processMessage, NULL);
    if (nanoOsIoCommandParameters.length > 0) {
      buffer[nanoOsIoCommandParameters.length] = '\0';
      returnValue = buffer;
    }
    processMessageRelease(processMessage);
  }

  return returnValue;
}

/// @fn int nanoOsIoVFScanf(FILE *stream, const char *format, va_list args)
///
/// @brief Read formatted input from a file stream into arguments provided in
/// a va_list.
///
/// @param stream A pointer to the FILE stream to read from.  Currently, only
///   stdin is supported.
/// @param format The string specifying the expected format of the input data.
/// @param args The va_list containing the arguments to store the parsed values
///   into.
///
/// @return Returns the number of items parsed on success, EOF on failure.
int nanoOsIoVFScanf(FILE *stream, const char *format, va_list args) {
  int returnValue = EOF;

  if (stream == stdin) {
    ConsoleBuffer *nanoOsIoBuffer = nanoOsIoWaitForInput();
    if (nanoOsIoBuffer == NULL) {
      return returnValue; // EOF
    }

    returnValue = vsscanf(nanoOsIoBuffer->buffer, format, args);
    // Release the buffer.
    sendNanoOsMessageToPid(
      NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
      /* func= */ 0, /* data= */ (intptr_t) nanoOsIoBuffer, false);
  }

  return returnValue;
}

/// @fn int nanoOsIoFScanf(FILE *stream, const char *format, ...)
///
/// @brief Read formatted input from a file stream into provided arguments.
///
/// @param stream A pointer to the FILE stream to read from.  Currently, only
///   stdin is supported.
/// @param format The string specifying the expected format of the input data.
/// @param ... The arguments to store the parsed values into.
///
/// @return Returns the number of items parsed on success, EOF on failure.
int nanoOsIoFScanf(FILE *stream, const char *format, ...) {
  int returnValue = EOF;
  va_list args;

  va_start(args, format);
  returnValue = nanoOsIoVFScanf(stream, format, args);
  va_end(args);

  return returnValue;
}

/// @fn int nanoOsIoScanf(const char *format, ...)
///
/// @brief Read formatted input from the nanoOs into provided arguments.
///
/// @param format The string specifying the expected format of the input data.
/// @param ... The arguments to store the parsed values into.
///
/// @return Returns the number of items parsed on success, EOF on failure.
int nanoOsIoScanf(const char *format, ...) {
  int returnValue = EOF;
  va_list args;

  va_start(args, format);
  returnValue = nanoOsIoVFScanf(stdin, format, args);
  va_end(args);

  return returnValue;
}

// Output support functions.

/// @fn ConsoleBuffer* nanoOsIoGetBuffer(void)
///
/// @brief Get a buffer from the runConsole process by sending it a command
/// message and getting its response.
///
/// @return Returns a pointer to a ConsoleBuffer from the runConsole process on
/// success, NULL on failure.
ConsoleBuffer* nanoOsIoGetBuffer(void) {
  ConsoleBuffer *returnValue = NULL;
  struct timespec ts = {0, 0};

  // It's possible that all the nanoOs buffers are in use at the time this call
  // is made, so we may have to try multiple times.  Do a while loop until we
  // get a buffer back or until an error occurs.
  while (returnValue == NULL) {
    ProcessMessage *processMessage = sendNanoOsMessageToPid(
      NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_GET_BUFFER, 0, 0, true);
    if (processMessage == NULL) {
      break; // will return returnValue, which is NULL
    }

    // We want to make sure the handler is done processing the message before
    // we wait for a reply.  Do a blocking wait.
    if (processMessageWaitForDone(processMessage, NULL) != processSuccess) {
      // Something is wrong.  Bail.
      processMessageRelease(processMessage);
      break; // will return returnValue, which is NULL
    }
    processMessageRelease(processMessage);

    // The handler only marks the message as done if it has successfully sent
    // us a reply or if there was an error and it could not send a reply.  So,
    // we don't want an infinite timeout to waitForDataMessage, we want zero
    // wait.  That's why we need the zeroed timespec above and we want to
    // manually wait for done above.
    processMessage
      = processMessageQueueWaitForType(CONSOLE_RETURNING_BUFFER, &ts);
    if (processMessage == NULL) {
      // The handler marked the sent message done but did not send a reply.
      // That means something is wrong internally to it.  Bail.
      break; // will return returnValue, which is NULL
    }

    returnValue = nanoOsMessageDataPointer(processMessage, ConsoleBuffer*);
    processMessageRelease(processMessage);
    if (returnValue == NULL) {
      // Yield control to give the nanoOs a chance to get done processing the
      // buffers that are in use.
      processYield();
    }
  }

  return returnValue;
}

/// @fn int nanoOsIoWriteBuffer(FILE *stream, ConsoleBuffer *nanoOsIoBuffer)
///
/// @brief Send a CONSOLE_WRITE_BUFFER command to the nanoOs process.
///
/// @param stream A pointer to a FILE object designating which file to output
///   to (stdout or stderr).
/// @param nanoOsIoBuffer A pointer to a ConsoleBuffer previously returned from
///   a call to nanoOsIoGetBuffer.
///
/// @return Returns 0 on success, EOF on failure.
int nanoOsIoWriteBuffer(FILE *stream, ConsoleBuffer *nanoOsIoBuffer) {
  int returnValue = 0;
  if ((stream == stdout) || (stream == stderr)) {
    FileDescriptor *outputFd = schedulerGetFileDescriptor(stream);
    if (outputFd == NULL) {
      printString(
        "ERROR!!!  Could not get output file descriptor for process ");
      printInt(getRunningProcessId());
      printString(" and stream ");
      printInt((intptr_t) stream);
      printString(".\n");

      // Release the buffer to avoid creating a leak.
      sendNanoOsMessageToPid(
        NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
        /* func= */ 0, /* data= */ (intptr_t) nanoOsIoBuffer, false);

      // We can't proceed, so bail.
      returnValue = EOF;
      return returnValue;
    }
    IoPipe *outputPipe = &outputFd->outputPipe;

    if ((outputPipe != NULL) && (outputPipe->processId != PROCESS_ID_NOT_SET)) {
      if ((stream == stdout) || (stream == stderr)) {
        ProcessMessage *processMessage = sendNanoOsMessageToPid(
          outputPipe->processId, outputPipe->messageType,
          0, (intptr_t) nanoOsIoBuffer, true);
        if (processMessage != NULL) {
          processMessageWaitForDone(processMessage, NULL);
          processMessageRelease(processMessage);
        } else {
          returnValue = EOF;
        }
      } else {
        printString("ERROR!!!  Request to write to invalid stream ");
        printInt((intptr_t) stream);
        printString(" from process ");
        printInt(getRunningProcessId());
        printString(".\n");

        // Release the buffer to avoid creating a leak.
        sendNanoOsMessageToPid(
          NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
          /* func= */ 0, /* data= */ (intptr_t) nanoOsIoBuffer, false);

        returnValue = EOF;
      }
    } else {
      printString(
        "ERROR!!!  Request to write with no output pipe set from process ");
      printInt(getRunningProcessId());
      printString(".\n");

      // Release the buffer to avoid creating a leak.
      sendNanoOsMessageToPid(
        NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
        /* func= */ 0, /* data= */ (intptr_t) nanoOsIoBuffer, false);

      returnValue = EOF;
    }
  } else {
    // stream is a regular FILE.
    NanoOsIoCommandParameters nanoOsIoCommandParameters = {
      .file = stream,
      .buffer = nanoOsIoBuffer->buffer,
      .length = (uint32_t) strlen(nanoOsIoBuffer->buffer)
    };
    ProcessMessage *processMessage = sendNanoOsMessageToPid(
      NANO_OS_FILESYSTEM_PROCESS_ID,
      NANO_OS_IO_WRITE_FILE,
      /* func= */ 0,
      /* data= */ (intptr_t) &nanoOsIoCommandParameters,
      true);
    processMessageWaitForDone(processMessage, NULL);
    if (nanoOsIoCommandParameters.length == 0) {
      returnValue = EOF;
    }
    processMessageRelease(processMessage);
  }

  return returnValue;
}

/// @fn int nanoOsIoFPuts(const char *s, FILE *stream)
///
/// @brief Print a raw string to the nanoOs.  Uses the CONSOLE_WRITE_STRING
/// command to print the literal string passed in.  Since this function has no
/// way of knowing whether or not the provided string is dynamically allocated,
/// it always waits for the nanoOs message handler to complete before
/// returning.
///
/// @param s A pointer to the string to print.
/// @param stream The file stream to print to.  Ignored by this function.
///
/// @return This function always returns 0.
int nanoOsIoFPuts(const char *s, FILE *stream) {
  int returnValue = EOF;
  ConsoleBuffer *nanoOsIoBuffer = nanoOsIoGetBuffer();
  if (nanoOsIoBuffer == NULL) {
    // Nothing we can do.
    return returnValue;
  }

  strncpy(nanoOsIoBuffer->buffer, s, CONSOLE_BUFFER_SIZE);
  returnValue = nanoOsIoWriteBuffer(stream, nanoOsIoBuffer);

  return returnValue;
}


/// @fn int nanoOsIoPuts(const char *s)
///
/// @brief Print a string followed by a newline to stdout.  Calls nanoOsIoFPuts
/// twice:  Once to print the provided string and once to print the trailing
/// newline.
///
/// @param s A pointer to the string to print.
///
/// @return Returns the value of nanoOsIoFPuts when printing the newline.
int nanoOsIoPuts(const char *s) {
  nanoOsIoFPuts(s, stdout);
  return nanoOsIoFPuts("\n", stdout);
}

/// @fn int nanoOsIoVFPrintf(FILE *stream, const char *format, va_list args)
///
/// @brief Print a formatted string to the nanoOs.  Gets a string buffer from
/// the nanoOs, writes the formatted string to that buffer, then sends a
/// command to the nanoOs to print the buffer.  If the stream being printed to
/// is stderr, blocks until the buffer is printed to the nanoOs.
///
/// @param stream A pointer to the FILE stream to print to (stdout or stderr).
/// @param format The format string for the printf message.
/// @param args The va_list of arguments that were passed into one of the
///   higher-level printf functions.
///
/// @return Returns the number of bytes printed on success, -1 on error.
int nanoOsIoVFPrintf(FILE *stream, const char *format, va_list args) {
  int returnValue = -1;
  ConsoleBuffer *nanoOsIoBuffer = nanoOsIoGetBuffer();
  if (nanoOsIoBuffer == NULL) {
    // Nothing we can do.
    return returnValue;
  }

  returnValue
    = vsnprintf(nanoOsIoBuffer->buffer, CONSOLE_BUFFER_SIZE, format, args);
  if (nanoOsIoWriteBuffer(stream, nanoOsIoBuffer) == EOF) {
    returnValue = -1;
  }

  return returnValue;
}

/// @fn int nanoOsIoFPrintf(FILE *stream, const char *format, ...)
///
/// @brief Print a formatted string to the nanoOs.  Constructs a va_list from
/// the arguments provided and then calls nanoOsVFPrintf.
///
/// @param stream A pointer to the FILE stream to print to (stdout or stderr).
/// @param format The format string for the printf message.
/// @param ... Any additional arguments needed by the format string.
///
/// @return Returns the number of bytes printed on success, -1 on error.
int nanoOsIoFPrintf(FILE *stream, const char *format, ...) {
  int returnValue = 0;
  va_list args;

  va_start(args, format);
  returnValue = nanoOsIoVFPrintf(stream, format, args);
  va_end(args);

  return returnValue;
}

/// @fn int nanoOsIoFPrintf(FILE *stream, const char *format, ...)
///
/// @brief Print a formatted string to stdout.  Constructs a va_list from the
/// arguments provided and then calls nanoOsVFPrintf with stdout as the first
/// parameter.
///
/// @param format The format string for the printf message.
/// @param ... Any additional arguments needed by the format string.
///
/// @return Returns the number of bytes printed on success, -1 on error.
int nanoOsIoPrintf(const char *format, ...) {
  int returnValue = 0;
  va_list args;

  va_start(args, format);
  returnValue = nanoOsIoVFPrintf(stdout, format, args);
  va_end(args);

  return returnValue;
}

