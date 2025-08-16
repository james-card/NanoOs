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

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

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

/// @struct FilesystemState
///
/// @brief State metadata the filesystem process uses to provide access to
/// files.
///
/// @param blockDevice A pointer to an allocated and initialized
///   BlockStorageDevice to use for reading and writing blocks.
/// @param blockSize The size of a block as it is known to the filesystem.
/// @param blockBuffer A pointer to the read/write buffer that is blockSize
///   bytes in length.
/// @param startLba The address of the first block of the filesystem.
/// @param endLba The address of the last block of the filesystem.
/// @param numOpenFiles The number of files currently open by the filesystem.
///   If this number is zero then the blockBuffer pointer may be NULL.
typedef struct FilesystemState {
  BlockStorageDevice *blockDevice;
  uint16_t blockSize;
  uint8_t *blockBuffer;
  uint32_t startLba;
  uint32_t endLba;
  uint8_t  numOpenFiles;
} FilesystemState;

/// @struct FilesystemIoCommandParameters
///
/// @brief Parameters needed for an I/O command in a filesystem.
///
/// @param file A pointer to the FILE object returned from a call to fopen.
/// @param buffer A pointer to the memory that is either to be read from or
///   written to.
/// @param length The number of bytes to read into the buffer or write from the
///   buffer.
typedef struct FilesystemIoCommandParameters {
  FILE *file;
  void *buffer;
  uint32_t length;
} FilesystemIoCommandParameters;

/// @struct FilesystemSeekParameters
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
typedef struct FilesystemSeekParameters {
  FILE *stream;
  long offset;
  int whence;
} FilesystemSeekParameters;

/// @typedef FilesystemCommandHandler
///
/// @brief Definition of a filesystem command handler function.
typedef int (*FilesystemCommandHandler)(FilesystemState*, ProcessMessage*);

/// @enum FilesystemCommandResponse
///
/// @brief Commands and responses understood by the filesystem inter-process
/// message handler.
typedef enum FilesystemCommandResponse {
  // Commands:
  FILESYSTEM_OPEN_FILE,
  FILESYSTEM_CLOSE_FILE,
  FILESYSTEM_READ_FILE,
  FILESYSTEM_WRITE_FILE,
  FILESYSTEM_REMOVE_FILE,
  FILESYSTEM_SEEK_FILE,
  NUM_FILESYSTEM_COMMANDS,
  // Responses:
} FilesystemCommandResponse;

// Exported functionality
FILE* filesystemFOpen(const char *pathname, const char *mode);
#ifdef fopen
#undef fopen
#endif // fopen
#define fopen filesystemFOpen

int filesystemFClose(FILE *stream);
#ifdef fclose
#undef fclose
#endif // fclose
#define fclose filesystemFClose

int filesystemRemove(const char *pathname);
#ifdef remove
#undef remove
#endif // remove
#define remove filesystemRemove

int filesystemFSeek(FILE *stream, long offset, int whence);
#ifdef fseek
#undef feek
#endif // fseek
#define fseek filesystemFSeek

size_t filesystemFRead(void *ptr, size_t size, size_t nmemb, FILE *stream);
#ifdef fread
#undef fread
#endif // fread
#define fread filesystemFRead

size_t filesystemFWrite(
  const void *ptr, size_t size, size_t nmemb, FILE *stream);
#ifdef fwrite
#undef fwrite
#endif // fwrite
#define fwrite filesystemFWrite

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

#endif // FILESYSTEM_H
