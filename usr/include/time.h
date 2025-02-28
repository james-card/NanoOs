///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              02.22.2025
///
/// @file              time.h
///
/// @brief             Implementation of time.h for NanoOs C programs.
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

#ifndef TIME_H
#define TIME_H

#ifdef __cplusplus
extern "C"
{
#endif

// Standard C includes
#include <stdint.h>

// NanoOs includes
#include "NanoOsSystemCalls.h"

/// @typedef time_t
///
/// @brief Definition for time in seconds since the epoch.
typedef int64_t time_t;

/// @struct timespec
///
/// @brief Structure to hold a time value in seconds and nanoseconds.
///
/// @param tv_sec The number of whole seconds.
/// @param tv_nsec The number of nanoseconds (0-999999999).
struct timespec {
  time_t tv_sec;
  long tv_nsec;
};

/// @def TIME_UTC
///
/// @brief Value to pass to timespec_get to request UTC time.
#define TIME_UTC 1

/// @fn int timespec_get(struct timespec *ts, int base)
///
/// @brief Get the current time in a timespec structure.
///
/// @param ts A pointer to the timespec structure to populate.
/// @param base The time base to use (currently only TIME_UTC is supported).
///
/// @return Returns the base value on success, 0 on failure.
static inline int timespec_get(volatile struct timespec *ts, int base) {
  volatile register struct timespec *a0 asm("a0") = ts;         // timespec ptr
  register int a1 asm("a1") = base;                             // base value
  register int a7 asm("a7") = NANO_OS_SYSCALL_TIMESPEC_GET;     // syscall number
  
  asm volatile(
    "ecall"
    : "+r"(a0)
    : "r"(a1), "r"(a7)
  );
  
  return (int) a0;
}

/// @fn int nanosleep(const struct timespec *req, struct timespec *rem)
///
/// @brief Sleep for the specified amount of time.
///
/// @param req A pointer to the timespec structure specifying how long to sleep.
/// @param rem A pointer to a timespec structure to store remaining time if
///   interrupted, or NULL.
///
/// @return Returns 0 on success, -1 on failure.
static inline int nanosleep(
  const struct timespec *req, struct timespec *rem
) {
  register const struct timespec *a0 asm("a0") = req;       // request timespec
  register struct timespec *a1 asm("a1") = rem;             // remaining timespec
  register int a7 asm("a7") = NANO_OS_SYSCALL_NANOSLEEP;    // syscall number
  
  asm volatile(
    "ecall"
    : "+r"(a0)
    : "r"(a1), "r"(a7)
  );
  
  return (int) a0;
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // TIME_H

