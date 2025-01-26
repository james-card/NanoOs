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
  state->fileHandle = fopen(filename, "w+b");
  if (state->fileHandle == NULL) {
    return -1;
  }

  // Initialize buffer state
  state->bufferBaseOffset = 0;
  state->bufferValidBytes = 0;
  memset(state->buffer, 0, VIRTUAL_MEMORY_BUFFER_SIZE);
  strcpy(state->filename, filename);

  return 0;
}

/// @fn void virtualMemoryCleanup(VirtualMemoryState *state)
///
/// @brief Clean up virtual memory system resources.
///
/// @param state Pointer to state structure to clean up.
///
/// @return This function returns no value.
void virtualMemoryCleanup(VirtualMemoryState *state) {
  fclose(state->fileHandle); state->fileHandle = NULL;
  remove(state->filename);
  *state->filename = '\0';
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

  // Check if requested byte is in buffer
  if ((offset >= state->bufferBaseOffset) && 
      (offset < state->bufferBaseOffset + state->bufferValidBytes)) {
    return &state->buffer[offset - state->bufferBaseOffset];
  }

  // Need to load new data into buffer
  if (state->bufferValidBytes > 0) {
    // Write current buffer if it contains data
    fseek(state->fileHandle, state->bufferBaseOffset, SEEK_SET);
    fwrite(state->buffer, 1, state->bufferValidBytes, state->fileHandle);
  }

  // Read new buffer from requested location
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

