///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              02.16.2025
///
/// @file              NanoOsExe.h
///
/// @brief             Library for interacting with NanoOs executable files.
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

#ifndef NANO_OS_EXE_H
#define NANO_OS_EXE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// All offsets are relative to the *END* of the file, i.e. the number of bytes
// backward from the end.  All values are 32-bit values.  Only the signature
// offset and version offset are defined.  All other offsets are version-
// specific.

/// @def NANO_OS_EXE_SIGNATURE_OFFSET
///
/// @brief The number of bytes from the end of the file where the executable
/// signature is found.
#define NANO_OS_EXE_SIGNATURE_OFFSET 4

/// @def NANO_OS_EXE_VERSION_OFFSET
///
/// @brief The number of bytes from the end of the file where the executable
/// version is found.
#define NANO_OS_EXE_VERSION_OFFSET   8

/// @struct NanoOsExeMetadata
///
/// @brief Metadata elements contained in a NanoOs executable file.
///
/// @param programLength The length, in bytes, of the program segment of the
///   executable.
/// @param dataLength The length, in bytes, of the data segment of the
///   executable.
typedef struct NanoOsExeMetadata {
  uint32_t programLength;
  uint32_t dataLength;
} NanoOsExeMetadata;

/// @def nanoOsExeMetadataProgramLength
///
/// @brief Define to get the length of the program segment of a NanoOs
/// executable.
#define nanoOsExeMetadataProgramLength(nanoOsExeMetadata) \
  (nanoOsExeMetadata)->programLength

/// @def nanoOsExeMetadataDataLength
///
/// @brief Define to get the length of the data segment of a NanoOs executable.
#define nanoOsExeMetadataDataLength(nanoOsExeMetadata) \
  (nanoOsExeMetadata)->dataLength

// Function prototypes
NanoOsExeMetadata* nanoOsExeMetadataRead(const char *exePath);
NanoOsExeMetadata* nanoOsExeMetadataDestroy(
  NanoOsExeMetadata *nanoOsExeMetadata);
int nanoOsExeMetadataV1Write(const char *exePath, const char *binPath);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NANO_OS_EXE_H
