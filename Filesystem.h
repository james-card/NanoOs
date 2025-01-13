///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              12.02.2024
///
/// @file              Filesystem.h
///
/// @brief             Filesystem functionality for NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2024 James Card
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

/// @enum FilesystemCommandResponse
///
/// @brief Commands and responses understood by the filesystem inter-process
/// message handler.
typedef enum FilesystemCommandResponse {
  // Commands:
  FILESYSTEM_OPEN_FILE,
  // Responses:
  NUM_FILESYSTEM_COMMANDS,
} FilesystemCommandResponse;

// Exported functionality
void* runFilesystem(void *args);

FILE* filesystemFOpen(const char *pathname, const char *mode);
#ifdef fopen
#undef fopen
#endif // fopen
#define fopen filesystemFOpen

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FILESYSTEM_H
