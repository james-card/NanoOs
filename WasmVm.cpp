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

#include "WasmVm.h"

/// @fn int wasmVmInit(WasmVm *wasmVm, const char *programPath)
///
/// @brief Initialize all the state information for starting a WASI VM process.
///
/// @param wasmVm A pointer to the WasmVm state object maintained by the
///   wasmVmMain function.
/// @param programPath The full path to the WASI binary on disk.
///
/// @return Returns 0 on success, -1 on failure.
int wasmVmInit(WasmVm *wasmVm, const char *programPath) {
  char tempFilename[13];

  if (virtualMemoryInit(&wasmVm->codeSegment, programPath) != 0) {
    return -1;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasmVm->linearMemory, tempFilename) != 0) {
    return -1;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasmVm->globalStack, tempFilename) != 0) {
    return -1;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasmVm->callStack, tempFilename) != 0) {
    return -1;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasmVm->globalStorage, tempFilename) != 0) {
    return -1;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasmVm->tableSpace, tempFilename) != 0) {
    return -1;
  }

  return 0;
}

/// @fn void wasmVmCleanup(WasmVm *wasmVm)
///
/// @brief Release all the resources allocated and held by a WASI VM state.
///
/// @param wasmVm A pointer to the WasmVm state object maintained by the
///   wasmVmMain function.
///
/// @return This function returns no value.
void wasmVmCleanup(WasmVm *wasmVm) {
  virtualMemoryCleanup(&wasmVm->tableSpace,    true);
  virtualMemoryCleanup(&wasmVm->globalStorage, true);
  virtualMemoryCleanup(&wasmVm->callStack,     true);
  virtualMemoryCleanup(&wasmVm->globalStack,   true);
  virtualMemoryCleanup(&wasmVm->linearMemory,  true);
  virtualMemoryCleanup(&wasmVm->codeSegment,   false);

  return;
}

