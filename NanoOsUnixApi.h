///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              09.01.2025
///
/// @file              NanoOsUnixApi.h
///
/// @brief             Functionality from Single Unix Specification API that is
///                    to be exported to user programs.
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

#ifndef NANO_OS_UNIX_API_H
#define NANO_OS_UNIX_API_H

#undef FILE

#define FILE C_FILE
#include <stdarg.h>
#include <stddef.h>
#undef FILE
#undef stdin
#undef stdout
#undef stderr

typedef struct NanoOsFile NanoOsFile;
#define FILE NanoOsFile

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct NanoOsUnixApi {
  // Standard streams:
  FILE *stdin;
  FILE *stdout;
  FILE *stderr;
  
  // File operations:
  FILE* (*fopen)(const char *pathname, const char *mode);
  int (*fclose)(FILE *stream);
  int (*remove)(const char *pathname);
  int (*fseek)(FILE *stream, long offset, int whence);
  
  // Formatted I/O:
  int (*vsscanf)(const char *buffer, const char *format, va_list args);
  int (*sscanf)(const char *buffer, const char *format, ...);
  int (*vfscanf)(FILE *stream, const char *format, va_list ap);
  int (*fscanf)(FILE *stream, const char *format, ...);
  int (*scanf)(const char *format, ...);
  int (*vfprintf)(FILE *stream, const char *format, va_list args);
  int (*fprintf)(FILE *stream, const char *format, ...);
  int (*printf)(const char *format, ...);
  int (*vsprintf)(char *str, const char *format, va_list ap);
  int (*vsnprintf)(char *str, size_t size, const char *format, va_list ap);
  int (*sprintf)(char *str, const char *format, ...);
  int (*snprintf)(char *str, size_t size, const char *format, ...);
  
  // Character I/O:
  int (*fputs)(const char *s, FILE *stream);
  int (*puts)(const char *s);
  char* (*fgets)(char *buffer, int size, FILE *stream);
  
  // Direct I/O:
  size_t (*fread)(void *ptr, size_t size, size_t nmemb, FILE *stream);
  size_t (*fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream);
  
  // Memory management:
  void (*free)(void *ptr);
  void* (*realloc)(void *ptr, size_t size);
  void* (*malloc)(size_t size);
  void* (*calloc)(size_t nmemb, size_t size);
  
  // Copying functions:
  void* (*memcpy)(void *dest, const void *src, size_t n);
  void* (*memmove)(void *dest, const void *src, size_t n);
  char* (*strcpy)(char *dst, const char *src);
  char* (*strncpy)(char *dst, const char *src, size_t dsize);
  char* (*strcat)(char *dst, const char *src);
  char* (*strncat)(char *dst, const char *src, size_t ssize);
  
  // Search functions:
  int (*memcmp)(const void *s1, const void *s2, size_t n);
  int (*strcmp)(const char *s1, const char *s2);
  int (*strncmp)(const char *s1, const char *s2, size_t n);
  char* (*strstr)(const char *haystack, const char *needle);
  char* (*strchr)(const char *s, int c);
  char* (*strrchr)(const char *s, int c);
  size_t (*strspn)(const char *s, const char *accept);
  size_t (*strcspn)(const char *s, const char *reject);
  
  // Miscellaaneous string functions:
  void* (*memset)(void *s, int c, size_t n);
  char* (*strerror)(int errnum);
  size_t (*strlen)(const char *s);
  
  // Other stdlib functions:
  char* (*getenv)(const char *name);
  
  // unistd functions:
  int (*gethostname)(char *name, size_t len);
  int (*sethostname)(const char *name, size_t len);
  
  // errno functions:
  int* (*errno_)(void);
} NanoOsUnixApi;

extern NanoOsUnixApi nanoOsUnixApi;

#ifdef __cplusplus
}
#endif

#endif // NANO_OS_UNIX_API_H

