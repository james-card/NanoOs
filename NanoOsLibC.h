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

//// #define NANO_OS_DEBUG
#include "NanoOsStdio.h"

/*
 * This file is included by Coroutines.h to provide functionality missing from
 * the Arduino C implementation and to provide some debugging tools when
 * debugging the Corutines library from NanoOs.
 */

#ifndef NANO_OS_LIB_C_H
#define NANO_OS_LIB_C_H

// Standard C includes
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
#if defined(__AVR__)
struct timespec {
  time_t tv_sec;
  long   tv_nsec;
};
#endif // __arm__
#define TIME_UTC 1
int timespec_get(struct timespec* spec, int base);

/// @def MIN
///
/// @brief Get the minimum of two values.
///
/// @param x The first value to compare.
/// @param y The second value to compare.
#define MIN(x, y) (((x) <= (y)) ? (x) : (y))

/// @def MAX
///
/// @brief Get the maximum of two values.
///
/// @param x The first value to compare.
/// @param y The second value to compare.
#define MAX(x, y) (((x) >= (y)) ? (x) : (y))

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


// The errors defined by the compiler's version of errno.h are not helpful
// because most things are defined to be ENOERR.  So, we need to define some of
// our own.
#define ENOERR           0      /* Success */
#define EUNKNOWN         1      /* Unknown error */
#define EBUSY            2      /* Device or resource busy */
#define ENOMEM           3      /* Out of memory */
#define EACCES           4      /* Permission denied */
#define EINVAL           5      /* Invalid argument */
#define EIO              6      /* I/O error */
#define ENOSPC           7      /* No space left on device */
#define ENOENT           8      /* No such entry found */
#define ENOTEMPTY        9      /* Directory not empty */
#define EOVERFLOW       10      /* Overflow detected */
#define EFAULT          11      /* Invalid address */
#define ENAMETOOLONG    12      /* Name too long */
#define EEND            13      /* End of error codes */

int* errno_(void);
#define errno (*errno_())

typedef void TypeDescriptor;

#define typeString ((TypeDescriptor*) ((intptr_t)  1))
#define typeInt    ((TypeDescriptor*) ((intptr_t)  2))

#define STOP       ((void*) ((intptr_t) -1))

extern const char *boolNames[];
unsigned long getElapsedMilliseconds(unsigned long startTime);
void msleep(unsigned int durationMs);

char* nanoOsStrError(int errnum);
#ifdef strerror
#undef strerror
#endif
#define strerror nanoOsStrError

char* nanoOsGetenv(const char *name);
#ifdef getenv
#undef getenv
#endif
#define getenv(name) nanoOsGetenv(name)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NANO_OS_LIB_C_H
