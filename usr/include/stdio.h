///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              02.20.2025
///
/// @file              stdio.h
///
/// @brief             Implementation of stdio.h for NanoOs C programs.
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

#ifndef STDIO_H
#define STDIO_H

#ifdef __cplusplus
extern "C"
{
#endif

// Standard C includes
#include <stddef.h>
#include <string.h>

// NanoOs includes
#include "NanoOsSystemCalls.h"

/// @typedef FILE
///
/// @brief Opaque type that represents an open file for a process.
typedef struct FILE FILE;

/// @var stdin
///
/// @brief Implementation of the stdin pointer that's used to read input from a
/// process's input stream.
static FILE *stdin  = (FILE*) 0x1;

/// @var stdout
///
/// @brief Implementation of the stdout pointer that's used to write output to
/// a process's output stream.
static FILE *stdout = (FILE*) 0x2;

/// @var stderr
///
/// @brief Implementation of the stderr pointer that's used to write output to
/// a process's error stream.
static FILE *stderr = (FILE*) 0x3;

/// @def EOF
///
/// @brief Value returned by stdio functions to indicate that the end of the
/// specified file has been reached.
#define EOF -1

/// @fn size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
///
/// @brief Implementation of the standard C fwrite function.  Writes the
/// the specified number of bytes from the provided pointer to the provided
/// file stream.
///
/// @param ptr A pointer to the data in memory to write to the file.
/// @param size The size, in bytes, of each element to write.
/// @param nmemb The total number of elements to write.
/// @param stream A pointer to the FILE object to write to.
///
/// @return Returns the number of objects successfully written to the file.
static inline size_t fwrite(
  const void *ptr, size_t size, size_t nmemb, FILE *stream
) {
  size_t numBytesWritten = 0;

  for (size_t totalBytes = size * nmemb; totalBytes > 0; ) {
    size_t numBytesToWrite
      = (totalBytes > NANO_OS_MAX_READ_WRITE_LENGTH)
      ? NANO_OS_MAX_READ_WRITE_LENGTH
      : totalBytes;

    // Write to stdout using syscall
    register FILE *a0 asm("a0") = stream;              // file pointer
    register const char *a1 asm("a1") = ptr;           // buffer address
    register int a2 asm("a2") = numBytesToWrite;       // length
    register int a7 asm("a7") = NANO_OS_SYSCALL_WRITE; // write syscall

    asm volatile(
      "ecall"
      : "+r"(a0)
      : "r"(a1), "r"(a2), "r"(a7)
    );

    numBytesWritten += numBytesToWrite;
    totalBytes -= numBytesToWrite;
  }
  
  return numBytesWritten / size;
}

/// @fn int fputs(const char *s, FILE *stream)
///
/// @brief Implementation of the standard C fputs function.  Prints the provided
/// string to the provided FILE stream.
///
/// @param s A pointer to the C string to print.
/// @param stream A pointer to the FILE stream to direct the string to.
///
/// @return Returns the number of characters written on success, EOF on failure.
static inline int fputs(const char *s, FILE *stream) {
  // Calculate string length
  size_t length = strlen(s);
  size_t numBytesWritten = fwrite(s, 1, length, stream);
  
  return (int) numBytesWritten;
}

/// @fn size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
///
/// @brief Implementation of the standard C fread function.  Reads up to the
/// the specified number of bytes into the provided pointer from the provided
/// file stream.
///
/// @param ptr A pointer to the data in memory to read from the file.
/// @param size The size, in bytes, of each element to write.
/// @param nmemb The total number of elements to write.
/// @param stream A pointer to the FILE object to read from.
///
/// @return Returns the number of objects successfully read from the file.
static inline size_t fread(
  void *ptr, size_t size, size_t nmemb, FILE *stream
) {
  size_t numBytesRead = 0;

  for (size_t totalBytes = size * nmemb; totalBytes > 0; ) {
    size_t numBytesToRead
      = (totalBytes > NANO_OS_MAX_READ_WRITE_LENGTH)
      ? NANO_OS_MAX_READ_WRITE_LENGTH
      : totalBytes;

    // Write to stdout using syscall
    register uintptr_t a0 asm("a0") = (uintptr_t) stream; // file pointer
    register const char *a1 asm("a1") = ptr;              // buffer address
    register int a2 asm("a2") = numBytesToRead;           // length
    register int a7 asm("a7") = NANO_OS_SYSCALL_READ;     // write syscall

    asm volatile(
      "ecall"
      : "+r"(a0)
      : "r"(a1), "r"(a2), "r"(a7)
    );

    numBytesRead += a0;
    totalBytes -= a0;
    
    char numBytes[2] = {0};
    fputs("Read ", stdout);
    numBytes[0] = '0' + numBytesRead;
    fputs(numBytes, stdout);
    fputs(" bytes\n", stdout);

    // We only want to continue if we consumed our buffer this round
    if (a0 != numBytesToRead) {
      break;
    }
  }
  
  return numBytesRead / size;
}

/// @fn char *fgets(char *s, int size, FILE *stream)
///
/// @brief Get a string of input from a FILE stream.
///
/// @param s A pointer to the string to read input into.
/// @param size The total size of the string pointed to by s.  One fewer than
///   this number of bytes will be read, max.
/// @param stream A pointer to the FILE stream to read data from.
///
/// @return Returns the provided s parameter on success, NULL on failure.
static inline char *fgets(char *s, int size, FILE *stream) {
  char *returnValue = s;

  size_t numBytesRead = fread(s, 1, size - 1, stream);
  if (numBytesRead > 0) {
    s[numBytesRead] = '\0';
  } else {
    returnValue = NULL;
  }

  return returnValue;
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // STDIO_H

