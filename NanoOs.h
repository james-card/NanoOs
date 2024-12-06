///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              11.21.2024
///
/// @file              NanoOs.h
///
/// @brief             Core nanokernel functionality for NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2024 James Card
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

#ifndef NANO_OS_H
#define NANO_OS_H

// Arduino includes
#include <Arduino.h>
#include <HardwareSerial.h>

// Standard C includes
#include <limits.h>
#include <string.h>
#include <stdlib.h>

// Custom includes
#include "NanoOsLibC.h"
#include "Coroutines.h"
#include "Commands.h"
#include "MemoryManager.h"
#include "Processes.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// @def NANO_OS_NUM_COMMANDS
///
/// @brief The total number of concurrent processes that can be run by the OS,
/// including the scheduler.
///
/// @note If this value is increased beyond 8, the number of bits used to store
/// the owner in a MemNode in MemoryManager.cpp must be extended and the value
/// of COROUTINE_ID_NOT_SET must be changed in NanoOsLibC.h.  If this value is
/// increased beyond 127, then the type defined by COROUTINE_ID_TYPE in
/// NanoOsLibC.h must also be extended.
#define NANO_OS_NUM_COMMANDS             8

/// @def NANO_OS_STACK_SIZE
///
/// @brief The minimum size for an individual process's stack.  Actual size will
/// be slightly larger than this.  This value needs to be a multiple of 
/// COROUTINE_STACK_CHUNK_SIZE in Coroutines.h
#define NANO_OS_STACK_SIZE               512
#if ((NANO_OS_STACK_SIZE % COROUTINE_STACK_CHUNK_SIZE) != 0)
#error "NANO_OS_STACK_SIZE must be a multiple of COROUTINE_STACK_CHUNK_SIZE."
#endif

/// @def NANO_OS_NUM_MESSAGES
///
/// @brief The total number of inter-process messages that will be available
/// for use by processes.
#define NANO_OS_NUM_MESSAGES               8

/// @def NANO_OS_SCHEDULER_PROCESS_ID
///
/// @brief The process ID (PID) of the process that is reserved for the
/// scheduler.
#define NANO_OS_SCHEDULER_PROCESS_ID       0

/// @def NANO_OS_CONSOLE_PROCESS_ID
///
/// @brief The process ID (PID) of the process that will run the console.  This
/// must be the lowest value after the scheduler process (i.e. 1).
#define NANO_OS_CONSOLE_PROCESS_ID         1

/// @def NANO_OS_MEMORY_MANAGER_PROCESS_ID
///
/// @brief The process ID (PID) of the first user process.
#define NANO_OS_MEMORY_MANAGER_PROCESS_ID  2

/// @def NANO_OS_RESERVED_PROCESS_ID
///
/// @brief The process ID (PID) of the process that is reserved for system
/// processes.  This needs to be the last of the system processes because of
/// how this value is used in the scheduler's initialization.
#define NANO_OS_RESERVED_PROCESS_ID          3

/// @def NANO_OS_FIRST_PROCESS_ID
///
/// @brief The process ID (PID) of the first user process, i.e. the first ID
/// after the last system process ID.
#define NANO_OS_FIRST_PROCESS_ID           4

/// @def NANO_OS_VERSION
///
/// @brief The version string for NanoOs
#define NANO_OS_VERSION "0.0.1"

// Support functions
long getElapsedMilliseconds(unsigned long startTime);
void timespecFromDelay(struct timespec *ts, long int delayMs);

#ifdef __cplusplus
} // extern "C"
#endif

// This has to be included separately and last.
#include "Console.h"

#endif // NANO_OS_H
