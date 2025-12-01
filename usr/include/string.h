///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              09.05.2025
///
/// @file              string.h
///
/// @brief             Definitions used for accessing functionality in the C
///                    string library.
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

#ifndef STRING_H
#define STRING_H

#include "NanoOsUser.h"

// Copying functions:
#define memcpy(dest, src, n) \
  overlayMap.header.osApi->memcpy(dest, src, n)
#define memmove(dest, src, n) \
  overlayMap.header.osApi->memmove(dest, src, n)
#define strcpy(dest, src) \
  overlayMap.header.osApi->strcpy(dest, src)
#define strncpy(dest, src, dsize) \
  overlayMap.header.osApi->strncpy(dest, src, dsize)
#define strcat(dest, src) \
  overlayMap.header.osApi->strcat(dest, src)
#define strncat(dest, src, ssize) \
  overlayMap.header.osApi->strncat(dest, src, ssize)

// Search functions:
#define memcmp(dest, src, n) \
  overlayMap.header.osApi->memcmp(dest, src, n)
#define strcmp(dest, src) \
  overlayMap.header.osApi->strcmp(dest, src)
#define strncmp(dest, src, n) \
  overlayMap.header.osApi->strncmp(dest, src, n)
#define strstr(haystack, needle) \
  overlayMap.header.osApi->strstr(haystack, needle)
#define strchr(s, c) \
  overlayMap.header.osApi->strchr(s, c)
#define strrchr(s, c) \
  overlayMap.header.osApi->strrchr(s, c)
#define strspn(s, accept) \
  overlayMap.header.osApi->strspn(s, accept)
#define strcspn(s, reject) \
  overlayMap.header.osApi->strcspn(s, reject)

// Miscellaaneous string functions:
#define memset(s, c, n) \
  overlayMap.header.osApi->memset(s, c, n)
#define strerror(errnum) \
  overlayMap.header.osApi->strerror(errnum)
#define strlen(s) \
  overlayMap.header.osApi->strlen(s)

#endif // STRING_H

