///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              12.02.2024
///
/// @file              Filesystem.h
///
/// @brief             Filesystem functionality for NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2024 James Card
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

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

// Custom includes
#include "NanoOs.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// @struct FilesystemState
///
/// @brief State metadata the filesystem process uses to provide access to
/// files.
///
/// @param blockDevice A pointer to an allocated and initialized
///   BlockStorageDevice to use for reading and writing blocks.
/// @param blockSize The size of a block as it is known to the filesystem.
/// @param blockBuffer A pointer to the read/write buffer that is blockSize
///   bytes in length.
/// @param startLba The address of the first block of the filesystem.
/// @param endLba The address of the last block of the filesystem.
typedef struct FilesystemState {
  BlockStorageDevice *blockDevice;
  uint16_t blockSize;
  uint8_t *blockBuffer;
  uint32_t startLba;
  uint32_t endLba;
} FilesystemState;

/// @typedef FilesystemCommandHandler
///
/// @brief Definition of a filesystem command handler function.
typedef int (*FilesystemCommandHandler)(FilesystemState*, ProcessMessage*);

/// @enum FilesystemCommandResponse
///
/// @brief Commands and responses understood by the filesystem inter-process
/// message handler.
typedef enum FilesystemCommandResponse {
  // Commands:
  FILESYSTEM_OPEN_FILE,
  FILESYSTEM_CLOSE_FILE,
  NUM_FILESYSTEM_COMMANDS,
  // Responses:
} FilesystemCommandResponse;

// Exported functionality
void* runFilesystem(void *args);

FILE* filesystemFOpen(const char *pathname, const char *mode);
#ifdef fopen
#undef fopen
#endif // fopen
#define fopen filesystemFOpen

int filesystemFClose(FILE *stream);
#ifdef fclose
#undef fclose
#endif // fclose
#define fclose filesystemFClose

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FILESYSTEM_H
