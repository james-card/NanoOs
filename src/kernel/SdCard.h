///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              1.13.2025
///
/// @file              SdCard.h
///
/// @brief             SD card functionality for NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
/// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///                                James Card
///                         http://www.jamescard.org
///
///////////////////////////////////////////////////////////////////////////////

#ifndef SD_CARD_H
#define SD_CARD_H

// Custom includes
#include "NanoOs.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// @struct SdCommandParams
///
/// @param startBlock The block number to start the command on.
/// @param numBlocks The number of blocks to perform the command on.
/// @param blockSize The number of bytes in each block.
/// @param buffer A pointer to the memory to read from or write to.
typedef struct SdCommandParams {
  uint32_t startBlock;
  uint32_t numBlocks;
  uint16_t blockSize;
  uint8_t *buffer;
} SdCommandParams;

/// @enum SdCardCommandResponse
///
/// @brief Commands and responses understood by the SD card inter-process
/// message handler.
typedef enum SdCardCommandResponse {
  // Commands:
  SD_CARD_READ_BLOCKS,
  SD_CARD_WRITE_BLOCKS,
  NUM_SD_CARD_COMMANDS,
  // Responses:
} SdCardCommandResponse;

extern void* (*runSdCard)(void *args);
int sdReadBlocks(void *context, uint32_t startBlock,
  uint32_t numBlocks, uint16_t blockSize, uint8_t *buffer);
int sdWriteBlocks(void *context, uint32_t startBlock,
  uint32_t numBlocks, uint16_t blockSize, const uint8_t *buffer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SD_CARD_H
