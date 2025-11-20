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
#include "SdCard.h"
#include "NanoOs.h"

void* (*runSdCard)(void *args);

/// @fn int sdReadBlocks(void *context, uint32_t startBlock,
///   uint32_t numBlocks, uint16_t blockSize, uint8_t *buffer)
///
/// @brief Read a specified number of blocks of a given size from the SD card
/// into a provided buffer.
///
/// @param context The process ID of the SD card process to read from, cast to
///   a void*.
/// @param startBlock The start block to read from in terms of the caller's
///   context.
/// @param numBlocks The number of blocks to read in terms of the caller's
///   context.
/// @param blockSize The size of the blocks as known to the caller.
/// @param buffer A pointer to the byte buffer to read the data into.
///
/// @return Returns 0 on success, POSIX error code on failure.
int sdReadBlocks(void *context, uint32_t startBlock,
  uint32_t numBlocks, uint16_t blockSize, uint8_t *buffer
) {
  intptr_t sdCardProcess = (intptr_t) context;
  SdCommandParams sdCommandParams;
  sdCommandParams.startBlock = startBlock;
  sdCommandParams.numBlocks = numBlocks;
  sdCommandParams.blockSize = blockSize;
  sdCommandParams.buffer = buffer;

  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    sdCardProcess, SD_CARD_READ_BLOCKS,
    /* func= */ 0, /* data= */ (intptr_t) &sdCommandParams, true);
  processMessageWaitForDone(processMessage, NULL);
  int returnValue = nanoOsMessageDataValue(processMessage, int);
  processMessageRelease(processMessage);

  return returnValue;
}

/// @fn int sdWriteBlocks(void *context, uint32_t startBlock,
///   uint32_t numBlocks, uint16_t blockSize, const uint8_t *buffer)
///
/// @brief Write a specified number of blocks of a given size to the SD card
/// from a provided buffer.
///
/// @param context The process ID of the SD card process to write to, cast to
///   a void*.
/// @param startBlock The start block to write to in terms of the caller's
///   context.
/// @param numBlocks The number of blocks to write out terms of the caller's
///   context.
/// @param blockSize The size of the blocks as known to the caller.
/// @param buffer A pointer to the byte buffer to write the data from.
///
/// @return Returns 0 on success, POSIX error code on failure.
int sdWriteBlocks(void *context, uint32_t startBlock,
  uint32_t numBlocks, uint16_t blockSize, const uint8_t *buffer
) {
  intptr_t sdCardProcess = (intptr_t) context;
  SdCommandParams sdCommandParams;
  sdCommandParams.startBlock = startBlock;
  sdCommandParams.numBlocks = numBlocks;
  sdCommandParams.blockSize = blockSize;
  sdCommandParams.buffer = (uint8_t*) buffer;

  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    sdCardProcess, SD_CARD_WRITE_BLOCKS,
    /* func= */ 0, /* data= */ (intptr_t) &sdCommandParams, true);
  processMessageWaitForDone(processMessage, NULL);
  int returnValue = nanoOsMessageDataValue(processMessage, int);
  processMessageRelease(processMessage);

  return returnValue;
}

