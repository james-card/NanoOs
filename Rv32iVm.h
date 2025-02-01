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

#define RV32I_INSTRUCTION_SIZE                 4
#define RV32I_PROGRAM_START               0x1000
#define RV32I_MEMORY_SIZE              0x1000000
#define RV32I_STACK_START      RV32I_MEMORY_SIZE

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
typedef struct Rv32iVm {
  Rv32iCoreRegisters rv32iCoreRegisters;
  VirtualMemoryState physicalMemory;
  VirtualMemoryState mappedMemory;
} Rv32iVm;

int runRv32iProcess(int argc, char **argv);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // RV32I_H
