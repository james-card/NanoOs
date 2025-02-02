///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              01.31.2025
///
/// @file              Rv32iVm.h
///
/// @brief             Infrastructure for running RV32I-compiled programs in a
///                    virtual machine within the OS.
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
/// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///                                James Card
///                         http://www.jamescard.org
///
///////////////////////////////////////////////////////////////////////////////

#ifndef RV32I_H
#define RV32I_H

// Custom includes
#include "NanoOs.h"
#include "VirtualMemory.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RV32I_INSTRUCTION_SIZE                          4
#define RV32I_PROGRAM_START                        0x1000
#define RV32I_MEMORY_SIZE                       0x1000000
#define RV32I_STACK_START               RV32I_MEMORY_SIZE
#define RV32I_CLINT_BASE_ADDR                  0x02000000
#define RV32I_CLINT_ADDR_MASK (RV32I_CLINT_BASE_ADDR - 1)

// ECALL support
#define RV32I_SYSCALL_WRITE 64
#define RV32I_SYSCALL_EXIT 93
#define RV32I_STDOUT_FILENO 1
#define RV32I_STDERR_FILENO 2
#define RV32I_MAX_WRITE_LENGTH 256

/// @enum Rv32iOpCode
///
/// @brief Standard RISC-V RV32I base instruction set opcodes (7 bits)
typedef enum Rv32iOpCode {
  RV32I_LOAD           = 0x03, // Load instructions (lb, lh, lw, lbu, lhu)
  RV32I_LOAD_FP        = 0x07, // Floating-point load (not in RV32I)
  RV32I_CUSTOM_0       = 0x0B, // Custom instruction space 0
  RV32I_MISC_MEM       = 0x0F, // Misc memory operations (fence)
  RV32I_OP_IMM         = 0x13, // Integer register-immediate instructions
  RV32I_AUIPC          = 0x17, // Add upper immediate to PC
  RV32I_OP_IMM_32      = 0x1B, // 32-bit integer register-immediate (RV64I)
  RV32I_STORE          = 0x23, // Store instructions (sb, sh, sw)
  RV32I_STORE_FP       = 0x27, // Floating-point store (not in RV32I)
  RV32I_CUSTOM_1       = 0x2B, // Custom instruction space 1
  RV32I_AMO            = 0x2F, // Atomic memory operations
  RV32I_OP             = 0x33, // Integer register-register instructions
  RV32I_LUI            = 0x37, // Load upper immediate
  RV32I_OP_32          = 0x3B, // 32-bit integer register-register (RV64I)
  RV32I_MADD           = 0x43, // Multiply-add (not in RV32I)
  RV32I_MSUB           = 0x47, // Multiply-subtract (not in RV32I)
  RV32I_NMSUB          = 0x4B, // Negative multiply-subtract (not in RV32I)
  RV32I_NMADD          = 0x4F, // Negative multiply-add (not in RV32I)
  RV32I_OP_FP          = 0x53, // Floating-point operations (not in RV32I)
  RV32I_CUSTOM_2       = 0x5B, // Custom instruction space 2
  RV32I_BRANCH         = 0x63, // Conditional branch instructions
  RV32I_JALR           = 0x67, // Jump and link register
  RV32I_JAL            = 0x6F, // Jump and link
  RV32I_SYSTEM         = 0x73, // System/CSR instructions
  RV32I_CUSTOM_3       = 0x7B  // Custom instruction space 3
} Rv32iOpCode;

/// @enum Rv32iRegisterFunct3
///
/// @brief Function 3 codes for R-type register operations
typedef enum Rv32iRegisterFunct3 {
  RV32I_FUNCT3_ADD_SUB = 0x0,  // ADD/SUB based on funct7
  RV32I_FUNCT3_SLL     = 0x1,  // Shift Left Logical
  RV32I_FUNCT3_SLT     = 0x2,  // Set Less Than (signed)
  RV32I_FUNCT3_SLTU    = 0x3,  // Set Less Than Unsigned
  RV32I_FUNCT3_XOR     = 0x4,  // XOR
  RV32I_FUNCT3_SRL_SRA = 0x5,  // Shift Right Logical/Arithmetic
  RV32I_FUNCT3_OR      = 0x6,  // OR
  RV32I_FUNCT3_AND     = 0x7   // AND
} Rv32iRegisterFunct3;

/// @enum Rv32iRegisterFunct7
///
/// @brief Function 7 codes for R-type register operations
typedef enum Rv32iRegisterFunct7 {
  RV32I_FUNCT7_ADD     = 0x00, // Addition
  RV32I_FUNCT7_SUB     = 0x20, // Subtraction
  RV32I_FUNCT7_SRL     = 0x00, // Shift Right Logical
  RV32I_FUNCT7_SRA     = 0x20  // Shift Right Arithmetic
} Rv32iRegisterFunct7;

/// @enum Rv32iImmediateFunct3
///
/// @brief Function 3 codes for I-type immediate operations
typedef enum Rv32iImmediateFunct3 {
  RV32I_FUNCT3_ADDI  = 0x0,  // Add Immediate
  RV32I_FUNCT3_SLLI  = 0x1,  // Shift Left Logical Immediate
  RV32I_FUNCT3_SLTI  = 0x2,  // Set Less Than Immediate
  RV32I_FUNCT3_SLTIU = 0x3,  // Set Less Than Immediate Unsigned
  RV32I_FUNCT3_XORI  = 0x4,  // XOR Immediate
  RV32I_FUNCT3_SRLI_SRAI = 0x5,  // Shift Right Logical/Arithmetic Immediate
  RV32I_FUNCT3_ORI   = 0x6,  // OR Immediate
  RV32I_FUNCT3_ANDI  = 0x7   // AND Immediate
} Rv32iImmediateFunct3;

/// @enum Rv32iLoadFunct3
///
/// @brief Function 3 codes for load operations
typedef enum Rv32iLoadFunct3 {
  RV32I_FUNCT3_LB  = 0x0,  // Load Byte
  RV32I_FUNCT3_LH  = 0x1,  // Load Halfword
  RV32I_FUNCT3_LW  = 0x2,  // Load Word
  RV32I_FUNCT3_LBU = 0x4,  // Load Byte Unsigned
  RV32I_FUNCT3_LHU = 0x5   // Load Halfword Unsigned
} Rv32iLoadFunct3;

/// @enum Rv32iStoreFunct3
///
/// @brief Function 3 codes for store operations
typedef enum Rv32iStoreFunct3 {
  RV32I_FUNCT3_SB = 0x0,  // Store Byte
  RV32I_FUNCT3_SH = 0x1,  // Store Halfword
  RV32I_FUNCT3_SW = 0x2   // Store Word
} Rv32iStoreFunct3;

/// @enum Rv32iBranchFunct3
///
/// @brief Function 3 codes for branch operations
typedef enum Rv32iBranchFunct3 {
  RV32I_FUNCT3_BEQ  = 0x0,  // Branch Equal
  RV32I_FUNCT3_BNE  = 0x1,  // Branch Not Equal
  RV32I_FUNCT3_BLT  = 0x4,  // Branch Less Than
  RV32I_FUNCT3_BGE  = 0x5,  // Branch Greater Than or Equal
  RV32I_FUNCT3_BLTU = 0x6,  // Branch Less Than Unsigned
  RV32I_FUNCT3_BGEU = 0x7   // Branch Greater Than or Equal Unsigned
} Rv32iBranchFunct3;

/// @enum Rv32iSystemFunct3
///
/// @brief Function 3 codes for system operations
typedef enum Rv32iSystemFunct3 {
  RV32I_FUNCT3_ECALL_EBREAK = 0x0,  // Environment Call / Environment Break
  RV32I_FUNCT3_CSRRW   = 0x1,  // CSR Read/Write
  RV32I_FUNCT3_CSRRS   = 0x2,  // CSR Read and Set Bits
  RV32I_FUNCT3_CSRRC   = 0x3,  // CSR Read and Clear Bits
  RV32I_FUNCT3_CSRRWI  = 0x5,  // CSR Read/Write Immediate
  RV32I_FUNCT3_CSRRSI  = 0x6,  // CSR Read and Set Bits Immediate
  RV32I_FUNCT3_CSRRCI  = 0x7   // CSR Read and Clear Bits Immediate
} Rv32iSystemFunct3;

/// @enum Rv32iSystemImm12
///
/// @brief 12-bit immediate values for system calls
typedef enum Rv32iSystemImm12 {
  RV32I_IMM12_ECALL  = 0x000,  // Environment Call
  RV32I_IMM12_EBREAK = 0x001   // Environment Break
} Rv32iSystemImm12;

/// @enum Rv32iImmediateFunct7
///
/// @brief Function 7 codes for immediate shift operations
typedef enum Rv32iImmediateFunct7 {
  RV32I_FUNCT7_SRLI = 0x00,  // Shift Right Logical Immediate
  RV32I_FUNCT7_SRAI = 0x20   // Shift Right Arithmetic Immediate
} Rv32iImmediateFunct7;

/// @enum Rv32iCsrAddresses
///
/// @brief Standard CSR addresses
typedef enum Rv32iCsrAddresses {
  // Machine Information Registers
  RV32I_CSR_MVENDORID = 0xF11,  // Vendor ID
  RV32I_CSR_MARCHID   = 0xF12,  // Architecture ID
  RV32I_CSR_MIMPID    = 0xF13,  // Implementation ID
  RV32I_CSR_MHARTID   = 0xF14,  // Hardware Thread ID
  
  // Machine Trap Setup
  RV32I_CSR_MSTATUS   = 0x300,  // Machine Status
  RV32I_CSR_MISA      = 0x301,  // ISA and Extensions
  RV32I_CSR_MIE       = 0x304,  // Machine Interrupt Enable
  RV32I_CSR_MTVEC     = 0x305,  // Machine Trap Vector
  
  // Machine Trap Handling
  RV32I_CSR_MSCRATCH  = 0x340,  // Scratch Register
  RV32I_CSR_MEPC      = 0x341,  // Exception Program Counter
  RV32I_CSR_MCAUSE    = 0x342,  // Trap Cause
  RV32I_CSR_MTVAL     = 0x343,  // Trap Value
  RV32I_CSR_MIP       = 0x344   // Machine Interrupt Pending
} Rv32iCsrAddresses;

/// @struct Rv32iCoreRegisters
///
/// @brief Structure to keep track of the state of a single virtual RV32I core.
///
/// @param x General-purpose registers (x0 - x31).
/// @param pc The program counter.
/// @param mstatus The machine status register.
/// @param misa The machine ISA register.
/// @param mie The machine interrupt enable.
/// @param mtvec The machine trap vector.
/// @param mscratch The machine scratch register.
/// @param mepc The machine exception program counter.
/// @param mcause The machine cause register.
/// @param mtval The machine trap value.
/// @param mip The machine interrupt pending.
typedef struct Rv32iCoreRegisters {
  uint32_t x[32];
  uint32_t pc;
  uint32_t mstatus;
  uint32_t misa;
  uint32_t mie;
  uint32_t mtvec;
  uint32_t mscratch;
  uint32_t mepc;
  uint32_t mcause;
  uint32_t mtval;
  uint32_t mip;
} Rv32iCoreRegisters;

/// @struct Rv32iVm
///
/// @brief Full state needed to run an RV32I process.
///
/// @param rv32iCoreRegisters The registers for a single RV32I core.
/// @param physicalMemory The VirtualMemoryState that allows for access to the
///   virtual memory that will correpsond to the physical memory of the VM.
/// @param mappedMemory The VirtualMemoryState that allows for access to the
///   virtual memory that will correpsond to the mapped memory of the VM.
/// @param running Whether or not the VM is currently in a running state.
/// @param exitCode The exit code to return to the caller when the process
///   exits.
typedef struct Rv32iVm {
  Rv32iCoreRegisters rv32iCoreRegisters;
  VirtualMemoryState physicalMemory;
  VirtualMemoryState mappedMemory;
  bool running;
  int exitCode;
} Rv32iVm;

int runRv32iProcess(int argc, char **argv);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // RV32I_H
