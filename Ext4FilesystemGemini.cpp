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
#include "Ext4FilesystemGemini.h"
#include "NanoOs.h"
#include "Filesystem.h"

//
// Constants and Enums
//

#define EXT4_SUPERBLOCK_OFFSET 1024
#define EXT4_MAGIC 0xEF53
#define EXT4_ROOT_INO 2
#define EXT4_NAME_LEN 255

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

// Dir Entry File types
#define EXT4_FT_DIR 2 // Directory

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
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    
    uint32_t firstDataBlock;
    readBytes(&firstDataBlock, &state->superblock.sFirstDataBlock);
    
    // The on-disk block numbers are relative to the start of the fs, not partition.
    // However, the block device driver expects LBA relative to the start of the disk.
    // Our startLba handles the partition offset. So blockNum is correct here.
    return state->fsState->blockDevice->readBlocks(
        state->fsState->blockDevice->context,
        state->fsState->startLba + blockNum,
        1,
        blockSize,
        (uint8_t*) buffer
    );
}

static int writeBlock(Ext4State *state, uint64_t blockNum, const void *buffer) {
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;

    return state->fsState->blockDevice->writeBlocks(
        state->fsState->blockDevice->context,
        state->fsState->startLba + blockNum,
        1,
        blockSize,
        (const uint8_t*) buffer
    );
}

static uint64_t getInodeSize(Ext4State *state, Ext4Inode *inode) {
    uint32_t sizeLo, sizeHi;
    readBytes(&sizeLo, &inode->iSizeLo);
    uint16_t mode;
    readBytes(&mode, &inode->iMode);
    if ((mode & EXT4_S_IFREG) && state->is64bit) { // Only regular files use 64-bit size
        readBytes(&sizeHi, &inode->iSizeHigh);
        return ((uint64_t)sizeHi << 32) | sizeLo;
    }
    return sizeLo;
}

static void setInodeSize(Ext4State *state, Ext4Inode *inode, uint64_t size) {
    uint32_t sizeLo = size & 0xFFFFFFFF;
    writeBytes(&inode->iSizeLo, &sizeLo);
    uint16_t mode;
    readBytes(&mode, &inode->iMode);
    if ((mode & EXT4_S_IFREG) && state->is64bit) {
        uint32_t sizeHi = size >> 32;
        writeBytes(&inode->iSizeHigh, &sizeHi);
    }
}


static int readInode(Ext4State *state, uint32_t inodeNum, Ext4Inode *inode) {
    if (inodeNum == 0) return -1;
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
    
    uint8_t* blockBuffer = (uint8_t*)malloc(blockSize);
    if (!blockBuffer) return -1;
    
    if (readBlock(state, block, blockBuffer) != 0) {
        free(blockBuffer);
        return -1;
    }

    readBytes(inode, blockBuffer + offsetInBlock);
    free(blockBuffer);
    return 0;
}

static int writeInode(Ext4State *state, uint32_t inodeNum, Ext4Inode *inode) {
    if (inodeNum == 0) return -1;
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

    uint8_t* blockBuffer = (uint8_t*)malloc(blockSize);
    if (!blockBuffer) return -1;

    // Read the block first to avoid corrupting other inodes in the same block
    if (readBlock(state, block, blockBuffer) != 0) {
        free(blockBuffer);
        return -1;
    }

    // Write the modified inode into the buffer
    writeBytes(blockBuffer + offsetInBlock, inode);

    // Write the modified block back to the device
    int result = writeBlock(state, block, blockBuffer);
    free(blockBuffer);
    return result;
}

static uint64_t inodeToBlock(Ext4State *state, Ext4Inode *inode, uint32_t fileBlockNum) {
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint32_t pointersPerBlock = blockSize / sizeof(uint32_t);
    uint8_t* buffer = (uint8_t*)malloc(blockSize);
    if (!buffer) return 0;

    // Direct blocks
    if (fileBlockNum < 12) {
        uint32_t blockNum;
        readBytes(&blockNum, &inode->iBlock[fileBlockNum]);
        free(buffer);
        return blockNum;
    }

    // Single-indirect block
    fileBlockNum -= 12;
    if (fileBlockNum < pointersPerBlock) {
        uint32_t indirectBlockNum;
        readBytes(&indirectBlockNum, &inode->iBlock[12]);
        if (indirectBlockNum == 0) { free(buffer); return 0; } // Hole

        if (readBlock(state, indirectBlockNum, buffer) != 0) {
            free(buffer);
            return 0; // Read error
        }
        uint32_t blockNum;
        readBytes(&blockNum, buffer + (fileBlockNum * sizeof(uint32_t)));
        free(buffer);
        return blockNum;
    }

    // Double-indirect block
    fileBlockNum -= pointersPerBlock;
    uint64_t pointersPerBlockSquared = (uint64_t)pointersPerBlock * pointersPerBlock;
    if (fileBlockNum < pointersPerBlockSquared) {
        uint32_t doubleIndirectBlockNum;
        readBytes(&doubleIndirectBlockNum, &inode->iBlock[13]);
        if (doubleIndirectBlockNum == 0) { free(buffer); return 0; } // Hole

        if (readBlock(state, doubleIndirectBlockNum, buffer) != 0) {
            free(buffer);
            return 0; // Read error
        }
        uint32_t indirectBlockNum;
        uint32_t indirectBlockIndex = fileBlockNum / pointersPerBlock;
        readBytes(&indirectBlockNum, buffer + (indirectBlockIndex * sizeof(uint32_t)));
        if (indirectBlockNum == 0) { free(buffer); return 0; } // Hole

        if (readBlock(state, indirectBlockNum, buffer) != 0) {
            free(buffer);
            return 0; // Read error
        }
        uint32_t finalBlockIndex = fileBlockNum % pointersPerBlock;
        uint32_t blockNum;
        readBytes(&blockNum, buffer + (finalBlockIndex * sizeof(uint32_t)));
        free(buffer);
        return blockNum;
    }
    
    // Triple-indirect block
    fileBlockNum -= pointersPerBlockSquared;
    uint32_t tripleIndirectBlockNum;
    readBytes(&tripleIndirectBlockNum, &inode->iBlock[14]);
    if (tripleIndirectBlockNum == 0) { free(buffer); return 0; } // Hole

    if (readBlock(state, tripleIndirectBlockNum, buffer) != 0) {
        free(buffer);
        return 0; // Read error
    }
    uint32_t doubleIndirectBlockIndex = fileBlockNum / pointersPerBlockSquared;
    uint32_t doubleIndirectBlockNum;
    readBytes(&doubleIndirectBlockNum, buffer + (doubleIndirectBlockIndex * sizeof(uint32_t)));
    if(doubleIndirectBlockNum == 0) { free(buffer); return 0; } // Hole

    if (readBlock(state, doubleIndirectBlockNum, buffer) != 0) {
        free(buffer);
        return 0; // Read error
    }
    uint32_t indirectBlockIndex = (fileBlockNum / pointersPerBlock) % pointersPerBlock;
    uint32_t indirectBlockNum;
    readBytes(&indirectBlockNum, buffer + (indirectBlockIndex * sizeof(uint32_t)));
    if(indirectBlockNum == 0) { free(buffer); return 0; } // Hole

    if (readBlock(state, indirectBlockNum, buffer) != 0) {
        free(buffer);
        return 0; // Read error
    }
    uint32_t finalBlockIndex = fileBlockNum % pointersPerBlock;
    uint32_t blockNum;
    readBytes(&blockNum, buffer + (finalBlockIndex * sizeof(uint32_t)));
    free(buffer);
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

    uint8_t* blockBuffer = (uint8_t*)malloc(blockSize);
    if (!blockBuffer) return 0;

    for (uint32_t i = 0; i < numBlocks; ++i) {
        uint64_t blockNum = inodeToBlock(state, &dirInode, i);
        if (blockNum == 0) continue; // Hole in directory file

        if (readBlock(state, blockNum, blockBuffer) != 0) {
            continue; // Read error
        }

        uint32_t offset = 0;
        while (offset < blockSize) {
            Ext4DirEntry *entry = (Ext4DirEntry*)(blockBuffer + offset);

            uint16_t recLen;
            readBytes(&recLen, &entry->recLen);
            if (recLen == 0 || recLen > blockSize) break; // End of entries or corruption

            uint32_t entryInodeNum;
            readBytes(&entryInodeNum, &entry->inode);
            uint8_t nameLen;
            readBytes(&nameLen, &entry->nameLen);

            if (entryInodeNum != 0 && nameLen == strlen(name)) {
                if (strncmp(name, entry->name, nameLen) == 0) {
                    free(blockBuffer);
                    return entryInodeNum;
                }
            }
            if (recLen < 8) break; // Corrupted, avoid infinite loop
            offset += recLen;
        }
    }
    
    free(blockBuffer);
    return 0; // Not found
}

static uint32_t pathToInode(Ext4State *state, const char *pathname) {
    if (pathname == NULL || pathname[0] != '/') {
        return 0; // Invalid path
    }

    if (strcmp(pathname, "/") == 0) {
        return EXT4_ROOT_INO;
    }

    char *pathCopy = (char*) malloc(strlen(pathname) + 1);
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
    uint8_t* buffer = (uint8_t*)malloc(blockSize);
    if (!buffer) return 0;

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

            if (readBlock(state, bitmapBlock, buffer) != 0) {
                continue; // Error reading bitmap, try next group
            }

            for (uint32_t i = 0; i < blockSize * 8; ++i) {
                uint8_t byte = buffer[i / 8];
                if (!((byte >> (i % 8)) & 1)) { // Found a free block
                    // Mark as used
                    buffer[i / 8] |= (1 << (i % 8));
                    if (writeBlock(state, bitmapBlock, buffer) != 0) {
                        free(buffer);
                        return 0; // Error writing back bitmap
                    }
                    free(buffer);

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
    free(buffer);
    return 0; // No free blocks found
}

static uint64_t allocateBlockForInode(Ext4State *state, uint32_t inodeNum, Ext4Inode *inode, uint32_t fileBlockNum) {
    uint64_t physBlock = inodeToBlock(state, inode, fileBlockNum);
    if (physBlock != 0) return physBlock;

    uint32_t inodesPerGroup;
    readBytes(&inodesPerGroup, &state->superblock.sInodesPerGroup);
    physBlock = findAndAllocateFreeBlock(state, (inodeNum - 1) / inodesPerGroup);
    if (physBlock == 0) return 0; // No space

    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint32_t pointersPerBlock = blockSize / sizeof(uint32_t);
    uint8_t* buffer = (uint8_t*)malloc(blockSize);
    if (!buffer) { /* cannot free block here easily, major problem */ return 0; }

    uint32_t physBlockLo = physBlock & 0xFFFFFFFF;

    // Direct
    if (fileBlockNum < 12) {
        writeBytes(&inode->iBlock[fileBlockNum], &physBlockLo);
        free(buffer);
        return physBlock;
    }

    // Single Indirect
    fileBlockNum -= 12;
    if (fileBlockNum < pointersPerBlock) {
        uint32_t indirectBlockNum;
        readBytes(&indirectBlockNum, &inode->iBlock[12]);
        if (indirectBlockNum == 0) {
            indirectBlockNum = findAndAllocateFreeBlock(state, (inodeNum - 1) / inodesPerGroup);
            if (indirectBlockNum == 0) { free(buffer); return 0; }
            writeBytes(&inode->iBlock[12], &indirectBlockNum);
            memset(buffer, 0, blockSize);
            writeBlock(state, indirectBlockNum, buffer);
        }
        readBlock(state, indirectBlockNum, buffer);
        writeBytes(buffer + fileBlockNum * sizeof(uint32_t), &physBlockLo);
        writeBlock(state, indirectBlockNum, buffer);
        free(buffer);
        return physBlock;
    }
    
    free(buffer);
    return 0; // Double/Triple indirect omitted for brevity
}

/**
 * @brief Splits a full path into its parent directory path and the final filename.
 * @note The caller is responsible for freeing the memory allocated for parentPath and fileName.
 */
static int parsePath(const char *pathname, char **parentPath, char **fileName) {
    const char *lastSlash = strrchr(pathname, '/');
    if (lastSlash == NULL) { // No slash in path, invalid absolute path
        return -1;
    }

    if (lastSlash == pathname) { // Path is in root directory, e.g., "/file.txt"
        *parentPath = (char*)malloc(2);
        if (!*parentPath) return -1;
        strcpy(*parentPath, "/");
        *fileName = (char*) malloc(strlen(lastSlash + 1) + 1);
        if (*fileName != NULL) {
            strcpy(*fileName, lastSlash + 1);
        }
    } else { // Path is in a subdirectory, e.g., "/dir/file.txt"
        size_t parentLen = lastSlash - pathname;
        *parentPath = (char*)malloc(parentLen + 1);
        if (!*parentPath) return -1;
        strncpy(*parentPath, pathname, parentLen);
        (*parentPath)[parentLen] = '\0';
        *fileName = (char*) malloc(strlen(lastSlash + 1) + 1);
        if (*fileName != NULL) {
            strcpy(*fileName, lastSlash + 1);
        }
    }

    if (!*fileName) {
        free(*parentPath);
        *parentPath = NULL;
        return -1;
    }
    if (strlen(*fileName) == 0) { // Trailing slash case, e.g. "/dir/"
        free(*parentPath);
        free(*fileName);
        *parentPath = NULL;
        *fileName = NULL;
        return -1;
    }

    return 0;
}

/**
 * @brief Marks a specific block as free in the block bitmap.
 */
static int freeBlock(Ext4State *state, uint64_t blockNum) {
    uint32_t blocksPerGroup;
    readBytes(&blocksPerGroup, &state->superblock.sBlocksPerGroup);
    uint32_t firstDataBlock;
    readBytes(&firstDataBlock, &state->superblock.sFirstDataBlock);

    if (blockNum < firstDataBlock) return -1; // Cannot free metadata blocks

    uint32_t group = (blockNum - firstDataBlock) / blocksPerGroup;
    uint32_t indexInGroup = (blockNum - firstDataBlock) % blocksPerGroup;
    
    if (group >= state->numBlockGroups) return -1;

    Ext4GroupDesc *groupDesc = &state->groupDescs[group];
    
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint8_t* buffer = (uint8_t*)malloc(blockSize);
    if (!buffer) return -1;
    
    // Read block bitmap
    uint32_t bitmapBlockLo, bitmapBlockHi;
    readBytes(&bitmapBlockLo, &groupDesc->bgBlockBitmapLo);
    uint64_t bitmapBlock = bitmapBlockLo;
    if (state->is64bit) {
        readBytes(&bitmapBlockHi, &groupDesc->bgBlockBitmapHi);
        bitmapBlock |= ((uint64_t)bitmapBlockHi << 32);
    }

    if (readBlock(state, bitmapBlock, buffer) != 0) { free(buffer); return -1; }

    // Clear the bit
    buffer[indexInGroup / 8] &= ~(1 << (indexInGroup % 8));
    if (writeBlock(state, bitmapBlock, buffer) != 0) { free(buffer); return -1; }
    free(buffer);

    // Update group descriptor free blocks count
    uint16_t freeBlocksLo, freeBlocksHi;
    readBytes(&freeBlocksLo, &groupDesc->bgFreeBlocksCountLo);
    uint32_t freeBlocks = freeBlocksLo;
    if (state->is64bit) {
        readBytes(&freeBlocksHi, &groupDesc->bgFreeBlocksCountHi);
        freeBlocks |= ((uint32_t)freeBlocksHi << 16);
    }
    freeBlocks++;
    freeBlocksLo = freeBlocks & 0xFFFF;
    writeBytes(&groupDesc->bgFreeBlocksCountLo, &freeBlocksLo);
    if (state->is64bit) {
        freeBlocksHi = freeBlocks >> 16;
        writeBytes(&groupDesc->bgFreeBlocksCountHi, &freeBlocksHi);
    }
    
    // Update superblock free blocks count
    uint32_t sFreeBlocksLo, sFreeBlocksHi;
    readBytes(&sFreeBlocksLo, &state->superblock.sFreeBlocksCountLo);
    uint64_t sFreeBlocks = sFreeBlocksLo;
    if (state->is64bit) {
        readBytes(&sFreeBlocksHi, &state->superblock.sFreeBlocksCountHi);
        sFreeBlocks |= ((uint64_t)sFreeBlocksHi << 32);
    }
    sFreeBlocks++;
    sFreeBlocksLo = sFreeBlocks & 0xFFFFFFFF;
    writeBytes(&state->superblock.sFreeBlocksCountLo, &sFreeBlocksLo);
    if (state->is64bit) {
        sFreeBlocksHi = sFreeBlocks >> 32;
        writeBytes(&state->superblock.sFreeBlocksCountHi, &sFreeBlocksHi);
    }
    return 0;
}

/**
 * @brief Deallocates all data blocks associated with an inode.
 */
static void freeInodeBlocks(Ext4State *state, Ext4Inode *inode) {
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint32_t pointersPerBlock = blockSize / sizeof(uint32_t);
    uint8_t* buffer = (uint8_t*)malloc(blockSize);
    if (!buffer) return;

    // Free direct blocks
    for (int i = 0; i < 12; i++) {
        uint32_t blockNum;
        readBytes(&blockNum, &inode->iBlock[i]);
        if (blockNum != 0) {
            freeBlock(state, blockNum);
        }
    }

    // Free single-indirect blocks
    uint32_t singleIndirectPtr;
    readBytes(&singleIndirectPtr, &inode->iBlock[12]);
    if (singleIndirectPtr != 0) {
        if (readBlock(state, singleIndirectPtr, buffer) == 0) {
            for (uint32_t i = 0; i < pointersPerBlock; i++) {
                uint32_t blockNum;
                readBytes(&blockNum, buffer + i * sizeof(uint32_t));
                if (blockNum != 0) {
                    freeBlock(state, blockNum);
                }
            }
        }
        freeBlock(state, singleIndirectPtr);
    }

    // Double-indirect and triple-indirect freeing omitted for brevity.
    free(buffer);
}

/**
 * @brief Marks a specific inode as free in the inode bitmap.
 */
static int freeInode(Ext4State *state, uint32_t inodeNum) {
    uint32_t inodesPerGroup;
    readBytes(&inodesPerGroup, &state->superblock.sInodesPerGroup);

    uint32_t group = (inodeNum - 1) / inodesPerGroup;
    uint32_t index = (inodeNum - 1) % inodesPerGroup;

    if (group >= state->numBlockGroups) return -1;
    Ext4GroupDesc* groupDesc = &state->groupDescs[group];
    
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint8_t* buffer = (uint8_t*)malloc(blockSize);
    if (!buffer) return -1;

    // Read inode bitmap
    uint32_t bitmapBlockLo, bitmapBlockHi;
    readBytes(&bitmapBlockLo, &groupDesc->bgInodeBitmapLo);
    uint64_t bitmapBlock = bitmapBlockLo;
    if (state->is64bit) {
        readBytes(&bitmapBlockHi, &groupDesc->bgInodeBitmapHi);
        bitmapBlock |= ((uint64_t)bitmapBlockHi << 32);
    }

    if (readBlock(state, bitmapBlock, buffer) != 0) { free(buffer); return -1; }

    // Clear the bit
    buffer[index / 8] &= ~(1 << (index % 8));
    if (writeBlock(state, bitmapBlock, buffer) != 0) { free(buffer); return -1; }
    free(buffer);

    // Update counts in group descriptor
    uint16_t freeInodesLo, freeInodesHi;
    readBytes(&freeInodesLo, &groupDesc->bgFreeInodesCountLo);
    uint32_t freeInodes = freeInodesLo;
    if (state->is64bit) {
        readBytes(&freeInodesHi, &groupDesc->bgFreeInodesCountHi);
        freeInodes |= ((uint32_t)freeInodesHi << 16);
    }
    freeInodes++;
    freeInodesLo = freeInodes & 0xFFFF;
    writeBytes(&groupDesc->bgFreeInodesCountLo, &freeInodesLo);
    if (state->is64bit) {
        freeInodesHi = freeInodes >> 16;
        writeBytes(&groupDesc->bgFreeInodesCountHi, &freeInodesHi);
    }

    // Update counts in superblock
    uint32_t sFreeInodesCount;
    readBytes(&sFreeInodesCount, &state->superblock.sFreeInodesCount);
    sFreeInodesCount++;
    writeBytes(&state->superblock.sFreeInodesCount, &sFreeInodesCount);

    return 0;
}

/**
 * @brief Finds a directory entry and removes it by setting its inode number to 0.
 * @note This is a simplified removal; it doesn't merge with the previous entry.
 */
static int findAndRemoveEntry(Ext4State *state, uint32_t dirInodeNum, const char *name, uint32_t *removedInodeNum) {
    Ext4Inode dirInode;
    if (readInode(state, dirInodeNum, &dirInode) != 0) return -1;
    
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint8_t* buffer = (uint8_t*)malloc(blockSize);
    if (!buffer) return -1;

    uint64_t dirSize = getInodeSize(state, &dirInode);
    uint32_t numBlocks = (dirSize + blockSize - 1) / blockSize;

    for (uint32_t i = 0; i < numBlocks; ++i) {
        uint64_t blockNum = inodeToBlock(state, &dirInode, i);
        if (blockNum == 0) continue;

        if (readBlock(state, blockNum, buffer) != 0) continue;

        uint32_t offset = 0;
        while (offset < blockSize) {
            Ext4DirEntry *entry = (Ext4DirEntry*)(buffer + offset);
            
            uint16_t recLen;
            readBytes(&recLen, &entry->recLen);
            if (recLen == 0 || recLen > blockSize) break;

            uint8_t nameLen;
            readBytes(&nameLen, &entry->nameLen);
            uint32_t entryInodeNum;
            readBytes(&entryInodeNum, &entry->inode);

            if (entryInodeNum != 0 && nameLen == strlen(name) && strncmp(name, entry->name, nameLen) == 0) {
                *removedInodeNum = entryInodeNum;
                uint32_t zeroInode = 0;
                writeBytes(&entry->inode, &zeroInode); // Set inode to 0 to "remove"
                
                int result = writeBlock(state, blockNum, buffer);
                free(buffer);
                return result;
            }
            if (recLen < 8) break;
            offset += recLen;
        }
    }
    free(buffer);
    return -1; // Not found
}

static uint32_t findAndAllocateFreeInode(Ext4State *state, uint32_t parentInodeNum) {
    uint32_t inodesPerGroup;
    readBytes(&inodesPerGroup, &state->superblock.sInodesPerGroup);
    
    // Heuristic: try to allocate in the same group as the parent directory
    uint32_t startGroup = (parentInodeNum > 0) ? (parentInodeNum - 1) / inodesPerGroup : 0;
    
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint8_t* buffer = (uint8_t*)malloc(blockSize);
    if (!buffer) return 0;

    for (uint32_t g = 0; g < state->numBlockGroups; ++g) {
        uint32_t groupNum = (startGroup + g) % state->numBlockGroups;
        Ext4GroupDesc *groupDesc = &state->groupDescs[groupNum];

        uint16_t freeInodesLo, freeInodesHi;
        readBytes(&freeInodesLo, &groupDesc->bgFreeInodesCountLo);
        uint32_t freeInodes = freeInodesLo;
        if (state->is64bit) {
            readBytes(&freeInodesHi, &groupDesc->bgFreeInodesCountHi);
            freeInodes |= ((uint32_t)freeInodesHi << 16);
        }

        if (freeInodes > 0) {
            uint32_t bitmapBlockLo, bitmapBlockHi;
            readBytes(&bitmapBlockLo, &groupDesc->bgInodeBitmapLo);
            uint64_t bitmapBlock = bitmapBlockLo;
            if (state->is64bit) {
                readBytes(&bitmapBlockHi, &groupDesc->bgInodeBitmapHi);
                bitmapBlock |= ((uint64_t)bitmapBlockHi << 32);
            }

            if (readBlock(state, bitmapBlock, buffer) != 0) {
                continue; // Error reading bitmap, try next group
            }
            
            for (uint32_t i = 0; i < inodesPerGroup; ++i) { // inodesPerGroup is the number of bits to check
                uint8_t byte = buffer[i / 8];
                if (!((byte >> (i % 8)) & 1)) { // Found a free inode
                    // Mark as used
                    buffer[i / 8] |= (1 << (i % 8));
                    if (writeBlock(state, bitmapBlock, buffer) != 0) {
                        free(buffer);
                        return 0; // Error writing back bitmap
                    }
                    free(buffer);

                    // Update counts in group descriptor
                    freeInodes--;
                    freeInodesLo = freeInodes & 0xFFFF;
                    writeBytes(&groupDesc->bgFreeInodesCountLo, &freeInodesLo);
                    if (state->is64bit) {
                        freeInodesHi = freeInodes >> 16;
                        writeBytes(&groupDesc->bgFreeInodesCountHi, &freeInodesHi);
                    }

                    // Update counts in superblock
                    uint32_t sFreeInodesCount;
                    readBytes(&sFreeInodesCount, &state->superblock.sFreeInodesCount);
                    sFreeInodesCount--;
                    writeBytes(&state->superblock.sFreeInodesCount, &sFreeInodesCount);

                    return (uint32_t)groupNum * inodesPerGroup + i + 1;
                }
            }
        }
    }
    free(buffer);
    return 0; // No free inodes found
}

static int addEntryToDir(Ext4State *state, uint32_t dirInodeNum, Ext4Inode *dirInode, const char *name, uint32_t newInodeNum, uint8_t fileType) {
    uint8_t nameLen = strlen(name);
    uint16_t neededLen = (sizeof(Ext4DirEntry) + nameLen + 3) & ~3;

    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint8_t* buffer = (uint8_t*)malloc(blockSize);
    if (!buffer) return -1;
    
    uint64_t dirSize = getInodeSize(state, dirInode);
    uint32_t numBlocks = dirSize / blockSize;

    // First, try to find space in existing blocks
    for (uint32_t i = 0; i < numBlocks; i++) {
        uint64_t blockNum = inodeToBlock(state, dirInode, i);
        if (blockNum == 0) continue;
        if (readBlock(state, blockNum, buffer) != 0) continue;

        uint32_t offset = 0;
        while (offset < blockSize) {
            Ext4DirEntry *entry = (Ext4DirEntry*)(buffer + offset);
            uint16_t recLen;
            readBytes(&recLen, &entry->recLen);
            if (recLen == 0 || recLen > blockSize) break;

            uint8_t currentNameLen;
            readBytes(&currentNameLen, &entry->nameLen);
            uint16_t actualLen = (sizeof(Ext4DirEntry) + currentNameLen + 3) & ~3;

            if ((recLen - actualLen) >= neededLen) {
                writeBytes(&entry->recLen, &actualLen);
                
                offset += actualLen;
                Ext4DirEntry *newEntry = (Ext4DirEntry*)(buffer + offset);
                
                uint16_t newRecLen = recLen - actualLen;
                writeBytes(&newEntry->inode, &newInodeNum);
                writeBytes(&newEntry->recLen, &newRecLen);
                writeBytes(&newEntry->nameLen, &nameLen);
                writeBytes(&newEntry->fileType, &fileType);
                memcpy(newEntry->name, name, nameLen);
                
                writeBlock(state, blockNum, buffer);
                free(buffer);
                return 0; // Success
            }
            if (recLen < 8) break;
            offset += recLen;
        }
    }

    // No space found, allocate a new block
    uint64_t newBlockNum = allocateBlockForInode(state, dirInodeNum, dirInode, numBlocks);
    if (newBlockNum == 0) { free(buffer); return -1; }

    dirSize += blockSize;
    setInodeSize(state, dirInode, dirSize);

    memset(buffer, 0, blockSize);
    Ext4DirEntry *newEntry = (Ext4DirEntry*)buffer;
    uint16_t newRecLen = blockSize;
    writeBytes(&newEntry->inode, &newInodeNum);
    writeBytes(&newEntry->recLen, &newRecLen);
    writeBytes(&newEntry->nameLen, &nameLen);
    writeBytes(&newEntry->fileType, &fileType);
    memcpy(newEntry->name, name, nameLen);

    int result = writeBlock(state, newBlockNum, buffer);
    free(buffer);
    return result;
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
    
    // Create a temporary buffer for superblock reading.
    uint8_t *buffer = (uint8_t*)malloc(EXT4_SUPERBLOCK_OFFSET + sizeof(Ext4Superblock));
    if (!buffer) {
        free(state);
        *statePtr = NULL;
        return -1;
    }

    if (fsState->blockDevice->readBlocks(fsState->blockDevice->context, fsState->startLba, 1, EXT4_SUPERBLOCK_OFFSET + sizeof(Ext4Superblock), buffer) != 0) {
        free(buffer);
        free(state);
        *statePtr = NULL;
        return -1;
    }

    readBytes(&state->superblock, buffer + EXT4_SUPERBLOCK_OFFSET);
    free(buffer);

    uint16_t magic;
    readBytes(&magic, &state->superblock.sMagic);
    if (magic != EXT4_MAGIC) {
        free(state);
        *statePtr = NULL;
        return -1;
    }

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
    if (descSize == 0) descSize = 32;
    if (state->is64bit && descSize < 64) descSize = 64;
    uint32_t gdtSize = state->numBlockGroups * descSize;

    state->groupDescs = (Ext4GroupDesc*)malloc(gdtSize);
    if (!state->groupDescs) {
        free(state);
        *statePtr = NULL;
        return -1;
    }

    uint32_t firstDataBlock;
    readBytes(&firstDataBlock, &state->superblock.sFirstDataBlock);
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    
    // GDT starts on the block after the superblock
    uint64_t gdtBlock = (firstDataBlock == 0) ? 1 : firstDataBlock;
    if (blockSize > 1024) gdtBlock = firstDataBlock;
    else gdtBlock = 2; // for 1k block size
    
    uint32_t numGdtBlocks = (gdtSize + blockSize - 1) / blockSize;
    uint8_t *gdtBuffer = (uint8_t*)malloc(numGdtBlocks * blockSize);
    if (!gdtBuffer) {
        free(state->groupDescs);
        free(state);
        *statePtr = NULL;
        return -1;
    }

    for (uint32_t i = 0; i < numGdtBlocks; ++i) {
        if (readBlock(state, gdtBlock + i, gdtBuffer + i * blockSize) != 0) {
            free(gdtBuffer);
            free(state->groupDescs);
            free(state);
            *statePtr = NULL;
            return -1;
        }
    }
    memcpy(state->groupDescs, gdtBuffer, gdtSize);
    free(gdtBuffer);

    return 0;
}

int ext4Unmount(Ext4State *state) {
    if (!state) return -1;
    
    while (state->openFiles) {
        ext4CloseFile(state->openFiles);
    }
    
    free(state->groupDescs);
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
    uint8_t* blockBuffer = (uint8_t*)malloc(blockSize);
    if (!blockBuffer) return -1;
    
    uint32_t bytesRead = 0;
    while (bytesRead < length) {
        uint32_t fileBlockNum = handle->pos / blockSize;
        uint32_t offsetInBlock = handle->pos % blockSize;

        uint64_t physBlock = inodeToBlock(handle->state, &inode, fileBlockNum);
        if (physBlock == 0) {
            memset(blockBuffer, 0, blockSize);
        } else {
            if (readBlock(handle->state, physBlock, blockBuffer) != 0) {
                free(blockBuffer);
                return -1;
            }
        }
        
        uint32_t toRead = blockSize - offsetInBlock;
        if (bytesRead + toRead > length) {
            toRead = length - bytesRead;
        }

        memcpy((char*)buffer + bytesRead, blockBuffer + offsetInBlock, toRead);
        bytesRead += toRead;
        handle->pos += toRead;
    }

    free(blockBuffer);
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

        uint64_t physBlock = allocateBlockForInode(handle->state,
            handle->inodeNum, &inode, fileBlockNum);
        if (physBlock == 0) {
            // Allocation failed, break and return bytes written so far
            break; 
        }

        uint32_t toWrite = blockSize - offsetInBlock;
        if (bytesWritten + toWrite > length) {
            toWrite = length - bytesWritten;
        }

        // If not writing a full block, we need to read the original content
        // first
        if (toWrite < blockSize) {
            if (readBlock(state, physBlock, state->fsState->blockBuffer) != 0) {
                break; // Read error
            }
        }

        // Copy new data into the buffer and write it back
        memcpy(state->fsState->blockBuffer + offsetInBlock,
            data + bytesWritten, toWrite);
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
    if (!state || !pathname || strcmp(pathname, "/") == 0) {
        return -1; // Cannot remove root
    }

    char *parentPath = NULL;
    char *fileName = NULL;
    if (parsePath(pathname, &parentPath, &fileName) != 0) {
        return -1; // Invalid path
    }

    if (strcmp(fileName, ".") == 0 || strcmp(fileName, "..") == 0) {
        free(parentPath);
        free(fileName);
        return -1; // Cannot remove . or ..
    }

    uint32_t parentInodeNum = pathToInode(state, parentPath);
    if (parentInodeNum == 0) {
        free(parentPath);
        free(fileName);
        return -1; // Parent directory not found
    }

    uint32_t fileInodeNum = 0;
    if (findAndRemoveEntry(state, parentInodeNum, fileName, &fileInodeNum) != 0) {
        free(parentPath);
        free(fileName);
        return -1; // File not found in parent directory
    }
    
    free(parentPath);
    free(fileName);

    Ext4Inode fileInode;
    if (readInode(state, fileInodeNum, &fileInode) != 0) {
        return -1; 
    }

    uint16_t mode;
    readBytes(&mode, &fileInode.iMode);
    if ((mode & EXT4_S_IFDIR) != 0) {
        // Proper rmdir should check if directory is empty first.
        return -1;
    }

    uint16_t linksCount;
    readBytes(&linksCount, &fileInode.iLinksCount);
    
    if (linksCount > 0) linksCount--;
    writeBytes(&fileInode.iLinksCount, &linksCount);

    if (linksCount == 0) {
        uint32_t dtime = 1; // Placeholder for time
        writeBytes(&fileInode.iDtime, &dtime);
        freeInodeBlocks(state, &fileInode);
        if (freeInode(state, fileInodeNum) != 0) return -1;
    }
    
    if (writeInode(state, fileInodeNum, &fileInode) != 0) {
        return -1;
    }

    return 0; // Success
}

int ext4SeekFile(Ext4FileHandle *handle, int64_t offset, int whence) {
    if (!handle) return -1;

    Ext4Inode inode;
    if (readInode(handle->state, handle->inodeNum, &inode) != 0) return -1;
    uint64_t fileSize = getInodeSize(handle->state, &inode);
    
    int64_t newPos;

    switch (whence) {
        case SEEK_SET: newPos = offset; break;
        case SEEK_CUR: newPos = handle->pos + offset; break;
        case SEEK_END: newPos = fileSize + offset; break;
        default: return -1;
    }

    if (newPos < 0) return -1;
    if (!(handle->mode & Ext4ModeWrite) && (((uint64_t) newPos) > fileSize)) {
        newPos = fileSize;
    }

    handle->pos = newPos;
    return 0;
}

int ext4CreateDir(Ext4State *state, const char *pathname) {
    if (!state || !pathname || (pathname[0] != '/') || (strlen(pathname) > EXT4_NAME_LEN)) {
        return -1; // Invalid state or path
    }
    if (strcmp(pathname, "/") == 0) {
        return -1; // Cannot create root
    }

    char *parentPath = NULL;
    char *dirName = NULL;
    if (parsePath(pathname, &parentPath, &dirName) != 0) {
        return -1;
    }

    if ((strcmp(dirName, ".") == 0) || (strcmp(dirName, "..") == 0)) {
        free(parentPath);
        free(dirName);
        return -1; // Cannot create '.' or '..'
    }

    uint32_t parentInodeNum = pathToInode(state, parentPath);
    free(parentPath);
    if (parentInodeNum == 0) {
        free(dirName);
        return -1; // Parent directory does not exist
    }

    Ext4Inode parentInode;
    if (readInode(state, parentInodeNum, &parentInode) != 0) {
        free(dirName);
        return -1; // Could not read parent inode
    }

    uint16_t parentMode;
    readBytes(&parentMode, &parentInode.iMode);
    if ((parentMode & EXT4_S_IFDIR) == 0) {
        free(dirName);
        return -1; // Parent is not a directory
    }
    
    if (findEntryInDir(state, parentInodeNum, dirName) != 0) {
        free(dirName);
        return -1; // Directory/file already exists
    }

    uint32_t newInodeNum = findAndAllocateFreeInode(state, parentInodeNum);
    if (newInodeNum == 0) {
        free(dirName);
        return -1; // Out of inodes
    }

    uint32_t inodesPerGroup;
    readBytes(&inodesPerGroup, &state->superblock.sInodesPerGroup);
    uint32_t parentGroup = (parentInodeNum - 1) / inodesPerGroup;
    uint64_t newBlockNum = findAndAllocateFreeBlock(state, parentGroup);
    if (newBlockNum == 0) {
        freeInode(state, newInodeNum); // Rollback
        free(dirName);
        return -1; // Out of data blocks
    }
    
    Ext4Inode newInode;
    memset(&newInode, 0, sizeof(Ext4Inode));
    uint16_t mode = EXT4_S_IFDIR | 0755;
    uint16_t linksCount = 2;
    uint32_t logBlockSize;
    readBytes(&logBlockSize, &state->superblock.sLogBlockSize);
    uint32_t blockSize = 1024 << logBlockSize;
    uint32_t now = 1; // Placeholder for current time

    writeBytes(&newInode.iMode, &mode);
    writeBytes(&newInode.iLinksCount, &linksCount);
    setInodeSize(state, &newInode, blockSize);
    writeBytes(&newInode.iAtime, &now);
    writeBytes(&newInode.iCtime, &now);
    writeBytes(&newInode.iMtime, &now);
    uint32_t blocksLo = blockSize / 512;
    writeBytes(&newInode.iBlocksLo, &blocksLo);
    uint32_t newBlockNumLo = newBlockNum & 0xFFFFFFFF;
    writeBytes(&newInode.iBlock[0], &newBlockNumLo);

    if (writeInode(state, newInodeNum, &newInode) != 0) {
        freeInode(state, newInodeNum);
        freeBlock(state, newBlockNum);
        free(dirName);
        return -1;
    }

    uint8_t *blockBuffer = (uint8_t*)malloc(blockSize);
    if (!blockBuffer) { return -1; }
    memset(blockBuffer, 0, blockSize);

    Ext4DirEntry *dotEntry = (Ext4DirEntry*)blockBuffer;
    uint16_t recLen = 12;
    uint8_t nameLen = 1;
    uint8_t fileType = EXT4_FT_DIR;
    writeBytes(&dotEntry->inode, &newInodeNum);
    writeBytes(&dotEntry->recLen, &recLen);
    writeBytes(&dotEntry->nameLen, &nameLen);
    writeBytes(&dotEntry->fileType, &fileType);
    dotEntry->name[0] = '.';
    
    Ext4DirEntry *dotDotEntry = (Ext4DirEntry*)(blockBuffer + 12);
    recLen = blockSize - 12;
    nameLen = 2;
    writeBytes(&dotDotEntry->inode, &parentInodeNum);
    writeBytes(&dotDotEntry->recLen, &recLen);
    writeBytes(&dotDotEntry->nameLen, &nameLen);
    writeBytes(&dotDotEntry->fileType, &fileType);
    dotDotEntry->name[0] = '.';
    dotDotEntry->name[1] = '.';
    
    if (writeBlock(state, newBlockNum, blockBuffer) != 0) {
        free(blockBuffer); free(dirName);
        freeInode(state, newInodeNum); freeBlock(state, newBlockNum);
        return -1;
    }
    free(blockBuffer);

    if (addEntryToDir(state, parentInodeNum, &parentInode, dirName, newInodeNum, fileType) != 0) {
        free(dirName); return -1;
    }
    free(dirName);

    readBytes(&linksCount, &parentInode.iLinksCount);
    linksCount++;
    writeBytes(&parentInode.iLinksCount, &linksCount);
    writeBytes(&parentInode.iMtime, &now);

    if (writeInode(state, parentInodeNum, &parentInode) != 0) return -1;
    
    uint32_t group = (newInodeNum - 1) / inodesPerGroup;
    Ext4GroupDesc *groupDesc = &state->groupDescs[group];
    uint16_t usedDirsLo, usedDirsHi;
    readBytes(&usedDirsLo, &groupDesc->bgUsedDirsCountLo);
    uint32_t usedDirs = usedDirsLo;
    if (state->is64bit) {
        readBytes(&usedDirsHi, &groupDesc->bgUsedDirsCountHi);
        usedDirs |= ((uint32_t)usedDirsHi << 16);
    }
    usedDirs++;
    usedDirsLo = usedDirs & 0xFFFF;
    writeBytes(&groupDesc->bgUsedDirsCountLo, &usedDirsLo);
    if (state->is64bit) {
        usedDirsHi = usedDirs >> 16;
        writeBytes(&groupDesc->bgUsedDirsCountHi, &usedDirsHi);
    }
    
    return 0; // Success
}

