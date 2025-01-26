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
  char tempFilename[13];

  if (virtualMemoryInit(&wasiVm->codeSegment, programPath) != 0) {
    return -1;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm->linearMemory, tempFilename) != 0) {
    return -1;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm->globalStack, tempFilename) != 0) {
    return -1;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm->callStack, tempFilename) != 0) {
    return -1;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm->globalStorage, tempFilename) != 0) {
    return -1;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm->tableSpace, tempFilename) != 0) {
    return -1;
  }

  return 0;
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
  virtualMemoryCleanup(&wasiVm->tableSpace,    true);
  virtualMemoryCleanup(&wasiVm->globalStorage, true);
  virtualMemoryCleanup(&wasiVm->callStack,     true);
  virtualMemoryCleanup(&wasiVm->globalStack,   true);
  virtualMemoryCleanup(&wasiVm->linearMemory,  true);
  virtualMemoryCleanup(&wasiVm->codeSegment,   false);

  return;
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
      &wasiVm.codeSegment, wasiVm.programCounter++, &opcode) != 0
    ) {
      returnValue = -2;
      goto exit;
    }
  }

exit:
  wasiVmCleanup(&wasiVm);
  return returnValue;
}

