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
  if (length > NANO_OS_MAX_READ_WRITE_LENGTH) {
    length = NANO_OS_MAX_READ_WRITE_LENGTH;
  }
  
  // Read string from VM memory
  char *buffer = (char*) malloc(length);
  int segmentIndex = 0;
  rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &bufferAddress);
  uint32_t bytesRead
    = virtualMemoryRead(&rv32iVm->memorySegments[segmentIndex],
    bufferAddress, length, buffer);
  //// printDebug("length: ");
  //// printDebug(length);
  //// printDebug("\n");
  Serial.write(buffer, length);
  
  // Write to the stream
  fwrite(buffer, 1, bytesRead, stream);

  // Free the host-side memory
  free(buffer); buffer = NULL;
  
  // Return number of bytes written
  rv32iVm->rv32iCoreRegisters->x[10] = bytesRead;
  return 0;
}

/// @fn int32_t nanoOsSystemCallHandleRead(Rv32iVm *rv32iVm)
///
/// @brief Handle a user process reading from a FILE pointer.
///
/// @param rv32iVm A pointer to the Rv32iVm object that's managing the program's
/// state.
///
/// @return Returns 0 on success, negative error code on failure.
int32_t nanoOsSystemCallHandleRead(Rv32iVm *rv32iVm) {
  // Get parameters from a0-a2 (x10-x12)
  FILE *stream = (FILE*) rv32iVm->rv32iCoreRegisters->x[10];
  uint32_t bufferAddress = rv32iVm->rv32iCoreRegisters->x[11];
  uint32_t length = rv32iVm->rv32iCoreRegisters->x[12];
  
  // Limit maximum read length
  if (length > NANO_OS_MAX_READ_WRITE_LENGTH) {
    length = NANO_OS_MAX_READ_WRITE_LENGTH;
  }
  
  // Read string from VM memory
  char *buffer = (char*) malloc(length);

  // Write to the stream
  size_t bytesRead = fread(buffer, 1, length, stream);

  int segmentIndex = 0;
  rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &bufferAddress);
  virtualMemoryWrite(&rv32iVm->memorySegments[segmentIndex],
    bufferAddress, bytesRead, buffer);
  
  // Free the host-side memory
  free(buffer); buffer = NULL;
  
  // Return number of bytes read
  rv32iVm->rv32iCoreRegisters->x[10] = bytesRead;
  return 0;
}

/// @fn int32_t nanoOsSystemCallHandleTimespecGet(Rv32iVm *rv32iVm)
///
/// @brief Handle a user process requesting the current time.
///
/// @param rv32iVm A pointer to the Rv32iVm object that's managing the program's
/// state.
///
/// @return Returns 0 on success, negative error code on failure.
int32_t nanoOsSystemCallHandleTimespecGet(Rv32iVm *rv32iVm) {
  // Get parameters from a0-a1 (x10-x11)
  uint32_t timespecAddress = rv32iVm->rv32iCoreRegisters->x[10];
  int base = rv32iVm->rv32iCoreRegisters->x[11];
  
  // Get current time
  struct timespec currentTime;
  int result = timespec_get(&currentTime, base);
  
  if (result != 0) {
    // Write timespec to VM memory
    int segmentIndex = 0;
    rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &timespecAddress);
    virtualMemoryWrite(&rv32iVm->memorySegments[segmentIndex],
      timespecAddress, sizeof(struct timespec), &currentTime);
  }
  
  // Return result
  rv32iVm->rv32iCoreRegisters->x[10] = result;
  return 0;
}

/// @fn int32_t nanoOsSystemCallHandleNanosleep(Rv32iVm *rv32iVm)
///
/// @brief Handle a user process requesting to sleep.
///
/// @param rv32iVm A pointer to the Rv32iVm object that's managing the program's
/// state.
///
/// @return Returns 0 on success, negative error code on failure.
int32_t nanoOsSystemCallHandleNanosleep(Rv32iVm *rv32iVm) {
  // Get parameters from a0-a1 (x10-x11)
  uint32_t requestAddress = rv32iVm->rv32iCoreRegisters->x[10];
  uint32_t remainAddress = rv32iVm->rv32iCoreRegisters->x[11];
  
  // Read request timespec from VM memory
  struct timespec request;
  int segmentIndex = 0;
  rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &requestAddress);
  virtualMemoryRead(&rv32iVm->memorySegments[segmentIndex],
    requestAddress, sizeof(struct timespec), &request);
  
  // Sleep
  struct timespec remain = {};
  int result = 0;
  Comessage *comessage = comessageQueueWait(&request);
  
  if (comessage != NULL) {
    // Return an error
    result = -1;

    if (remainAddress != 0) {
      int64_t requestTime
        = (request.tv_sec * 1000000000LL) + ((int64_t) request.tv_nsec);
      timespec_get(&request, TIME_UTC);
      int64_t now
        = (request.tv_sec * 1000000000LL) + ((int64_t) request.tv_nsec);
      if (requestTime > now) {
        remain.tv_sec = (requestTime - now) / 1000000000LL;
        remain.tv_nsec = (requestTime - now) % 1000000000LL;
      }
      // Write remaining time to VM memory if requested
      rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &remainAddress);
      virtualMemoryWrite(&rv32iVm->memorySegments[segmentIndex],
        remainAddress, sizeof(struct timespec), &remain);
    }
  }
  
  // Return result
  rv32iVm->rv32iCoreRegisters->x[10] = result;
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
  nanoOsSystemCallHandleExit,        // NANO_OS_SYSCALL_EXIT
  nanoOsSystemCallHandleWrite,       // NANO_OS_SYSCALL_WRITE
  nanoOsSystemCallHandleRead,        // NANO_OS_SYSCALL_READ
  nanoOsSystemCallHandleTimespecGet, // NANO_OS_SYSCALL_TIMESPEC_GET
  nanoOsSystemCallHandleNanosleep,   // NANO_OS_SYSCALL_NANOSLEEP
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

