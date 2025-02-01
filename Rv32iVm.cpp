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
#include "Rv32iVm.h"

int rv32iVmInit(Rv32iVm *rv32iVm, const char *programName) {
  char virtualMemoryFilename[13];
  VirtualMemoryState programBinary = {};
  if (virtualMemoryInit(&programBinary, programName) != 0) {
    return -1;
  }

  sprintf(virtualMemoryFilename, "%lu.mem", getElapsedMilliseconds(0));
  if (virtualMemoryInit(&rv32iVm->physicalMemory, virtualMemoryFilename) != 0) {
    virtualMemoryCleanup(&programBinary, false);
    return -1;
  }

  if (virtualMemoryCopy(&programBinary, 0, &rv32iVm->physicalMemory, 0x1000,
    programBinary.fileSize) < programBinary.fileSize
  ) {
    virtualMemoryCleanup(&programBinary, false);
    return -1;
  }

  virtualMemoryCleanup(&programBinary, false);

  sprintf(virtualMemoryFilename, "%lu.mem", getElapsedMilliseconds(0));
  if (virtualMemoryInit(&rv32iVm->mappedMemory, virtualMemoryFilename) != 0) {
    return -1;
  }

  return 0;
}

void rv32iVmCleanup(Rv32iVm *rv32iVm) {
  virtualMemoryCleanup(&rv32iVm->mappedMemory, true);
  virtualMemoryCleanup(&rv32iVm->physicalMemory, true);
}

int runRv32iProcess(int argc, char **argv) {
  (void) argc;
  Rv32iVm rv32iVm = {};
  if (rv32iVmInit(&rv32iVm, argv[0]) != 0) {
    rv32iVmCleanup(&rv32iVm);
  }

  // VM logic goes here

  rv32iVmCleanup(&rv32iVm);
  return 0;
}

