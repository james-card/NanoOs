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
/// @brief The integer type to use of coroutine IDs.  This must be a signed
/// type.  We will have fewer than 128 coroutines in NanoOs, so an int8_t will
/// work just fine for us.  This will save us some memory in the definition of
/// the Coroutine data structure, of course.
#define COROUTINE_ID_TYPE int8_t

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
#define EBUSY            1      /* Device or resource busy */
#define ENOMEM           2      /* Out of memory */


// Debug functions
int printString(const char *string);
int printInt(int integer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NANO_OS_LIB_C
