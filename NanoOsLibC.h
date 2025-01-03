///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              11.21.2024
///
/// @file              NanoOsLibC.h
///
/// @brief             Definitions of some standard C functionality missing
///                    from the Arduino Nano libraries.
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

/*
 * This file is included by Coroutines.h to provide functionality missing from
 * the Arduino C implementation and to provide some debugging tools when
 * debugging the Corutines library from NanoOs.
 */

#ifndef NANO_OS_LIB_C
#define NANO_OS_LIB_C

#include <time.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

/// @def SINGLE_CORE_COROUTINES
///
/// @brief This define causes the Coroutines library to omit multi-thread
/// support.
#define SINGLE_CORE_COROUTINES

/// @def CoroutineId
///
/// @brief The integer type to use of coroutine IDs.  This must be an unsigned
/// type.  We will have fewer than 256 coroutines in NanoOs, so a uint8_t will
/// work just fine for us.  This will save us some memory in the definition of
/// the Coroutine data structure, of course.
#define CoroutineId uint8_t

/// @def COROUTINE_ID_NOT_SET
///
/// @brief The integer value to use to indicate that a Coroutine's ID is not
/// set.  This must be a negative value, so the most-significant bit must be
/// set.  In NanoOs, we only use 8 processes, so a value of -8 will work fine
/// for us.  Also, in the MemoryManager library, we use 4 bits to store the ID
/// of the owner, so this is an appropriate value.
#define COROUTINE_ID_NOT_SET ((uint8_t) 0x0f)

// Missing from Arduino
struct timespec {
  time_t tv_sec;
  long   tv_nsec;
};
#define TIME_UTC 1
int timespec_get(struct timespec* spec, int base);

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


//// #define NANO_OS_DEBUG
#ifdef NANO_OS_DEBUG

/// @def startDebugMessage
///
/// @brief Print a non-newline-terminated debug message.
#define startDebugMessage(message) \
  printString("["); \
  printULong(getElapsedMilliseconds(0)); \
  printString(" Process "); \
  printUInt(coroutineId(getRunningCoroutine())); \
  printString(" "); \
  printString((strrchr(__FILE__, '/') + 1)); \
  printString(":"); \
  printString(__func__); \
  printString("."); \
  printULong(__LINE__); \
  printString("] "); \
  printString(message);

/// @def printDebugStackDepth()
///
/// @brief Print the depth of the current coroutine stack.
#define printDebugStackDepth() \
  { \
    char temp; \
    startDebugMessage("Stack depth: "); \
    printInt(ABS_DIFF((uintptr_t) &temp, (uintptr_t) getRunningCoroutine())); \
    printString("\n"); \
  } \

#else // NANO_OS_DEBUG

#define startDebugMessage(message) {}
#define printDebugStackDepth() {}

#endif // NANO_OS_DEBUG


// The errors defined by the compiler's version of errno.h are not helpful
// because most things are defined to be ENOERR.  So, we need to define some of
// our own.
#define ENOERR           0      /* Success */
#define EUNKNOWN         1      /* Unknown error */
#define EBUSY            2      /* Device or resource busy */
#define ENOMEM           3      /* Out of memory */
#define EACCES           4      /* Permission denied */
#define EINVAL           5      /* Invalid argument */

typedef void TypeDescriptor;

#define typeString ((TypeDescriptor*) ((intptr_t)  1))
#define typeInt    ((TypeDescriptor*) ((intptr_t)  2))

#define STOP       ((void*) ((intptr_t) -1))

extern const char *boolNames[];

// Debug functions
int printString_(const char *string);
#define printString printString_
int printInt_(int integer);
#define printInt printInt_
int printUInt_(unsigned int integer);
#define printUInt printUInt_
int printLong_(long int integer);
#define printLong printLong_
int printULong_(unsigned long int integer);
#define printULong printULong_
int printLongLong_(long long int integer);
#define printLongLong printLongLong_
int printULongLong_(unsigned long long int integer);
#define printULongLong printULongLong_
int printDouble(double floatingPointValue);
int printList_(const char *firstString, ...);
#define printList(firstString, ...) printList_(firstString, ##__VA_ARGS__, STOP)
unsigned long getElapsedMilliseconds(unsigned long startTime);

// C-like functions
int vsscanf(const char *buffer, const char *format, va_list args);
int sscanf(const char *buffer, const char *format, ...);
const char* nanoOsStrError(int errnum);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NANO_OS_LIB_C
