///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              02.17.2025
///
/// @file              NanoOsSystemCalls.h
///
/// @brief             Library for system calls supported by NanoOs for user
///                    space programs.
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

#ifndef NANO_OS_SYSTEM_CALLS_H
#define NANO_OS_SYSTEM_CALLS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/// @enum NanoOsSystemCall
///
/// @brief Enumeration of the system calls supported by NanoOs.
typedef enum NanoOsSystemCall {
  NANO_OS_SYSCALL_EXIT,
  NANO_OS_SYSCALL_WRITE,
  NUM_NANO_OS_SYSCALLS
} NanoOsSystemCall;

/// @def NANO_OS_MAX_WRITE_LENGTH
///
/// @brief The maximum number of characters that can be written by a user space
/// program in a single call.
#define NANO_OS_MAX_WRITE_LENGTH 128

int32_t nanoOsSystemCallHandle(void *vm);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NANO_OS_SYSTEM_CALLS_H
