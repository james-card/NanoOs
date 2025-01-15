////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2024 James Card                     //
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
#include "SdCard.h"

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

/// @fn void sdEnd(int chipSelect)
///
/// @brief End communication with the SD card.
///
/// @param chipSelect The I/O pin connected to the SD card's chip select line.
///
/// @return This function returns no value.
__attribute__((noinline)) void sdEnd(int chipSelect) {
  // Deselect the SD chip select pin.
  digitalWrite(chipSelect, HIGH);
  for (int index = 0; index < 8; index++) {
    SPI.transfer(0xFF); // 8 clock pulses
  }

  return;
}

/// @fn uint8_t sdSendCommand(uint8_t chipSelect, uint8_t cmd, uint32_t arg)
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
uint8_t sdSendCommand(uint8_t chipSelect, uint8_t cmd, uint32_t arg) {
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
  for (int i = 0; i < 10; i++) {
    response = SPI.transfer(0xFF);
    if ((response & 0x80) == 0) {
      break; // Exit if valid response
    }
  }
  
  return response;
}

/// @fn int sdCardInit(uint8_t chipSelect)
///
/// @brief Initialize the SD card for communication with the OS.
///
/// @param chipSelect The pin tht the SD card's chip select line is connected
///   to.
///
/// @return Returns the version of the connected card on success (1 or 2),
/// 0 on error.
int sdCardInit(uint8_t chipSelect) {
  uint8_t response;
  uint16_t timeoutCount;
  bool isSDv2 = false;
  
  // Set up chip select pin
  pinMode(chipSelect, OUTPUT);
  digitalWrite(chipSelect, HIGH);
  
  // Set up SPI at low speed
  SPI.begin();
  
  // Extended power up sequence - Send more clock cycles
  for (int index = 0; index < 32; index++) {
    SPI.transfer(0xFF);
  }
  
  // Send CMD0 to enter SPI mode
  timeoutCount = 200;  // Extended timeout
  do {
    for (int index = 0; index < 8; index++) {  // More dummy clocks
      SPI.transfer(0xFF);
    }
    response = sdSendCommand(chipSelect, CMD0, 0);
    if (--timeoutCount == 0) {
      sdEnd(chipSelect);
      return -1;
    }
  } while (response != R1_IDLE_STATE);
  
  // Send CMD8 to check version
  for (int index = 0; index < 8; index++) {
    SPI.transfer(0xFF);
  }
  response = sdSendCommand(chipSelect, CMD8, 0x000001AA);
  if (response == R1_IDLE_STATE) {
    isSDv2 = true;
    for (int index = 0; index < 4; index++) {
      response = SPI.transfer(0xFF);
    }
  }
  sdEnd(chipSelect);
  
  // Initialize card with ACMD41
  timeoutCount = 20000;  // Much longer timeout
  do {
    response = sdSendCommand(chipSelect, CMD55, 0);
    sdEnd(chipSelect);
    
    for (int index = 0; index < 8; index++) {
      SPI.transfer(0xFF);
    }
    
    // Try both with and without HCS bit based on card version
    uint32_t acmd41Arg = isSDv2 ? 0x40000000 : 0;
    response = sdSendCommand(chipSelect, ACMD41, acmd41Arg);
    sdEnd(chipSelect);
    
    if (--timeoutCount == 0) {
      sdEnd(chipSelect);
      return -5;
    }
  } while (response != 0);
  
  // If we get here, card is initialized
  for (int index = 0; index < 8; index++) {
    SPI.transfer(0xFF);
  }
  
  sdEnd(chipSelect);
  return isSDv2 ? 2 : 1;
}

/// @fn bool sdReadBlock(uint32_t blockNumber, uint8_t *buffer)
///
/// @brief Read a 512-byte block from the SD card.
///
/// @param chipSelect The pin tht the SD card's chip select line is connected
///   to.
/// @param blockNumber The logical block number to read from the card.
/// @param buffer A pointer to a character buffer to read the block into.
///
/// @return Returns 0 on success, error code on failure.
int sdReadBlock(uint8_t chipSelect, uint32_t blockNumber, uint8_t *buffer) {
  // Check that buffer is not null
  if (buffer == NULL) {
    return EINVAL;
  }
  
  // Convert block number to byte address (multiply by 512)
  uint32_t address = blockNumber << 9;
  
  // Send READ_SINGLE_BLOCK command
  uint8_t response = sdSendCommand(chipSelect, CMD17, address);
  if (response != 0x00) {
    sdEnd(chipSelect);
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
      sdEnd(chipSelect);
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
  
  sdEnd(chipSelect);
  return 0;
}

/// @fn int sdWriteBlock(uint8_t chipSelect,
///   uint32_t blockNumber, const uint8_t *buffer)
/// 
/// @brief Write a 512-byte block to the SD card.
///
/// @param chipSelect The pin tht the SD card's chip select line is connected
///   to.
/// @param blockNumber The logical block number to write from the card.
/// @param buffer A pointer to a character buffer to write the block from.
///
/// @return Returns 0 on success, error code on failure.
int sdWriteBlock(uint8_t chipSelect,
  uint32_t blockNumber, const uint8_t *buffer
) {
  if (buffer == NULL) {
    return EINVAL;
  }
  
  uint32_t address = blockNumber << 9;  // Convert to byte address
  
  // Send WRITE_BLOCK command
  uint8_t response = sdSendCommand(chipSelect, CMD24, address);
  if (response != 0x00) {
    sdEnd(chipSelect);
    return EIO; // Command failed
  }
  
  // Send start token
  SPI.transfer(0xFE);
  
  // Write data
  for (int i = 0; i < 512; i++) {
    SPI.transfer(buffer[i]);
  }
  
  // Send dummy CRC
  SPI.transfer(0xFF);
  SPI.transfer(0xFF);
  
  // Get data response
  response = SPI.transfer(0xFF);
  if ((response & 0x1F) != 0x05) {
    sdEnd(chipSelect);
    return EIO; // Bad response
  }
  
  // Wait for write to complete
  uint16_t timeout = 10000;
  while (timeout--) {
    if (SPI.transfer(0xFF) != 0x00) {
      break;
    }
    if (timeout == 0) {
      sdEnd(chipSelect);
      return EIO; // Write timeout
    }
  }
  
  sdEnd(chipSelect);
  return 0;
}

/// @fn int sdGetBlockCount(uint8_t chipSelect)
///
/// @brief Get the total number of available blocks on an SD card.
///
/// @param chipSelect The pin tht the SD card's chip select line is connected
///   to.
///
/// @return Returns the number of blocks available on success, negative error
/// code on failure.
int32_t sdGetBlockCount(uint8_t chipSelect) {
  uint8_t cardSpecificData[16];
  uint32_t blockCount = 0;
  
  // Send SEND_CSD command
  uint8_t response = sdSendCommand(chipSelect, CMD9, 0);
  if (response != 0x00) {
    sdEnd(chipSelect);
    printString("ERROR! CMD9 returned ");
    printInt(response);
    printString("\n");
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
      sdEnd(chipSelect);
      return -2;
    }
  }
  
  // Read CSD register
  for (int index = 0; index < 16; index++) {
    cardSpecificData[index] = SPI.transfer(0xFF);
  }
  
  sdEnd(chipSelect);
  
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

/// @fn void* runSdCard(void *args)
///
/// @brief Process entry-point for the SD card process.  Sets up and
/// configures access to the SD card reader and then enters an infinite loop
/// for processing commands.
///
/// @param args Any arguments to this function, cast to a void*.  Currently
///   ignored by this function.
///
/// @return This function never returns, but would return NULL if it did.
void* runSdCard(void *args) {
  uint8_t chipSelect = (uint8_t) ((intptr_t) args);

  int sdCardVersion = sdCardInit(chipSelect);
  if (sdCardVersion > 0) {
#ifdef SD_CARD_DEBUG
    printString("Card is ");
    printString((sdCardVersion == 1) ? "SDSC" : "SDHC/SDXC");
    printString("\n");

    int32_t totalBlocks = sdGetBlockCount(chipSelect);
    printLong(totalBlocks);
    printString(" total blocks (");
    printLongLong(((int64_t) totalBlocks) << 9);
    printString(" total bytes)\n");
#endif // SD_CARD_DEBUG
  } else {
    printString("ERROR! sdCardInit returned status ");
    printInt(sdCardVersion);
    printString("\n");
  }

  while (1) {
    processYield();
  }

  return NULL;
}
