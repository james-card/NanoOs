///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              01.25.2025
///
/// @file              VirtualMemory.h
///
/// @brief             Infrastructure to support virtual memory used in
///                    processing commands run in a VM.
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

#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H

// Custom includes
#include "NanoOs.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define VIRTUAL_MEMORY_BUFFER_SIZE 16

/// @struct VirtualMemoryState
///
/// @brief Structure to maintain virtual memory state
///
/// @param filename The FAT16 name of the backing file.
/// @param fileHandle Handle to the memory file.
/// @param buffer Buffer for cached data.
/// @param bufferBaseOffset File offset where buffer starts.
/// @param bufferValidBytes Number of valid bytes in buffer.
typedef struct VirtualMemoryState {
  char      filename[13];
  FILE     *fileHandle;
  uint32_t  fileSize;
  uint8_t   buffer[VIRTUAL_MEMORY_BUFFER_SIZE];
  uint32_t  bufferBaseOffset;
  uint32_t  bufferValidBytes;
} VirtualMemoryState;

int32_t virtualMemoryInit(
  VirtualMemoryState *state, const char *filename);
void virtualMemoryCleanup(VirtualMemoryState *state, bool removeFile);
int32_t virtualMemoryRead8(
  VirtualMemoryState *state, uint32_t offset, uint8_t *value);
int32_t virtualMemoryRead32(
  VirtualMemoryState *state, uint32_t offset, uint32_t *value);
int32_t virtualMemoryRead64(
  VirtualMemoryState *state, uint32_t offset, uint64_t *value);
int32_t virtualMemoryWrite8(
  VirtualMemoryState *state, uint32_t offset, uint8_t value);
int32_t virtualMemoryWrite32(
  VirtualMemoryState *state, uint32_t offset, uint32_t value);
int32_t virtualMemoryWrite64(
  VirtualMemoryState *state, uint32_t offset, uint64_t value);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // VIRTUAL_MEMORY_H
