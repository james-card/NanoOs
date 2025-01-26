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

#include "WasiVm.h"

/// @fn int wasiFdTableInit(WasiFdTable *table)
///
/// @brief Initialize a WASI file descriptor table with standard streams.
///
/// @param table Pointer to file descriptor table to initialize.
///
/// @return Returns 0 on success, negative value on error.
int wasiFdTableInit(WasiFdTable *table) {
  // Start with space for 8 file descriptors
  table->maxFds = 8;
  table->nextFreeFd = 0;
  
  table->entries = (WasiFdEntry*) malloc(sizeof(WasiFdEntry) * table->maxFds);
  if (table->entries == NULL) {
    return -1;
  }

  // Initialize stdin
  table->entries[0].hostFd = 0;
  table->entries[0].type = __WASI_FILETYPE_CHARACTER_DEVICE;
  table->entries[0].flags = 0;
  table->entries[0].rights = __WASI_RIGHT_FD_READ
                           | __WASI_RIGHT_FD_ADVISE
                           | __WASI_RIGHT_FD_TELL
                           | __WASI_RIGHT_POLL_FD_READWRITE;
  table->entries[0].rightsInherit = 0;
  table->entries[0].offset = 0;
  table->entries[0].isPreopened = true;
  table->nextFreeFd++;

  // Initialize stdout
  table->entries[1].hostFd = 1;
  table->entries[1].type = __WASI_FILETYPE_CHARACTER_DEVICE;
  table->entries[1].flags = 0;
  table->entries[1].rights = __WASI_RIGHT_FD_WRITE
                           | __WASI_RIGHT_FD_ADVISE
                           | __WASI_RIGHT_FD_TELL
                           | __WASI_RIGHT_POLL_FD_READWRITE;
  table->entries[1].rightsInherit = 0;
  table->entries[1].offset = 0;
  table->entries[1].isPreopened = true;
  table->nextFreeFd++;

  // Initialize stderr
  table->entries[2].hostFd = 2;
  table->entries[2].type = __WASI_FILETYPE_CHARACTER_DEVICE;
  table->entries[2].flags = 0;
  table->entries[2].rights = __WASI_RIGHT_FD_WRITE
                           | __WASI_RIGHT_FD_ADVISE
                           | __WASI_RIGHT_FD_TELL
                           | __WASI_RIGHT_POLL_FD_READWRITE;
  table->entries[2].rightsInherit = 0;
  table->entries[2].offset = 0;
  table->entries[2].isPreopened = true;
  table->nextFreeFd++;

  return 0;
}

/// @fn void wasiFdTableCleanup(WasiFdTable *table)
///
/// @brief Clean up resources used by a WASI file descriptor table.
///
/// @param table Pointer to a previously-initialized file descriptor table to
///   clean up.
///
/// @return This function returns no value.
void wasiFdTableCleanup(WasiFdTable *table) {
  if (table->entries != NULL) {
    free(table->entries);
    table->entries = NULL;
  }
  table->maxFds = 0;
  table->nextFreeFd = 0;
}

/// @fn int wasiVmInit(WasiVm *wasiVm, const char *programPath)
///
/// @brief Initialize all the state information for starting a WASI VM process.
///
/// @param wasiVm A pointer to the WasiVm state object maintained by the
///   wasiVmMain function.
/// @param programPath The full path to the WASI binary on disk.
///
/// @return Returns 0 on success, -1 on failure.
int wasiVmInit(WasiVm *wasiVm, const char *programPath) {
  int returnValue = wasmVmInit(&wasiVm->wasmVm, programPath);
  if (returnValue == 0) {
    returnValue = wasiFdTableInit(&wasiVm->wasiFdTable);
  }

  return returnValue;
}

/// @fn void wasiVmCleanup(WasiVm *wasiVm)
///
/// @brief Release all the resources allocated and held by a WASI VM state.
///
/// @param wasiVm A pointer to the WasiVm state object maintained by the
///   wasiVmMain function.
///
/// @return This function returns no value.
void wasiVmCleanup(WasiVm *wasiVm) {
  wasiFdTableCleanup(&wasiVm->wasiFdTable);
  wasmVmCleanup(&wasiVm->wasmVm);

  return;
}

/// @fn int32_t wasiArgsSizesGet(WasiVm *wasiVm, int hostArgc, char **hostArgv)
///
/// @brief Get sizes needed for storing command line arguments in WASI
///
/// @param wasiVm Pointer to WASI VM state
/// @param hostArgc Number of command line arguments from host
/// @param hostArgv Array of command line argument strings from host
///
/// @return Returns 0 on success, negative value on error
int32_t wasiArgsSizesGet(WasiVm *wasiVm, int hostArgc, char **hostArgv) {
  // Pop memory locations from operand stack
  uint32_t argvBufSizePtr;
  uint32_t argcPtr;
  
  // Pop in reverse order since they were pushed in the opposite order
  if (wasmStackPop32(&wasiVm->wasmVm.globalStack, &argvBufSizePtr) != 0) {
    return -1;
  }
  if (wasmStackPop32(&wasiVm->wasmVm.globalStack, &argcPtr) != 0) {
    return -1;
  }

  // Calculate total size needed for all arguments including null terminators
  uint32_t totalSize = 0;
  for (int ii = 0; ii < hostArgc; ii++) {
    totalSize += strlen(hostArgv[ii]) + 1;  // +1 for null terminator
  }

  // Write values to linear memory
  if (virtualMemoryWrite32(&wasiVm->wasmVm.linearMemory, argcPtr,
    (uint32_t) hostArgc) != 0
  ) {
    return -1;
  }
  if (virtualMemoryWrite32(&wasiVm->wasmVm.linearMemory, argvBufSizePtr,
    totalSize) != 0
  ) {
    return -1;
  }

  // Push success return value (0) onto operand stack
  if (wasmStackPush32(&wasiVm->wasmVm.globalStack, 0) != 0) {
    return -1;
  }

  return 0;
}

/// @fn int wasiVmMain(int argc, char **argv)
///
/// @brief Main entry point for running a WASI-compiled binary.
///
/// @param argc The number of arguments parsed from the command line.
/// @param argv The array of string pointers to parsed command line arguments.
///
/// @return Returns 0 on success, negative error value on VM error, positive
/// error value on program error.
int wasiVmMain(int argc, char **argv) {
  int returnValue = 0;
  WasiVm wasiVm = {};
  uint8_t opcode = 0;

  if (wasiVmInit(&wasiVm, argv[0]) != 0) {
    returnValue = -1;
    goto exit;
  }

  while (1) {
    if (virtualMemoryRead8(
      &wasiVm.wasmVm.codeSegment, wasiVm.wasmVm.programCounter, &opcode) != 0
    ) {
      returnValue = -2;
      goto exit;
    }
  }

exit:
  wasiVmCleanup(&wasiVm);
  return returnValue;
}

