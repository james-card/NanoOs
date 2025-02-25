/**
 * 
 *  @author            James Card
 *  @date              02.25.2025
 * 
 *  @file              rv32i.ld
 * 
 *  @brief             Linker script for RISC-V 32-bit organized around address
 *                     0x1000.  For use with riscv32-unknown-elf-gcc.
 * 
 *  @copyright
 *                    Copyright (c) 2012-2025 James Card
 * 
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 * 
 *  The above copyright notice and this permission notice shall be included
 *  in all copies or substantial portions of the Software.
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 * 
 *                                 James Card
 *                          http://www.jamescard.org
 * 
 */

OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

/* Set the global pointer value to 0x1000 as requested */
__global_pointer$ = 0x1000;

SECTIONS
{
  /* Code section - starts at address 0x1000 */
  .text 0x1000 : {
    PROVIDE(_text_start = .);
    *(.text.init)
    *(.text .text.*)
    PROVIDE(_text_end = .);
  } :text

  /* Read-only data */
  .rodata : {
    PROVIDE(_rodata_start = .);
    *(.rodata .rodata.*)
    PROVIDE(_rodata_end = .);
  } :rodata

  /* Small data section that is addressable by the global pointer */
  .sdata : {
    _gp = . + 0x800; /* Traditional placement of _gp is 0x800 bytes into .sdata */
    PROVIDE(_sdata_start = .);
    *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata*)
    *(.sdata .sdata.* .gnu.linkonce.s.*)
    PROVIDE(_sdata_end = .);
  } :data

  /* Initialized data section */
  .data : {
    PROVIDE(_data_start = .);
    *(.data .data.*)
    PROVIDE(_data_end = .);
  } :data

  /* BSS section - zero-initialized data */
  .bss (NOLOAD) : {
    PROVIDE(_bss_start = .);
    *(.bss .bss.*)
    *(COMMON)
    PROVIDE(_bss_end = .);
  } :bss

  /* Discard unused sections */
  /DISCARD/ : {
    *(.eh_frame)
    *(.comment)
  }
}

/* Define memory segment permissions */
PHDRS
{
  text   PT_LOAD FLAGS(5);  /* 5 = RX (read + execute) */
  rodata PT_LOAD FLAGS(4);  /* 4 = R (read-only) */
  data   PT_LOAD FLAGS(6);  /* 6 = RW (read + write) */
  bss    PT_LOAD FLAGS(6);  /* 6 = RW (read + write) */
}

