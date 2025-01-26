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
  (void) state;
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
  (void) state;
  (void) offset;
  (void) value;
  return 0;
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
  (void) state;
  (void) offset;
  (void) value;
  return 0;
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
  (void) state;
  (void) offset;
  (void) value;
  return 0;
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
  (void) state;
  (void) offset;
  (void) value;
  return 0;
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
  (void) state;
  (void) offset;
  (void) value;
  return 0;
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
  (void) state;
  (void) offset;
  (void) value;
  return 0;
}

