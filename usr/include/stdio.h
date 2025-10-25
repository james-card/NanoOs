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

// Standard streams:
#define stdin \
  overlayMap.header.unixApi->stdin
#define stdout \
  overlayMap.header.unixApi->stdout
#define stderr \
  overlayMap.header.unixApi->stderr

// File operations:
#define fopen(pathname, mode) \
  overlayMap.header.unixApi->fopen(pathname, mode)
#define fclose(stream) \
  overlayMap.header.unixApi->fclose(stream)
#define remove(pathname) \
  overlayMap.header.unixApi->remove(pathname)
#define fseek(stream, offset, whence) \
  overlayMap.header.unixApi->fseek(stream, offset, whence)
#define fileno(stream) \
  overlayMap.header.unixApi->fileno(stream)

// Formatted I/O:
#define vsscanf(buffer, format, args) \
  overlayMap.header.unixApi->vsscanf(buffer, format, args)
#define sscanf(buffer, ...) \
  overlayMap.header.unixApi->sscanf(buffer, __VA_ARGS__)
#define vfscanf(stream, format, ap) \
  overlayMap.header.unixApi->vfscanf(stream, format, ap)
#define fscanf(stream, ...) \
  overlayMap.header.unixApi->fscanf(stream, __VA_ARGS__)
#define scanf(...) \
  overlayMap.header.unixApi->scanf(__VA_ARGS__)
#define vfprintf(stream, format, args) \
  overlayMap.header.unixApi->vfprintf(stream, format, args)
#define fprintf(stream, ...) \
  overlayMap.header.unixApi->fprintf(stream, __VA_ARGS__)
#define printf(...) \
  overlayMap.header.unixApi->printf(__VA_ARGS__)
#define vsprintf(str, format, ap) \
  overlayMap.header.unixApi->vsprintf(str, format, ap)
#define vsnprintf(str, size, format, ap) \
  overlayMap.header.unixApi->vsnprintf(str, size, format, ap)
#define sprintf(str, ...) \
  overlayMap.header.unixApi->sprintf(str, __VA_ARGS__)
#define snprintf(str, size, ...) \
  overlayMap.header.unixApi->snprintf(str, size, __VA_ARGS__)

// Character I/O:
#define fputs(s, stream) \
  overlayMap.header.unixApi->fputs(s, stream)
#define puts(s) \
  overlayMap.header.unixApi->puts(s)
#define fgets(buffer, size, stream) \
  overlayMap.header.unixApi->fgets(buffer, size, stream)

// Direct I/O:
#define fread(ptr, size, nmemb, stream) \
  overlayMap.header.unixApi->fread(ptr, size, nmemb, stream)
#define fwrite(ptr, size, nmemb, stream) \
  overlayMap.header.unixApi->fwrite(ptr, size, nmemb, stream)

#endif // STDIO_H

