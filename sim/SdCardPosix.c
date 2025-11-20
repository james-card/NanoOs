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
#include "SdCardPosix.h"
#include <string.h>
#include <stdio.h>

/// @var sdCardDevicePath
///
/// @brief Full path to the device file to open from the filesystem.
const char *sdCardDevicePath = NULL;

/// @struct SdCardState
///
/// @brief State maintained by an SdCard process.
///
/// @param blockSize The number of bytes per block on the SD card as presented
///   to the host.
/// @param numBlocks The total number of blocks available on the SD card.
/// @param sdCardVersion The version of the card (1 or 2).
/// @param bsDevice A pointer to the BlockStorageDevice that abstracts this
///   card.
typedef struct SdCardState {
  uint16_t blockSize;
  uint32_t numBlocks;
  int sdCardVersion;
  BlockStorageDevice *bsDevice;
  FILE *sdCardFile;
} SdCardState;

/// @typedef SdCardCommandHandler
///
/// @brief Definition of a filesystem command handler function.
typedef int (*SdCardCommandHandler)(SdCardState*, ProcessMessage*);

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

/// @fn void* runSdCardPosix(void *args)
///
/// @brief Process entry-point for the SD card process.  Sets up and
/// configures access to the SD card reader and then enters an infinite loop
/// for processing commands.
///
/// @param args Any arguments to this function, cast to a void*.  Currently
///   ignored by this function.
///
/// @return This function never returns, but would return NULL if it did.
void* runSdCardPosix(void *args) {
  (void) args;

  SdCardState sdCardState;
  memset(&sdCardState, 0, sizeof(sdCardState));
  BlockStorageDevice sdDevice = {
    .context = (void*) ((intptr_t) getRunningProcessId()),
    .readBlocks = sdReadBlocks,
    .writeBlocks = sdWriteBlocks,
    .blockSize = 512,
    .blockBitShift = 0,
    .partitionNumber = 0,
  };
  sdCardState.bsDevice = &sdDevice;
  sdCardState.blockSize = 512;
  sdCardState.numBlocks = 2000000000; // About 1 TB
  sdCardState.sdCardVersion = 2;
  sdCardState.sdCardFile = fopen(sdCardDevicePath, "r+");

  coroutineYield(&sdDevice);

  ProcessMessage *schedulerMessage = NULL;
  while (1) {
    schedulerMessage = (ProcessMessage*) coroutineYield(NULL);
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

