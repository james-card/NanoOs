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
#include "NanoOsExe.h"
#include "NanoOsSystemCalls.h"

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

  NanoOsExeMetadata *nanoOsExeMetadata = nanoOsExeMetadataRead(programPath);
  if (nanoOsExeMetadata == NULL) {
    // This is not one of our executables and there's nothing we can do.  Bail.
    //// printDebug("Not our exe");
    nanoOsExeMetadata = nanoOsExeMetadataDestroy(nanoOsExeMetadata);
    return -1;
  }

  rv32iVm->rv32iCoreRegisters
    = (Rv32iCoreRegisters*) malloc(sizeof(Rv32iCoreRegisters));
  if (rv32iVm->rv32iCoreRegisters == NULL) {
    // Nothing we can do.
    nanoOsExeMetadata = nanoOsExeMetadataDestroy(nanoOsExeMetadata);
    return -1;
  }

  // We're going to use the same file for both program and data memory.
  sprintf(virtualMemoryFilename, "pid%uphy.mem", getRunningProcessId());
  if (virtualMemoryInit(
    &rv32iVm->memorySegments[RV32I_PROGRAM_MEMORY],
    virtualMemoryFilename, 128, NULL) != 0
  ) {
    //// printDebug("phy init failed");
    nanoOsExeMetadata = nanoOsExeMetadataDestroy(nanoOsExeMetadata);
    return -1;
  }
  //// size_t freeMemory = getFreeMemory();
  if (virtualMemoryInit(
    &rv32iVm->memorySegments[RV32I_DATA_MEMORY],
    virtualMemoryFilename,
    sizeof(rv32iVm->dataCacheBuffer),
    rv32iVm->dataCacheBuffer) != 0
  ) {
    //// printDebug("data init failed");
    nanoOsExeMetadata = nanoOsExeMetadataDestroy(nanoOsExeMetadata);
    return -1;
  }
  //// printDebug("virtualMemoryInit consumed ");
  //// printDebug(freeMemory - getFreeMemory());
  //// printDebug(" bytes\n");

  VirtualMemoryState programBinary = {};
  if (virtualMemoryInit(&programBinary, programPath, 0, NULL) != 0) {
    //// printDebug("program init failed");
    nanoOsExeMetadata = nanoOsExeMetadataDestroy(nanoOsExeMetadata);
    return -1;
  }

  // We only need to do one copy to physical memory.
  if (virtualMemoryCopy(&programBinary, 0,
    &rv32iVm->memorySegments[RV32I_PROGRAM_MEMORY], RV32I_PROGRAM_START,
    virtualMemorySize(&programBinary)) < virtualMemorySize(&programBinary)
  ) {
    //// printDebug("copy failed");
    virtualMemoryCleanup(&programBinary, false);
    nanoOsExeMetadata = nanoOsExeMetadataDestroy(nanoOsExeMetadata);
    return -1;
  }
  virtualMemorySetSize(
    &rv32iVm->memorySegments[RV32I_PROGRAM_MEMORY],
    virtualMemorySize(&programBinary) + RV32I_PROGRAM_START);
  virtualMemorySetSize(
    &rv32iVm->memorySegments[RV32I_DATA_MEMORY],
    virtualMemorySize(&programBinary) + RV32I_PROGRAM_START);
  virtualMemoryCleanup(&programBinary, false);

  rv32iVm->dataStart
    = RV32I_PROGRAM_START + nanoOsExeMetadataProgramLength(nanoOsExeMetadata);
  rv32iVm->dataEnd
    = rv32iVm->dataStart + nanoOsExeMetadataDataLength(nanoOsExeMetadata);
  nanoOsExeMetadata = nanoOsExeMetadataDestroy(nanoOsExeMetadata);

  // Prime the program and data virtual memory segments.
  virtualMemoryRead8(&rv32iVm->memorySegments[RV32I_PROGRAM_MEMORY],
    RV32I_PROGRAM_START, (uint8_t*) virtualMemoryFilename);
  virtualMemoryRead8(&rv32iVm->memorySegments[RV32I_DATA_MEMORY],
    rv32iVm->dataStart, (uint8_t*) virtualMemoryFilename);

  sprintf(virtualMemoryFilename, "pid%ustk.mem", getRunningProcessId());
  if (virtualMemoryInit(&rv32iVm->memorySegments[RV32I_STACK_MEMORY],
    virtualMemoryFilename, 32, NULL) != 0
  ) {
    return -1;
  }
  virtualMemoryRead8(&rv32iVm->memorySegments[RV32I_STACK_MEMORY],
    0x0, (uint8_t*) virtualMemoryFilename);

  sprintf(virtualMemoryFilename, "pid%umap.mem", getRunningProcessId());
  if (virtualMemoryInit(&rv32iVm->memorySegments[RV32I_MAPPED_MEMORY],
    virtualMemoryFilename,
    sizeof(rv32iVm->mapCacheBuffer),
    rv32iVm->mapCacheBuffer) != 0
  ) {
    return -1;
  }

  // Initialize MISA register to indicate RV32IM support
  rv32iVm->rv32iCoreRegisters->misa = RV32_MISA_MXL_32 | 
    RV32_MISA_I_EXT | RV32_MISA_M_EXT;

  rv32iVm->running = true;
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
  virtualMemoryCleanup(&rv32iVm->memorySegments[RV32I_MAPPED_MEMORY], false);
  virtualMemoryCleanup(&rv32iVm->memorySegments[RV32I_STACK_MEMORY], true);
  virtualMemoryCleanup(&rv32iVm->memorySegments[RV32I_DATA_MEMORY], true);
  virtualMemoryCleanup(&rv32iVm->memorySegments[RV32I_PROGRAM_MEMORY], true);
  free(rv32iVm->rv32iCoreRegisters); // No need to nullify it
}

/// @fn void rv32iGetMemorySegmentAndAddress(
///   Rv32iVm *rv32iVm, int *segmentIndex, uint32_t *address)
///
/// @brief Get the segment index and true address offset given a raw address
/// value.
///
/// @param rv32iVm A pointer to the Rv32iVm that contains the virtual memory
///   states.
/// @param segmentIndex Pointer to an integer that will hold the index of the
///   memory segment that is to be used for the memory access.
/// @param address Pointer to the raw address that was specified.  May be
///   updated by this function with the real offset to use within the
///   determined memory segment.
///
/// @return This function returns no value.
void rv32iGetMemorySegmentAndAddress(Rv32iVm *rv32iVm,
  int *segmentIndex, uint32_t *address
) {
  *segmentIndex = *address >> RV32I_MEMORY_SEGMENT_SHIFT;
  switch (*segmentIndex) {
    case RV32I_PROGRAM_MEMORY:
      {
        if (*address >= rv32iVm->dataStart) {
          *segmentIndex = RV32I_DATA_MEMORY;
        }
        break;
      }
    case RV32I_STACK_MEMORY:
      {
        *address = RV32I_STACK_START - *address - RV32I_INSTRUCTION_SIZE;
        break;
      }
    case RV32I_MAPPED_MEMORY:
      {
        *address &= RV32I_CLINT_ADDR_MASK;
        break;
      }
    // default:  No need to do anything to the address.
  }
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
  int segmentIndex = 0;
  rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &address);
  return virtualMemoryRead32(
    &rv32iVm->memorySegments[segmentIndex], address, value);
}

/// @fn int32_t rv32iMemoryRead16(
///   Rv32iVm *rv32iVm, uint32_t address, uint16_t *value);
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
int32_t rv32iMemoryRead16(Rv32iVm *rv32iVm, uint32_t address, uint16_t *value) {
  int segmentIndex = 0;
  rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &address);
  return virtualMemoryRead16(
    &rv32iVm->memorySegments[segmentIndex], address, value);
}

/// @fn int32_t rv32iMemoryRead8(
///   Rv32iVm *rv32iVm, uint32_t address, uint8_t *value);
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
int32_t rv32iMemoryRead8(Rv32iVm *rv32iVm, uint32_t address, uint8_t *value) {
  int segmentIndex = 0;
  printDebug("Reading byte at address 0x");
  printDebug(address, HEX);
  rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &address);
  printDebug(" (");
  printDebug(segmentIndex);
  printDebug(", 0x");
  printDebug(address, HEX);
  printDebug(")\n");
  return virtualMemoryRead8(
    &rv32iVm->memorySegments[segmentIndex], address, value);
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
  int segmentIndex = 0;
  rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &address);
  return virtualMemoryWrite32(
    &rv32iVm->memorySegments[segmentIndex], address, value);
}

/// @fn int32_t rv32iMemoryWrite16(
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
int32_t rv32iMemoryWrite16(Rv32iVm *rv32iVm, uint32_t address, uint32_t value) {
  int segmentIndex = 0;
  rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &address);
  return virtualMemoryWrite16(
    &rv32iVm->memorySegments[segmentIndex], address, value);
}

/// @fn int32_t rv32iMemoryWrite8(
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
int32_t rv32iMemoryWrite8(Rv32iVm *rv32iVm, uint32_t address, uint32_t value) {
  int segmentIndex = 0;
  rv32iGetMemorySegmentAndAddress(rv32iVm, &segmentIndex, &address);

  return virtualMemoryWrite8(
    &rv32iVm->memorySegments[segmentIndex], address, value);
}

/// @fn int32_t fetchInstruction(Rv32iVm *rv32iVm, uint32_t *instruction)
///
/// @brief Fetch the next instruction from memory at the current PC
///
/// @param rv32iVm Pointer to the VM state
/// @param instruction Pointer to store the fetched instruction
///
/// @return 0 on success, negative on error
static inline int32_t fetchInstruction(
  Rv32iVm *rv32iVm, uint32_t *instruction
) {
  return virtualMemoryRead32(&rv32iVm->memorySegments[RV32I_PROGRAM_MEMORY],
    rv32iVm->rv32iCoreRegisters->pc, instruction);
}

/// @fn static inline int32_t executeMultiplyDivide(
///   Rv32iVm *rv32iVm, uint32_t rd, uint32_t rs1, uint32_t rs2,
///   uint32_t funct3)
///
/// @brief Execute a multiply or divide operation (M-extension)
///
/// @param rv32iVm Pointer to the VM state
/// @param rd Destination register number
/// @param rs1 First source register number
/// @param rs2 Second source register number
/// @param funct3 The funct3 field from instruction
///
/// @return 0 on success, negative on error
static inline int32_t executeMultiplyDivide(
  Rv32iVm *rv32iVm, uint32_t rd, uint32_t rs1, uint32_t rs2,
  uint32_t funct3
) {
  int32_t src1Signed = (int32_t) rv32iVm->rv32iCoreRegisters->x[rs1];
  int32_t src2Signed = (int32_t) rv32iVm->rv32iCoreRegisters->x[rs2];
  uint32_t src1Unsigned = rv32iVm->rv32iCoreRegisters->x[rs1];
  uint32_t src2Unsigned = rv32iVm->rv32iCoreRegisters->x[rs2];
  
  switch (funct3) {
    case RV32M_FUNCT3_MUL: {
      // Multiply low
      rv32iVm->rv32iCoreRegisters->x[rd] = src1Signed * src2Signed;
      break;
    }
    
    case RV32M_FUNCT3_MULH: {
      // Multiply high (signed x signed)
      int64_t result = ((int64_t) src1Signed) * ((int64_t) src2Signed);
      rv32iVm->rv32iCoreRegisters->x[rd] = (uint32_t) (result >> 32);
      break;
    }
    
    case RV32M_FUNCT3_MULHSU: {
      // Multiply high (signed x unsigned)
      int64_t result = ((int64_t) src1Signed) * ((uint64_t) src2Unsigned);
      rv32iVm->rv32iCoreRegisters->x[rd] = (uint32_t) (result >> 32);
      break;
    }
    
    case RV32M_FUNCT3_MULHU: {
      // Multiply high (unsigned x unsigned)
      uint64_t result = ((uint64_t) src1Unsigned) * ((uint64_t) src2Unsigned);
      rv32iVm->rv32iCoreRegisters->x[rd] = (uint32_t) (result >> 32);
      break;
    }
    
    case RV32M_FUNCT3_DIV: {
      // Divide signed
      if (src2Signed == 0) {
        // Division by zero returns -1
        rv32iVm->rv32iCoreRegisters->x[rd] = -1;
      } else if (
        src1Signed == INT32_MIN && src2Signed == -1
      ) {
        // Overflow case: most negative number divided by -1
        rv32iVm->rv32iCoreRegisters->x[rd] = INT32_MIN;
      } else {
        rv32iVm->rv32iCoreRegisters->x[rd] = src1Signed / src2Signed;
      }
      break;
    }
    
    case RV32M_FUNCT3_DIVU: {
      // Divide unsigned
      if (src2Unsigned == 0) {
        // Division by zero returns maximum unsigned value
        rv32iVm->rv32iCoreRegisters->x[rd] = UINT32_MAX;
      } else {
        rv32iVm->rv32iCoreRegisters->x[rd] = src1Unsigned / src2Unsigned;
      }
      break;
    }
    
    case RV32M_FUNCT3_REM: {
      // Remainder signed
      if (src2Signed == 0) {
        // Remainder with division by zero returns dividend
        rv32iVm->rv32iCoreRegisters->x[rd] = src1Signed;
      } else if (
        src1Signed == INT32_MIN && src2Signed == -1
      ) {
        // Overflow case: most negative number divided by -1
        rv32iVm->rv32iCoreRegisters->x[rd] = 0;
      } else {
        rv32iVm->rv32iCoreRegisters->x[rd] = src1Signed % src2Signed;
      }
      break;
    }
    
    case RV32M_FUNCT3_REMU: {
      // Remainder unsigned
      if (src2Unsigned == 0) {
        // Remainder with division by zero returns dividend
        rv32iVm->rv32iCoreRegisters->x[rd] = src1Unsigned;
      } else {
        rv32iVm->rv32iCoreRegisters->x[rd] = src1Unsigned % src2Unsigned;
      }
      break;
    }
    
    default: {
      return -1; // Invalid funct3
    }
  }
  
  return 0;
}

/// @fn static inline int32_t executeRegisterOperation(
///   Rv32iVm *rv32iVm, uint32_t rd, uint32_t rs1, uint32_t rs2,
///   uint32_t funct3, uint32_t funct7)
///
/// @brief Execute a register-register operation (R-type instruction)
///
/// @param rv32iVm Pointer to the VM state
/// @param rd Destination register number
/// @param rs1 First source register number
/// @param rs2 Second source register number
/// @param funct3 The funct3 field from instruction
/// @param funct7 The funct7 field from instruction
///
/// @return 0 on success, negative on error
static inline int32_t executeRegisterOperation(
  Rv32iVm *rv32iVm, uint32_t rd, uint32_t rs1, uint32_t rs2,
  uint32_t funct3, uint32_t funct7
) {
  if (funct7 == RV32M_FUNCT7) {
    return executeMultiplyDivide(rv32iVm, rd, rs1, rs2, funct3);
  }

  switch (funct3) {
    case RV32I_FUNCT3_ADD_SUB: {
      if (funct7 == RV32I_FUNCT7_ADD) { // ADD
        rv32iVm->rv32iCoreRegisters->x[rd] = 
          rv32iVm->rv32iCoreRegisters->x[rs1] + 
          rv32iVm->rv32iCoreRegisters->x[rs2];
      } else if (funct7 == RV32I_FUNCT7_SUB) { // SUB
        rv32iVm->rv32iCoreRegisters->x[rd] = 
          rv32iVm->rv32iCoreRegisters->x[rs1] - 
          rv32iVm->rv32iCoreRegisters->x[rs2];
      } else {
        return -1; // Invalid funct7
      }
      break;
    }
    case RV32I_FUNCT3_SLL: { // SLL (Shift Left Logical)
      if (funct7 != RV32I_FUNCT7_ADD) { // Uses ADD's funct7 encoding
        return -1;
      }
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        rv32iVm->rv32iCoreRegisters->x[rs1] << 
        (rv32iVm->rv32iCoreRegisters->x[rs2] & 0x1F);
      break;
    }
    case RV32I_FUNCT3_SLT: { // SLT (Set Less Than)
      if (funct7 != RV32I_FUNCT7_ADD) {
        return -1;
      }
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        ((int32_t) rv32iVm->rv32iCoreRegisters->x[rs1]) < 
        ((int32_t) rv32iVm->rv32iCoreRegisters->x[rs2]) ? 1 : 0;
      break;
    }
    case RV32I_FUNCT3_SLTU: { // SLTU (Set Less Than Unsigned)
      if (funct7 != RV32I_FUNCT7_ADD) {
        return -1;
      }
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        rv32iVm->rv32iCoreRegisters->x[rs1] < 
        rv32iVm->rv32iCoreRegisters->x[rs2] ? 1 : 0;
      break;
    }
    case RV32I_FUNCT3_XOR: { // XOR
      if (funct7 != RV32I_FUNCT7_ADD) {
        return -1;
      }
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        rv32iVm->rv32iCoreRegisters->x[rs1] ^ 
        rv32iVm->rv32iCoreRegisters->x[rs2];
      break;
    }
    case RV32I_FUNCT3_SRL_SRA: {
      if (funct7 == RV32I_FUNCT7_SRL) { // SRL (Shift Right Logical)
        rv32iVm->rv32iCoreRegisters->x[rd] = 
          rv32iVm->rv32iCoreRegisters->x[rs1] >> 
          (rv32iVm->rv32iCoreRegisters->x[rs2] & 0x1F);
      } else if (funct7 == RV32I_FUNCT7_SRA) { // SRA (Shift Right Arithmetic)
        rv32iVm->rv32iCoreRegisters->x[rd] = 
          ((int32_t) rv32iVm->rv32iCoreRegisters->x[rs1]) >> 
          (rv32iVm->rv32iCoreRegisters->x[rs2] & 0x1F);
      } else {
        return -1; // Invalid funct7
      }
      break;
    }
    case RV32I_FUNCT3_OR: { // OR
      if (funct7 != RV32I_FUNCT7_ADD) {
        return -1;
      }
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        rv32iVm->rv32iCoreRegisters->x[rs1] | 
        rv32iVm->rv32iCoreRegisters->x[rs2];
      break;
    }
    case RV32I_FUNCT3_AND: { // AND
      if (funct7 != RV32I_FUNCT7_ADD) {
        return -1;
      }
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        rv32iVm->rv32iCoreRegisters->x[rs1] & 
        rv32iVm->rv32iCoreRegisters->x[rs2];
      break;
    }
    default: {
      return -1; // Invalid funct3
    }
  }
  return 0;
}

/// @fn static inline int32_t executeImmediateOperation(
///   Rv32iVm *rv32iVm, uint32_t rd, uint32_t rs1, int32_t immediate,
///   uint32_t funct3)
///
/// @brief Execute an immediate operation (I-type instruction)
///
/// @param rv32iVm Pointer to the VM state
/// @param rd Destination register number
/// @param rs1 Source register number
/// @param immediate Sign-extended 12-bit immediate value
/// @param funct3 The funct3 field from instruction
///
/// @return 0 on success, negative on error
static inline int32_t executeImmediateOperation(
  Rv32iVm *rv32iVm, uint32_t rd, uint32_t rs1, int32_t immediate,
  uint32_t funct3
) {
  switch (funct3) {
    case RV32I_FUNCT3_ADDI: {
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        rv32iVm->rv32iCoreRegisters->x[rs1] + immediate;
      break;
    }
    case RV32I_FUNCT3_SLLI: {
      uint32_t shamt = immediate & 0x1F; // Shift amount is bottom 5 bits
      if ((immediate & 0xFE0) != 0) { // Top bits must be zero
        return -1;
      }
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        rv32iVm->rv32iCoreRegisters->x[rs1] << shamt;
      break;
    }
    case RV32I_FUNCT3_SLTI: {
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        ((int32_t) rv32iVm->rv32iCoreRegisters->x[rs1]) < immediate ? 1 : 0;
      break;
    }
    case RV32I_FUNCT3_SLTIU: {
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        rv32iVm->rv32iCoreRegisters->x[rs1] < 
        (uint32_t) immediate ? 1 : 0;
      break;
    }
    case RV32I_FUNCT3_XORI: {
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        rv32iVm->rv32iCoreRegisters->x[rs1] ^ immediate;
      break;
    }
    case RV32I_FUNCT3_SRLI_SRAI: {
      uint32_t shamt = immediate & 0x1F; // Shift amount is bottom 5 bits
      uint32_t funct7 = (immediate >> 5) & 0x7F;
      
      if (funct7 == RV32I_FUNCT7_SRLI) {
        rv32iVm->rv32iCoreRegisters->x[rd] = 
          rv32iVm->rv32iCoreRegisters->x[rs1] >> shamt;
      } else if (funct7 == RV32I_FUNCT7_SRAI) {
        rv32iVm->rv32iCoreRegisters->x[rd] = 
          ((int32_t) rv32iVm->rv32iCoreRegisters->x[rs1]) >> shamt;
      } else {
        return -1; // Invalid funct7
      }
      break;
    }
    case RV32I_FUNCT3_ORI: {
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        rv32iVm->rv32iCoreRegisters->x[rs1] | immediate;
      break;
    }
    case RV32I_FUNCT3_ANDI: {
      rv32iVm->rv32iCoreRegisters->x[rd] = 
        rv32iVm->rv32iCoreRegisters->x[rs1] & immediate;
      break;
    }
    default: {
      return -1; // Invalid funct3
    }
  }
  return 0;
}

/// @fn static inline int32_t executeLoad(
///   Rv32iVm *rv32iVm, uint32_t rd, uint32_t rs1, int32_t immediate,
///   uint32_t funct3)
///
/// @brief Execute a load operation from memory
///
/// @param rv32iVm Pointer to the VM state
/// @param rd Destination register number
/// @param rs1 Source register number (base address)
/// @param immediate Sign-extended 12-bit immediate offset
/// @param funct3 The funct3 field from instruction
///
/// @return 0 on success, negative on error
static inline int32_t executeLoad(
  Rv32iVm *rv32iVm, uint32_t rd, uint32_t rs1, int32_t immediate,
  uint32_t funct3
) {
  // Calculate effective address
  uint32_t address = rv32iVm->rv32iCoreRegisters->x[rs1] + immediate;
  
  // Read based on the load type
  switch (funct3) {
    case RV32I_FUNCT3_LB: {
      // Load byte and sign extend
      uint8_t byteValue = 0;
      int32_t result = rv32iMemoryRead8(rv32iVm, address, &byteValue);
      if (result != 0) {
        return result;
      }
      // Sign extend the byte
      rv32iVm->rv32iCoreRegisters->x[rd] = ((int8_t) byteValue);
      break;
    }
    case RV32I_FUNCT3_LH: {
      // Load halfword and sign extend
      uint16_t halfwordValue = 0;
      int32_t result = rv32iMemoryRead16(rv32iVm, address, &halfwordValue);
      if (result != 0) {
        return result;
      }
      // Sign extend the halfword
      rv32iVm->rv32iCoreRegisters->x[rd] = ((int16_t) halfwordValue);
      break;
    }
    case RV32I_FUNCT3_LW: {
      // Load word (no sign extension needed)
      uint32_t wordValue = 0;
      int32_t result = rv32iMemoryRead32(rv32iVm, address, &wordValue);
      if (result != 0) {
        return result;
      }
      rv32iVm->rv32iCoreRegisters->x[rd] = wordValue;
      break;
    }
    case RV32I_FUNCT3_LBU: {
      // Load byte unsigned (zero extend)
      //// printDebug("x15 = ");
      //// printDebug(rv32iVm->rv32iCoreRegisters->x[15], HEX);
      //// printDebug("\n");
      uint8_t byteValue = 0;
      int32_t result = rv32iMemoryRead8(rv32iVm, address, &byteValue);
      if (result != 0) {
        return result;
      }
      rv32iVm->rv32iCoreRegisters->x[rd] = byteValue;
      break;
    }
    case RV32I_FUNCT3_LHU: {
      // Load halfword unsigned (zero extend)
      uint16_t halfwordValue = 0;
      int32_t result = rv32iMemoryRead16(rv32iVm, address, &halfwordValue);
      if (result != 0) {
        return result;
      }
      rv32iVm->rv32iCoreRegisters->x[rd] = halfwordValue;
      break;
    }
    default: {
      return -1; // Invalid funct3
    }
  }
  
  return 0;
}

/// @fn static inline int32_t executeStore(
///   Rv32iVm *rv32iVm, uint32_t rs1, uint32_t rs2, int32_t immediate,
///   uint32_t funct3)
///
/// @brief Execute a store operation to memory
///
/// @param rv32iVm Pointer to the VM state
/// @param rs1 Source register number (base address)
/// @param rs2 Source register number (value to store)
/// @param immediate Sign-extended 12-bit immediate offset
/// @param funct3 The funct3 field from instruction
///
/// @return 0 on success, negative on error
static inline int32_t executeStore(
  Rv32iVm *rv32iVm, uint32_t rs1, uint32_t rs2, int32_t immediate,
  uint32_t funct3
) {
  // Calculate effective address
  uint32_t address = rv32iVm->rv32iCoreRegisters->x[rs1] + immediate;
  
  // Get value to store from rs2
  uint32_t value = rv32iVm->rv32iCoreRegisters->x[rs2];
  
  // Perform store based on width
  switch (funct3) {
    case RV32I_FUNCT3_SB: {
      // Store byte (lowest 8 bits)
      return rv32iMemoryWrite8(rv32iVm, address, value & 0xFF);
    }
    case RV32I_FUNCT3_SH: {
      // Store halfword (lowest 16 bits)
      return rv32iMemoryWrite16(rv32iVm, address, value & 0xFFFF);
    }
    case RV32I_FUNCT3_SW: {
      // Store word (all 32 bits)
      return rv32iMemoryWrite32(rv32iVm, address, value);
    }
    default: {
      return -1; // Invalid funct3
    }
  }
}

/// @fn static inline int32_t executeBranch(
///   Rv32iVm *rv32iVm, uint32_t rs1, uint32_t rs2, int32_t immediate,
///   uint32_t funct3, uint32_t *nextPc)
///
/// @brief Execute a branch operation
///
/// @param rv32iVm Pointer to the VM state
/// @param rs1 First source register number
/// @param rs2 Second source register number
/// @param immediate Sign-extended branch offset
/// @param funct3 The funct3 field from instruction
/// @param nextPc Pointer to next program counter value to update on branch
///
/// @return 0 on success, negative on error
static inline int32_t executeBranch(
  Rv32iVm *rv32iVm, uint32_t rs1, uint32_t rs2, int32_t immediate,
  uint32_t funct3, uint32_t *nextPc
) {
  bool takeBranch = false;
  
  switch (funct3) {
    case RV32I_FUNCT3_BEQ: {
      // Branch if equal
      takeBranch = rv32iVm->rv32iCoreRegisters->x[rs1] == 
        rv32iVm->rv32iCoreRegisters->x[rs2];
      break;
    }
    case RV32I_FUNCT3_BNE: {
      // Branch if not equal
      takeBranch = rv32iVm->rv32iCoreRegisters->x[rs1] != 
        rv32iVm->rv32iCoreRegisters->x[rs2];
      break;
    }
    case RV32I_FUNCT3_BLT: {
      // Branch if less than (signed)
      takeBranch = ((int32_t) rv32iVm->rv32iCoreRegisters->x[rs1]) < 
        ((int32_t) rv32iVm->rv32iCoreRegisters->x[rs2]);
      break;
    }
    case RV32I_FUNCT3_BGE: {
      // Branch if greater than or equal (signed)
      takeBranch = ((int32_t) rv32iVm->rv32iCoreRegisters->x[rs1]) >= 
        ((int32_t) rv32iVm->rv32iCoreRegisters->x[rs2]);
      break;
    }
    case RV32I_FUNCT3_BLTU: {
      // Branch if less than (unsigned)
      takeBranch = rv32iVm->rv32iCoreRegisters->x[rs1] < 
        rv32iVm->rv32iCoreRegisters->x[rs2];
      break;
    }
    case RV32I_FUNCT3_BGEU: {
      // Branch if greater than or equal (unsigned)
      takeBranch = rv32iVm->rv32iCoreRegisters->x[rs1] >= 
        rv32iVm->rv32iCoreRegisters->x[rs2];
      break;
    }
    default: {
      return -1; // Invalid funct3
    }
  }
  
  if (takeBranch) {
    *nextPc = rv32iVm->rv32iCoreRegisters->pc + immediate;
  }
  
  return 0;
}

/// @fn static inline int32_t executeLoadUpperImmediate(
///   Rv32iVm *rv32iVm, uint32_t rd, int32_t immediate)
///
/// @brief Execute a load upper immediate instruction
///
/// @param rv32iVm Pointer to the VM state
/// @param rd Destination register number
/// @param immediate The U-type immediate value (already shifted for upper 20 bits)
///
/// @return 0 on success, negative on error
static inline int32_t executeLoadUpperImmediate(
  Rv32iVm *rv32iVm, uint32_t rd, int32_t immediate
) {
  rv32iVm->rv32iCoreRegisters->x[rd] = immediate;
  return 0;
}

/// @fn static inline int32_t executeAddUpperImmediatePc(
///   Rv32iVm *rv32iVm, uint32_t rd, int32_t immediate)
///
/// @brief Execute an add upper immediate to PC instruction
///
/// @param rv32iVm Pointer to the VM state
/// @param rd Destination register number
/// @param immediate The U-type immediate value (already shifted for upper 20 bits)
///
/// @return 0 on success, negative on error
static inline int32_t executeAddUpperImmediatePc(
  Rv32iVm *rv32iVm, uint32_t rd, int32_t immediate
) {
  rv32iVm->rv32iCoreRegisters->x[rd] = 
    rv32iVm->rv32iCoreRegisters->pc + immediate;
  return 0;
}

/// @fn static inline int32_t executeJumpAndLink(
///   Rv32iVm *rv32iVm, uint32_t rd, int32_t immediate, uint32_t *nextPc)
///
/// @brief Execute a jump and link instruction
///
/// @param rv32iVm Pointer to the VM state
/// @param rd Destination register number
/// @param immediate The J-type immediate value (already sign-extended)
/// @param nextPc Pointer to next program counter value to update
///
/// @return 0 on success, negative on error
static inline int32_t executeJumpAndLink(
  Rv32iVm *rv32iVm, uint32_t rd, int32_t immediate, uint32_t *nextPc
) {
  // Store return address (pc + 4) in rd
  rv32iVm->rv32iCoreRegisters->x[rd] = rv32iVm->rv32iCoreRegisters->pc + 4;
  
  // Calculate target address
  *nextPc = rv32iVm->rv32iCoreRegisters->pc + immediate;
  
  return 0;
}

/// @fn static inline int32_t executeJumpAndLinkRegister(
///   Rv32iVm *rv32iVm, uint32_t rd, uint32_t rs1, int32_t immediate,
///   uint32_t *nextPc)
///
/// @brief Execute a jump and link register instruction
///
/// @param rv32iVm Pointer to the VM state
/// @param rd Destination register number
/// @param rs1 Source register number containing base address
/// @param immediate The I-type immediate offset value
/// @param nextPc Pointer to next program counter value to update
///
/// @return 0 on success, negative on error
static inline int32_t executeJumpAndLinkRegister(
  Rv32iVm *rv32iVm, uint32_t rd, uint32_t rs1, int32_t immediate,
  uint32_t *nextPc
) {
  // Save the return address before calculating target
  rv32iVm->rv32iCoreRegisters->x[rd] = rv32iVm->rv32iCoreRegisters->pc + 4;
  
  //// if (rv32iVm->rv32iCoreRegisters->pc == 0x103C) {
  ////   printDebug("rd ");
  ////   printDebug(rd);
  ////   printDebug(", base address ");
  ////   printDebug(rv32iVm->rv32iCoreRegisters->x[rs1], HEX);
  ////   printDebug(", rs1 ");
  ////   printDebug(rs1);
  ////   printDebug(", immediate ");
  ////   printDebug(immediate, HEX);
  ////   printDebug("\n");
  //// }

  // Calculate target address: (rs1 + immediate) & ~1
  // The & ~1 clears the least significant bit as per RISC-V spec
  *nextPc = (rv32iVm->rv32iCoreRegisters->x[rs1] + immediate) & ~1;
  
  return 0;
}

/// @fn static inline int32_t executeSystem(
///   Rv32iVm *rv32iVm, uint32_t rd, uint32_t rs1, int32_t immediate,
///   uint32_t funct3)
///
/// @brief Execute a system instruction (CSR access, ECALL, or EBREAK)
///
/// @param rv32iVm Pointer to the VM state
/// @param rd Destination register number
/// @param rs1 Source register number for CSR operations
/// @param immediate Contains CSR number or immediate value
/// @param funct3 The funct3 field from instruction
///
/// @return 0 on success, negative on error, positive for ECALL/EBREAK
static inline int32_t executeSystem(
  Rv32iVm *rv32iVm, uint32_t rd, uint32_t rs1, int32_t immediate,
  uint32_t funct3
) {
  // Handle ECALL/EBREAK first (funct3 == 0)
  if (funct3 == RV32I_FUNCT3_ECALL_EBREAK) {
    if (immediate == RV32I_IMM12_ECALL) {
      return nanoOsSystemCallHandle(rv32iVm);
    } else if (immediate == RV32I_IMM12_EBREAK) {
      // EBREAK
      // This is intended to drop into a debugger, which we don't support.
      // We will just ignore this if we get it, so return 0 and move on.
      return 0;
    } else {
      return -1; // Invalid immediate for ECALL/EBREAK
    }
  }
  
  // Extract CSR number from immediate field
  uint32_t csrNumber = (uint32_t) immediate & 0xFFF;
  
  // Read current CSR value based on number
  uint32_t oldCsrValue = 0;
  switch (csrNumber) {
    case RV32I_CSR_MSTATUS:
      oldCsrValue = rv32iVm->rv32iCoreRegisters->mstatus;
      break;
    case RV32I_CSR_MISA:
      oldCsrValue = rv32iVm->rv32iCoreRegisters->misa;
      break;
    case RV32I_CSR_MIE:
      oldCsrValue = rv32iVm->rv32iCoreRegisters->mie;
      break;
    case RV32I_CSR_MTVEC:
      oldCsrValue = rv32iVm->rv32iCoreRegisters->mtvec;
      break;
    case RV32I_CSR_MSCRATCH:
      oldCsrValue = rv32iVm->rv32iCoreRegisters->mscratch;
      break;
    case RV32I_CSR_MEPC:
      oldCsrValue = rv32iVm->rv32iCoreRegisters->mepc;
      break;
    case RV32I_CSR_MCAUSE:
      oldCsrValue = rv32iVm->rv32iCoreRegisters->mcause;
      break;
    case RV32I_CSR_MTVAL:
      oldCsrValue = rv32iVm->rv32iCoreRegisters->mtval;
      break;
    case RV32I_CSR_MIP:
      oldCsrValue = rv32iVm->rv32iCoreRegisters->mip;
      break;
    case RV32I_CSR_MVENDORID:
    case RV32I_CSR_MARCHID:
    case RV32I_CSR_MIMPID:
    case RV32I_CSR_MHARTID:
      // Read-only CSRs return 0 in our implementation
      oldCsrValue = 0;
      break;
    default:
      return -1; // Unsupported CSR
  }
  
  // Always capture old value in rd (unless rd is x0)
  rv32iVm->rv32iCoreRegisters->x[rd] = oldCsrValue;
  
  // Calculate new CSR value based on instruction type
  uint32_t newCsrValue = oldCsrValue;
  uint32_t writeValue;
  
  switch (funct3) {
    case RV32I_FUNCT3_CSRRW: // CSR Read/Write
      writeValue = rv32iVm->rv32iCoreRegisters->x[rs1];
      newCsrValue = writeValue;
      break;
      
    case RV32I_FUNCT3_CSRRS: // CSR Read and Set Bits
      writeValue = rv32iVm->rv32iCoreRegisters->x[rs1];
      if (rs1 != 0) {
        newCsrValue = oldCsrValue | writeValue;
      }
      break;
      
    case RV32I_FUNCT3_CSRRC: // CSR Read and Clear Bits
      writeValue = rv32iVm->rv32iCoreRegisters->x[rs1];
      if (rs1 != 0) {
        newCsrValue = oldCsrValue & ~writeValue;
      }
      break;
      
    case RV32I_FUNCT3_CSRRWI: // CSR Read/Write Immediate
      writeValue = rs1; // rs1 field holds the immediate value
      newCsrValue = writeValue;
      break;
      
    case RV32I_FUNCT3_CSRRSI: // CSR Read and Set Bits Immediate
      writeValue = rs1; // rs1 field holds the immediate value
      if (writeValue != 0) {
        newCsrValue = oldCsrValue | writeValue;
      }
      break;
      
    case RV32I_FUNCT3_CSRRCI: // CSR Read and Clear Bits Immediate
      writeValue = rs1; // rs1 field holds the immediate value
      if (writeValue != 0) {
        newCsrValue = oldCsrValue & ~writeValue;
      }
      break;
      
    default:
      return -1;
  }
  
  // Update CSR if value changed and not a read-only CSR
  if (newCsrValue != oldCsrValue) {
    switch (csrNumber) {
      case RV32I_CSR_MSTATUS:
        rv32iVm->rv32iCoreRegisters->mstatus = newCsrValue;
        break;
      case RV32I_CSR_MISA:
        // MISA is read-only in our implementation
        break;
      case RV32I_CSR_MIE:
        rv32iVm->rv32iCoreRegisters->mie = newCsrValue;
        break;
      case RV32I_CSR_MTVEC:
        rv32iVm->rv32iCoreRegisters->mtvec = newCsrValue;
        break;
      case RV32I_CSR_MSCRATCH:
        rv32iVm->rv32iCoreRegisters->mscratch = newCsrValue;
        break;
      case RV32I_CSR_MEPC:
        rv32iVm->rv32iCoreRegisters->mepc = newCsrValue;
        break;
      case RV32I_CSR_MCAUSE:
        rv32iVm->rv32iCoreRegisters->mcause = newCsrValue;
        break;
      case RV32I_CSR_MTVAL:
        rv32iVm->rv32iCoreRegisters->mtval = newCsrValue;
        break;
      case RV32I_CSR_MIP:
        rv32iVm->rv32iCoreRegisters->mip = newCsrValue;
        break;
    }
  }
  
  return 0;
}

/// @fn int32_t executeInstruction(Rv32iVm *rv32iVm, uint32_t instruction)
///
/// @brief Execute a single RV32I instruction
///
/// @param rv32iVm Pointer to the VM state
/// @param instruction The instruction to execute
///
/// @return 0 on success, negative on error, positive for program exit
int32_t executeInstruction(Rv32iVm *rv32iVm, uint32_t instruction) {
  // Decode instruction fields
  uint32_t opcode = instruction & 0x7F;
  uint32_t rd = (instruction >> 7) & 0x1F;
  uint32_t funct3 = (instruction >> 12) & 0x7;
  uint32_t rs1 = (instruction >> 15) & 0x1F;
  uint32_t rs2 = (instruction >> 20) & 0x1F;
  uint32_t funct7 = (instruction >> 25) & 0x7F;
  
  // R-type immediate
  int32_t immI = ((int32_t) instruction) >> 20;
  if (instruction & 0x80000000) {
    // Sign-extend the value.
    immI |= 0xfffff000;
  }
  
  // S-type immediate
  int32_t immS = (
    // imm[11:5] from instruction[31:25]
    // No need to bitmask since we want all of the high-order bits and we want
    // the sign extension that comes along with the right shift.
    ((int32_t)(instruction >> 20))
    // imm[4:0] from instruction[11:7]
    | ((instruction >> 7) & 0x1F)
  );
  if (instruction & 0x80000000) {
    // Sign-extend the value.
    immS |= 0xfffff000;
  }
  
  // B-type immediate
  int32_t immB = (
    // Sign extension from bit 12 (bit 31 of instruction)
    ((int32_t)(instruction & 0x80000000) >> 19) |
    // imm[11] (bit 7 of instruction)
    ((instruction & 0x00000080) << 4) | 
    // imm[10:5] (bits 30:25 of instruction)
    ((instruction >> 20) & 0x7E0) |
    // imm[4:1] (bits 11:8 of instruction)
    ((instruction >> 7) & 0x1E)
  );
  if (instruction & 0x80000000) {
    // Sign-extend the value.
    immB |= 0xffffe000;
  }
    
  // U-type immediate
  int32_t immU = (int32_t) instruction & 0xFFFFF000;
  
  // J-type immediate
  int32_t immJ = (
    ((instruction & 0x80000000) >> 11) |  // Sign bit -> 20
    ((instruction & 0x7FE00000) >> 20) |  // imm[10:1] -> imm[10:1]
    ((instruction & 0x00100000) >> 9) |   // imm[11] -> imm[11]
    (instruction & 0x000FF000)            // imm[19:12] -> imm[19:12]
  );
  
  // Sign extend if negative
  if (immJ & 0x00100000) {
    immJ |= 0xFFE00000;
  }

  // Always increment PC by default
  uint32_t nextPc = rv32iVm->rv32iCoreRegisters->pc + RV32I_INSTRUCTION_SIZE;
  
  int32_t result = 0;

  switch (opcode) {
    case RV32I_OP: {
      result = executeRegisterOperation(
        rv32iVm, rd, rs1, rs2, funct3, funct7);
      break;
    }
    
    case RV32I_OP_IMM: {
      result = executeImmediateOperation(
        rv32iVm, rd, rs1, immI, funct3);
      break;
    }

    case RV32I_LOAD: {
      result = executeLoad(
        rv32iVm, rd, rs1, immI, funct3);
      break;
    }

    case RV32I_STORE: {
      result = executeStore(
        rv32iVm, rs1, rs2, immS, funct3);
      break;
    }

    case RV32I_BRANCH: {
      result = executeBranch(
        rv32iVm, rs1, rs2, immB, funct3, &nextPc);
      break;
    }

    case RV32I_LUI: {
      result = executeLoadUpperImmediate(
        rv32iVm, rd, immU);
      break;
    }

    case RV32I_AUIPC: {
      result = executeAddUpperImmediatePc(
        rv32iVm, rd, immU);
      break;
    }

    case RV32I_JAL: {
      result = executeJumpAndLink(
        rv32iVm, rd, immJ, &nextPc);
      break;
    }

    case RV32I_JALR: {
      result = executeJumpAndLinkRegister(
        rv32iVm, rd, rs1, immI, &nextPc);
      break;
    }

    case RV32I_SYSTEM: {
      result = executeSystem(
        rv32iVm, rd, rs1, immI, funct3);
      break;
    }

    case RV32I_MISC_MEM: {
      // Fence is a no-op in our implementation
      break;
    }

    default: {
      // Invalid opcode
      return -1;
    }
  }

  if (result != 0) {
    return result;
  }

  //// if (nextPc == 0x1658) {
  ////   printDebug("Instruction 0x");
  ////   printDebug(instruction, HEX);
  ////   printDebug(" at address ");
  ////   printDebug(rv32iVm->rv32iCoreRegisters->pc, HEX);
  ////   printDebug(" yielded nexPc of 0x1658\n");
  //// }

  // Update PC to next instruction
  rv32iVm->rv32iCoreRegisters->pc = nextPc;
  return 0;
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
  //// printDebug("sizeof(Rv32iVm) = ");
  //// printDebug(sizeof(Rv32iVm));
  //// printDebug("\n");
  //// size_t freeMemory = getFreeMemory();
  //// printDebug(freeMemory);
  //// printDebug(" bytes free before rv32iVmInit\n");
  if (rv32iVmInit(&rv32iVm, argv[0]) != 0) {
    rv32iVmCleanup(&rv32iVm);
    printString("rv32iVmInit failed\n");
    return -1;
  }
  //// printDebug(getFreeMemory());
  //// printDebug(" bytes free after rv32iVmInit\n");
  //// printDebug("rv32iVmInit consumed ");
  //// printDebug(freeMemory - getFreeMemory() - 516);
  //// printDebug(" bytes\n");

  rv32iVm.rv32iCoreRegisters->pc = RV32I_PROGRAM_START;
  rv32iVm.rv32iCoreRegisters->x[2] = RV32I_STACK_START;

  int returnValue = 0;
  uint32_t instruction = 0;
  //// uint32_t startTime = 0;
  //// uint32_t runTime = 0;
  //// uint32_t instructionCount = 0;
  //// startTime = getElapsedMilliseconds(0);
  //// printDebug("Starting ");
  //// printDebug(argv[0]);
  //// printDebug("\n");
  while ((rv32iVm.running == true) && (returnValue == 0)) {
    if (fetchInstruction(&rv32iVm, &instruction) != 0) {
      //// printDebug("Fetching instruction at address 0x");
      //// printDebug(rv32iVm.rv32iCoreRegisters->pc, HEX);
      //// printDebug(" failed\n");
      returnValue = -1;
      break;
    }

    rv32iVm.rv32iCoreRegisters->x[0] = 0;
    //// printDebug("Running instruction 0x");
    //// printDebug(instruction, HEX);
    //// printDebug(" at address 0x");
    //// printDebug(rv32iVm.rv32iCoreRegisters->pc, HEX);
    //// printDebug("\n");
    returnValue = executeInstruction(&rv32iVm, instruction);
    if (returnValue != 0) {
      printDebug("Instruction 0x");
      printDebug(instruction, HEX);
      printDebug(" at address 0x");
      printDebug(rv32iVm.rv32iCoreRegisters->pc, HEX);
      printDebug(" failed\n");
    }
    //// instructionCount++;
  }
  //// runTime = getElapsedMilliseconds(startTime);
  //// printDebug(instructionCount);
  //// printDebug("/");
  //// printDebug(runTime);
  //// printDebug("\n");
  //// printDebug("Runtime: ");
  //// printDebug(runTime);
  //// printDebug(" milliseconds\n");
  //// printDebug("instructionCount = ");
  //// printDebug(instructionCount);
  //// printDebug("\n");

  if (rv32iVm.running == false) {
    // VM exited gracefully.  Pull the status the process exited with.
    returnValue = rv32iVm.exitCode;
  }
  //// printDebug("Exiting with status ");
  //// printDebug(returnValue);
  //// printDebug("\n");

  rv32iVmCleanup(&rv32iVm);
  //// printDebug(getFreeMemory());
  //// printDebug(" bytes free\n");
  return returnValue;
}

