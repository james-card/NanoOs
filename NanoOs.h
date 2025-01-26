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

#ifndef NANO_OS_H
#define NANO_OS_H

// Arduino includes
#include <Arduino.h>
#include <HardwareSerial.h>

// Local headers
#include "NanoOsTypes.h"
#include "Coroutines.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// @def NANO_OS_STACK_SIZE
///
/// @brief The minimum size for an individual process's stack.  Actual size will
/// be slightly larger than this.  This value needs to be a multiple of 
/// COROUTINE_STACK_CHUNK_SIZE in Coroutines.h
#define NANO_OS_STACK_SIZE (COROUTINE_STACK_CHUNK_SIZE * 5)

/// @def NANO_OS_NUM_MESSAGES
///
/// @brief The total number of inter-process messages that will be available
/// for use by processes.
#define NANO_OS_NUM_MESSAGES                              6

/// @def NANO_OS_SCHEDULER_PROCESS_ID
///
/// @brief The process ID (PID) of the process that is reserved for the
/// scheduler.
#define NANO_OS_SCHEDULER_PROCESS_ID                      0

/// @def NANO_OS_CONSOLE_PROCESS_ID
///
/// @brief The process ID (PID) of the process that will run the console.  This
/// must be the lowest value after the scheduler process (i.e. 1).
#define NANO_OS_CONSOLE_PROCESS_ID                        1

/// @def NANO_OS_MEMORY_MANAGER_PROCESS_ID
///
/// @brief The process ID (PID) of the process that will manage memory.
#define NANO_OS_MEMORY_MANAGER_PROCESS_ID                 2

/// @def NANO_OS_SD_CARD_PROCESS_ID
///
/// @brief The process ID (PID) of the process that will manage the SD card.
#define NANO_OS_SD_CARD_PROCESS_ID                        3

/// @def NANO_OS_FILESYSTEM_PROCESS_ID
///
/// @brief The process ID (PID) of the process that will manage the filesystem.
#define NANO_OS_FILESYSTEM_PROCESS_ID                     4

/// @def NANO_OS_FIRST_USER_PROCESS_ID
///
/// @brief The process ID (PID) of the first user process, i.e. the first ID
/// after the last system process ID.
#define NANO_OS_FIRST_USER_PROCESS_ID                     5

/// @def NANO_OS_VERSION
///
/// @brief The version string for NanoOs
#define NANO_OS_VERSION                             "0.1.0"

/// @def ROOT_USER_ID
///
/// @brief The numerical ID of the root user.
#define ROOT_USER_ID                                      0

/// @def NO_USER_ID
///
/// @brief The numerical value that indicates that a process is not owned.
#define NO_USER_ID                                       -1

/// @def NUM_PROCESS_STORAGE_KEYS
///
/// @brief The total number of keys supported by the per-process storage.
#define NUM_PROCESS_STORAGE_KEYS                          1

/// @def FGETS_CONSOLE_BUFFER_KEY
///
/// @brief Per-process storage key for the consoleBufer pointer in consoleFGets.
#define FGETS_CONSOLE_BUFFER_KEY                          0

/// @def floatToInts
///
/// @brief Break a floating-point number into two integer values that represent
/// its whole component and its decimal component to a specified level of
/// precision.
///
/// @param number The number to break into integers.
/// @param precision The desired number of decimal places in the decimal
///   component.
#define floatToInts(number, precision) \
  (int) number, \
  ABS((int) (number * raiseUInt(10, precision))) % raiseUInt(10, precision)

/// @def stringDestroy
///
/// @brief Convenience macro for the common operation of destroying a string.
#define stringDestroy(string) ((char*) (free((void*) string), NULL))

#ifdef NANO_OS_DEBUG

/// @def printDebug
///
/// @brief Macro to identify debugging prints when necessary.
#define printDebug(message) Serial.print(message)

#else // NANO_OS_DEBUG

#define printDebug(message) {}

#endif // NANO_OS_DEBUG

/// @def STATIC_NANO_OS_MESSAGE
///
/// @brief Helper macro to define and initialize a NanoOs message for local
/// use.
#define STATIC_NANO_OS_MESSAGE( \
  variableName, type, funcValue, dataValue, waiting \
) \
  NanoOsMessage __nanoOsMessage; \
  ProcessMessage variableName = {}; \
  __nanoOsMessage.func = funcValue; \
  __nanoOsMessage.data = dataValue; \
  processMessageInit( \
    &variableName, type, &__nanoOsMessage, sizeof(__nanoOsMessage), waiting)

// Support functions
ProcessId getNumPipes(const char *commandLine);
void timespecFromDelay(struct timespec *ts, long int delayMs);
unsigned int raiseUInt(unsigned int x, unsigned int y);
const char* getUsernameByUserId(UserId userId);
UserId getUserIdByUsername(const char *username);
void login(void);
void *getProcessStorage(uint8_t key);
int setProcessStorage_(uint8_t key, void *val, int processId, ...);
#define setProcessStorage(key, val, ...) \
  setProcessStorage_(key, val, ##__VA_ARGS__, -1)

#ifdef __cplusplus
} // extern "C"
#endif

// NanoOs includes.  These have to be included separately and last.
#include "NanoOsLibC.h"
#include "Commands.h"
#include "MemoryManager.h"
#include "Processes.h"
#include "Console.h"
#include "Filesystem.h"

#endif // NANO_OS_H
