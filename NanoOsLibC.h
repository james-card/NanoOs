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

/*
 * This file is included by Coroutines.h to provide functionality missing from
 * the Arduino C implementation and to provide some debugging tools when
 * debugging the Corutines library from NanoOs.
 */

#ifndef NANO_OS_LIB_C
#define NANO_OS_LIB_C

// Standard C includes
#define FILE C_FILE
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#undef FILE

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint32_t off_t;

#ifdef size_t
#undef size_t
#endif // size_t
#define size_t uint32_t

typedef struct NanoOsFile NanoOsFile;
#ifdef FILE
#undef FILE
#endif // FILE
#define FILE NanoOsFile

#ifdef stdin
#undef stdin
#endif // stdin
#define stdin nanoOsIoStdin

#ifdef stdout
#undef stdout
#endif // stdout
#define stdout nanoOsIoStdout

#ifdef stderr
#undef stderr
#endif // stderr
#define stderr nanoOsIoStderr

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


#define NANO_OS_DEBUG
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
#define EIO              6      /* I/O error */
#define ENOSPC           7      /* No space left on device */

typedef void TypeDescriptor;

#define typeString ((TypeDescriptor*) ((intptr_t)  1))
#define typeInt    ((TypeDescriptor*) ((intptr_t)  2))

#define STOP       ((void*) ((intptr_t) -1))

extern const char *boolNames[];
extern FILE *nanoOsIoStdin;
extern FILE *nanoOsIoStdout;
extern FILE *nanoOsIoStderr;

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
unsigned long getElapsedMilliseconds(unsigned long startTime);

// C-like functions
int vsscanf(const char *buffer, const char *format, va_list args);
int sscanf(const char *buffer, const char *format, ...);
char* strnchr(char *s, int c, size_t size);

char* nanoOsStrError(int errnum);
#ifdef strerror
#undef strerror
#endif
#define strerror nanoOsStrError

// Standard seek mode definitions
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// Exported IO functions

int nanoOsIoFPuts(const char *s, FILE *stream);
#ifdef fputs
#undef fputs
#endif
#define fputs nanoOsIoFPuts

int nanoOsIoPuts(const char *s);
#ifdef puts
#undef puts
#endif
#define puts nanoOsIoPuts

int nanoOsIoVFPrintf(FILE *stream, const char *format, va_list args);
#ifdef vfprintf
#undef vfprintf
#endif
#define vfprintf nanoOsIoVFPrintf

int nanoOsIoFPrintf(FILE *stream, const char *format, ...);
#ifdef fprintf
#undef fprintf
#endif
#define fprintf nanoOsIoFPrintf

int nanoOsIoPrintf(const char *format, ...);
#ifdef printf
#undef printf
#endif
#define printf nanoOsIoPrintf

char *nanoOsIoFGets(char *buffer, int size, FILE *stream);
#ifdef fgets
#undef fgets
#endif
#define fgets nanoOsIoFGets

int nanoOsIoVFScanf(FILE *stream, const char *format, va_list ap);
#ifdef vfscanf
#undef vfscanf
#endif
#define vfscanf nanoOsIoVFScanf

int nanoOsIoFScanf(FILE *stream, const char *format, ...);
#ifdef fscanf
#undef fscanf
#endif
#define fscanf nanoOsIoFScanf

int nanoOsIoScanf(const char *format, ...);
#ifdef scanf
#undef scanf
#endif
#define scanf nanoOsIoScanf

FILE* nanoOsIoFOpen(const char *pathname, const char *mode);
#ifdef fopen
#undef fopen
#endif // fopen
#define fopen nanoOsIoFOpen

int nanoOsIoFClose(FILE *stream);
#ifdef fclose
#undef fclose
#endif // fclose
#define fclose nanoOsIoFClose

int nanoOsIoRemove(const char *pathname);
#ifdef remove
#undef remove
#endif // remove
#define remove nanoOsIoRemove

int nanoOsIoFSeek(FILE *stream, long offset, int whence);
#ifdef fseek
#undef feek
#endif // fseek
#define fseek nanoOsIoFSeek

size_t nanoOsIoFRead(void *ptr, size_t size, size_t nmemb, FILE *stream);
#ifdef fread
#undef fread
#endif // fread
#define fread nanoOsIoFRead

size_t nanoOsIoFWrite(
  const void *ptr, size_t size, size_t nmemb, FILE *stream);
#ifdef fwrite
#undef fwrite
#endif // fwrite
#define fwrite nanoOsIoFWrite

long nanoOsIoFTell(FILE *stream);
#ifdef ftell
#undef ftell
#endif // ftell
#define ftell nanoOsIoFTell

size_t nanoOsIoFCopy(FILE *srcFile, off_t srcStart,
  FILE *dstFile, off_t dstStart, size_t length);

/// @def rewind
///
/// @brief Function macro to implement the functionality of the standard C
/// rewind function.
///
/// @param stream A pointer to a previously-opened FILE object.
#define rewind(stream) \
  (void) fseek(stream, 0L, SEEK_SET)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NANO_OS_LIB_C
