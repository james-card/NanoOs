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

/// @def RV32I_TIMESPEC_SIZE
///
/// @brief The number of bytes that a `struct timespec` consumes in a program
/// running on the RV32I VM.  This size is different than the size that the
/// structure consumes in NanoOs.
#define RV32I_TIMESPEC_SIZE 16

/// @fn int nanoOsSystemCallGetVmTimespec(
///   Rv32iVm *rv32iVm, uint32_t vmTimespecAddress, struct timespec *ts)
///
/// @brief Get a `struct timespec` from the VM's memory and parse it into one
/// that's native to the OS memory alignment.
///
/// @param rv32iVm A pointer to the Rv32iVm object that manages the VM's state.
/// @param vmTimespecAddress The address of the `struct timespec` as known
///   within the VM's address space.
/// @param ts A pointer to the `struct timespec` within the OS's address space.
///
/// @return Returns 0 on success, negative error code on failure.
int nanoOsSystemCallGetVmTimespec(
  Rv32iVm *rv32iVm, uint32_t vmTimespecAddress, struct timespec *ts
) {
  // First, read the data from the right place in the VM.
  uint8_t timespecBuffer[RV32I_TIMESPEC_SIZE];
  int segmentIndex = 0;
  rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &vmTimespecAddress);
  virtualMemoryRead(&rv32iVm->memorySegments[segmentIndex],
    vmTimespecAddress, sizeof(timespecBuffer), timespecBuffer);

  // Pull the data out of the buffer into the struct timespec we were provided.
  // On the VM, a time_t (tv_sec) consumes 8 bytes and a long (tv_nsec)
  // consumes 4 bytes, so use the appropriate types and offsets.
  ts->tv_sec = *((int64_t*) &timespecBuffer[0]);
  ts->tv_nsec = *((int32_t*) &timespecBuffer[sizeof(int64_t)]);

  return 0;
}

/// @fn int nanoOsSystemCallSetVmTimespec(
///   Rv32iVm *rv32iVm, uint32_t vmTimespecAddress, struct timespec *ts)
///
/// @brief Write a `struct timespec` into the VM's memory based on one that's
/// native to the OS memory alignment.
///
/// @param rv32iVm A pointer to the Rv32iVm object that manages the VM's state.
/// @param vmTimespecAddress The address of the `struct timespec` as known
///   within the VM's address space.
/// @param ts A pointer to the `struct timespec` within the OS's address space.
///
/// @return Returns 0 on success, negative error code on failure.
int nanoOsSystemCallSetVmTimespec(
  Rv32iVm *rv32iVm, uint32_t vmTimespecAddress, struct timespec *ts
) {
  uint8_t timespecBuffer[RV32I_TIMESPEC_SIZE];
  // Put the data into the buffer from the struct timespec we were provided.
  // On the VM, a time_t (tv_sec) consumes 8 bytes and a long (tv_nsec)
  // consumes 4 bytes, so use the appropriate types and offsets.
  *((int64_t*) &timespecBuffer[0]) = ts->tv_sec;
  *((int32_t*) &timespecBuffer[sizeof(int64_t)]) = ts->tv_nsec;

  // Write the data to the right place in the VM.
  int segmentIndex = 0;
  rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &vmTimespecAddress);
  virtualMemoryWrite(&rv32iVm->memorySegments[segmentIndex],
    vmTimespecAddress, sizeof(timespecBuffer), timespecBuffer);

  return 0;
}

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
    nanoOsSystemCallSetVmTimespec(rv32iVm, timespecAddress, &currentTime);
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
  nanoOsSystemCallGetVmTimespec(rv32iVm, requestAddress, &request);
  
  // Sleep
  struct timespec remain = {};
  int result = 0;
  Comessage *comessage = comessageQueueWait(&request);
  
  if (comessage != NULL) {
    // Return an error
    result = -1;

    // Write remaining time to VM memory if requested
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
      nanoOsSystemCallSetVmTimespec(rv32iVm, remainAddress, &remain);
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

