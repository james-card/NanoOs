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

#include "NanoOsSystemCalls.h"
#include "Rv32iVm.h"

/// @fn int32_t nanoOsSystemCallHandleExit(Rv32iVm *rv32iVm)
///
/// @brief Handle a user process exiting.
///
/// @param rv32iVm A pointer to the Rv32iVm object that's managing the program's
/// state.
///
/// @return This function always returns 0.
int32_t nanoOsSystemCallHandleExit(Rv32iVm *rv32iVm) {
  // Get exit code from a0 (x10)
  rv32iVm->running = false;
  rv32iVm->exitCode = rv32iVm->rv32iCoreRegisters->x[10];
  return 0;
}

/// @fn int32_t nanoOsSystemCallHandleWrite(Rv32iVm *rv32iVm)
///
/// @brief Handle a user process writing to a FILE pointer.
///
/// @param rv32iVm A pointer to the Rv32iVm object that's managing the program's
/// state.
///
/// @return Returns 0 on success, negative error code on failure.
int32_t nanoOsSystemCallHandleWrite(Rv32iVm *rv32iVm) {
  // Get parameters from a0-a2 (x10-x12)
  FILE *stream = (FILE*) rv32iVm->rv32iCoreRegisters->x[10];
  uint32_t bufferAddress = rv32iVm->rv32iCoreRegisters->x[11];
  uint32_t length = rv32iVm->rv32iCoreRegisters->x[12];
  
  // Limit maximum write length
  if (length > NANO_OS_MAX_WRITE_LENGTH) {
    length = NANO_OS_MAX_WRITE_LENGTH;
  }
  
  // Read string from VM memory
  char *buffer = (char*) malloc(length);
  uint32_t bytesRead
    = virtualMemoryRead(&rv32iVm->memorySegments[RV32I_DATA_MEMORY],
    bufferAddress, length, buffer);
  
  // Write to the stream
  fwrite(buffer, 1, bytesRead, stream);

  // Free the host-side memory
  free(buffer); buffer = NULL;
  
  // Return number of bytes written
  rv32iVm->rv32iCoreRegisters->x[10] = bytesRead;
  return 0;
}

/// @typedef SystemCall
///
/// @brief Definition for the structure of a system call handler in this
/// library.
typedef int32_t (*SystemCall)(Rv32iVm *rv32iVm);

/// @var systemCalls
///
/// @brief Array of SystemCall functions that correpsond to the NanoOsSystemCall
/// values that can be used by user processes.
SystemCall systemCalls[] = {
  nanoOsSystemCallHandleExit,  // NANO_OS_SYSCALL_EXIT
  nanoOsSystemCallHandleWrite, // NANO_OS_SYSCALL_WRITE
};

/// @fn int32_t nanoOsSystemCallHandle(void *vm)
///
/// @brief Handle NanoOs system calls from the running program.
///
/// @param vm A pointer to a Rv32iVm state object cast to a void*
///
/// @return Returns 0 on success, negative on error.
int32_t nanoOsSystemCallHandle(void *vm) {
  Rv32iVm *rv32iVm = (Rv32iVm*) vm;

  // Get syscall number from a7 (x17)
  uint32_t syscallNumber = rv32iVm->rv32iCoreRegisters->x[17];
  
  if (syscallNumber < NUM_NANO_OS_SYSCALLS) {
    return systemCalls[syscallNumber](rv32iVm);
  }
  
  return -1;
}

