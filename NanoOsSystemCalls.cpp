#include "NanoOsSystemCalls.h"
#include "Rv32iVm.h"

int32_t nanoOsSystemCallHandleExit(Rv32iVm *rv32iVm) {
  // Get exit code from a0 (x10)
  rv32iVm->running = false;
  rv32iVm->exitCode = rv32iVm->rv32iCoreRegisters->x[10];
  return 0;
}

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

typedef int32_t (*SystemCall)(Rv32iVm *rv32iVm);
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

