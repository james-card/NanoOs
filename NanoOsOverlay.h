///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              09.01.2025
///
/// @file              NanoOsOverlay.h
///
/// @brief             Definitions used for exporting functionality of overlays
///                    so that they're accessible from the kernel.
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

#ifndef NANO_OS_OVERLAY_H
#define NANO_OS_OVERLAY_H

// Standard includes.
#include <stdint.h>

// Header from kernel space.
#include "NanoOsStdCApi.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// @def NANO_OS_OVERLAY_MAGIC
///
/// @brief Value used to validate that an overlay header is valid.
#define NANO_OS_OVERLAY_MAGIC 0x4c4f734f6f6e614e // "NanoOsOL"

/// @struct NanoOsOverlayExport
///
/// @brief Definition for a single function exported from an overlay.
///
/// @param name The string name of the overlay function.
/// @param fn A pointer to the function within the overlay.
typedef struct NanoOsOverlayExport {
  const char name[16];
  void* (*fn)(void*);
} NanoOsOverlayExport;

/// @struct NanoOsOverlayHeader
///
/// @brief The header used to export functionality within an overlay.
///
/// @param magic The value of NANO_OS_OVERLAY_MAGIC to identify that this is a
///   valid NanoOs overlay header.
/// @param version The version of the overlay header.  Format will be:
///   (major << 24) | (minor << 16) | (revision << 8) | (build << 0).
/// @param stdCApi A pointer to the NanoOsStdCApi structure in the kernel that
///   manages all the standard C API interfaces.
/// @param callOverlayFunction A pointer to the function that allows a function
///   in a different overlay to be called.
/// @param numExports The number of functions exported by the overlay.
typedef struct NanoOsOverlayHeader {
  uint64_t magic;         // Must be NANO_OS_OVERLAY_MAGIC
  uint32_t version;
  NanoOsStdCApi *stdCApi;
  void* (*callOverlayFunction)(void*);
  uint16_t numExports;
} NanoOsOverlayHeader;

/// @struct NanoOsOverlayMap
///
/// @brief The map of exported information from an overlay.
///
/// @param header An embedded NanoOsOverlayHeader structure.
/// @param exports Apointer to an array of NanoOsOverlayExport items exported by
///   an overlay.
typedef struct NanoOsOverlayMap {
  NanoOsOverlayHeader header;
  NanoOsOverlayExport *exports;
} NanoOsOverlayMap;

/// @struct MainArgs
///
/// @brief Structure to hold the standard arc/argv arguments to a C main
/// function.
///
/// @param argc The value of argc to be passed to a main function.
/// @param argv The value of argv to be passed to a main function.
typedef struct MainArgs {
  int argc;
  char **argv;
} MainArgs;

#ifdef __cplusplus
}
#endif

#endif // NANO_OS_OVERLAY_H

