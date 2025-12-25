///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              12.25.2025
///
/// @file              time.h
///
/// @brief             Replacement for standard C time.h header for NanoOs.
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

#if !defined(NANO_OS_TIME_H) && !defined(_SYS__TIMESPEC_H_)
#define NANO_OS_TIME_H
#define _SYS__TIMESPEC_H_

#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef int64_t time_t;

time_t time(time_t *tloc);

struct timespec {
  time_t tv_sec;
  long   tv_nsec;
};
int timespec_get(struct timespec* spec, int base);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NANO_OS_TIME_H

#ifndef TIME_UTC
#define TIME_UTC 1
#endif
