///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              01.26.2025
///
/// @file              WasiVm.h
///
/// @brief             Infrastructure to running WASI-compiled programs in a
///                    virtual machine.
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

#ifndef WASI_VM_H
#define WASI_VM_H

// Custom includes
#include "WasmVm.h"

#ifdef __cplusplus
extern "C"
{
#endif

// WASI rights flags
#define __WASI_RIGHT_FD_DATASYNC             (1ULL << 0)
#define __WASI_RIGHT_FD_READ                 (1ULL << 1)
#define __WASI_RIGHT_FD_SEEK                 (1ULL << 2)
#define __WASI_RIGHT_FD_FDSTAT_SET_FLAGS     (1ULL << 3)
#define __WASI_RIGHT_FD_SYNC                 (1ULL << 4)
#define __WASI_RIGHT_FD_TELL                 (1ULL << 5)
#define __WASI_RIGHT_FD_WRITE                (1ULL << 6)
#define __WASI_RIGHT_FD_ADVISE               (1ULL << 7)
#define __WASI_RIGHT_FD_ALLOCATE             (1ULL << 8)
#define __WASI_RIGHT_PATH_CREATE_DIRECTORY   (1ULL << 9)
#define __WASI_RIGHT_PATH_CREATE_FILE        (1ULL << 10)
#define __WASI_RIGHT_PATH_LINK_SOURCE        (1ULL << 11)
#define __WASI_RIGHT_PATH_LINK_TARGET        (1ULL << 12)
#define __WASI_RIGHT_PATH_OPEN               (1ULL << 13)
#define __WASI_RIGHT_FD_READDIR              (1ULL << 14)
#define __WASI_RIGHT_PATH_READLINK           (1ULL << 15)
#define __WASI_RIGHT_PATH_RENAME_SOURCE      (1ULL << 16)
#define __WASI_RIGHT_PATH_RENAME_TARGET      (1ULL << 17)
#define __WASI_RIGHT_PATH_FILESTAT_GET       (1ULL << 18)
#define __WASI_RIGHT_PATH_FILESTAT_SET_SIZE  (1ULL << 19)
#define __WASI_RIGHT_PATH_FILESTAT_SET_TIMES (1ULL << 20)
#define __WASI_RIGHT_FD_FILESTAT_GET         (1ULL << 21)
#define __WASI_RIGHT_FD_FILESTAT_SET_SIZE    (1ULL << 22)
#define __WASI_RIGHT_FD_FILESTAT_SET_TIMES   (1ULL << 23)
#define __WASI_RIGHT_PATH_SYMLINK            (1ULL << 24)
#define __WASI_RIGHT_PATH_REMOVE_DIRECTORY   (1ULL << 25)
#define __WASI_RIGHT_PATH_UNLINK_FILE        (1ULL << 26)
#define __WASI_RIGHT_POLL_FD_READWRITE       (1ULL << 27)
#define __WASI_RIGHT_SOCK_SHUTDOWN           (1ULL << 28)

// File descriptor types
#define __WASI_FILETYPE_UNKNOWN                         0
#define __WASI_FILETYPE_BLOCK_DEVICE                    1
#define __WASI_FILETYPE_CHARACTER_DEVICE                2
#define __WASI_FILETYPE_DIRECTORY                       3
#define __WASI_FILETYPE_REGULAR_FILE                    4
#define __WASI_FILETYPE_SOCKET_DGRAM                    5
#define __WASI_FILETYPE_SOCKET_STREAM                   6
#define __WASI_FILETYPE_SYMBOLIC_LINK                   7

// File descriptor flags
#define __WASI_FDFLAGS_APPEND                    (1 << 0)
#define __WASI_FDFLAGS_DSYNC                     (1 << 1)
#define __WASI_FDFLAGS_NONBLOCK                  (1 << 2)
#define __WASI_FDFLAGS_RSYNC                     (1 << 3)
#define __WASI_FDFLAGS_SYNC                      (1 << 4)

/// @struct WasiFdEntry
///
/// @brief Structure representing a WASI file descriptor entry.
///
/// @param hostFd The underlying host file descriptor.
/// @param type File type (regular, directory, etc.).
/// @param flags Flags for the file descriptor.
/// @param rights Base rights for the file descriptor.
/// @param rightsInherit Any inheriting rights.
/// @param offset The current offset into the file.
/// @param isPreopened Whether or not this is a preopened file descriptor.
typedef struct WasiFdEntry {
  int8_t       hostFd;
  uint8_t      type;
  uint16_t     flags;
  uint32_t     rights;
  uint32_t     rightsInherit;
  uint32_t     offset;
  bool         isPreopened;
} WasiFdEntry;

/// @struct WasiFdTable
///
/// @brief Table of file descriptors for a WASI process.
///
/// @param entries The array of file descriptor entries.
/// @param maxFds The maximum number of file descriptors in the table.
/// @param nextFreeFd The next available file descriptor number.
typedef struct WasiFdTable {
  WasiFdEntry *entries;
  uint8_t      maxFds;
  uint8_t      nextFreeFd;
} WasiFdTable;

/// @struct WasiVm
///
/// @brief State structure for WASI-compiled WASM process.
///
/// @param wasmVm WasmVm base member variable that holds the state for any
///   generic WASM process.
/// @param wasiFdTable The table of file descriptors that are available for use
///   by the WASI process.
typedef struct WasiVm {
  WasmVm      wasmVm;
  WasiFdTable wasiFdTable;
} WasiVm;


int wasiVmMain(int argc, char **argv);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WASI_VM_H
