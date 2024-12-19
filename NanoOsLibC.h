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

/// @def COROUTINE_ID_TYPE
///
/// @brief The integer type to use of coroutine IDs.  This must be an unsigned
/// type.  We will have fewer than 256 coroutines in NanoOs, so a uint8_t will
/// work just fine for us.  This will save us some memory in the definition of
/// the Coroutine data structure, of course.
#define COROUTINE_ID_TYPE uint8_t

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
int printDouble(double floatingPointValue);
int printList_(const char *firstString, ...);
#define printList(firstString, ...) printList_(firstString, ##__VA_ARGS__, STOP)
int vsscanf(const char *buffer, const char *format, va_list args);
int sscanf(const char *buffer, const char *format, ...);
const char* nanoOsStrError(int errnum);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NANO_OS_LIB_C
