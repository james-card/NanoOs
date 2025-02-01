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

/// @fn int rv32iVmInit(Rv32iVm *rv32iVm, const char *programPath)
///
/// @brief Initialize an Rv32iVm structure.
///
/// @param rv32iVm A pointer to the Rv32iVm structure to initialize.
/// @param programPath The full path to the command binary on the filesystem.
///
/// @return Returns 0 on success, -1 on failure.
int rv32iVmInit(Rv32iVm *rv32iVm, const char *programPath) {
  char virtualMemoryFilename[13];
  VirtualMemoryState programBinary = {};
  if (virtualMemoryInit(&programBinary, programPath) != 0) {
    return -1;
  }

  sprintf(virtualMemoryFilename, "%lu.mem", getElapsedMilliseconds(0));
  if (virtualMemoryInit(&rv32iVm->physicalMemory, virtualMemoryFilename) != 0) {
    virtualMemoryCleanup(&programBinary, false);
    return -1;
  }
  if (virtualMemoryZero(&rv32iVm->physicalMemory, 0, RISCV_MEMORY_SIZE)
    != RISCV_MEMORY_SIZE
  ) {
    virtualMemoryCleanup(&programBinary, false);
    return -1;
  }

  if (virtualMemoryCopy(&programBinary, 0,
    &rv32iVm->physicalMemory, RISCV_PROGRAM_START,
    virtualMemorySize(&programBinary)) < programBinary.fileSize
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

/// @fn void rv32iVmCleanup(Rv32iVm *rv32iVm)
///
/// @brief Release all the resources being used by an Rv32iVm object.
///
/// @param rv32iVm A pointer to the Rv32iVm that contains the resources to
///   release.
///
/// @return This function returns no value.
void rv32iVmCleanup(Rv32iVm *rv32iVm) {
  virtualMemoryCleanup(&rv32iVm->mappedMemory, true);
  virtualMemoryCleanup(&rv32iVm->physicalMemory, true);
}

/// @fn int32_t rv32iMemoryRead32(
///   Rv32iVm *rv32iVm, uint32_t address, uint32_t *value);
///
/// @brief Read memory at the given address, handling both physical and mapped
/// memory.
///
/// @param rv32iVm A pointer to the Rv32iVm that contains the virtual memory
///   states.
/// @param address 32-bit address (bit 25 determines physical vs mapped).
/// @param value Pointer to store the read value.
///
/// @return 0 on success, negative on error.
int32_t rv32iMemoryRead32(Rv32iVm *rv32iVm, uint32_t address, uint32_t *value) {
  if (address & 0x02000000) {
    // Mapped memory access - mask off the high bits
    return virtualMemoryRead32(
      &rv32iVm->mappedMemory,
      address & 0x01ffffff,
      value
    );
  } else {
    // Physical memory access
    return virtualMemoryRead32(&rv32iVm->physicalMemory, address, value);
  }
}

/// @fn int32_t rv32iMemoryWrite32(
///   Rv32iVm *rv32iVm, uint32_t address, uint32_t value)
///
/// @brief Write memory at the given address, handling both physical and mapped
/// memory
///
/// @param rv32iVm A pointer to the Rv32iVm that contains the virtual memory
///   states.
/// @param address 32-bit address (bit 25 determines physical vs mapped).
/// @param value Value to write.
///
/// @return 0 on success, negative on error.
int32_t rv32iMemoryWrite32(Rv32iVm *rv32iVm, uint32_t address, uint32_t value) {
  if (address & 0x02000000) {
    // Mapped memory access - mask off the high bits
    return virtualMemoryWrite32(
      &rv32iVm->mappedMemory,
      address & 0x01ffffff,
      value
    );
  } else {
    // Physical memory access
    return virtualMemoryWrite32(&rv32iVm->physicalMemory, address, value);
  }
}

/// @fn int runRv32iProcess(int argc, char **argv);
///
/// @brief Run a process compiled to the RV32I instruction set.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.  argv[0] must be the full path to the command
///   on the filesystem.
///
/// @return Returns a negative error code if there was a problem internal to
/// the VM, 0 on success, positive error code from the program if the program
/// itself failed.
int runRv32iProcess(int argc, char **argv) {
  (void) argc;
  Rv32iVm rv32iVm = {};
  if (rv32iVmInit(&rv32iVm, argv[0]) != 0) {
    rv32iVmCleanup(&rv32iVm);
  }

  // VM logic goes here
  rv32iVm.rv32iCoreRegisters.pc = RISCV_PROGRAM_START;
  rv32iVm.rv32iCoreRegisters.x[2] = RISCV_STACK_START;

  rv32iVmCleanup(&rv32iVm);
  return 0;
}

