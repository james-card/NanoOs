///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              02.20.2025
///
/// @file              stdlib.h
///
/// @brief             Implementation of stdlib.h for NanoOs C programs.
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

#ifndef STDLIB_H
#define STDLIB_H

#ifdef __cplusplus
extern "C"
{
#endif

// NanoOs includes
#include "NanoOsSystemCalls.h"

/// @fn void exit(int status)
///
/// @brief Implementation of the standard C exit function.  Returns the provided
/// status value back to the host operating system.
///
/// @param status The integer value to return back to the OS.
///
/// @return This function returns no value and, in fact, never returns.
static inline void exit(int status) {
  register int a0 asm("a0") = status;               // exit code 0
  register int a7 asm("a7") = NANO_OS_SYSCALL_EXIT; // exit syscall
  
  asm volatile(
    "ecall"
    : : "r"(a0), "r"(a7)
  );
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // STDLIB_H

