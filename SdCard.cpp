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
#define CMD17   0x51  // READ_SINGLE_BLOCK
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

/// @def sdEnd
///
/// @brief End communication with the SD card.
#define sdEnd(chipSelect) \
  /* Deselect the SD chip select pin. */ \
  digitalWrite(chipSelect, HIGH);

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
  int sdCardVersion = 0;

  // Set up SPI pins
  pinMode(chipSelect, OUTPUT);
  digitalWrite(chipSelect, HIGH);  // CS initially high (disabled)
  
  SPI.begin();
  
  printString("Initializing SD card...\n");
  
  // Power up sequence
  digitalWrite(chipSelect, HIGH);
  delay(1);
  
  // Send at least 74 clock cycles with CS high
  for (int i = 0; i < 10; i++) {
    SPI.transfer(0xFF);
  }
  
  // Try to initialize the card
  bool initialized = false;
  for (int i = 0; i < 10; i++) {  // Try 10 times
    if (sdSendCommand(chipSelect, CMD0, 0) == R1_IDLE_STATE) {
      printString("Card is in idle state\n");
      initialized = true;
      break;
    }
    delay(100);
  }
  
  if (!initialized) {
    printString("Failed to initialize card\n");
    sdEnd(chipSelect);
    return sdCardVersion; // 0
  }
  
  // Check if card supports version 2
  if (sdSendCommand(chipSelect, CMD8, 0x1AA) == R1_IDLE_STATE) {
    // Read rest of R7 response
    uint8_t response[4];
    uint8_t ii = 0;
    for (; ii < 4; ii++) {
      response[ii] = SPI.transfer(0xFF);
    }
    if (response[3] == 0xAA) {
      sdCardVersion = 2;
    } else {
      printString("CMD8 response: ");
      Serial.print(response[0], HEX);
      Serial.print(response[1], HEX);
      Serial.print(response[2], HEX);
      Serial.print(response[3], HEX);
      printString("\n");
    }
  } else {
    sdCardVersion = 1;
  }

  sdEnd(chipSelect);
  return sdCardVersion;
}

/// @fn bool readBlock(uint32_t blockNumber, uint8_t *buffer)
///
/// @brief Read a 512-byte block from the SD card.
///
/// @param blockNumber The logical block number to read from the card.
/// @param buffer A pointer to a character buffer to read the block into.
///
/// @return Returns 0 on success, error code on failure.
int readBlock(uint8_t chipSelect, uint32_t blockNumber, uint8_t *buffer) {
  // Check that buffer is not null
  if (!buffer) {
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
  
  // Send 8 clock pulses
  SPI.transfer(0xFF);
  
  return 0;
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
    printString("Card is SD version ");
    printInt(sdCardVersion);
    printString("\n");
  } else {
    printString("ERROR!  sdCardInit failed!\n");
  }

  while (1) {
    processYield();
  }

  return NULL;
}
