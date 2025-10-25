///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              10.21.2025
///
/// @file              stdlib.h
///
/// @brief             Functionality in the C stdlib library.
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

#ifndef STDLIB_H
#define STDLIB_H

#include "NanoOsUser.h"

#define free(ptr) \
  overlayMap.header.unixApi->free(ptr)
#define realloc(ptr, size) \
  overlayMap.header.unixApi->realloc(ptr, size)
#define malloc(size) \
  overlayMap.header.unixApi->malloc(size)
#define calloc(nmemb, size) \
  overlayMap.header.unixApi->calloc(nmemb, size)
#define getenv(s) \
  overlayMap.header.unixApi->getenv(s)

#endif // STDLIB_H

