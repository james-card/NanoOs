///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              09.07.2025
///
/// @file              ext4.c
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
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
/// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///                                James Card
///                         http://www.jamescard.org
///
///////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdlib.h>
#include "Ext4Filesystem.h"
#include "NanoOs.h"
#include "Filesystem.h"

//
// Constants and Enums
//

#define EXT4_SUPERBLOCK_OFFSET 1024
#define EXT4_MAGIC 0xEF53
#define EXT4_ROOT_INO 2

// Open modes
typedef enum Ext4OpenMode {
    Ext4ModeRead      = 1 << 0,
    Ext4ModeWrite     = 1 << 1,
    Ext4ModeAppend    = 1 << 2,
    Ext4ModeCreate    = 1 << 3,
} Ext4OpenMode;

// Inode types
#define EXT4_S_IFREG  0x8000  // regular file
#define EXT4_S_IFDIR  0x4000  // directory

//
// Packed data structures
//

// __attribute__((packed)) is used to prevent the compiler from adding padding
// between struct members, ensuring the C struct layout matches the on-disk format.

typedef struct __attribute__((packed)) {
    uint32_t sInodesCount;
    uint32_t sBlocksCountLo;
    uint32_t sRBlocksCountLo;
    uint32_t sFreeBlocksCountLo;
    uint32_t sFreeInodesCount;
    uint32_t sFirstDataBlock;
    uint32_t sLogBlockSize;
    uint32_t sLogClusterSize;
    uint32_t sBlocksPerGroup;
    uint32_t sClustersPerGroup;
    uint32_t sInodesPerGroup;
    uint32_t sMtime;
    uint32_t sWtime;
    uint16_t sMntCount;
    uint16_t sMaxMntCount;
    uint16_t sMagic;
    // ... other superblock fields ...
} Ext4Superblock;

typedef struct __attribute__((packed)) {
    uint32_t bgBlockBitmapLo;
    uint32_t bgInodeBitmapLo;
    uint32_t bgInodeTableLo;
    uint16_t bgFreeBlocksCountLo;
    uint16_t bgFreeInodesCountLo;
    // ... other group descriptor fields ...
} Ext4GroupDesc;

typedef struct __attribute__((packed)) {
    uint16_t iMode;
    uint16_t iUid;
    uint32_t iSizeLo;
    uint32_t iAtime;
    uint32_t iCtime;
    uint32_t iMtime;
    uint32_t iDtime;
    uint16_t iGid;
    uint16_t iLinksCount;
    uint32_t iBlocksLo;
    uint32_t iFlags;
    uint32_t iBlock[15]; // Direct, indirect, double-indirect, triple-indirect
    // ... other inode fields ...
} Ext4Inode;

typedef struct __attribute__((packed)) {
    uint32_t inode;
    uint16_t recLen;
    uint8_t  nameLen;
    uint8_t  fileType;
    char     name[];
} Ext4DirEntry;


// Filesystem state structure
typedef struct Ext4State {
    FilesystemState *fsState;
    Ext4Superblock superblock;
    Ext4GroupDesc *groupDescs;
    uint32_t numBlockGroups;
    Ext4FileHandle *openFiles;
} Ext4State;


//
// Private utility functions
//

static int readBlock(Ext4State *state, uint32_t blockNum, void *buffer) {
    return state->fsState->blockDevice->readBlocks(
        state->fsState->blockDevice->context,
        state->fsState->startLba + blockNum,
        1,
        state->fsState->blockSize,
        (uint8_t*) buffer
    );
}

static int writeBlock(Ext4State *state, uint32_t blockNum, const void *buffer) {
    return state->fsState->blockDevice->writeBlocks(
        state->fsState->blockDevice->context,
        state->fsState->startLba + blockNum,
        1,
        state->fsState->blockSize,
        (uint8_t*) buffer
    );
}

static int readInode(Ext4State *state, uint32_t inodeNum, Ext4Inode *inode) {
    uint32_t inodesPerGroup;
    readBytes(&inodesPerGroup, &state->superblock.sInodesPerGroup);
    
    uint32_t group = (inodeNum - 1) / inodesPerGroup;
    uint32_t index = (inodeNum - 1) % inodesPerGroup;

    uint32_t inodeTableBlock;
    readBytes(&inodeTableBlock, &state->groupDescs[group].bgInodeTableLo);
    
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint32_t inodeSize = 256; // Assuming 256-byte inodes
    uint32_t offset = index * inodeSize;

    uint32_t block = inodeTableBlock + (offset / blockSize);
    uint32_t offsetInBlock = offset % blockSize;
    
    if (readBlock(state, block, state->fsState->blockBuffer) != 0) {
        return -1;
    }

    readBytes(inode, state->fsState->blockBuffer + offsetInBlock);
    return 0;
}

// ... other helper functions like writeInode, findFreeInode, etc. would go here ...

static uint32_t pathToInode(Ext4State *state, const char *pathname) {
    (void) state;

    if (strcmp(pathname, "/") == 0) {
        return EXT4_ROOT_INO;
    }
    // This is a simplified implementation. A full implementation would parse the path.
    // For now, we only support root.
    return 0; 
}


//
// Public API Functions
//

int ext4Mount(FilesystemState *fsState, Ext4State **statePtr) {
    *statePtr = (Ext4State*)malloc(sizeof(Ext4State));
    if (!*statePtr) return -1; // Out of memory
    Ext4State* state = *statePtr;

    memset(state, 0, sizeof(Ext4State));
    state->fsState = fsState;

    if (fsState->numOpenFiles == 0) {
        fsState->blockBuffer = (uint8_t*)malloc(fsState->blockSize);
        if (!fsState->blockBuffer) {
            free(state);
            *statePtr = NULL;
            return -1;
        }
    }
    
    // Read superblock
    // This assumes lseek and read are available, which may not be true in NanoOs.
    // A proper implementation would use the block device functions.
    // For this example, we'll read block 0 and find the superblock.
    if (readBlock(state, 0, state->fsState->blockBuffer) != 0) {
         if (fsState->numOpenFiles == 0) {
            free(fsState->blockBuffer);
            fsState->blockBuffer = NULL;
        }
        free(state);
        *statePtr = NULL;
        return -1;
    }
    readBytes(&state->superblock, state->fsState->blockBuffer + EXT4_SUPERBLOCK_OFFSET);

    uint16_t magic;
    readBytes(&magic, &state->superblock.sMagic);
    if (magic != EXT4_MAGIC) {
        if (fsState->numOpenFiles == 0) {
            free(fsState->blockBuffer);
            fsState->blockBuffer = NULL;
        }
        free(state);
        *statePtr = NULL;
        return -1; // Not an ext4 filesystem
    }

    uint32_t blocksCount;
    readBytes(&blocksCount, &state->superblock.sBlocksCountLo);
    uint32_t blocksPerGroup;
    readBytes(&blocksPerGroup, &state->superblock.sBlocksPerGroup);

    state->numBlockGroups = (blocksCount + blocksPerGroup - 1) / blocksPerGroup;
    uint32_t gdtSize = state->numBlockGroups * sizeof(Ext4GroupDesc);
    state->groupDescs = (Ext4GroupDesc*)malloc(gdtSize);
    if (!state->groupDescs) {
        // ... cleanup ...
        if (fsState->numOpenFiles == 0) {
            free(fsState->blockBuffer);
            fsState->blockBuffer = NULL;
        }
        free(state);
        *statePtr = NULL;
        return -1;
    }

    // Read Group Descriptor Table
    // Simplified: assumes GDT is in one block
    uint32_t firstDataBlock;
    readBytes(&firstDataBlock, &state->superblock.sFirstDataBlock);
    if (readBlock(state, firstDataBlock + 1, state->groupDescs) !=0) {
        // ... cleanup ...
        free(state->groupDescs);
         if (fsState->numOpenFiles == 0) {
            free(fsState->blockBuffer);
            fsState->blockBuffer = NULL;
        }
        free(state);
        *statePtr = NULL;
        return -1;
    }

    return 0;
}

int ext4Unmount(Ext4State *state) {
    if (!state) return -1;
    
    // Ensure all files are closed
    while (state->openFiles) {
        ext4CloseFile(state->openFiles);
    }
    
    free(state->groupDescs);
    
    if (state->fsState->numOpenFiles == 0 && state->fsState->blockBuffer) {
        free(state->fsState->blockBuffer);
        state->fsState->blockBuffer = NULL;
    }
    
    free(state);
    return 0;
}


Ext4FileHandle* ext4OpenFile(Ext4State *state, const char *pathname, const char *mode) {
    uint8_t openMode = 0;
    if (strchr(mode, 'r')) openMode |= Ext4ModeRead;
    if (strchr(mode, 'w')) openMode |= Ext4ModeWrite | Ext4ModeCreate;
    if (strchr(mode, 'a')) openMode |= Ext4ModeAppend | Ext4ModeCreate;

    uint32_t inodeNum = pathToInode(state, pathname);

    if (inodeNum == 0 && !(openMode & Ext4ModeCreate)) {
        return NULL; // File not found
    }

    if (inodeNum == 0) {
        // Create file (not implemented in this stub)
        return NULL;
    }

    Ext4FileHandle *handle = (Ext4FileHandle*)malloc(sizeof(Ext4FileHandle));
    if (!handle) return NULL;

    handle->inodeNum = inodeNum;
    handle->state = state;
    handle->mode = openMode;

    if (openMode & Ext4ModeAppend) {
        Ext4Inode inode;
        readInode(state, inodeNum, &inode);
        uint32_t size;
        readBytes(&size, &inode.iSizeLo);
        handle->pos = size;
    } else {
        handle->pos = 0;
    }
    
    handle->next = state->openFiles;
    state->openFiles = handle;

    state->fsState->numOpenFiles++;
    
    return handle;
}

int ext4CloseFile(Ext4FileHandle *handle) {
    if (!handle) return -1;

    Ext4State *state = handle->state;
    Ext4FileHandle **p = &state->openFiles;
    while (*p && *p != handle) {
        p = &(*p)->next;
    }
    if (*p) {
        *p = handle->next;
    }

    free(handle);
    state->fsState->numOpenFiles--;

    if (state->fsState->numOpenFiles == 0) {
        if (state->fsState->blockBuffer) {
            free(state->fsState->blockBuffer);
            state->fsState->blockBuffer = NULL;
        }
    }

    return 0;
}

int32_t ext4ReadFile(Ext4FileHandle *handle, void *buffer, uint32_t length) {
    if (!handle || !(handle->mode & Ext4ModeRead)) {
        return -1;
    }
    
    Ext4Inode inode;
    if (readInode(handle->state, handle->inodeNum, &inode) != 0) {
        return -1;
    }

    uint32_t fileSize;
    readBytes(&fileSize, &inode.iSizeLo);

    if (handle->pos >= fileSize) {
        return 0; // EOF
    }

    if (handle->pos + length > fileSize) {
        length = fileSize - handle->pos;
    }
    
    // Simplified read from direct blocks only
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &handle->state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint32_t startBlockIdx = handle->pos / blockSize;
    uint32_t endBlockIdx = (handle->pos + length - 1) / blockSize;
    
    uint32_t bytesRead = 0;
    for (uint32_t i = startBlockIdx; i <= endBlockIdx && i < 12; ++i) { // Only direct blocks
        uint32_t blockNum;
        readBytes(&blockNum, &inode.iBlock[i]);
        if(blockNum == 0) break; // Hole in file

        if (readBlock(handle->state, blockNum, handle->state->fsState->blockBuffer) != 0) {
            return -1; // read error
        }
        
        uint32_t offsetInBlock = (i == startBlockIdx) ? handle->pos % blockSize : 0;
        uint32_t toRead = blockSize - offsetInBlock;
        if(bytesRead + toRead > length) {
            toRead = length - bytesRead;
        }

        memcpy((char*)buffer + bytesRead, handle->state->fsState->blockBuffer + offsetInBlock, toRead);
        bytesRead += toRead;
    }

    handle->pos += bytesRead;
    return bytesRead;
}

int32_t ext4WriteFile(Ext4FileHandle *handle, const void *buffer, uint32_t length) {
    (void) buffer;
    (void) length;

    if (!handle || !(handle->mode & Ext4ModeWrite)) {
        return -1;
    }
    // Writing logic is complex and involves block allocation. This is a stub.
    return -1; // Not implemented
}

int ext4RemoveFile(Ext4State *state, const char *pathname) {
    (void) state;
    (void) pathname;

    // File removal logic is complex. This is a stub.
    return -1; // Not implemented
}

int ext4SeekFile(Ext4FileHandle *handle, int64_t offset, int whence) {
    if (!handle) return -1;

    Ext4Inode inode;
    if (readInode(handle->state, handle->inodeNum, &inode) != 0) {
        return -1;
    }
    uint32_t fileSize;
    readBytes(&fileSize, &inode.iSizeLo);
    
    int64_t newPos;

    switch (whence) {
        case SEEK_SET:
            newPos = offset;
            break;
        case SEEK_CUR:
            newPos = handle->pos + offset;
            break;
        case SEEK_END:
            newPos = fileSize + offset;
            break;
        default:
            return -1;
    }

    if (newPos < 0 || newPos > fileSize) { // Can't seek before start or beyond EOF for now
        return -1;
    }

    handle->pos = newPos;
    return 0;
}

int ext4CreateDir(Ext4State *state, const char *pathname) {
    (void) state;
    (void) pathname;

    // Directory creation is complex. This is a stub.
    return -1; // Not implemented
}

