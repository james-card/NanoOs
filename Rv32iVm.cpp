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
  if (virtualMemoryZero(&rv32iVm->physicalMemory, 0, RV32I_MEMORY_SIZE)
    != RV32I_MEMORY_SIZE
  ) {
    virtualMemoryCleanup(&programBinary, false);
    return -1;
  }

  if (virtualMemoryCopy(&programBinary, 0,
    &rv32iVm->physicalMemory, RV32I_PROGRAM_START,
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
  if (address & RV32I_CLINT_BASE_ADDR) {
    // Mapped memory access - mask off the high bits
    return virtualMemoryRead32(
      &rv32iVm->mappedMemory,
      address & RV32I_CLINT_ADDR_MASK,
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
  if (address & RV32I_CLINT_BASE_ADDR) {
    // Mapped memory access - mask off the high bits
    return virtualMemoryWrite32(
      &rv32iVm->mappedMemory,
      address & RV32I_CLINT_ADDR_MASK,
      value
    );
  } else {
    // Physical memory access
    return virtualMemoryWrite32(&rv32iVm->physicalMemory, address, value);
  }
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
  return
    rv32iMemoryRead32(rv32iVm, rv32iVm->rv32iCoreRegisters.pc, instruction);
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
  switch (funct3) {
    case RV32I_FUNCT3_ADD_SUB: {
      if (funct7 == RV32I_FUNCT7_ADD) { // ADD
        rv32iVm->rv32iCoreRegisters.x[rd] = 
          rv32iVm->rv32iCoreRegisters.x[rs1] + 
          rv32iVm->rv32iCoreRegisters.x[rs2];
      } else if (funct7 == RV32I_FUNCT7_SUB) { // SUB
        rv32iVm->rv32iCoreRegisters.x[rd] = 
          rv32iVm->rv32iCoreRegisters.x[rs1] - 
          rv32iVm->rv32iCoreRegisters.x[rs2];
      } else {
        return -1; // Invalid funct7
      }
      break;
    }
    case RV32I_FUNCT3_SLL: { // SLL (Shift Left Logical)
      if (funct7 != RV32I_FUNCT7_ADD) { // Uses ADD's funct7 encoding
        return -1;
      }
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        rv32iVm->rv32iCoreRegisters.x[rs1] << 
        (rv32iVm->rv32iCoreRegisters.x[rs2] & 0x1F);
      break;
    }
    case RV32I_FUNCT3_SLT: { // SLT (Set Less Than)
      if (funct7 != RV32I_FUNCT7_ADD) {
        return -1;
      }
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        ((int32_t) rv32iVm->rv32iCoreRegisters.x[rs1]) < 
        ((int32_t) rv32iVm->rv32iCoreRegisters.x[rs2]) ? 1 : 0;
      break;
    }
    case RV32I_FUNCT3_SLTU: { // SLTU (Set Less Than Unsigned)
      if (funct7 != RV32I_FUNCT7_ADD) {
        return -1;
      }
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        rv32iVm->rv32iCoreRegisters.x[rs1] < 
        rv32iVm->rv32iCoreRegisters.x[rs2] ? 1 : 0;
      break;
    }
    case RV32I_FUNCT3_XOR: { // XOR
      if (funct7 != RV32I_FUNCT7_ADD) {
        return -1;
      }
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        rv32iVm->rv32iCoreRegisters.x[rs1] ^ 
        rv32iVm->rv32iCoreRegisters.x[rs2];
      break;
    }
    case RV32I_FUNCT3_SRL_SRA: {
      if (funct7 == RV32I_FUNCT7_SRL) { // SRL (Shift Right Logical)
        rv32iVm->rv32iCoreRegisters.x[rd] = 
          rv32iVm->rv32iCoreRegisters.x[rs1] >> 
          (rv32iVm->rv32iCoreRegisters.x[rs2] & 0x1F);
      } else if (funct7 == RV32I_FUNCT7_SRA) { // SRA (Shift Right Arithmetic)
        rv32iVm->rv32iCoreRegisters.x[rd] = 
          ((int32_t) rv32iVm->rv32iCoreRegisters.x[rs1]) >> 
          (rv32iVm->rv32iCoreRegisters.x[rs2] & 0x1F);
      } else {
        return -1; // Invalid funct7
      }
      break;
    }
    case RV32I_FUNCT3_OR: { // OR
      if (funct7 != RV32I_FUNCT7_ADD) {
        return -1;
      }
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        rv32iVm->rv32iCoreRegisters.x[rs1] | 
        rv32iVm->rv32iCoreRegisters.x[rs2];
      break;
    }
    case RV32I_FUNCT3_AND: { // AND
      if (funct7 != RV32I_FUNCT7_ADD) {
        return -1;
      }
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        rv32iVm->rv32iCoreRegisters.x[rs1] & 
        rv32iVm->rv32iCoreRegisters.x[rs2];
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
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        rv32iVm->rv32iCoreRegisters.x[rs1] + immediate;
      break;
    }
    case RV32I_FUNCT3_SLLI: {
      uint32_t shamt = immediate & 0x1F; // Shift amount is bottom 5 bits
      if ((immediate & 0xFE0) != 0) { // Top bits must be zero
        return -1;
      }
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        rv32iVm->rv32iCoreRegisters.x[rs1] << shamt;
      break;
    }
    case RV32I_FUNCT3_SLTI: {
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        ((int32_t) rv32iVm->rv32iCoreRegisters.x[rs1]) < immediate ? 1 : 0;
      break;
    }
    case RV32I_FUNCT3_SLTIU: {
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        rv32iVm->rv32iCoreRegisters.x[rs1] < 
        (uint32_t) immediate ? 1 : 0;
      break;
    }
    case RV32I_FUNCT3_XORI: {
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        rv32iVm->rv32iCoreRegisters.x[rs1] ^ immediate;
      break;
    }
    case RV32I_FUNCT3_SRLI_SRAI: {
      uint32_t shamt = immediate & 0x1F; // Shift amount is bottom 5 bits
      uint32_t funct7 = (immediate >> 5) & 0x7F;
      
      if (funct7 == RV32I_FUNCT7_SRLI) {
        rv32iVm->rv32iCoreRegisters.x[rd] = 
          rv32iVm->rv32iCoreRegisters.x[rs1] >> shamt;
      } else if (funct7 == RV32I_FUNCT7_SRAI) {
        rv32iVm->rv32iCoreRegisters.x[rd] = 
          ((int32_t) rv32iVm->rv32iCoreRegisters.x[rs1]) >> shamt;
      } else {
        return -1; // Invalid funct7
      }
      break;
    }
    case RV32I_FUNCT3_ORI: {
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        rv32iVm->rv32iCoreRegisters.x[rs1] | immediate;
      break;
    }
    case RV32I_FUNCT3_ANDI: {
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        rv32iVm->rv32iCoreRegisters.x[rs1] & immediate;
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
  uint32_t address = rv32iVm->rv32iCoreRegisters.x[rs1] + immediate;
  
  // Read full word from memory
  uint32_t value = 0;
  int32_t result = rv32iMemoryRead32(rv32iVm, address, &value);
  if (result != 0) {
    return result;
  }
  
  // Extract and sign extend based on load type
  switch (funct3) {
    case RV32I_FUNCT3_LB: {
      // Load byte and sign extend
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        ((int32_t) (value & 0xFF) << 24) >> 24;
      break;
    }
    case RV32I_FUNCT3_LH: {
      // Load halfword and sign extend
      rv32iVm->rv32iCoreRegisters.x[rd] = 
        ((int32_t) (value & 0xFFFF) << 16) >> 16;
      break;
    }
    case RV32I_FUNCT3_LW: {
      // Load word (no sign extension needed)
      rv32iVm->rv32iCoreRegisters.x[rd] = value;
      break;
    }
    case RV32I_FUNCT3_LBU: {
      // Load byte unsigned (zero extend)
      rv32iVm->rv32iCoreRegisters.x[rd] = value & 0xFF;
      break;
    }
    case RV32I_FUNCT3_LHU: {
      // Load halfword unsigned (zero extend)
      rv32iVm->rv32iCoreRegisters.x[rd] = value & 0xFFFF;
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
  uint32_t address = rv32iVm->rv32iCoreRegisters.x[rs1] + immediate;
  
  // Get value to store from rs2
  uint32_t value = rv32iVm->rv32iCoreRegisters.x[rs2];
  
  // Perform store based on width
  switch (funct3) {
    case RV32I_FUNCT3_SB: {
      // Store byte (lowest 8 bits)
      return rv32iMemoryWrite32(rv32iVm, address, value & 0xFF);
    }
    case RV32I_FUNCT3_SH: {
      // Store halfword (lowest 16 bits)
      return rv32iMemoryWrite32(rv32iVm, address, value & 0xFFFF);
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
  
  // S-type immediate
  int32_t immS = (((int32_t) instruction) >> 20 & ~0x1F) | 
    ((instruction >> 7) & 0x1F);
    
  // B-type immediate
  int32_t immB = (((int32_t) instruction >> 19) & ~0x1FFF) |
    ((instruction & 0x80) << 4) | // imm[11]
    ((instruction >> 20) & 0x7E0) | // imm[10:5]
    ((instruction >> 7) & 0x1E); // imm[4:1]
    
  // U-type immediate
  int32_t immU = (int32_t) instruction & 0xFFFFF000;
  
  // J-type immediate
  int32_t immJ = (((int32_t) instruction >> 11) & ~0x1FFFFF) |
    (instruction & 0xFF000) | // imm[19:12]
    ((instruction & 0x100000) >> 9) | // imm[11]
    ((instruction & 0x7FE00000) >> 20); // imm[10:1]

  // Always increment PC by default
  uint32_t nextPc = rv32iVm->rv32iCoreRegisters.pc + RV32I_INSTRUCTION_SIZE;
  
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

    case RV32I_FENCE: {
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

  // Update PC to next instruction
  rv32iVm->rv32iCoreRegisters.pc = nextPc;
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
  if (rv32iVmInit(&rv32iVm, argv[0]) != 0) {
    rv32iVmCleanup(&rv32iVm);
  }

  // VM logic goes here
  rv32iVm.rv32iCoreRegisters.pc = RV32I_PROGRAM_START;
  rv32iVm.rv32iCoreRegisters.x[0] = 0;
  rv32iVm.rv32iCoreRegisters.x[2] = RV32I_STACK_START;

  int returnValue = 0;
  uint32_t instruction = 0;
  while (returnValue = 0) {
    if (fetchInstruction(&rv32iVm, &instruction) != 0) {
      returnValue = -1;
      break;
    }

    returnValue = executeInstruction(&rv32iVm, instruction);
  }

  rv32iVmCleanup(&rv32iVm);
  return returnValue;
}

