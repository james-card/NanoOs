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
#include "SdCardSpi.h"
#include "Hal.h"
#include "NanoOs.h"
#include "Processes.h"

// Must come last
#include "../user/NanoOsStdio.h"

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

// HACK:  SPI DIO pins that are currently defined in HalArduinoNano33Iot.h.
// Redefined here for now to avoid including the platform-specific header.
// This is temporary until I create a more-generic way to go about specifying
// the pins.
//
// JBC 2025-12-08
#define SPI_COPI_DIO 11
#define SPI_CIPO_DIO 12
#define SPI_SCK_DIO 13

/// @def SD_CARD_PIN_CHIP_SELECT
///
/// @brief Pin to use for the MicroSD card reader's SPI chip select line.
#define SD_CARD_PIN_CHIP_SELECT 4

/// @def SD_CARD_SPI_DEVICE
///
/// @brief The SPI device ID to use in SPI calls in the HAL.
#define SD_CARD_SPI_DEVICE 0

/// @fn uint8_t sdSpiSendCommand(int sdCardSpiDevice, uint8_t cmd, uint32_t arg)
///
/// @brief Send a command and its argument to the SD card over the SPI
/// interface.
///
/// @brief sdCardSpiDevice The zero-based SPI device ID to use.
/// @param cmd The 8-bit SD command to send to the SD card.
/// @param arg The 32-bit arguent to send for the SD command.
///
/// @return Returns the 8-bit command response from the SD card.
uint8_t sdSpiSendCommand(int sdCardSpiDevice, uint8_t cmd, uint32_t arg) {
  HAL->startSpiTransfer(sdCardSpiDevice);
  
  // Command byte
  HAL->spiTransfer8(sdCardSpiDevice, cmd | 0x40);
  
  // Argument
  HAL->spiTransfer8(sdCardSpiDevice, (arg >> 24) & 0xff);
  HAL->spiTransfer8(sdCardSpiDevice, (arg >> 16) & 0xff);
  HAL->spiTransfer8(sdCardSpiDevice, (arg >>  8) & 0xff);
  HAL->spiTransfer8(sdCardSpiDevice, (arg >>  0) & 0xff);
  
  // CRC - only needed for CMD0 and CMD8
  uint8_t crc = 0xFF;
  if (cmd == CMD0) {
    crc = 0x95; // Valid CRC for CMD0
  } else if (cmd == CMD8) {
    crc = 0x87; // Valid CRC for CMD8 (0x1AA)
  }
  HAL->spiTransfer8(sdCardSpiDevice, crc);
  
  // Wait for response
  uint8_t response;
  for (int ii = 0; ii < 10; ii++) {
    response = HAL->spiTransfer8(sdCardSpiDevice, 0xFF);
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
  
  // Set up SPI at the default speed
  int initStatus = HAL->initSpiDevice(SD_CARD_SPI_DEVICE,
    chipSelect, SPI_SCK_DIO, SPI_COPI_DIO, SPI_CIPO_DIO);
  if (initStatus != 0) {
    // Just pass the error upward.
    return initStatus;
  }
  
  // Extended power up sequence - Send more clock cycles
  for (int ii = 0; ii < 32; ii++) {
    HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
  }
  
  // Send CMD0 to enter SPI mode
  timeoutCount = 200;  // Extended timeout
  do {
    for (int ii = 0; ii < 8; ii++) {  // More dummy clocks
      HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
    }
    response = sdSpiSendCommand(SD_CARD_SPI_DEVICE, CMD0, 0);
    if (--timeoutCount == 0) {
      HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
      return -ETIMEDOUT;
    }
  } while (response != R1_IDLE_STATE);
  
  // Send CMD8 to check version
  for (int ii = 0; ii < 8; ii++) {
    HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
  }
  response = sdSpiSendCommand(SD_CARD_SPI_DEVICE, CMD8, 0x000001AA);
  if (response == R1_IDLE_STATE) {
    isSDv2 = true;
    for (int ii = 0; ii < 4; ii++) {
      response = HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
    }
  }
  HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
  
  // Initialize card with ACMD41
  timeoutCount = 20000;  // Much longer timeout
  do {
    response = sdSpiSendCommand(SD_CARD_SPI_DEVICE, CMD55, 0);
    HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
    
    for (int ii = 0; ii < 8; ii++) {
      HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
    }
    
    // Try both with and without HCS bit based on card version
    uint32_t acmd41Arg = isSDv2 ? 0x40000000 : 0;
    response = sdSpiSendCommand(SD_CARD_SPI_DEVICE, ACMD41, acmd41Arg);
    HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
    
    if (--timeoutCount == 0) {
      HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
      return -ETIMEDOUT;
    }
  } while (response != 0);
  
  // If we get here, card is initialized
  for (int ii = 0; ii < 8; ii++) {
    HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
  }
  
  HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
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
  uint8_t response = sdSpiSendCommand(SD_CARD_SPI_DEVICE, CMD17, address);
  if (response != 0x00) {
    HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
    return EIO; // Command failed
  }
  
  // Wait for data token (0xFE)
  uint16_t timeout = 10000;
  while (timeout--) {
    response = HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
    if (response == 0xFE) {
      break;
    }
    if (timeout == 0) {
      HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
      return EIO;  // Timeout waiting for data
    }
  }
  
  // Read 512 byte block
  for (int ii = 0; ii < 512; ii++) {
    buffer[ii] = HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
  }
  
  // Read CRC (2 bytes, ignored)
  HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
  HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
  
  HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
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
  HAL->startSpiTransfer(SD_CARD_SPI_DEVICE);
  uint8_t response = HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
  if (response != 0xFF) {
    HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
    return EIO;
  }
  
  uint32_t address = blockNumber;
  if (sdCardState->sdCardVersion == 1) {
    address *= sdCardState->blockSize; // Convert to byte address
  }
  
  // Send WRITE_BLOCK command
  response = sdSpiSendCommand(SD_CARD_SPI_DEVICE, CMD24, address);
  if (response != 0x00) {
    HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
    return EIO; // Command failed
  }
  
  // Wait for card to be ready before sending data
  uint16_t timeout = 10000;
  do {
    response = HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
    if (--timeout == 0) {
      HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
      return EIO;
    }
  } while (response != 0xFF);
  
  // Send start token
  HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFE);
  
  // Write data
  for (int ii = 0; ii < 512; ii++) {
    HAL->spiTransfer8(SD_CARD_SPI_DEVICE, buffer[ii]);
  }
  
  // Send dummy CRC
  HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
  HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
  
  // Get data response
  response = HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF);
  if ((response & 0x1F) != 0x05) {
    HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
    return EIO; // Bad response
  }
  
  // Wait for write to complete
  timeout = 10000;
  while (timeout--) {
    if (HAL->spiTransfer8(SD_CARD_SPI_DEVICE, 0xFF) != 0x00) {
      break;
    }
    if (timeout == 0) {
      HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
      return EIO; // Write timeout
    }
  }
  
  HAL->endSpiTransfer(SD_CARD_SPI_DEVICE);
  return 0;
}

/// @fn int16_t sdSpiGetBlockSize(int sdCardSpiDevice)
///
/// @brief Get the size, in bytes, of blocks on the SD card as presented to the
/// host.
///
/// @param sdCardSpiDevice The zero-based ID of the SPI device.
///
/// @return Returns the number of bytes per block on success, negative error
/// code on failure.
int16_t sdSpiGetBlockSize(int sdCardSpiDevice) {
  uint8_t response = sdSpiSendCommand(sdCardSpiDevice, CMD9, 0);
  if (response != 0x00) {
    HAL->endSpiTransfer(sdCardSpiDevice);
    printString(__func__);
    printString(": ERROR! CMD9 returned ");
    printInt(response);
    printString("\n");
    return -1;
  }

  for(int i = 0; i < 100; i++) {
    response = HAL->spiTransfer8(sdCardSpiDevice, 0xFF);
    if (response == 0xFE) {
      break;  // Data token
    }
  }
  
  // Read 16-byte CSD register
  uint8_t csd[16];
  for(int i = 0; i < 16; i++) {
    csd[i] = HAL->spiTransfer8(sdCardSpiDevice, 0xFF);
  }
  
  // Read 2 CRC bytes
  HAL->spiTransfer8(sdCardSpiDevice, 0xFF);
  HAL->spiTransfer8(sdCardSpiDevice, 0xFF);
  HAL->endSpiTransfer(sdCardSpiDevice);

  // For CSD Version 1.0 and 2.0, READ_BL_LEN is at the same location
  uint8_t readBlockLength = (csd[5] & 0x0F);
  return (int16_t) (((uint16_t) 1) << readBlockLength);
}

/// @fn int sdSpiGetBlockCount(sdCardSpiDevice)
///
/// @brief Get the total number of available blocks on an SD card.
///
/// @param sdCardSpiDevice The zero-based ID of the SPI device.
///
/// @return Returns the number of blocks available on success, negative error
/// code on failure.
int32_t sdSpiGetBlockCount(int sdCardSpiDevice) {
  uint8_t cardSpecificData[16];
  uint32_t blockCount = 0;
  
  // Send SEND_CSD command
  uint8_t response = sdSpiSendCommand(sdCardSpiDevice, CMD9, 0);
  if (response != 0x00) {
    HAL->endSpiTransfer(sdCardSpiDevice);
    printString(__func__);
    printString(": ERROR! CMD9 returned ");
    printInt(response);
    printString("\n");
    return -1;
  }
  
  // Wait for data token
  uint16_t timeoutCount = 10000;
  while (timeoutCount--) {
    response = HAL->spiTransfer8(sdCardSpiDevice, 0xFF);
    if (response == 0xFE) {
      break;
    }
    if (timeoutCount == 0) {
      HAL->endSpiTransfer(sdCardSpiDevice);
      return -2;
    }
  }
  
  // Read CSD register
  for (int ii = 0; ii < 16; ii++) {
    cardSpecificData[ii] = HAL->spiTransfer8(sdCardSpiDevice, 0xFF);
  }
  
  HAL->endSpiTransfer(sdCardSpiDevice);
  
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

/// @fn int sdCardReadBlocksCommandHandler(
///   SdCardState *sdCardState, ProcessMessage *processMessage)
///
/// @brief Command handler for the SD_CARD_READ_BLOCKS command.
///
/// @param sdCardState A pointer to the SdCardState object maintained by the
///   SD card process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the SD card process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int sdCardReadBlocksCommandHandler(
  SdCardState *sdCardState, ProcessMessage *processMessage
) {
  SdCommandParams *sdCommandParams
    = nanoOsMessageDataPointer(processMessage, SdCommandParams*);
  uint32_t startSdBlock = 0, numSdBlocks = 0;
  int returnValue = sdCardGetReadWriteParameters(
    sdCardState, sdCommandParams, &startSdBlock, &numSdBlocks);

  if (returnValue == 0) {
    uint8_t *buffer = sdCommandParams->buffer;

    for (uint32_t ii = 0; ii < numSdBlocks; ii++) {
      returnValue = sdSpiReadBlock(sdCardState, startSdBlock + ii, buffer);
      if (returnValue != 0) {
        break;
      }
      buffer += sdCardState->blockSize;
    }
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = returnValue;
  processMessageSetDone(processMessage);

  return 0;
}

/// @fn int sdCardWriteBlocksCommandHandler(
///   SdCardState *sdCardState, ProcessMessage *processMessage)
///
/// @brief Command handler for the SD_CARD_WRITE_BLOCKS command.
///
/// @param sdCardState A pointer to the SdCardState object maintained by the
///   SD card process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the SD card process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int sdCardWriteBlocksCommandHandler(
  SdCardState *sdCardState, ProcessMessage *processMessage
) {
  SdCommandParams *sdCommandParams
    = nanoOsMessageDataPointer(processMessage, SdCommandParams*);
  uint32_t startSdBlock = 0, numSdBlocks = 0;
  int returnValue = sdCardGetReadWriteParameters(
    sdCardState, sdCommandParams, &startSdBlock, &numSdBlocks);

  if (returnValue == 0) {
    uint8_t *buffer = sdCommandParams->buffer;

    for (uint32_t ii = 0; ii < numSdBlocks; ii++) {
      returnValue = sdSpiWriteBlock(sdCardState, startSdBlock + ii, buffer);
      if (returnValue != 0) {
        break;
      }
      buffer += sdCardState->blockSize;
    }
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = returnValue;
  processMessageSetDone(processMessage);

  return 0;
}

/// @var sdCardCommandHandlers
///
/// @brief Array of SdCardCommandHandler function pointers to handle commands
/// received by the runSdCard function.
SdCardCommandHandler sdCardCommandHandlers[] = {
  sdCardReadBlocksCommandHandler,  // SD_CARD_READ_BLOCKS
  sdCardWriteBlocksCommandHandler, // SD_CARD_WRITE_BLOCKS
};

/// @fn void handleSdCardMessages(SdCardState *sdCardState)
///
/// @brief Handle sdCard messages from the process's queue until there are no
/// more waiting.
///
/// @param sdCardState A pointer to the SdCardState structure maintained by the
///   sdCard process.
///
/// @return This function returns no value.
void handleSdCardMessages(SdCardState *sdCardState) {
  ProcessMessage *processMessage = processMessageQueuePop();
  while (processMessage != NULL) {
    SdCardCommandResponse messageType
      = (SdCardCommandResponse) processMessageType(processMessage);
    if (messageType >= NUM_SD_CARD_COMMANDS) {
      processMessage = processMessageQueuePop();
      continue;
    }
    
    sdCardCommandHandlers[messageType](sdCardState, processMessage);
    processMessage = processMessageQueuePop();
  }
  
  return;
}

/// @fn void* runSdCardSpi(void *args)
///
/// @brief Process entry-point for the SD card process.  Sets up and
/// configures access to the SD card reader and then enters an infinite loop
/// for processing commands.
///
/// @param args Any arguments to this function, cast to a void*.  Currently
///   ignored by this function.
///
/// @return This function never returns, but would return NULL if it did.
void* runSdCardSpi(void *args) {
  (void) args;

  SdCardState sdCardState;
  memset(&sdCardState, 0, sizeof(sdCardState));
  BlockStorageDevice blockStorageDevice = {
    .context = (void*) ((intptr_t) getRunningProcessId()),
    .readBlocks = sdReadBlocks,
    .writeBlocks = sdWriteBlocks,
    .blockSize = 0,
    .blockBitShift = 0,
    .partitionNumber = 0,
  };
  sdCardState.bsDevice = &blockStorageDevice;

  sdCardState.sdCardVersion = sdSpiCardInit(SD_CARD_PIN_CHIP_SELECT);
  if (sdCardState.sdCardVersion > 0) {
    sdCardState.blockSize = blockStorageDevice.blockSize
      = sdSpiGetBlockSize(SD_CARD_SPI_DEVICE);
    sdCardState.numBlocks = sdSpiGetBlockCount(SD_CARD_SPI_DEVICE);
#ifdef SD_CARD_DEBUG
    printString("Card is ");
    printString((sdCardState.sdCardVersion == 1) ? "SDSC" : "SDHC/SDXC");
    printString("\n");

    printString("Card block size = ");
    printInt(blockStorageDevice.blockSize);
    printString("\n");
    printLong(sdCardState.numBlocks);
    printString(" total blocks (");
    printLongLong(((int64_t) sdCardState.numBlocks)
      * ((int64_t) sdCardState.blockSize));
    printString(" total bytes)\n");
#endif // SD_CARD_DEBUG
  } else {
    printString("ERROR! sdSpiCardInit returned status: ");
    printString(strerror(-sdCardState.sdCardVersion));
    printString("\n");
  }
  coroutineYield(&blockStorageDevice, 0);

  ProcessMessage *schedulerMessage = NULL;
  while (1) {
    schedulerMessage = (ProcessMessage*) coroutineYield(NULL, 0);
    if (schedulerMessage != NULL) {
      // We have a message from the scheduler that we need to process.  This
      // is not the expected case, but it's the priority case, so we need to
      // list it first.
      SdCardCommandResponse messageType
        = (SdCardCommandResponse) processMessageType(schedulerMessage);
      if (messageType < NUM_SD_CARD_COMMANDS) {
        sdCardCommandHandlers[messageType](&sdCardState, schedulerMessage);
      } else {
        printString("ERROR: Received unknown sdCard command ");
        printInt(messageType);
        printString(" from scheduler.\n");
      }
    } else {
      handleSdCardMessages(&sdCardState);
    }
  }

  return NULL;
}

