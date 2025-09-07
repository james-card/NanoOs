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

// Feature flags
#define EXT4_FEATURE_INCOMPAT_64BIT 0x0080

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
    int32_t  sLogClusterSize;
    uint32_t sBlocksPerGroup;
    uint32_t sClustersPerGroup;
    uint32_t sInodesPerGroup;
    uint32_t sMtime;
    uint32_t sWtime;
    uint16_t sMntCount;
    uint16_t sMaxMntCount;
    uint16_t sMagic;
    uint16_t sState;
    uint16_t sErrors;
    uint16_t sMinorRevLevel;
    uint32_t sLastcheck;
    uint32_t sCheckinterval;
    uint32_t sCreatorOs;
    uint32_t sRevLevel;
    uint16_t sDefResuid;
    uint16_t sDefResgid;
    uint32_t sFirstIno;
    uint16_t sInodeSize;
    uint16_t sBlockGroupNr;
    uint32_t sFeatureCompat;
    uint32_t sFeatureIncompat;
    uint32_t sFeatureRoCompat;
    uint8_t  sUuid[16];
    char     sVolumeName[16];
    char     sLastMounted[64];
    uint32_t sAlgorithmUsageBitmap;
    uint8_t  sPreallocBlocks;
    uint8_t  sPreallocDirBlocks;
    uint16_t sReservedGdtBlocks;
    uint8_t  sJournalUuid[16];
    uint32_t sJournalInum;
    uint32_t sJournalDev;
    uint32_t sLastOrphan;
    uint32_t sHashSeed[4];
    uint8_t  sDefHashVersion;
    uint8_t  sJnlBackupType;
    uint16_t sDescSize;
    uint32_t sDefaultMountOpts;
    uint32_t sFirstMetaBg;
    uint32_t sMkfsTime;
    uint32_t sJnlBlocks[17];
    uint32_t sBlocksCountHi;
    uint32_t sRBlocksCountHi;
    uint32_t sFreeBlocksCountHi;
    uint16_t sMinExtraIsize;
    uint16_t sWantExtraIsize;
    uint32_t sFlags;
    uint16_t sRaidStride;
    uint16_t sMmpInterval;
    uint64_t sMmpBlock;
    uint32_t sRaidStripeWidth;
    uint8_t  sLogGroupsPerFlex;
    uint8_t  sChecksumType;
    uint16_t sReservedPad;
    uint64_t sKbytesWritten;
    // ... other superblock fields ...
} Ext4Superblock;

typedef struct __attribute__((packed)) {
    uint32_t bgBlockBitmapLo;
    uint32_t bgInodeBitmapLo;
    uint32_t bgInodeTableLo;
    uint16_t bgFreeBlocksCountLo;
    uint16_t bgFreeInodesCountLo;
    uint16_t bgUsedDirsCountLo;
    uint16_t bgFlags;
    uint32_t bgExcludeBitmapLo;
    uint32_t bgBlockBitmapCsumLo;
    uint32_t bgInodeBitmapCsumLo;
    uint16_t bgItableUnusedLo;
    uint16_t bgChecksum;
    uint32_t bgBlockBitmapHi;
    uint32_t bgInodeBitmapHi;
    uint32_t bgInodeTableHi;
    uint16_t bgFreeBlocksCountHi;
    uint16_t bgFreeInodesCountHi;
    uint16_t bgUsedDirsCountHi;
    uint16_t bgItableUnusedHi;
    uint32_t bgExcludeBitmapHi;
    uint32_t bgBlockBitmapCsumHi;
    uint32_t bgInodeBitmapCsumHi;
    uint32_t bgReserved;
    // ... other group descriptor fields ...
} Ext4GroupDesc;

typedef struct __attribute__((packed)) Ext4Inode {
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
    uint32_t iGeneration;
    uint32_t iFileAclLo;
    uint32_t iSizeHigh;
    uint32_t iObsoFaddr;
    uint32_t iBlock[15]; // Direct, indirect, double-indirect, triple-indirect
    uint16_t iExtraIsize;
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
    bool is64bit;
    Ext4FileHandle *openFiles;
} Ext4State;


//
// Private utility functions
//

static int readBlock(Ext4State *state, uint64_t blockNum, void *buffer) {
    return state->fsState->blockDevice->readBlocks(
        state->fsState->blockDevice->context,
        state->fsState->startLba + blockNum,
        1,
        state->fsState->blockSize,
        (uint8_t*) buffer
    );
}

static int writeBlock(Ext4State *state, uint64_t blockNum, const void *buffer) {
    return state->fsState->blockDevice->writeBlocks(
        state->fsState->blockDevice->context,
        state->fsState->startLba + blockNum,
        1,
        state->fsState->blockSize,
        (const uint8_t*) buffer
    );
}

static uint64_t getInodeSize(Ext4State *state, Ext4Inode *inode) {
    uint32_t sizeLo, sizeHi;
    readBytes(&sizeLo, &inode->iSizeLo);
    if (state->is64bit) {
        readBytes(&sizeHi, &inode->iSizeHigh);
        return ((uint64_t)sizeHi << 32) | sizeLo;
    }
    return sizeLo;
}

static void setInodeSize(Ext4State *state, Ext4Inode *inode, uint64_t size) {
    uint32_t sizeLo = size & 0xFFFFFFFF;
    writeBytes(&inode->iSizeLo, &sizeLo);
    if (state->is64bit) {
        uint32_t sizeHi = size >> 32;
        writeBytes(&inode->iSizeHigh, &sizeHi);
    }
}


static int readInode(Ext4State *state, uint32_t inodeNum, Ext4Inode *inode) {
    uint32_t inodesPerGroup;
    readBytes(&inodesPerGroup, &state->superblock.sInodesPerGroup);
    
    uint32_t group = (inodeNum - 1) / inodesPerGroup;
    if (group >= state->numBlockGroups) return -1;
    uint32_t index = (inodeNum - 1) % inodesPerGroup;

    uint32_t inodeTableBlockLo;
    readBytes(&inodeTableBlockLo, &state->groupDescs[group].bgInodeTableLo);
    uint64_t inodeTableBlock = inodeTableBlockLo;
    if (state->is64bit) {
      uint32_t inodeTableBlockHi;
      readBytes(&inodeTableBlockHi, &state->groupDescs[group].bgInodeTableHi);
      inodeTableBlock |= ((uint64_t)inodeTableBlockHi << 32);
    }

    uint16_t inodeSize;
    readBytes(&inodeSize, &state->superblock.sInodeSize);
    uint32_t offset = index * inodeSize;
    
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;

    uint64_t block = inodeTableBlock + (offset / blockSize);
    uint32_t offsetInBlock = offset % blockSize;
    
    if (readBlock(state, block, state->fsState->blockBuffer) != 0) {
        return -1;
    }

    readBytes(inode, state->fsState->blockBuffer + offsetInBlock);
    return 0;
}

static int writeInode(Ext4State *state, uint32_t inodeNum, Ext4Inode *inode) {
    uint32_t inodesPerGroup;
    readBytes(&inodesPerGroup, &state->superblock.sInodesPerGroup);

    uint32_t group = (inodeNum - 1) / inodesPerGroup;
    if (group >= state->numBlockGroups) return -1;
    uint32_t index = (inodeNum - 1) % inodesPerGroup;
    
    uint32_t inodeTableBlockLo;
    readBytes(&inodeTableBlockLo, &state->groupDescs[group].bgInodeTableLo);
    uint64_t inodeTableBlock = inodeTableBlockLo;
    if (state->is64bit) {
      uint32_t inodeTableBlockHi;
      readBytes(&inodeTableBlockHi, &state->groupDescs[group].bgInodeTableHi);
      inodeTableBlock |= ((uint64_t)inodeTableBlockHi << 32);
    }
    
    uint16_t inodeSize;
    readBytes(&inodeSize, &state->superblock.sInodeSize);
    uint32_t offset = index * inodeSize;

    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    
    uint64_t block = inodeTableBlock + (offset / blockSize);
    uint32_t offsetInBlock = offset % blockSize;

    // Read the block first to avoid corrupting other inodes in the same block
    if (readBlock(state, block, state->fsState->blockBuffer) != 0) {
        return -1;
    }

    // Write the modified inode into the buffer
    writeBytes(state->fsState->blockBuffer + offsetInBlock, inode);

    // Write the modified block back to the device
    return writeBlock(state, block, state->fsState->blockBuffer);
}

static uint64_t inodeToBlock(Ext4State *state, Ext4Inode *inode, uint32_t fileBlockNum) {
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint32_t pointersPerBlock = blockSize / sizeof(uint32_t);

    // Direct blocks
    if (fileBlockNum < 12) {
        uint32_t blockNum;
        readBytes(&blockNum, &inode->iBlock[fileBlockNum]);
        return blockNum;
    }

    // Single-indirect block
    fileBlockNum -= 12;
    if (fileBlockNum < pointersPerBlock) {
        uint32_t indirectBlockNum;
        readBytes(&indirectBlockNum, &inode->iBlock[12]);
        if (indirectBlockNum == 0) return 0; // Hole

        if (readBlock(state, indirectBlockNum, state->fsState->blockBuffer) != 0) {
            return 0; // Read error
        }
        uint32_t blockNum;
        readBytes(&blockNum, state->fsState->blockBuffer + (fileBlockNum * sizeof(uint32_t)));
        return blockNum;
    }

    // Double-indirect block
    fileBlockNum -= pointersPerBlock;
    uint64_t pointersPerBlockSquared = (uint64_t)pointersPerBlock * pointersPerBlock;
    if (fileBlockNum < pointersPerBlockSquared) {
        uint32_t doubleIndirectBlockNum;
        readBytes(&doubleIndirectBlockNum, &inode->iBlock[13]);
        if (doubleIndirectBlockNum == 0) return 0; // Hole

        if (readBlock(state, doubleIndirectBlockNum, state->fsState->blockBuffer) != 0) {
            return 0; // Read error
        }
        uint32_t indirectBlockNum;
        uint32_t indirectBlockIndex = fileBlockNum / pointersPerBlock;
        readBytes(&indirectBlockNum, state->fsState->blockBuffer + (indirectBlockIndex * sizeof(uint32_t)));
        if (indirectBlockNum == 0) return 0; // Hole

        if (readBlock(state, indirectBlockNum, state->fsState->blockBuffer) != 0) {
            return 0; // Read error
        }
        uint32_t finalBlockIndex = fileBlockNum % pointersPerBlock;
        uint32_t blockNum;
        readBytes(&blockNum, state->fsState->blockBuffer + (finalBlockIndex * sizeof(uint32_t)));
        return blockNum;
    }
    
    // Triple-indirect block
    fileBlockNum -= pointersPerBlockSquared;
    uint32_t tripleIndirectBlockNum;
    readBytes(&tripleIndirectBlockNum, &inode->iBlock[14]);
    if (tripleIndirectBlockNum == 0) return 0; // Hole

    if (readBlock(state, tripleIndirectBlockNum, state->fsState->blockBuffer) != 0) {
        return 0; // Read error
    }
    uint32_t doubleIndirectBlockIndex = fileBlockNum / pointersPerBlockSquared;
    uint32_t doubleIndirectBlockNum;
    readBytes(&doubleIndirectBlockNum, state->fsState->blockBuffer + (doubleIndirectBlockIndex * sizeof(uint32_t)));
    if(doubleIndirectBlockNum == 0) return 0; // Hole

    if (readBlock(state, doubleIndirectBlockNum, state->fsState->blockBuffer) != 0) {
        return 0; // Read error
    }
    uint32_t indirectBlockIndex = (fileBlockNum / pointersPerBlock) % pointersPerBlock;
    uint32_t indirectBlockNum;
    readBytes(&indirectBlockNum, state->fsState->blockBuffer + (indirectBlockIndex * sizeof(uint32_t)));
    if(indirectBlockNum == 0) return 0; // Hole

    if (readBlock(state, indirectBlockNum, state->fsState->blockBuffer) != 0) {
        return 0; // Read error
    }
    uint32_t finalBlockIndex = fileBlockNum % pointersPerBlock;
    uint32_t blockNum;
    readBytes(&blockNum, state->fsState->blockBuffer + (finalBlockIndex * sizeof(uint32_t)));
    return blockNum;
}

static uint32_t findEntryInDir(Ext4State *state, uint32_t dirInodeNum, const char *name) {
    Ext4Inode dirInode;
    if (readInode(state, dirInodeNum, &dirInode) != 0) {
        return 0;
    }

    uint16_t mode;
    readBytes(&mode, &dirInode.iMode);
    if ((mode & EXT4_S_IFDIR) == 0) {
        return 0; // Not a directory
    }

    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    
    uint64_t dirSize = getInodeSize(state, &dirInode);
    uint32_t numBlocks = (dirSize + blockSize - 1) / blockSize;

    for (uint32_t i = 0; i < numBlocks; ++i) {
        uint64_t blockNum = inodeToBlock(state, &dirInode, i);
        if (blockNum == 0) continue; // Hole in directory file

        if (readBlock(state, blockNum, state->fsState->blockBuffer) != 0) {
            return 0; // Read error
        }

        uint32_t offset = 0;
        while (offset < blockSize) {
            Ext4DirEntry *entry = (Ext4DirEntry*)(state->fsState->blockBuffer + offset);

            uint16_t recLen;
            readBytes(&recLen, &entry->recLen);
            if (recLen == 0) break; // End of directory entries in this block

            uint32_t entryInodeNum;
            readBytes(&entryInodeNum, &entry->inode);
            uint8_t nameLen;
            readBytes(&nameLen, &entry->nameLen);

            if (entryInodeNum != 0 && nameLen > 0) {
                if (strncmp(name, entry->name, nameLen) == 0 && name[nameLen] == '\0') {
                    return entryInodeNum;
                }
            }
            offset += recLen;
        }
    }

    return 0; // Not found
}

static uint32_t pathToInode(Ext4State *state, const char *pathname) {
    if (pathname == NULL || pathname[0] != '/') {
        return 0; // Invalid path
    }

    if (strcmp(pathname, "/") == 0) {
        return EXT4_ROOT_INO;
    }

    char *pathCopy = (char*)malloc(strlen(pathname) + 1);
    if (!pathCopy) {
        return 0; // Out of memory
    }
    strcpy(pathCopy, pathname);

    uint32_t currentInode = EXT4_ROOT_INO;
    char *token = strtok(pathCopy + 1, "/");

    while (token != NULL) {
        currentInode = findEntryInDir(state, currentInode, token);
        if (currentInode == 0) {
            break; // Component not found
        }
        token = strtok(NULL, "/");
    }

    free(pathCopy);
    return currentInode;
}

static uint64_t findAndAllocateFreeBlock(Ext4State *state, uint32_t startGroup) {
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;

    for (uint32_t g = 0; g < state->numBlockGroups; ++g) {
        uint32_t groupNum = (startGroup + g) % state->numBlockGroups;
        Ext4GroupDesc *groupDesc = &state->groupDescs[groupNum];

        uint16_t freeBlocksLo, freeBlocksHi;
        readBytes(&freeBlocksLo, &groupDesc->bgFreeBlocksCountLo);
        uint32_t freeBlocks = freeBlocksLo;
        if (state->is64bit) {
            readBytes(&freeBlocksHi, &groupDesc->bgFreeBlocksCountHi);
            freeBlocks |= ((uint32_t)freeBlocksHi << 16);
        }

        if (freeBlocks > 0) {
            uint32_t bitmapBlockLo, bitmapBlockHi;
            readBytes(&bitmapBlockLo, &groupDesc->bgBlockBitmapLo);
            uint64_t bitmapBlock = bitmapBlockLo;
            if (state->is64bit) {
                readBytes(&bitmapBlockHi, &groupDesc->bgBlockBitmapHi);
                bitmapBlock |= ((uint64_t)bitmapBlockHi << 32);
            }

            if (readBlock(state, bitmapBlock, state->fsState->blockBuffer) != 0) {
                continue; // Error reading bitmap, try next group
            }

            for (uint32_t i = 0; i < blockSize * 8; ++i) {
                uint8_t byte = state->fsState->blockBuffer[i / 8];
                if (!((byte >> (i % 8)) & 1)) { // Found a free block
                    // Mark as used
                    state->fsState->blockBuffer[i / 8] |= (1 << (i % 8));
                    if (writeBlock(state, bitmapBlock, state->fsState->blockBuffer) != 0) {
                        return 0; // Error writing back bitmap
                    }

                    // Update counts
                    freeBlocks--;
                    freeBlocksLo = freeBlocks & 0xFFFF;
                    writeBytes(&groupDesc->bgFreeBlocksCountLo, &freeBlocksLo);
                    if (state->is64bit) {
                        freeBlocksHi = freeBlocks >> 16;
                        writeBytes(&groupDesc->bgFreeBlocksCountHi, &freeBlocksHi);
                    }

                    uint32_t sFreeBlocksLo, sFreeBlocksHi;
                    readBytes(&sFreeBlocksLo, &state->superblock.sFreeBlocksCountLo);
                    uint64_t sFreeBlocks = sFreeBlocksLo;
                     if (state->is64bit) {
                        readBytes(&sFreeBlocksHi, &state->superblock.sFreeBlocksCountHi);
                        sFreeBlocks |= ((uint64_t)sFreeBlocksHi << 32);
                    }
                    sFreeBlocks--;
                    sFreeBlocksLo = sFreeBlocks & 0xFFFFFFFF;
                    writeBytes(&state->superblock.sFreeBlocksCountLo, &sFreeBlocksLo);
                    if (state->is64bit) {
                        sFreeBlocksHi = sFreeBlocks >> 32;
                        writeBytes(&state->superblock.sFreeBlocksCountHi, &sFreeBlocksHi);
                    }
                    
                    uint32_t blocksPerGroup;
                    readBytes(&blocksPerGroup, &state->superblock.sBlocksPerGroup);
                    uint32_t firstDataBlock;
                    readBytes(&firstDataBlock, &state->superblock.sFirstDataBlock);
                    
                    return (uint64_t)groupNum * blocksPerGroup + i + firstDataBlock;
                }
            }
        }
    }
    return 0; // No free blocks found
}

static uint64_t allocateBlockForInode(Ext4FileHandle *handle, Ext4Inode *inode, uint32_t fileBlockNum) {
    Ext4State *state = handle->state;
    uint64_t physBlock = inodeToBlock(state, inode, fileBlockNum);
    if (physBlock != 0) return physBlock;

    physBlock = findAndAllocateFreeBlock(state, (handle->inodeNum - 1) / state->superblock.sInodesPerGroup);
    if (physBlock == 0) return 0; // No space

    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint32_t pointersPerBlock = blockSize / sizeof(uint32_t);

    uint32_t physBlockLo = physBlock & 0xFFFFFFFF;

    // Direct
    if (fileBlockNum < 12) {
        writeBytes(&inode->iBlock[fileBlockNum], &physBlockLo);
        return physBlock;
    }

    // Single Indirect
    fileBlockNum -= 12;
    if (fileBlockNum < pointersPerBlock) {
        uint32_t indirectBlockNum;
        readBytes(&indirectBlockNum, &inode->iBlock[12]);
        if (indirectBlockNum == 0) {
            indirectBlockNum = findAndAllocateFreeBlock(state, (handle->inodeNum - 1) / state->superblock.sInodesPerGroup);
            if (indirectBlockNum == 0) return 0;
            writeBytes(&inode->iBlock[12], &indirectBlockNum);
            // Must zero out the new indirect block
            memset(state->fsState->blockBuffer, 0, blockSize);
            writeBlock(state, indirectBlockNum, state->fsState->blockBuffer);
        }
        readBlock(state, indirectBlockNum, state->fsState->blockBuffer);
        writeBytes(state->fsState->blockBuffer + fileBlockNum * sizeof(uint32_t), &physBlockLo);
        writeBlock(state, indirectBlockNum, state->fsState->blockBuffer);
        return physBlock;
    }

    // Double Indirect, etc. - Omitting for brevity, but the pattern continues.
    // Production code would require implementing these levels as well.
    
    return 0; // Beyond implemented indirection level
}

//
// Public API Functions
//

int ext4Mount(FilesystemState *fsState, Ext4State **statePtr) {
    *statePtr = (Ext4State*)malloc(sizeof(Ext4State));
    if (!*statePtr) return -1;
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
    
    if (readBlock(state, 0, fsState->blockBuffer) != 0) {
         if (fsState->numOpenFiles == 0) {
            free(fsState->blockBuffer);
            fsState->blockBuffer = NULL;
        }
        free(state);
        *statePtr = NULL;
        return -1;
    }
    readBytes(&state->superblock, fsState->blockBuffer + EXT4_SUPERBLOCK_OFFSET);

    uint16_t magic;
    readBytes(&magic, &state->superblock.sMagic);
    if (magic != EXT4_MAGIC) { /* ... cleanup ... */ return -1; }

    uint32_t featureIncompat;
    readBytes(&featureIncompat, &state->superblock.sFeatureIncompat);
    state->is64bit = (featureIncompat & EXT4_FEATURE_INCOMPAT_64BIT) != 0;

    uint32_t blocksCountLo, blocksCountHi;
    readBytes(&blocksCountLo, &state->superblock.sBlocksCountLo);
    uint64_t blocksCount = blocksCountLo;
    if (state->is64bit) {
        readBytes(&blocksCountHi, &state->superblock.sBlocksCountHi);
        blocksCount |= ((uint64_t)blocksCountHi << 32);
    }
    
    uint32_t blocksPerGroup;
    readBytes(&blocksPerGroup, &state->superblock.sBlocksPerGroup);

    state->numBlockGroups = (blocksCount + blocksPerGroup - 1) / blocksPerGroup;
    
    uint16_t descSize;
    readBytes(&descSize, &state->superblock.sDescSize);
    if (descSize == 0) descSize = 32; // Default for older versions
    uint32_t gdtSize = state->numBlockGroups * descSize;

    state->groupDescs = (Ext4GroupDesc*)malloc(gdtSize);
    if (!state->groupDescs) { /* ... cleanup ... */ return -1; }

    uint32_t firstDataBlock;
    readBytes(&firstDataBlock, &state->superblock.sFirstDataBlock);
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t gdtBlock = (logBlockSize == 0) ? firstDataBlock + 1 : firstDataBlock;
    
    if (readBlock(state, gdtBlock, state->groupDescs) != 0) { /* ... cleanup ... */ return -1; }

    return 0;
}

int ext4Unmount(Ext4State *state) {
    if (!state) return -1;
    
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
        handle->pos = getInodeSize(state, &inode);
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

    if (state->fsState->numOpenFiles == 0 && state->fsState->blockBuffer) {
        free(state->fsState->blockBuffer);
        state->fsState->blockBuffer = NULL;
    }

    return 0;
}

int32_t ext4ReadFile(Ext4FileHandle *handle, void *buffer, uint32_t length) {
    if (!handle || !(handle->mode & Ext4ModeRead)) return -1;
    
    Ext4Inode inode;
    if (readInode(handle->state, handle->inodeNum, &inode) != 0) return -1;

    uint64_t fileSize = getInodeSize(handle->state, &inode);
    if (handle->pos >= fileSize) return 0; // EOF

    if (handle->pos + length > fileSize) {
        length = fileSize - handle->pos;
    }
    
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &handle->state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    
    uint32_t bytesRead = 0;
    while (bytesRead < length) {
        uint32_t fileBlockNum = handle->pos / blockSize;
        uint32_t offsetInBlock = handle->pos % blockSize;

        uint64_t physBlock = inodeToBlock(handle->state, &inode, fileBlockNum);
        if (physBlock == 0) { // Hole in file, read as zeros
            memset(handle->state->fsState->blockBuffer, 0, blockSize);
        } else {
            if (readBlock(handle->state, physBlock, handle->state->fsState->blockBuffer) != 0) {
                return -1; // read error
            }
        }
        
        uint32_t toRead = blockSize - offsetInBlock;
        if (bytesRead + toRead > length) {
            toRead = length - bytesRead;
        }

        memcpy((char*)buffer + bytesRead, handle->state->fsState->blockBuffer + offsetInBlock, toRead);
        bytesRead += toRead;
        handle->pos += toRead;
    }

    return bytesRead;
}

int32_t ext4WriteFile(Ext4FileHandle *handle, const void *buffer, uint32_t length) {
    if (!handle || !(handle->mode & Ext4ModeWrite)) {
        return -1;
    }
    if (length == 0) return 0;

    Ext4State* state = handle->state;
    Ext4Inode inode;
    if (readInode(state, handle->inodeNum, &inode) != 0) {
        return -1;
    }
    
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;

    uint32_t bytesWritten = 0;
    const uint8_t* data = (const uint8_t*)buffer;

    while (bytesWritten < length) {
        uint32_t fileBlockNum = handle->pos / blockSize;
        uint32_t offsetInBlock = handle->pos % blockSize;

        uint64_t physBlock = allocateBlockForInode(handle, &inode, fileBlockNum);
        if (physBlock == 0) {
            // Allocation failed, break and return bytes written so far
            break; 
        }

        uint32_t toWrite = blockSize - offsetInBlock;
        if (bytesWritten + toWrite > length) {
            toWrite = length - bytesWritten;
        }

        // If not writing a full block, we need to read the original content first
        if (toWrite < blockSize) {
            if (readBlock(state, physBlock, state->fsState->blockBuffer) != 0) {
                break; // Read error
            }
        }

        // Copy new data into the buffer and write it back
        memcpy(state->fsState->blockBuffer + offsetInBlock, data + bytesWritten, toWrite);
        if (writeBlock(state, physBlock, state->fsState->blockBuffer) != 0) {
            break; // Write error
        }

        bytesWritten += toWrite;
        handle->pos += toWrite;
    }

    // Update inode size if we wrote past the end of the file
    uint64_t currentSize = getInodeSize(state, &inode);
    if (handle->pos > currentSize) {
        setInodeSize(state, &inode, handle->pos);
    }
    
    // Update modification time (simplified)
    // A real implementation would get the current time from the OS.
    uint32_t mtime;
    readBytes(&mtime, &inode.iMtime);
    mtime++;
    writeBytes(&inode.iMtime, &mtime);

    // Write the updated inode back to disk
    if (writeInode(state, handle->inodeNum, &inode) != 0) {
        // This is a serious error, filesystem might be inconsistent
        return -1;
    }

    return bytesWritten;
}

int ext4RemoveFile(Ext4State *state, const char *pathname) {
    (void) state;
    (void) pathname;
    return -1; // Not implemented
}

int ext4SeekFile(Ext4FileHandle *handle, int64_t offset, int whence) {
    if (!handle) return -1;

    Ext4Inode inode;
    if (readInode(handle->state, handle->inodeNum, &inode) != 0) {
        return -1;
    }
    uint64_t fileSize = getInodeSize(handle->state, &inode);
    
    int64_t newPos;

    switch (whence) {
        case SEEK_SET: newPos = offset; break;
        case SEEK_CUR: newPos = handle->pos + offset; break;
        case SEEK_END: newPos = fileSize + offset; break;
        default: return -1;
    }

    // Allow seeking past EOF for writing, but clamp for reading.
    if (newPos < 0) return -1;
    if (!(handle->mode & Ext4ModeWrite) && (((uint64_t) newPos) > fileSize)) {
        newPos = fileSize;
    }

    handle->pos = newPos;
    return 0;
}

int ext4CreateDir(Ext4State *state, const char *pathname) {
    (void) state;
    (void) pathname;
    return -1; // Not implemented
}

