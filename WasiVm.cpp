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

#include "WasiVm.h"

int wasiMain(int argc, char **argv) {
  int returnValue = 0;
  WasiVm wasiVm = {};
  char tempFilename[13];

  if (virtualMemoryInit(&wasiVm.codeSegment, argv[0]) != 0) {
    returnValue = -1;
    goto exit;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm.linearMemory, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupCodeSegment;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm.globalStack, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupLinearMemory;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm.callStack, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupGlobalStack;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm.globalStorage, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupCallStack;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm.tableSpace, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupGlobalStorage;
  }

cleanupTableSpace:
  virtualMemoryCleanup(&wasiVm.tableSpace, true);

cleanupGlobalStorage:
  virtualMemoryCleanup(&wasiVm.globalStorage, true);

cleanupCallStack:
  virtualMemoryCleanup(&wasiVm.callStack, true);

cleanupGlobalStack:
  virtualMemoryCleanup(&wasiVm.globalStack, true);

cleanupLinearMemory:
  virtualMemoryCleanup(&wasiVm.linearMemory, true);

cleanupCodeSegment:
  virtualMemoryCleanup(&wasiVm.codeSegment, false);

exit:
  return returnValue;
}

