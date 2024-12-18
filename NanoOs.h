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

// Coroutines
#include "Coroutines.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// @def NANO_OS_NUM_PROCESSES
///
/// @brief The total number of concurrent processes that can be run by the OS,
/// including the scheduler.
///
/// @note If this value is increased beyond 15, the number of bits used to store
/// the owner in a MemNode in MemoryManager.cpp must be extended and the value
/// of COROUTINE_ID_NOT_SET must be changed in NanoOsLibC.h.  If this value is
/// increased beyond 255, then the type defined by COROUTINE_ID_TYPE in
/// NanoOsLibC.h must also be extended.
#define NANO_OS_NUM_PROCESSES               8

/// @def NANO_OS_STACK_SIZE
///
/// @brief The minimum size for an individual process's stack.  Actual size will
/// be slightly larger than this.  This value needs to be a multiple of 
/// COROUTINE_STACK_CHUNK_SIZE in Coroutines.h
#define NANO_OS_STACK_SIZE                 512
#if ((NANO_OS_STACK_SIZE % COROUTINE_STACK_CHUNK_SIZE) != 0)
#error "NANO_OS_STACK_SIZE must be a multiple of COROUTINE_STACK_CHUNK_SIZE."
#endif

/// @def NANO_OS_NUM_MESSAGES
///
/// @brief The total number of inter-process messages that will be available
/// for use by processes.
#define NANO_OS_NUM_MESSAGES                 8

/// @def NANO_OS_SCHEDULER_PROCESS_ID
///
/// @brief The process ID (PID) of the process that is reserved for the
/// scheduler.
#define NANO_OS_SCHEDULER_PROCESS_ID         0

/// @def NANO_OS_CONSOLE_PROCESS_ID
///
/// @brief The process ID (PID) of the process that will run the console.  This
/// must be the lowest value after the scheduler process (i.e. 1).
#define NANO_OS_CONSOLE_PROCESS_ID           1

/// @def NANO_OS_MEMORY_MANAGER_PROCESS_ID
///
/// @brief The process ID (PID) of the first user process.
#define NANO_OS_MEMORY_MANAGER_PROCESS_ID    2

/// @def NANO_OS_FIRST_PROCESS_ID
///
/// @brief The process ID (PID) of the first user process, i.e. the first ID
/// after the last system process ID.
#define NANO_OS_FIRST_PROCESS_ID             3

/// @def NANO_OS_VERSION
///
/// @brief The version string for NanoOs
#define NANO_OS_VERSION "0.0.1"

/// @def ROOT_USER_ID
///
/// @brief The numerical ID of the root user.
#define ROOT_USER_ID 0

/// @def NO_USER_ID
///
/// @brief The numerical value that indicates that a process is not owned.
#define NO_USER_ID -1

/// @def MIN
///
/// @brief Get the minimum of two values.
///
/// @param x The first value to compare.
/// @param y The second value to compare.
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/// @def MAX
///
/// @brief Get the maximum of two values.
///
/// @param x The first value to compare.
/// @param y The second value to compare.
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

/// @def ABS
///
/// @brief Get the absolute value of a value.
///
/// @param x The value to get the absolute value of.
#define ABS(x) (((x) >= 0) ? (x) : (-(x)))

/// @def ABS_DIFF
///
/// @brief Get the absolute value of the difference between two values.
///
/// @param x The first value to evaluate in the difference.
/// @param y The second value to evaluate in the difference.
#define ABS_DIFF(x, y) (((x) >= (y)) ? (x) - (y) : (y) - (x))

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

/// @def printDebug
///
/// @brief Macro to identify debugging prints when necessary.
#define printDebug Serial.print

/// @typedef UserId
///
/// @brief The type to use to represent a numeric user ID.
typedef int16_t UserId;

/// @struct User
///
/// @param userId The numeric ID for the user.
/// @param username The literal name of the user.
/// @param password The SHA1 hash of the user's password.
typedef struct User {
  UserId        userId;
  const char   *username;
  const char   *password;
} User;

/// @typedef NanoOsMessageData
///
/// @brief Data type used in a NanoOsMessage.
typedef unsigned long long int NanoOsMessageData;

/// @struct NanoOsMessage
///
/// @brief A generic message that can be exchanged between processes.
///
/// @param func Information about the function to run, cast to an unsigned long
///   long int.
/// @param data Information about the data to use, cast to an unsigned long
///   long int.
/// @param comessage A pointer to the comessage that points to this
///   NanoOsMessage.
typedef struct NanoOsMessage {
  NanoOsMessageData  func;
  NanoOsMessageData  data;
  Comessage         *comessage;
} NanoOsMessage;

// Support functions
long getElapsedMilliseconds(unsigned long startTime);
void timespecFromDelay(struct timespec *ts, long int delayMs);
unsigned int raiseUInt(unsigned int x, unsigned int y);
int sha1Digest(uint8_t *digest, char *hexdigest,
  const uint8_t *data, size_t databytes,
  uint32_t *W, uint8_t *datatail);
int sha1HexToDigest(const char *hexDigest, uint8_t *digest);
char* getHexDigest(const char *inputString);
const char* getUsernameByUserId(UserId userId);
UserId getUserIdByUsername(const char *username);
void login(void);

#ifdef __cplusplus
} // extern "C"
#endif

// NanoOs includes.  These have to be included separately and last.
#include "NanoOsLibC.h"
#include "Commands.h"
#include "MemoryManager.h"
#include "Processes.h"
#include "Console.h"

#endif // NANO_OS_H
