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
#include "VirtualMemory.h"

/// @fn int32_t virtualMemoryInit(
///   VirtualMemoryState *state, const char *filename)
///
/// @brief Initialize the virtual memory system.
///
/// @param state Pointer to state structure to initialize.
/// @param filename Name of file to use as backing store.
///
/// @return Returns 0 on success, -1 on error.
int32_t virtualMemoryInit(
  VirtualMemoryState *state, const char *filename
) {
  // Validate parameters
  if ((state == NULL) || (filename == NULL)) {
    return -1;
  }

  // Open backing store file
  state->fileHandle = fopen(filename, "r+b");
  if (state->fileHandle == NULL) {
    state->fileHandle = fopen(filename, "w+b");
    if (state->fileHandle == NULL) {
      return -1;
    }
  }

  // Initialize buffer state
  state->bufferBaseOffset = 0;
  state->bufferValidBytes = 0;
  memset(state->buffer, 0, VIRTUAL_MEMORY_BUFFER_SIZE);
  strcpy(state->filename, filename);

  return 0;
}

/// @fn void virtualMemoryCleanup(VirtualMemoryState *state, bool removeFile)
///
/// @brief Clean up virtual memory system resources.
///
/// @param state Pointer to state structure to clean up.
/// @param removeFile Whether or not the backing file should be removed from
///   the filesystem.
///
/// @return This function returns no value.
void virtualMemoryCleanup(VirtualMemoryState *state, bool removeFile) {
  fclose(state->fileHandle); state->fileHandle = NULL;
  if (removeFile) {
    remove(state->filename);
  }
  *state->filename = '\0';
}

/// @fn void virtualMemoryPrepare(
///   VirtualMemoryState *state, uint32_t endOffset)
///
/// @brief Prepare the virtual memory for reading or writing.
///
/// @param state A pointer to the VirtualMemoryState that manages the virtual
///   memory storage.
/// @param endOffset The offset of the last byte in the virtual memory that is
///   to be read or written.
///
/// @return This function returns no value.
void virtualMemoryPrepare(VirtualMemoryState *state, uint32_t endOffset) {
  if (state->bufferValidBytes > 0) {
    // Write current buffer if it contains data
    fseek(state->fileHandle, state->bufferBaseOffset, SEEK_SET);
    fwrite(state->buffer, 1, state->bufferValidBytes, state->fileHandle);
  }

  // Clear out anything that was in the buffer.
  memset(state->buffer, 0, VIRTUAL_MEMORY_BUFFER_SIZE);

  // Make sure the data exists
  if (state->fileSize < endOffset) {
    fseek(state->fileHandle, state->fileSize, SEEK_SET);
    for (
      uint32_t ii = state->fileSize;
      ii < endOffset;
      ii += VIRTUAL_MEMORY_BUFFER_SIZE
    ) {
      fwrite(state->buffer, 1, VIRTUAL_MEMORY_BUFFER_SIZE, state->fileHandle);
      state->fileSize += VIRTUAL_MEMORY_BUFFER_SIZE;
    }
  }

  return;
}

/// @fn void* virtualMemoryGet(VirtualMemoryState *state, uint32_t offset)
///
/// @brief Get a pointer to the place in virtual memory specified by the
/// provided offset.
///
/// @param state Pointer to virtual memory state.
/// @param offset Offset in file to read from.
///
/// @return Returns a pointer to the specified memory on success, NULL on
/// failure.
void* virtualMemoryGet(VirtualMemoryState *state, uint32_t offset) {
  if ((state == NULL) || (state->fileHandle == NULL)) {
    return NULL;
  }

  // Check if requested memory is in the buffer.
  if ((offset >= state->bufferBaseOffset) && 
      (offset < state->bufferBaseOffset + state->bufferValidBytes)) {
    return &state->buffer[offset - state->bufferBaseOffset];
  }

  // Need to load new data into the buffer.
  virtualMemoryPrepare(state, offset + VIRTUAL_MEMORY_BUFFER_SIZE);

  // Read new buffer from the requested location.
  state->bufferBaseOffset
    = (offset / VIRTUAL_MEMORY_BUFFER_SIZE) * VIRTUAL_MEMORY_BUFFER_SIZE;
  fseek(state->fileHandle, state->bufferBaseOffset, SEEK_SET);
  state->bufferValidBytes
    = fread(state->buffer, 1, VIRTUAL_MEMORY_BUFFER_SIZE, state->fileHandle);

  if (state->bufferValidBytes == 0) {
    return NULL;
  }

  return &state->buffer[offset - state->bufferBaseOffset];
}

/// @fn int32_t virtualMemoryRead8(
///   VirtualMemoryState *state, uint32_t offset, uint8_t *value)
///
/// @brief Read a single byte from virtual memory.
///
/// @param state Pointer to virtual memory state.
/// @param offset Offset in file to read from.
/// @param value Pointer to store read value.
///
/// @return Returns 0 on success, -1 on error.
int32_t virtualMemoryRead8(
  VirtualMemoryState *state, uint32_t offset, uint8_t *value
) {
  // The state variable and its necessary components will be verified by
  // virtualMemoryGet, so just check value here.
  if (value == NULL) {
    return -1;
  }

  int returnValue = 0;
  uint8_t *memoryAddr = (uint8_t*) virtualMemoryGet(state, offset);
  if (memoryAddr != NULL) {
    *value = *memoryAddr;
  } else {
    returnValue = -1;
  }

  return returnValue;
}

/// @fn int32_t virtualMemoryRead32(
///   VirtualMemoryState *state, uint32_t offset, uint32_t *value)
///
/// @brief Read a 32-bit value from virtual memory.
///
/// @param state Pointer to virtual memory state.
/// @param offset Offset in file to read from.
/// @param value Pointer to store read value.
///
/// @return Returns 0 on success, -1 on error.
int32_t virtualMemoryRead32(
  VirtualMemoryState *state, uint32_t offset, uint32_t *value
) {
  // The state variable and its necessary components will be verified by
  // virtualMemoryGet, so just check value here.
  if (value == NULL) {
    return -1;
  }

  int returnValue = 0;
  uint32_t *memoryAddr = (uint32_t*) virtualMemoryGet(state, offset);
  if (memoryAddr != NULL) {
    *value = *memoryAddr;
  } else {
    returnValue = -1;
  }

  return returnValue;
}

/// @fn int32_t virtualMemoryRead64(
///   VirtualMemoryState *state, uint32_t offset, uint64_t *value)
///
/// @brief Read a 64-bit value from virtual memory.
///
/// @param state Pointer to virtual memory state.
/// @param offset Offset in file to read from.
/// @param value Pointer to store read value.
///
/// @return Returns 0 on success, -1 on error.
int32_t virtualMemoryRead64(
  VirtualMemoryState *state, uint32_t offset, uint64_t *value
) {
  // The state variable and its necessary components will be verified by
  // virtualMemoryGet, so just check value here.
  if (value == NULL) {
    return -1;
  }

  int returnValue = 0;
  uint64_t *memoryAddr = (uint64_t*) virtualMemoryGet(state, offset);
  if (memoryAddr != NULL) {
    *value = *memoryAddr;
  } else {
    returnValue = -1;
  }

  return returnValue;
}

/// @fn int32_t virtualMemoryWrite8(
///   VirtualMemoryState *state, uint32_t offset, uint8_t *value)
///
/// @brief Write a single byte to virtual memory.
///
/// @param state Pointer to virtual memory state.
/// @param offset Offset in file to write to.
/// @param value Value to write to virtual memory.
///
/// @return Returns 0 on success, -1 on error.
int32_t virtualMemoryWrite8(
  VirtualMemoryState *state, uint32_t offset, uint8_t value
) {
  int returnValue = 0;
  uint8_t *memoryAddr = (uint8_t*) virtualMemoryGet(state, offset);
  if (memoryAddr != NULL) {
    *memoryAddr = value;
  } else {
    returnValue = -1;
  }

  return returnValue;
}

/// @fn int32_t virtualMemoryWrite32(
///   VirtualMemoryState *state, uint32_t offset, uint32_t *value)
///
/// @brief Write a 32-bit value to virtual memory.
///
/// @param state Pointer to virtual memory state.
/// @param offset Offset in file to write to.
/// @param value Value to write to virtual memory.
///
/// @return Returns 0 on success, -1 on error.
int32_t virtualMemoryWrite32(
  VirtualMemoryState *state, uint32_t offset, uint32_t value
) {
  int returnValue = 0;
  uint32_t *memoryAddr = (uint32_t*) virtualMemoryGet(state, offset);
  if (memoryAddr != NULL) {
    *memoryAddr = value;
  } else {
    returnValue = -1;
  }

  return returnValue;
}

/// @fn int32_t virtualMemoryWrite64(
///   VirtualMemoryState *state, uint32_t offset, uint64_t *value)
///
/// @brief Write a 64-bit value to virtual memory.
///
/// @param state Pointer to virtual memory state.
/// @param offset Offset in file to write to.
/// @param value Value to write to virtual memory.
///
/// @return Returns 0 on success, -1 on error.
int32_t virtualMemoryWrite64(
  VirtualMemoryState *state, uint32_t offset, uint64_t value
) {
  int returnValue = 0;
  uint64_t *memoryAddr = (uint64_t*) virtualMemoryGet(state, offset);
  if (memoryAddr != NULL) {
    *memoryAddr = value;
  } else {
    returnValue = -1;
  }

  return returnValue;
}

/// @fn uint32_t virtualMemoryRead(VirtualMemoryState *state,
///   uint32_t offset, uint32_t length, void *buffer)
///
/// @brief Read a specified number of bytes starting from a given offset from
///   a block of virtual memory into a provided buffer.
///
/// @param state Pointer to virtual memory state.
/// @param offset Offset in file to read from.
/// @param length The number of bytes to read.
/// @param buffer A pointer to a block of memory to read into.
///
/// @return Returns the number of bytes successfully read.
uint32_t virtualMemoryRead(VirtualMemoryState *state,
  uint32_t offset, uint32_t length, void *buffer
) {
  if ((state == NULL) || (state->fileHandle == NULL)
    || (length == 0) || (buffer == NULL)
  ) {
    return 0;
  }

  virtualMemoryPrepare(state, offset + length);
  // Invalidate the in-memory data.
  state->bufferValidBytes = 0;
  state->bufferBaseOffset = 0;

  // Read the data from the requested location
  fseek(state->fileHandle, offset, SEEK_SET);
  return fread(buffer, 1, length, state->fileHandle);
}

/// @fn uint32_t virtualMemoryWrite(VirtualMemoryState *state,
///   uint32_t offset, uint32_t length, const void *buffer)
///
/// @brief Write a specified number of bytes starting from a given offset from
///   a block of virtual memory into a provided buffer.
///
/// @param state Pointer to virtual memory state.
/// @param offset Offset in file to write to.
/// @param length The number of bytes to write.
/// @param buffer A pointer to a block of memory to write from.
///
/// @return Returns the number of bytes successfully written.
uint32_t virtualMemoryWrite(VirtualMemoryState *state,
  uint32_t offset, uint32_t length, const void *buffer
) {
  if ((state == NULL) || (state->fileHandle == NULL)
    || (length == 0) || (buffer == NULL)
  ) {
    return 0;
  }

  virtualMemoryPrepare(state, offset + length);
  // Invalidate the in-memory data.
  state->bufferValidBytes = 0;
  state->bufferBaseOffset = 0;

  // Write the data from the requested location
  fseek(state->fileHandle, offset, SEEK_SET);
  return fwrite(buffer, 1, length, state->fileHandle);
}

uint32_t virtualMemoryCopy(VirtualMemoryState *srcVm, uint32_t srcStart,
  VirtualMemoryState *dstVm, uint32_t dstStart, uint32_t length
) {
  virtualMemoryPrepare(srcVm, srcStart + length);
  srcVm->bufferValidBytes = 0;
  srcVm->bufferBaseOffset = 0;

  virtualMemoryPrepare(dstVm, dstStart + length);
  dstVm->bufferValidBytes = 0;
  dstVm->bufferBaseOffset = 0;

  return
    fcopy(srcVm->fileHandle, srcStart, dstVm->fileHandle, dstStart, length);
}

