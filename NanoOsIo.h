///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              12.02.2024
///
/// @file              Filesystem.h
///
/// @brief             Common filesystem functionality for NanoOs.
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

#ifndef NANO_OS_IO_H
#define NANO_OS_IO_H

// Custom includes
#include "NanoOs.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Standard seek mode definitions
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/// @struct NanoOsIoIoCommandParameters
///
/// @brief Parameters needed for an I/O command in a filesystem.
///
/// @param file A pointer to the FILE object returned from a call to fopen.
/// @param buffer A pointer to the memory that is either to be read from or
///   written to.
/// @param length The number of bytes to read into the buffer or write from the
///   buffer.
typedef struct NanoOsIoIoCommandParameters {
  FILE *file;
  void *buffer;
  uint32_t length;
} NanoOsIoIoCommandParameters;

/// @struct NanoOsIoSeekParameters
///
/// @brief Parameters needed for an fseek function call on a file.
///
/// @param stream A pointer to the FILE object to adjust the position indicator
///   of.
/// @param offset The offset to apply to the position specified by the whence
///   parameter.
/// @param whence The position the offset is understood to be relative to.  One
///   of SEEK_SET (beginning of the file), SEEK_CUR (the current position of
///   the file) or SEEK_END (the end of the file).
typedef struct NanoOsIoSeekParameters {
  FILE *stream;
  long offset;
  int whence;
} NanoOsIoSeekParameters;

/// @struct FcopyArgs
///
/// @brief Structure for holding the arguments needed for an fcopy call.  See
/// that function for descriptions of these member variables.
typedef struct FcopyArgs {
  FILE *srcFile;
  off_t srcStart;
  FILE *dstFile;
  off_t dstStart;
  size_t length;
} FcopyArgs;

/// @enum NanoOsIoCommandResponse
///
/// @brief Commands and responses understood by the NanoOs I/O inter-process
/// message handler.
typedef enum NanoOsIoCommandResponse {
  // Commands:
  NANO_OS_IO_OPEN_FILE,
  NANO_OS_IO_CLOSE_FILE,
  NANO_OS_IO_READ_FILE,
  NANO_OS_IO_WRITE_FILE,
  NANO_OS_IO_REMOVE_FILE,
  NANO_OS_IO_SEEK_FILE,
  NANO_OS_IO_COPY_FILE,
  NUM_NANO_OS_IO_COMMANDS,
  // Responses:
} NanoOsIoCommandResponse;

/// @struct FilesystemState
///
/// @brief State metadata the filesystem process uses to provide access to
/// files.
///
/// @param partitionNumber The one-based partition index to use for filesystem
///   access.
/// @param blockSize The size of a block as it is known to the filesystem.
/// @param blockBuffer A pointer to the read/write buffer that is blockSize
///   bytes in length.
/// @param startLba The address of the first block of the filesystem.
/// @param endLba The address of the last block of the filesystem.
/// @param numOpenFiles The number of files currently open by the filesystem.
///   If this number is zero then the blockBuffer pointer may be NULL.
typedef struct FilesystemState {
  uint8_t partitionNumber;
  uint16_t blockSize;
  uint8_t *blockBuffer;
  uint32_t startLba;
  uint32_t endLba;
  uint8_t  numOpenFiles;
} FilesystemState;

// Exported functionality
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

#endif // NANO_OS_IO_H
