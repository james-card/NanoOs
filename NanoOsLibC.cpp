////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2025 James Card                     //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included    //
// in all copies or substantial portions of the Software.                     //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//                                 James Card                                 //
//                          http://www.jamescard.org                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// Doxygen marker
/// @file

// Arduino includes
#include <Arduino.h>

#include "NanoOs.h"
#include "NanoOsLibC.h"
#include "Console.h"
#include "Filesystem.h"
#include "Scheduler.h"

/// @var errno
///
/// @brief Global errno value.
int errno = 0;

/// @fn int timespec_get(struct timespec* spec, int base)
///
/// @brief Get the current time in the form of a struct timespec.
///
/// @param spec A pointer to the sturct timespec to populate.
/// @param base The base for the time (TIME_UTC).
///
/// @return Returns teh value of base on success, 0 on failure.
int timespec_get(struct timespec* spec, int base) {
  if (spec == NULL) {
    return 0;
  }
  
  unsigned long now = getElapsedMilliseconds(0);
  spec->tv_sec = (time_t) (now / 1000UL);
  spec->tv_nsec = (now % 1000UL) * 1000000UL;

  return base;
}

/// @var errorStrings
///
/// @brief Array of error messages arranged by error code.
const char *errorStrings[] = {
  "Success",                 // ENOERR
  "Unknown error",           // EUNKNOWN
  "Device or resource busy", // EBUSY
  "Out of memory",           // ENOMEM
  "Permission denied",       // EACCES
  "Invalid argument",        // EINVAL
  "I/O error",               // EIO
  "No space left on device", // ENOSPC
  "No such entry found",     // ENOENT
  "Directory not empty",     // ENOTEMPTY
  "Overflow detected",       // EOVERFLOW
  "Invalid address",         // EFAULT          
  "Name too long",           // ENAMETOOLONG    
};

/// @var NUM_ERRORS
///
/// @brief Constant value to hold the number of errors defined in errorStrings.
const int NUM_ERRORS = sizeof(errorStrings) / sizeof(errorStrings[0]);

/// @fn char* nanoOsStrError(int errnum)
///
/// @brief nanoOsStrError implementation for NanoOs.
///
/// @param errnum The error code either set as the variable errno or returned
///   from a function.
///
/// @return This function always succeeds and returns the string corrsponding
/// to an error.  If the provided error number is outside the range of the
/// defined errors, the string "Unknown error" will be returned.
char* nanoOsStrError(int errnum) {
  if ((errnum < 0) || (errnum >= NUM_ERRORS)) {
    errnum = EUNKNOWN;
  }

  return (char*) errorStrings[errnum];
}

/// @fn unsigned long getElapsedMilliseconds(unsigned long startTime)
///
/// @brief Get the number of milliseconds that have elapsed since a specified
/// time in the past.
///
/// @param startTime The time in the past to calcualte the elapsed time from.
///
/// @return Returns the number of milliseconds that have elapsed since the
/// provided start time.
unsigned long getElapsedMilliseconds(unsigned long startTime) {
  unsigned long now = millis();

  if (now < startTime) {
    return ULONG_MAX;
  }

  return now - startTime;
}

/// @fn void msleep(unsigned int duration)
///
/// @brief Delay execution for a specified number of milliseconds.
///
/// @param durationMs The number of milliseconds to wait before contiuing
///   execution.
///
/// @return This function returns no value.
void msleep(unsigned int durationMs) {
  unsigned long start = getElapsedMilliseconds(0);
  while (getElapsedMilliseconds(start) < durationMs);
}

/// @fn char* nanoOsGetenv(const char *name)
///
/// @brief Implementation of the standard C getenv fundtion for NanoOs.
///
/// @param name The name of the environment variable to retrive.
///
/// @return Returns a pointer to the value of the named environment variable on
/// success, NULL on failure.
char* nanoOsGetenv(const char *name) {
  char **env = overlayMap->header.env;
  if ((name == NULL) || (*name == '\0') || (env == NULL)) {
    return NULL;
  }

  size_t nameLen = strlen(name);
  char *value = NULL;
  for (int ii = 0; env[ii] != NULL; ii++) {
    if ((strncmp(env[ii], name, nameLen) == 0) && env[ii][nameLen] == '=') {
      value = &env[ii][nameLen + 1];
      break;
    }
  }

  return value;
}

