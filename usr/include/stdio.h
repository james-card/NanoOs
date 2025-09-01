///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              09.01.2025
///
/// @file              stdio.h
///
/// @brief             Definitions used for accessing functionality in the C
///                    stdio library.
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

#include "NanoOsUser.h"

// File operations:
#define fopen(pathname, mode) overlayMap->header.stdCApi->fopen(pathname, mode)
#define fclose(stream) overlayMap->header.stdCApi->fclose(stream)
#define remove(pathname) overlayMap->header.stdCApi->remove(pathname)
#define fseek(stream, offset, whence) \
  overlayMap->header.stdCApi->fseek(stream, offset, whence)

// Formatted I/O:
#define vsscanf(buffer, format, args) \
  overlayMap->header.stdCApi->vsscanf(buffer, format, args)
#define sscanf(buffer, format, ...) \
  overlayMap->header.stdCApi->sscanf(buffer, format, ##__VA_ARGS__)
#define vfscanf(stream, format, ap) \
  overlayMap->header.stdCApi->vfscanf(stream, format, ap)
#define fscanf(stream, format, ...) \
  overlayMap->header.stdCApi->fscanf(stream, format, ##__VA_ARGS__)
#define scanf(format, ...) \
  overlayMap->header.stdCApi->scanf(format, ##__VA_ARGS__)
#define vfprintf(stream, format, args) \
  overlayMap->header.stdCApi->vfprintf(stream, format, args)
#define fprintf(stream, format, ...) \
  overlayMap->header.stdCApi->fprintf(stream, format, ##__VA_ARGS__)
#define printf(format, ...) \
  overlayMap->header.stdCApi->printf(format, ##__VA_ARGS__)
#define vsprintf(str, format, ap) \
  overlayMap->header.stdCApi->vsprintf(str, format, ap)
#define vsnprintf(str, size, format, ap) \
  overlayMap->header.stdCApi->vsnprintf(str, size, format, ap)
#define sprintf(str, format, ...) \
  overlayMap->header.stdCApi->sprintf(str, format, ##__VA_ARGS__)
#define snprintf(str, size, format, ...) \
  overlayMap->header.stdCApi->snprintf(str, size, format, ##__VA_ARGS__)

// Character I/O:
#define fputs(s, stream) overlayMap->header.stdCApi->fputs(s, stream)
#define puts(s) overlayMap->header.stdCApi->puts(s)
#define fgets(buffer, size, stream) \
  overlayMap->header.stdCApi->fgets(buffer, size, stream)

#endif // STDIO_H

