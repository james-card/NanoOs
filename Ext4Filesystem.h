///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              09.07.2025
///
/// @file              ext4.h
///
/// @brief             Driver for the ext4 filesystem.
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

#ifndef EXT4_H
#define EXT4_H

#include "Filesystem.h"
#include "NanoOs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct Ext4State Ext4State;
typedef struct Ext4Inode Ext4Inode;

// Ext4 File Handle Structure
typedef struct Ext4FileHandle {
    uint32_t inodeNum;
    uint64_t pos;
    uint8_t mode;
    Ext4State *state;
    struct Ext4FileHandle *next;
} Ext4FileHandle;


// Driver Functions
int ext4Mount(FilesystemState *fsState, Ext4State **state);
int ext4Unmount(Ext4State *state);
Ext4FileHandle* ext4OpenFile(Ext4State *state, const char *pathname, const char *mode);
int ext4CloseFile(Ext4FileHandle *handle);
int32_t ext4ReadFile(Ext4FileHandle *handle, void *buffer, uint32_t length);
int32_t ext4WriteFile(Ext4FileHandle *handle, const void *buffer, uint32_t length);
int ext4RemoveFile(Ext4State *state, const char *pathname);
int ext4SeekFile(Ext4FileHandle *handle, int64_t offset, int whence);
int ext4CreateDir(Ext4State *state, const char *pathname);

#ifdef __cplusplus
}
#endif

#endif // EXT4_H

