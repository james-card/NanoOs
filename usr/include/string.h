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
  overlayMap.header.stdCApi->memcpy(dest, src, n)
#define memmove(dest, src, n) \
  overlayMap.header.stdCApi->memmove(dest, src, n)
#define strcpy(dest, src) \
  overlayMap.header.stdCApi->strcpy(dest, src)
#define strncpy(dest, src, dsize) \
  overlayMap.header.stdCApi->strncpy(dest, src, dsize)
#define strcat(dest, src) \
  overlayMap.header.stdCApi->strcat(dest, src)
#define strncat(dest, src, ssize) \
  overlayMap.header.stdCApi->strncat(dest, src, ssize)

// Comparison functions:
#define memcmp(dest, src, n) \
  overlayMap.header.stdCApi->memcmp(dest, src, n)
#define strcmp(dest, src) \
  overlayMap.header.stdCApi->strcmp(dest, src)
#define strncmp(dest, src, n) \
  overlayMap.header.stdCApi->strncmp(dest, src, n)

// Miscellaaneous string functions:
#define memset(s, c, n) \
  overlayMap.header.stdCApi->memset(s, c, n)
#define strerror(errnum) \
  overlayMap.header.stdCApi->strerror(errnum)
#define strlen(s) \
  overlayMap.header.stdCApi->strlen(s)

#endif // STRING_H

