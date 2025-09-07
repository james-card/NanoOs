///////////////////////////////////////////////////////////////////////////////
///
/// @author            Assistant
/// @date              01.07.2025
///
/// @file              Ext4Filesystem.c
///
/// @brief             ext4 filesystem driver for NanoOs.
///
/// @copyright
///                   Copyright (c) 2025
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
///////////////////////////////////////////////////////////////////////////////

#include "NanoOs.h"
#include "Filesystem.h"
#include <string.h>
#include <stdlib.h>

// ext4 Constants
#define EXT4_SUPER_MAGIC              0xEF53
#define EXT4_ROOT_INO                 2
#define EXT4_GOOD_OLD_REV             0
#define EXT4_DYNAMIC_REV              1
#define EXT4_GOOD_OLD_INODE_SIZE      128
#define EXT4_NAME_LEN                 255
#define EXT4_NDIR_BLOCKS              12
#define EXT4_IND_BLOCK                12
#define EXT4_DIND_BLOCK               13
#define EXT4_TIND_BLOCK               14
#define EXT4_N_BLOCKS                 15
#define EXT4_MIN_BLOCK_SIZE           1024
#define EXT4_MAX_BLOCK_SIZE           65536
#define EXT4_MIN_DESC_SIZE            32
#define EXT4_MIN_DESC_SIZE_64BIT      64
#define EXT4_EXTENT_MAGIC             0xF30A
#define EXT4_MAX_EXTENT_DEPTH         5

// Inode mode flags
#define EXT4_S_IFMT                  0170000
#define EXT4_S_IFSOCK                 0140000
#define EXT4_S_IFLNK                  0120000
#define EXT4_S_IFREG                  0100000
#define EXT4_S_IFBLK                  0060000
#define EXT4_S_IFDIR                  0040000
#define EXT4_S_IFCHR                  0020000
#define EXT4_S_IFIFO                  0010000
#define EXT4_S_ISUID                  0004000
#define EXT4_S_ISGID                  0002000
#define EXT4_S_ISVTX                  0001000
#define EXT4_S_IRUSR                  0000400
#define EXT4_S_IWUSR                  0000200
#define EXT4_S_IXUSR                  0000100
#define EXT4_S_IRGRP                  0000040
#define EXT4_S_IWGRP                  0000020
#define EXT4_S_IXGRP                  0000010
#define EXT4_S_IROTH                  0000004
#define EXT4_S_IWOTH                  0000002
#define EXT4_S_IXOTH                  0000001

// Inode flags
#define EXT4_INODE_FLAG_EXTENTS       0x00080000
#define EXT4_INODE_FLAG_EA_INODE      0x00200000
#define EXT4_INODE_FLAG_INLINE_DATA   0x10000000

// Feature compatibility flags
#define EXT4_FEATURE_INCOMPAT_EXTENTS 0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT   0x0080
#define EXT4_FEATURE_INCOMPAT_FLEX_BG 0x0200

// Directory entry types
#define EXT4_FT_UNKNOWN               0
#define EXT4_FT_REG_FILE              1
#define EXT4_FT_DIR                   2
#define EXT4_FT_CHRDEV                3
#define EXT4_FT_BLKDEV                4
#define EXT4_FT_FIFO                  5
#define EXT4_FT_SOCK                  6
#define EXT4_FT_SYMLINK               7

// File open modes
#define EXT4_MODE_READ                0x01
#define EXT4_MODE_WRITE               0x02
#define EXT4_MODE_APPEND              0x04
#define EXT4_MODE_CREATE              0x08

// Structure definitions

/// @struct Ext4Superblock
///
/// @brief ext4 superblock structure
typedef struct __attribute__((packed)) Ext4Superblock {
  uint32_t inodesCount;
  uint32_t blocksCountLo;
  uint32_t reservedBlocksCountLo;
  uint32_t freeBlocksCountLo;
  uint32_t freeInodesCount;
  uint32_t firstDataBlock;
  uint32_t logBlockSize;
  uint32_t logClusterSize;
  uint32_t blocksPerGroup;
  uint32_t clustersPerGroup;
  uint32_t inodesPerGroup;
  uint32_t mountTime;
  uint32_t writeTime;
  uint16_t mountCount;
  uint16_t maxMountCount;
  uint16_t magic;
  uint16_t state;
  uint16_t errors;
  uint16_t minorRevLevel;
  uint32_t lastCheck;
  uint32_t checkInterval;
  uint32_t creatorOs;
  uint32_t revLevel;
  uint16_t defaultResUid;
  uint16_t defaultResGid;
  uint32_t firstIno;
  uint16_t inodeSize;
  uint16_t blockGroupNr;
  uint32_t featureCompat;
  uint32_t featureIncompat;
  uint32_t featureRoCompat;
  uint8_t  uuid[16];
  char     volumeName[16];
  char     lastMounted[64];
  uint32_t algorithmUsageBitmap;
  uint8_t  preallocBlocks;
  uint8_t  preallocDirBlocks;
  uint16_t reservedGdtBlocks;
  uint8_t  journalUuid[16];
  uint32_t journalInum;
  uint32_t journalDev;
  uint32_t lastOrphan;
  uint32_t hashSeed[4];
  uint8_t  defHashVersion;
  uint8_t  jnlBackupType;
  uint16_t descSize;
  uint32_t defaultMountOpts;
  uint32_t firstMetaBg;
  uint32_t mkfsTime;
  uint32_t jnlBlocks[17];
  uint32_t blocksCountHi;
  uint32_t reservedBlocksCountHi;
  uint32_t freeBlocksCountHi;
  uint16_t minExtraIsize;
  uint16_t wantExtraIsize;
  uint32_t flags;
  uint16_t raidStride;
  uint16_t mmpInterval;
  uint64_t mmpBlock;
  uint32_t raidStripeWidth;
  uint8_t  logGroupsPerFlex;
  uint8_t  checksumType;
  uint16_t reservedPad;
  uint64_t kbytesWritten;
  uint32_t snapshotInum;
  uint32_t snapshotId;
  uint64_t snapshotReservedBlocksCount;
  uint32_t snapshotList;
  uint32_t errorCount;
  uint32_t firstErrorTime;
  uint32_t firstErrorIno;
  uint64_t firstErrorBlock;
  uint8_t  firstErrorFunc[32];
  uint32_t firstErrorLine;
  uint32_t lastErrorTime;
  uint32_t lastErrorIno;
  uint32_t lastErrorLine;
  uint64_t lastErrorBlock;
  uint8_t  lastErrorFunc[32];
  uint8_t  mountOpts[64];
  uint32_t usrQuotaInum;
  uint32_t grpQuotaInum;
  uint32_t overheadBlocks;
  uint32_t backupBgs[2];
  uint8_t  encryptAlgos[4];
  uint8_t  encryptPwSalt[16];
  uint32_t lpfIno;
  uint32_t prjQuotaInum;
  uint32_t checksumSeed;
  uint8_t  reserved[98];
  uint32_t checksum;
} Ext4Superblock;

/// @struct Ext4GroupDesc
///
/// @brief ext4 block group descriptor
typedef struct __attribute__((packed)) Ext4GroupDesc {
  uint32_t blockBitmapLo;
  uint32_t inodeBitmapLo;
  uint32_t inodeTableLo;
  uint16_t freeBlocksCountLo;
  uint16_t freeInodesCountLo;
  uint16_t usedDirsCountLo;
  uint16_t flags;
  uint32_t excludeBitmapLo;
  uint16_t blockBitmapCsumLo;
  uint16_t inodeBitmapCsumLo;
  uint16_t itableUnusedLo;
  uint16_t checksum;
  uint32_t blockBitmapHi;
  uint32_t inodeBitmapHi;
  uint32_t inodeTableHi;
  uint16_t freeBlocksCountHi;
  uint16_t freeInodesCountHi;
  uint16_t usedDirsCountHi;
  uint16_t itableUnusedHi;
  uint32_t excludeBitmapHi;
  uint16_t blockBitmapCsumHi;
  uint16_t inodeBitmapCsumHi;
  uint32_t reserved;
} Ext4GroupDesc;

/// @struct Ext4Inode
///
/// @brief ext4 inode structure
typedef struct __attribute__((packed)) Ext4Inode {
  uint16_t mode;
  uint16_t uid;
  uint32_t sizeLo;
  uint32_t atime;
  uint32_t ctime;
  uint32_t mtime;
  uint32_t dtime;
  uint16_t gid;
  uint16_t linksCount;
  uint32_t blocksLo;
  uint32_t flags;
  uint32_t version;
  uint8_t  block[60];
  uint32_t generation;
  uint32_t fileAclLo;
  uint32_t sizeHi;
  uint32_t obsoFaddr;
  uint16_t blocksHi;
  uint16_t fileAclHi;
  uint16_t uidHi;
  uint16_t gidHi;
  uint16_t checksumLo;
  uint16_t reserved;
  uint16_t extraIsize;
  uint16_t checksumHi;
  uint32_t ctimeExtra;
  uint32_t mtimeExtra;
  uint32_t atimeExtra;
  uint32_t crtime;
  uint32_t crtimeExtra;
  uint32_t versionHi;
  uint32_t projid;
} Ext4Inode;

/// @struct Ext4ExtentHeader
///
/// @brief ext4 extent header
typedef struct __attribute__((packed)) Ext4ExtentHeader {
  uint16_t magic;
  uint16_t entries;
  uint16_t max;
  uint16_t depth;
  uint32_t generation;
} Ext4ExtentHeader;

/// @struct Ext4Extent
///
/// @brief ext4 extent leaf node
typedef struct __attribute__((packed)) Ext4Extent {
  uint32_t block;
  uint16_t len;
  uint16_t startHi;
  uint32_t startLo;
} Ext4Extent;

/// @struct Ext4ExtentIdx
///
/// @brief ext4 extent index node
typedef struct __attribute__((packed)) Ext4ExtentIdx {
  uint32_t block;
  uint32_t leafLo;
  uint16_t leafHi;
  uint16_t unused;
} Ext4ExtentIdx;

/// @struct Ext4DirEntry
///
/// @brief ext4 directory entry
typedef struct __attribute__((packed)) Ext4DirEntry {
  uint32_t inode;
  uint16_t recLen;
  uint8_t  nameLen;
  uint8_t  fileType;
  char     name[EXT4_NAME_LEN];
} Ext4DirEntry;

/// @struct Ext4FileHandle
///
/// @brief File handle for an open file
typedef struct Ext4FileHandle {
  uint32_t inodeNumber;
  Ext4Inode *inode;
  uint64_t currentPosition;
  uint32_t mode;
  struct Ext4FileHandle *next;
} Ext4FileHandle;

/// @struct Ext4State
///
/// @brief State structure for the ext4 filesystem
typedef struct Ext4State {
  FilesystemState *filesystemState;
  Ext4Superblock *superblock;
  uint32_t blockSize;
  uint32_t inodeSize;
  uint32_t groupDescSize;
  uint32_t groupsCount;
  uint32_t inodesPerGroup;
  uint32_t blocksPerGroup;
  Ext4GroupDesc *groupDescs;
  Ext4FileHandle *openFiles;
} Ext4State;

// Forward declarations
static int ext4ReadBlock(Ext4State *state, uint32_t blockNum, void *buffer);
static int ext4WriteBlock(Ext4State *state, uint32_t blockNum, 
  const void *buffer);
static int ext4ReadInode(Ext4State *state, uint32_t inodeNum, 
  Ext4Inode *inode);
static int ext4WriteInode(Ext4State *state, uint32_t inodeNum, 
  const Ext4Inode *inode);
static uint32_t ext4AllocateBlock(Ext4State *state);
static void ext4FreeBlock(Ext4State *state, uint32_t blockNum);
static uint32_t ext4AllocateInode(Ext4State *state);
static void ext4FreeInode(Ext4State *state, uint32_t inodeNum);
static uint64_t ext4GetBlockFromExtent(Ext4State *state, 
  const Ext4Inode *inode, uint32_t fileBlock);
static int ext4SetBlockInExtent(Ext4State *state, Ext4Inode *inode, 
  uint32_t fileBlock, uint64_t physBlock);
static uint32_t ext4FindInodeByPath(Ext4State *state, const char *path);
static int ext4CreateDirEntry(Ext4State *state, uint32_t parentInode, 
  const char *name, uint32_t inodeNum, uint8_t fileType);
static int ext4RemoveDirEntry(Ext4State *state, uint32_t parentInode, 
  const char *name);

/// @brief Initialize the ext4 filesystem
///
/// @param state Pointer to the ext4 state structure
///
/// @return 0 on success, negative on error
int ext4Initialize(Ext4State *state) {
  if (!state || !state->filesystemState) {
    return -1;
  }
  
  // Allocate superblock
  state->superblock = (Ext4Superblock*) malloc(sizeof(Ext4Superblock));
  if (!state->superblock) {
    return -1;
  }
  
  // Read superblock from block 1 (or block 0 if blocksize > 1024)
  uint32_t sbBlock = (state->filesystemState->blockSize > 1024) ? 0 : 1;
  uint32_t sbOffset = 1024 % state->filesystemState->blockSize;
  
  uint8_t *tempBuffer = (uint8_t*) malloc(state->filesystemState->blockSize);
  if (!tempBuffer) {
    free(state->superblock);
    return -1;
  }
  
  if (state->filesystemState->blockDevice->readBlocks(
      state->filesystemState->blockDevice->context,
      state->filesystemState->startLba + sbBlock,
      1,
      state->filesystemState->blockSize,
      tempBuffer) != 0) {
    free(tempBuffer);
    free(state->superblock);
    return -1;
  }
  
  // Copy superblock data
  copyBytes(state->superblock, tempBuffer + sbOffset, 
    sizeof(Ext4Superblock));
  free(tempBuffer);
  
  // Verify magic number
  uint16_t magic;
  readBytes(&magic, &state->superblock->magic);
  if (magic != EXT4_SUPER_MAGIC) {
    free(state->superblock);
    return -1;
  }
  
  // Calculate filesystem parameters
  uint32_t logBlockSize;
  readBytes(&logBlockSize, &state->superblock->logBlockSize);
  state->blockSize = EXT4_MIN_BLOCK_SIZE << logBlockSize;
  
  uint16_t inodeSize;
  readBytes(&inodeSize, &state->superblock->inodeSize);
  state->inodeSize = inodeSize;
  if (state->inodeSize == 0) {
    state->inodeSize = EXT4_GOOD_OLD_INODE_SIZE;
  }
  
  uint16_t descSize;
  readBytes(&descSize, &state->superblock->descSize);
  state->groupDescSize = descSize;
  if (state->groupDescSize == 0) {
    state->groupDescSize = EXT4_MIN_DESC_SIZE;
  }
  
  uint32_t blocksCount, inodesCount, blocksPerGroup, inodesPerGroup;
  readBytes(&blocksCount, &state->superblock->blocksCountLo);
  readBytes(&inodesCount, &state->superblock->inodesCount);
  readBytes(&blocksPerGroup, &state->superblock->blocksPerGroup);
  readBytes(&inodesPerGroup, &state->superblock->inodesPerGroup);
  
  state->blocksPerGroup = blocksPerGroup;
  state->inodesPerGroup = inodesPerGroup;
  state->groupsCount = (blocksCount + blocksPerGroup - 1) / blocksPerGroup;
  
  // Allocate and read group descriptors
  uint32_t gdtBlocks = (state->groupsCount * state->groupDescSize + 
    state->blockSize - 1) / state->blockSize;
  state->groupDescs = (Ext4GroupDesc*) malloc(gdtBlocks * state->blockSize);
  if (!state->groupDescs) {
    free(state->superblock);
    return -1;
  }
  
  uint32_t gdtBlock = (state->blockSize > 1024) ? 1 : 2;
  for (uint32_t ii = 0; ii < gdtBlocks; ii++) {
    if (ext4ReadBlock(state, gdtBlock + ii, 
        (uint8_t*)state->groupDescs + ii * state->blockSize) != 0) {
      free(state->groupDescs);
      free(state->superblock);
      return -1;
    }
  }
  
  state->openFiles = NULL;
  return 0;
}

/// @brief Clean up the ext4 filesystem
///
/// @param state Pointer to the ext4 state structure
void ext4Cleanup(Ext4State *state) {
  if (!state) {
    return;
  }
  
  // Close all open files
  Ext4FileHandle *current = state->openFiles;
  while (current) {
    Ext4FileHandle *next = current->next;
    if (current->inode) {
      free(current->inode);
    }
    free(current);
    current = next;
  }
  
  if (state->groupDescs) {
    free(state->groupDescs);
  }
  if (state->superblock) {
    free(state->superblock);
  }
}

/// @brief Read a block from the filesystem
///
/// @param state Pointer to the ext4 state structure
/// @param blockNum The block number to read
/// @param buffer Buffer to read the block into
///
/// @return 0 on success, negative on error
static int ext4ReadBlock(Ext4State *state, uint32_t blockNum, void *buffer) {
  if (!state || !state->filesystemState || !buffer) {
    return -1;
  }
  
  return state->filesystemState->blockDevice->readBlocks(
    state->filesystemState->blockDevice->context,
    state->filesystemState->startLba + blockNum,
    1,
    state->blockSize,
    buffer);
}

/// @brief Write a block to the filesystem
///
/// @param state Pointer to the ext4 state structure
/// @param blockNum The block number to write
/// @param buffer Buffer containing the data to write
///
/// @return 0 on success, negative on error
static int ext4WriteBlock(Ext4State *state, uint32_t blockNum, 
    const void *buffer) {
  if (!state || !state->filesystemState || !buffer) {
    return -1;
  }
  
  return state->filesystemState->blockDevice->writeBlocks(
    state->filesystemState->blockDevice->context,
    state->filesystemState->startLba + blockNum,
    1,
    state->blockSize,
    buffer);
}

/// @brief Read an inode from the filesystem
///
/// @param state Pointer to the ext4 state structure
/// @param inodeNum The inode number to read
/// @param inode Buffer to read the inode into
///
/// @return 0 on success, negative on error
static int ext4ReadInode(Ext4State *state, uint32_t inodeNum, 
    Ext4Inode *inode) {
  if (!state || !inode || inodeNum == 0) {
    return -1;
  }
  
  uint32_t group = (inodeNum - 1) / state->inodesPerGroup;
  uint32_t index = (inodeNum - 1) % state->inodesPerGroup;
  
  if (group >= state->groupsCount) {
    return -1;
  }
  
  // Get inode table location
  Ext4GroupDesc *gd = &state->groupDescs[group];
  uint32_t inodeTableLo;
  readBytes(&inodeTableLo, &gd->inodeTableLo);
  
  uint32_t inodeBlock = inodeTableLo + 
    (index * state->inodeSize) / state->blockSize;
  uint32_t inodeOffset = (index * state->inodeSize) % state->blockSize;
  
  uint8_t *buffer = (uint8_t*) malloc(state->blockSize);
  if (!buffer) {
    return -1;
  }
  
  if (ext4ReadBlock(state, inodeBlock, buffer) != 0) {
    free(buffer);
    return -1;
  }
  
  copyBytes(inode, buffer + inodeOffset, sizeof(Ext4Inode));
  free(buffer);
  
  return 0;
}

/// @brief Write an inode to the filesystem
///
/// @param state Pointer to the ext4 state structure
/// @param inodeNum The inode number to write
/// @param inode The inode data to write
///
/// @return 0 on success, negative on error
static int ext4WriteInode(Ext4State *state, uint32_t inodeNum, 
    const Ext4Inode *inode) {
  if (!state || !inode || inodeNum == 0) {
    return -1;
  }
  
  uint32_t group = (inodeNum - 1) / state->inodesPerGroup;
  uint32_t index = (inodeNum - 1) % state->inodesPerGroup;
  
  if (group >= state->groupsCount) {
    return -1;
  }
  
  // Get inode table location
  Ext4GroupDesc *gd = &state->groupDescs[group];
  uint32_t inodeTableLo;
  readBytes(&inodeTableLo, &gd->inodeTableLo);
  
  uint32_t inodeBlock = inodeTableLo + 
    (index * state->inodeSize) / state->blockSize;
  uint32_t inodeOffset = (index * state->inodeSize) % state->blockSize;
  
  uint8_t *buffer = (uint8_t*) malloc(state->blockSize);
  if (!buffer) {
    return -1;
  }
  
  if (ext4ReadBlock(state, inodeBlock, buffer) != 0) {
    free(buffer);
    return -1;
  }
  
  copyBytes(buffer + inodeOffset, inode, sizeof(Ext4Inode));
  
  int result = ext4WriteBlock(state, inodeBlock, buffer);
  free(buffer);
  
  return result;
}

/// @brief Get a block number from an extent tree
///
/// @param state Pointer to the ext4 state structure
/// @param inode The inode containing the extent tree
/// @param fileBlock The logical block number in the file
///
/// @return Physical block number, or 0 if not allocated
static uint64_t ext4GetBlockFromExtent(Ext4State *state, 
    const Ext4Inode *inode, uint32_t fileBlock) {
  if (!state || !inode) {
    return 0;
  }
  
  uint32_t flags;
  readBytes(&flags, &inode->flags);
  if (!(flags & EXT4_INODE_FLAG_EXTENTS)) {
    // Not using extents, handle traditional block pointers
    // This is simplified - full implementation would handle indirect blocks
    if (fileBlock < EXT4_NDIR_BLOCKS) {
      uint32_t block;
      readBytes(&block, &inode->block[fileBlock * 4]);
      return block;
    }
    return 0;
  }
  
  // Read extent header from inode
  Ext4ExtentHeader header;
  copyBytes(&header, inode->block, sizeof(Ext4ExtentHeader));
  
  uint16_t magic, depth;
  readBytes(&magic, &header.magic);
  readBytes(&depth, &header.depth);
  
  if (magic != EXT4_EXTENT_MAGIC) {
    return 0;
  }
  
  if (depth == 0) {
    // Leaf node - contains actual extents
    uint16_t entries;
    readBytes(&entries, &header.entries);
    
    Ext4Extent *extents = (Ext4Extent*)(inode->block + 
      sizeof(Ext4ExtentHeader));
    
    for (uint16_t ii = 0; ii < entries; ii++) {
      uint32_t block;
      uint16_t len;
      readBytes(&block, &extents[ii].block);
      readBytes(&len, &extents[ii].len);
      
      if (fileBlock >= block && fileBlock < block + len) {
        uint32_t startLo;
        uint16_t startHi;
        readBytes(&startLo, &extents[ii].startLo);
        readBytes(&startHi, &extents[ii].startHi);
        
        uint64_t physBlock = ((uint64_t)startHi << 32) | startLo;
        return physBlock + (fileBlock - block);
      }
    }
  } else {
    // Index node - need to traverse tree
    // Simplified implementation - full version would recursively traverse
    return 0;
  }
  
  return 0;
}

/// @brief Find an inode number by path
///
/// @param state Pointer to the ext4 state structure
/// @param path The path to search for
///
/// @return Inode number, or 0 if not found
static uint32_t ext4FindInodeByPath(Ext4State *state, const char *path) {
  if (!state || !path) {
    return 0;
  }
  
  // Start at root inode
  uint32_t currentInode = EXT4_ROOT_INO;
  
  if (path[0] == '/') {
    path++;
  }
  
  if (strlen(path) == 0) {
    return currentInode;
  }
  
  char *pathCopy = (char*) malloc(strlen(path) + 1);
  if (!pathCopy) {
    return 0;
  }
  strcpy(pathCopy, path);
  
  char *token = strtok(pathCopy, "/");
  
  while (token != NULL) {
    // Read current directory inode
    Ext4Inode dirInode;
    if (ext4ReadInode(state, currentInode, &dirInode) != 0) {
      free(pathCopy);
      return 0;
    }
    
    // Check if it's a directory
    uint16_t mode;
    readBytes(&mode, &dirInode.mode);
    if ((mode & EXT4_S_IFMT) != EXT4_S_IFDIR) {
      free(pathCopy);
      return 0;
    }
    
    // Search directory for entry
    uint32_t sizeLo;
    readBytes(&sizeLo, &dirInode.sizeLo);
    uint32_t blockCount = (sizeLo + state->blockSize - 1) / state->blockSize;
    
    uint8_t *dirBuffer = (uint8_t*) malloc(state->blockSize);
    if (!dirBuffer) {
      free(pathCopy);
      return 0;
    }
    
    bool found = false;
    for (uint32_t ii = 0; ii < blockCount && !found; ii++) {
      uint64_t blockNum = ext4GetBlockFromExtent(state, &dirInode, ii);
      if (blockNum == 0) {
        continue;
      }
      
      if (ext4ReadBlock(state, blockNum, dirBuffer) != 0) {
        continue;
      }
      
      uint32_t offset = 0;
      while (offset < state->blockSize) {
        Ext4DirEntry entry;
        copyBytes(&entry, dirBuffer + offset, 8);  // Read fixed part
        
        uint32_t inodeNum;
        uint16_t recLen;
        uint8_t nameLen;
        readBytes(&inodeNum, &entry.inode);
        readBytes(&recLen, &entry.recLen);
        readBytes(&nameLen, &entry.nameLen);
        
        if (recLen == 0) {
          break;
        }
        
        if (inodeNum != 0 && nameLen == strlen(token)) {
          copyBytes(entry.name, dirBuffer + offset + 8, nameLen);
          entry.name[nameLen] = '\0';
          
          if (strcmp(entry.name, token) == 0) {
            currentInode = inodeNum;
            found = true;
            break;
          }
        }
        
        offset += recLen;
      }
    }
    
    free(dirBuffer);
    
    if (!found) {
      free(pathCopy);
      return 0;
    }
    
    token = strtok(NULL, "/");
  }
  
  free(pathCopy);
  return currentInode;
}

/// @brief Allocate a new block from the filesystem
///
/// @param state Pointer to the ext4 state structure
///
/// @return Block number, or 0 if no blocks available
static uint32_t ext4AllocateBlock(Ext4State *state) {
  if (!state) {
    return 0;
  }
  
  uint8_t *bitmap = (uint8_t*) malloc(state->blockSize);
  if (!bitmap) {
    return 0;
  }
  
  for (uint32_t group = 0; group < state->groupsCount; group++) {
    Ext4GroupDesc *gd = &state->groupDescs[group];
    uint16_t freeBlocks;
    readBytes(&freeBlocks, &gd->freeBlocksCountLo);
    
    if (freeBlocks == 0) {
      continue;
    }
    
    uint32_t bitmapBlock;
    readBytes(&bitmapBlock, &gd->blockBitmapLo);
    
    if (ext4ReadBlock(state, bitmapBlock, bitmap) != 0) {
      continue;
    }
    
    // Find free bit in bitmap
    for (uint32_t byte = 0; byte < state->blockSize; byte++) {
      if (bitmap[byte] != 0xFF) {
        for (uint32_t bit = 0; bit < 8; bit++) {
          if (!(bitmap[byte] & (1 << bit))) {
            // Found free block
            bitmap[byte] |= (1 << bit);
            
            // Write updated bitmap
            if (ext4WriteBlock(state, bitmapBlock, bitmap) == 0) {
              // Update group descriptor
              freeBlocks--;
              writeBytes(&gd->freeBlocksCountLo, &freeBlocks);
              
              // Calculate absolute block number
              uint32_t blockNum = group * state->blocksPerGroup + 
                byte * 8 + bit;
              
              free(bitmap);
              return blockNum;
            }
          }
        }
      }
    }
  }
  
  free(bitmap);
  return 0;
}

/// @brief Free a block back to the filesystem
///
/// @param state Pointer to the ext4 state structure
/// @param blockNum The block number to free
static void ext4FreeBlock(Ext4State *state, uint32_t blockNum) {
  if (!state || blockNum == 0) {
    return;
  }
  
  uint32_t group = blockNum / state->blocksPerGroup;
  uint32_t blockInGroup = blockNum % state->blocksPerGroup;
  
  if (group >= state->groupsCount) {
    return;
  }
  
  uint8_t *bitmap = (uint8_t*) malloc(state->blockSize);
  if (!bitmap) {
    return;
  }
  
  Ext4GroupDesc *gd = &state->groupDescs[group];
  uint32_t bitmapBlock;
  readBytes(&bitmapBlock, &gd->blockBitmapLo);
  
  if (ext4ReadBlock(state, bitmapBlock, bitmap) != 0) {
    free(bitmap);
    return;
  }
  
  uint32_t byte = blockInGroup / 8;
  uint32_t bit = blockInGroup % 8;
  
  if (bitmap[byte] & (1 << bit)) {
    bitmap[byte] &= ~(1 << bit);
    
    if (ext4WriteBlock(state, bitmapBlock, bitmap) == 0) {
      uint16_t freeBlocks;
      readBytes(&freeBlocks, &gd->freeBlocksCountLo);
      freeBlocks++;
      writeBytes(&gd->freeBlocksCountLo, &freeBlocks);
    }
  }
  
  free(bitmap);
}

/// @brief Allocate a new inode from the filesystem
///
/// @param state Pointer to the ext4 state structure
///
/// @return Inode number, or 0 if no inodes available
static uint32_t ext4AllocateInode(Ext4State *state) {
  if (!state) {
    return 0;
  }
  
  uint8_t *bitmap = (uint8_t*) malloc(state->blockSize);
  if (!bitmap) {
    return 0;
  }
  
  for (uint32_t group = 0; group < state->groupsCount; group++) {
    Ext4GroupDesc *gd = &state->groupDescs[group];
    uint16_t freeInodes;
    readBytes(&freeInodes, &gd->freeInodesCountLo);
    
    if (freeInodes == 0) {
      continue;
    }
    
    uint32_t bitmapBlock;
    readBytes(&bitmapBlock, &gd->inodeBitmapLo);
    
    if (ext4ReadBlock(state, bitmapBlock, bitmap) != 0) {
      continue;
    }
    
    // Find free bit in bitmap
    for (uint32_t byte = 0; byte < state->blockSize; byte++) {
      if (bitmap[byte] != 0xFF) {
        for (uint32_t bit = 0; bit < 8; bit++) {
          if (!(bitmap[byte] & (1 << bit))) {
            // Check if this inode number is valid
            uint32_t inodeNum = group * state->inodesPerGroup + 
              byte * 8 + bit + 1;
            
            uint32_t firstIno;
            readBytes(&firstIno, &state->superblock->firstIno);
            if (inodeNum < firstIno) {
              continue;
            }
            
            // Found free inode
            bitmap[byte] |= (1 << bit);
            
            // Write updated bitmap
            if (ext4WriteBlock(state, bitmapBlock, bitmap) == 0) {
              // Update group descriptor
              freeInodes--;
              writeBytes(&gd->freeInodesCountLo, &freeInodes);
              
              free(bitmap);
              return inodeNum;
            }
          }
        }
      }
    }
  }
  
  free(bitmap);
  return 0;
}

/// @brief Free an inode back to the filesystem
///
/// @param state Pointer to the ext4 state structure
/// @param inodeNum The inode number to free
static void ext4FreeInode(Ext4State *state, uint32_t inodeNum) {
  if (!state || inodeNum == 0) {
    return;
  }
  
  uint32_t group = (inodeNum - 1) / state->inodesPerGroup;
  uint32_t inodeInGroup = (inodeNum - 1) % state->inodesPerGroup;
  
  if (group >= state->groupsCount) {
    return;
  }
  
  uint8_t *bitmap = (uint8_t*) malloc(state->blockSize);
  if (!bitmap) {
    return;
  }
  
  Ext4GroupDesc *gd = &state->groupDescs[group];
  uint32_t bitmapBlock;
  readBytes(&bitmapBlock, &gd->inodeBitmapLo);
  
  if (ext4ReadBlock(state, bitmapBlock, bitmap) != 0) {
    free(bitmap);
    return;
  }
  
  uint32_t byte = inodeInGroup / 8;
  uint32_t bit = inodeInGroup % 8;
  
  if (bitmap[byte] & (1 << bit)) {
    bitmap[byte] &= ~(1 << bit);
    
    if (ext4WriteBlock(state, bitmapBlock, bitmap) == 0) {
      uint16_t freeInodes;
      readBytes(&freeInodes, &gd->freeInodesCountLo);
      freeInodes++;
      writeBytes(&gd->freeInodesCountLo, &freeInodes);
    }
  }
  
  free(bitmap);
}

/// @brief Create a directory entry
///
/// @param state Pointer to the ext4 state structure
/// @param parentInode The parent directory inode number
/// @param name The name of the entry to create
/// @param inodeNum The inode number for the new entry
/// @param fileType The type of file
///
/// @return 0 on success, negative on error
static int ext4CreateDirEntry(Ext4State *state, uint32_t parentInode,
    const char *name, uint32_t inodeNum, uint8_t fileType) {
  if (!state || !name || strlen(name) > EXT4_NAME_LEN) {
    return -1;
  }
  
  Ext4Inode dirInode;
  if (ext4ReadInode(state, parentInode, &dirInode) != 0) {
    return -1;
  }
  
  uint32_t nameLen = strlen(name);
  uint32_t recLen = ((8 + nameLen + 3) / 4) * 4;  // Align to 4 bytes
  
  uint32_t sizeLo;
  readBytes(&sizeLo, &dirInode.sizeLo);
  uint32_t blockCount = (sizeLo + state->blockSize - 1) / state->blockSize;
  
  uint8_t *dirBuffer = (uint8_t*) malloc(state->blockSize);
  if (!dirBuffer) {
    return -1;
  }
  
  // Try to find space in existing blocks
  for (uint32_t ii = 0; ii < blockCount; ii++) {
    uint64_t blockNum = ext4GetBlockFromExtent(state, &dirInode, ii);
    if (blockNum == 0) {
      continue;
    }
    
    if (ext4ReadBlock(state, blockNum, dirBuffer) != 0) {
      continue;
    }
    
    uint32_t offset = 0;
    uint32_t lastOffset = 0;
    
    while (offset < state->blockSize) {
      Ext4DirEntry entry;
      copyBytes(&entry, dirBuffer + offset, 8);
      
      uint16_t currentRecLen;
      uint8_t currentNameLen;
      readBytes(&currentRecLen, &entry.recLen);
      readBytes(&currentNameLen, &entry.nameLen);
      
      if (currentRecLen == 0) {
        break;
      }
      
      // Check if this is the last entry and has extra space
      if (offset + currentRecLen >= state->blockSize) {
        uint32_t actualLen = ((8 + currentNameLen + 3) / 4) * 4;
        if (currentRecLen >= actualLen + recLen) {
          // Split this entry
          writeBytes(&entry.recLen, &actualLen);
          copyBytes(dirBuffer + offset, &entry, 8);
          
          // Create new entry
          Ext4DirEntry newEntry;
          writeBytes(&newEntry.inode, &inodeNum);
          uint16_t newRecLen = currentRecLen - actualLen;
          writeBytes(&newEntry.recLen, &newRecLen);
          writeBytes(&newEntry.nameLen, &nameLen);
          writeBytes(&newEntry.fileType, &fileType);
          
          copyBytes(dirBuffer + offset + actualLen, &newEntry, 8);
          copyBytes(dirBuffer + offset + actualLen + 8, name, nameLen);
          
          if (ext4WriteBlock(state, blockNum, dirBuffer) == 0) {
            free(dirBuffer);
            return 0;
          }
        }
      }
      
      lastOffset = offset;
      offset += currentRecLen;
    }
  }
  
  // Need to allocate new block
  uint32_t newBlock = ext4AllocateBlock(state);
  if (newBlock == 0) {
    free(dirBuffer);
    return -1;
  }
  
  // Create entry in new block
  Ext4DirEntry newEntry;
  writeBytes(&newEntry.inode, &inodeNum);
  uint16_t newRecLen = state->blockSize;
  writeBytes(&newEntry.recLen, &newRecLen);
  writeBytes(&newEntry.nameLen, &nameLen);
  writeBytes(&newEntry.fileType, &fileType);
  
  memset(dirBuffer, 0, state->blockSize);
  copyBytes(dirBuffer, &newEntry, 8);
  copyBytes(dirBuffer + 8, name, nameLen);
  
  if (ext4WriteBlock(state, newBlock, dirBuffer) != 0) {
    ext4FreeBlock(state, newBlock);
    free(dirBuffer);
    return -1;
  }
  
  // Update directory size
  sizeLo += state->blockSize;
  writeBytes(&dirInode.sizeLo, &sizeLo);
  
  // Add block to extent tree (simplified)
  if (ext4SetBlockInExtent(state, &dirInode, blockCount, newBlock) != 0) {
    ext4FreeBlock(state, newBlock);
    free(dirBuffer);
    return -1;
  }
  
  if (ext4WriteInode(state, parentInode, &dirInode) != 0) {
    ext4FreeBlock(state, newBlock);
    free(dirBuffer);
    return -1;
  }
  
  free(dirBuffer);
  return 0;
}

/// @brief Set a block in an extent tree (simplified)
///
/// @param state Pointer to the ext4 state structure
/// @param inode The inode to modify
/// @param fileBlock The logical block number
/// @param physBlock The physical block number to set
///
/// @return 0 on success, negative on error
static int ext4SetBlockInExtent(Ext4State *state, Ext4Inode *inode,
    uint32_t fileBlock, uint64_t physBlock) {
  if (!state || !inode) {
    return -1;
  }
  
  uint32_t flags;
  readBytes(&flags, &inode->flags);
  
  if (!(flags & EXT4_INODE_FLAG_EXTENTS)) {
    // Traditional block pointers
    if (fileBlock < EXT4_NDIR_BLOCKS) {
      uint32_t block = (uint32_t)physBlock;
      writeBytes(&inode->block[fileBlock * 4], &block);
      return 0;
    }
    // Would need to handle indirect blocks here
    return -1;
  }
  
  // Handle extent tree (simplified - only handles simple cases)
  Ext4ExtentHeader header;
  copyBytes(&header, inode->block, sizeof(Ext4ExtentHeader));
  
  uint16_t entries;
  readBytes(&entries, &header.entries);
  
  if (entries < 4) {  // Room for new extent in inode
    Ext4Extent *extents = (Ext4Extent*)(inode->block + 
      sizeof(Ext4ExtentHeader));
    
    // Create new extent
    Ext4Extent newExtent;
    writeBytes(&newExtent.block, &fileBlock);
    uint16_t len = 1;
    writeBytes(&newExtent.len, &len);
    uint16_t startHi = (physBlock >> 32) & 0xFFFF;
    uint32_t startLo = physBlock & 0xFFFFFFFF;
    writeBytes(&newExtent.startHi, &startHi);
    writeBytes(&newExtent.startLo, &startLo);
    
    copyBytes(&extents[entries], &newExtent, sizeof(Ext4Extent));
    
    entries++;
    writeBytes(&header.entries, &entries);
    copyBytes(inode->block, &header, sizeof(Ext4ExtentHeader));
    
    return 0;
  }
  
  // Would need to handle extent tree expansion here
  return -1;
}

/// @brief Remove a directory entry
///
/// @param state Pointer to the ext4 state structure
/// @param parentInode The parent directory inode
/// @param name The name to remove
///
/// @return 0 on success, negative on error
static int ext4RemoveDirEntry(Ext4State *state, uint32_t parentInode,
    const char *name) {
  if (!state || !name) {
    return -1;
  }
  
  Ext4Inode dirInode;
  if (ext4ReadInode(state, parentInode, &dirInode) != 0) {
    return -1;
  }
  
  uint32_t sizeLo;
  readBytes(&sizeLo, &dirInode.sizeLo);
  uint32_t blockCount = (sizeLo + state->blockSize - 1) / state->blockSize;
  
  uint8_t *dirBuffer = (uint8_t*) malloc(state->blockSize);
  if (!dirBuffer) {
    return -1;
  }
  
  for (uint32_t ii = 0; ii < blockCount; ii++) {
    uint64_t blockNum = ext4GetBlockFromExtent(state, &dirInode, ii);
    if (blockNum == 0) {
      continue;
    }
    
    if (ext4ReadBlock(state, blockNum, dirBuffer) != 0) {
      continue;
    }
    
    uint32_t offset = 0;
    uint32_t prevOffset = 0;
    Ext4DirEntry *prevEntry = NULL;
    
    while (offset < state->blockSize) {
      Ext4DirEntry entry;
      copyBytes(&entry, dirBuffer + offset, 8);
      
      uint32_t inodeNum;
      uint16_t recLen;
      uint8_t nameLen;
      readBytes(&inodeNum, &entry.inode);
      readBytes(&recLen, &entry.recLen);
      readBytes(&nameLen, &entry.nameLen);
      
      if (recLen == 0) {
        break;
      }
      
      if (inodeNum != 0 && nameLen == strlen(name)) {
        copyBytes(entry.name, dirBuffer + offset + 8, nameLen);
        entry.name[nameLen] = '\0';
        
        if (strcmp(entry.name, name) == 0) {
          // Found entry to remove
          if (prevEntry) {
            // Extend previous entry to cover this one
            uint16_t prevRecLen;
            readBytes(&prevRecLen, &prevEntry->recLen);
            prevRecLen += recLen;
            writeBytes(&prevEntry->recLen, &prevRecLen);
            copyBytes(dirBuffer + prevOffset + 6, &prevEntry->recLen, 2);
          } else {
            // First entry - mark as deleted
            uint32_t zero = 0;
            writeBytes(&entry.inode, &zero);
            copyBytes(dirBuffer + offset, &entry.inode, 4);
          }
          
          if (ext4WriteBlock(state, blockNum, dirBuffer) == 0) {
            free(dirBuffer);
            return 0;
          }
        }
      }
      
      prevEntry = &entry;
      prevOffset = offset;
      offset += recLen;
    }
  }
  
  free(dirBuffer);
  return -1;
}

/// @brief Open a file
///
/// @param state Pointer to the ext4 state structure
/// @param pathname The path to the file
/// @param mode The mode to open the file in
///
/// @return File handle, or NULL on error
FILE* ext4Open(Ext4State *state, const char *pathname, const char *mode) {
  if (!state || !pathname || !mode) {
    return NULL;
  }
  
  // Allocate buffer if needed
  if (state->filesystemState->numOpenFiles == 0) {
    state->filesystemState->blockBuffer = 
      (uint8_t*) malloc(state->filesystemState->blockSize);
    if (!state->filesystemState->blockBuffer) {
      return NULL;
    }
  }
  
  // Parse mode
  uint32_t openMode = 0;
  bool create = false;
  bool truncate = false;
  
  if (strchr(mode, 'r')) {
    openMode |= EXT4_MODE_READ;
  }
  if (strchr(mode, 'w')) {
    openMode |= EXT4_MODE_WRITE;
    create = true;
    truncate = true;
  }
  if (strchr(mode, 'a')) {
    openMode |= EXT4_MODE_WRITE | EXT4_MODE_APPEND;
  }
  if (strchr(mode, '+')) {
    openMode |= EXT4_MODE_READ | EXT4_MODE_WRITE;
  }
  
  // Find or create file
  uint32_t inodeNum = ext4FindInodeByPath(state, pathname);
  
  if (inodeNum == 0 && create) {
    // Need to create file
    // Find parent directory
    char *pathCopy = (char*) malloc(strlen(pathname) + 1);
    if (!pathCopy) {
      if (state->filesystemState->numOpenFiles == 0) {
        free(state->filesystemState->blockBuffer);
        state->filesystemState->blockBuffer = NULL;
      }
      return NULL;
    }
    strcpy(pathCopy, pathname);
    
    char *lastSlash = strrchr(pathCopy, '/');
    char *filename;
    uint32_t parentInode;
    
    if (lastSlash) {
      *lastSlash = '\0';
      filename = lastSlash + 1;
      parentInode = ext4FindInodeByPath(state, pathCopy);
    } else {
      filename = pathCopy;
      parentInode = EXT4_ROOT_INO;  // Current directory
    }
    
    if (parentInode == 0) {
      free(pathCopy);
      if (state->filesystemState->numOpenFiles == 0) {
        free(state->filesystemState->blockBuffer);
        state->filesystemState->blockBuffer = NULL;
      }
      return NULL;
    }
    
    // Allocate new inode
    inodeNum = ext4AllocateInode(state);
    if (inodeNum == 0) {
      free(pathCopy);
      if (state->filesystemState->numOpenFiles == 0) {
        free(state->filesystemState->blockBuffer);
        state->filesystemState->blockBuffer = NULL;
      }
      return NULL;
    }
    
    // Initialize new inode
    Ext4Inode newInode;
    memset(&newInode, 0, sizeof(Ext4Inode));
    
    uint16_t inodeMode = EXT4_S_IFREG | EXT4_S_IRUSR | EXT4_S_IWUSR;
    writeBytes(&newInode.mode, &inodeMode);
    
    uint32_t currentTime = 0;  // Would use real time
    writeBytes(&newInode.atime, &currentTime);
    writeBytes(&newInode.ctime, &currentTime);
    writeBytes(&newInode.mtime, &currentTime);
    
    uint16_t linksCount = 1;
    writeBytes(&newInode.linksCount, &linksCount);
    
    uint32_t inodeFlags = EXT4_INODE_FLAG_EXTENTS;
    writeBytes(&newInode.flags, &inodeFlags);
    
    // Initialize extent header
    Ext4ExtentHeader header;
    uint16_t magic = EXT4_EXTENT_MAGIC;
    uint16_t entries = 0;
    uint16_t max = 4;
    uint16_t depth = 0;
    writeBytes(&header.magic, &magic);
    writeBytes(&header.entries, &entries);
    writeBytes(&header.max, &max);
    writeBytes(&header.depth, &depth);
    
    copyBytes(newInode.block, &header, sizeof(Ext4ExtentHeader));
    
    if (ext4WriteInode(state, inodeNum, &newInode) != 0) {
      ext4FreeInode(state, inodeNum);
      free(pathCopy);
      if (state->filesystemState->numOpenFiles == 0) {
        free(state->filesystemState->blockBuffer);
        state->filesystemState->blockBuffer = NULL;
      }
      return NULL;
    }
    
    // Add directory entry
    if (ext4CreateDirEntry(state, parentInode, filename, inodeNum, 
        EXT4_FT_REG_FILE) != 0) {
      ext4FreeInode(state, inodeNum);
      free(pathCopy);
      if (state->filesystemState->numOpenFiles == 0) {
        free(state->filesystemState->blockBuffer);
        state->filesystemState->blockBuffer = NULL;
      }
      return NULL;
    }
    
    free(pathCopy);
  } else if (inodeNum == 0) {
    if (state->filesystemState->numOpenFiles == 0) {
      free(state->filesystemState->blockBuffer);
      state->filesystemState->blockBuffer = NULL;
    }
    return NULL;
  }
  
  // Allocate file handle
  Ext4FileHandle *handle = (Ext4FileHandle*) malloc(sizeof(Ext4FileHandle));
  if (!handle) {
    if (state->filesystemState->numOpenFiles == 0) {
      free(state->filesystemState->blockBuffer);
      state->filesystemState->blockBuffer = NULL;
    }
    return NULL;
  }
  
  handle->inodeNumber = inodeNum;
  handle->mode = openMode;
  handle->currentPosition = 0;
  
  // Read inode
  handle->inode = (Ext4Inode*) malloc(sizeof(Ext4Inode));
  if (!handle->inode) {
    free(handle);
    if (state->filesystemState->numOpenFiles == 0) {
      free(state->filesystemState->blockBuffer);
      state->filesystemState->blockBuffer = NULL;
    }
    return NULL;
  }
  
  if (ext4ReadInode(state, inodeNum, handle->inode) != 0) {
    free(handle->inode);
    free(handle);
    if (state->filesystemState->numOpenFiles == 0) {
      free(state->filesystemState->blockBuffer);
      state->filesystemState->blockBuffer = NULL;
    }
    return NULL;
  }
  
  // Truncate if needed
  if (truncate) {
    uint32_t zero = 0;
    writeBytes(&handle->inode->sizeLo, &zero);
    writeBytes(&handle->inode->sizeHi, &zero);
    ext4WriteInode(state, inodeNum, handle->inode);
  }
  
  // Set position for append mode
  if (openMode & EXT4_MODE_APPEND) {
    uint32_t sizeLo, sizeHi;
    readBytes(&sizeLo, &handle->inode->sizeLo);
    readBytes(&sizeHi, &handle->inode->sizeHi);
    handle->currentPosition = ((uint64_t)sizeHi << 32) | sizeLo;
  }
  
  // Add to open files list
  handle->next = state->openFiles;
  state->openFiles = handle;
  
  state->filesystemState->numOpenFiles++;
  
  // Return handle cast to FILE*
  return (FILE*)handle;
}

/// @brief Close a file
///
/// @param state Pointer to the ext4 state structure
/// @param stream The file handle to close
///
/// @return 0 on success, negative on error
int ext4Close(Ext4State *state, FILE *stream) {
  if (!state || !stream) {
    return -1;
  }
  
  Ext4FileHandle *handle = (Ext4FileHandle*)stream;
  
  // Remove from open files list
  Ext4FileHandle **current = &state->openFiles;
  while (*current) {
    if (*current == handle) {
      *current = handle->next;
      break;
    }
    current = &(*current)->next;
  }
  
  // Write back inode if modified
  if (handle->mode & EXT4_MODE_WRITE) {
    ext4WriteInode(state, handle->inodeNumber, handle->inode);
  }
  
  // Free handle
  if (handle->inode) {
    free(handle->inode);
  }
  free(handle);
  
  state->filesystemState->numOpenFiles--;
  
  // Free block buffer if no more open files
  if (state->filesystemState->numOpenFiles == 0 && 
      state->filesystemState->blockBuffer) {
    free(state->filesystemState->blockBuffer);
    state->filesystemState->blockBuffer = NULL;
  }
  
  return 0;
}

/// @brief Read from a file
///
/// @param state Pointer to the ext4 state structure
/// @param ptr Buffer to read into
/// @param size Size of each element
/// @param nmemb Number of elements
/// @param stream File handle
///
/// @return Number of elements read
size_t ext4Read(Ext4State *state, void *ptr, size_t size, size_t nmemb,
    FILE *stream) {
  if (!state || !ptr || !stream || size == 0 || nmemb == 0) {
    return 0;
  }
  
  Ext4FileHandle *handle = (Ext4FileHandle*)stream;
  
  if (!(handle->mode & EXT4_MODE_READ)) {
    return 0;
  }
  
  uint32_t sizeLo, sizeHi;
  readBytes(&sizeLo, &handle->inode->sizeLo);
  readBytes(&sizeHi, &handle->inode->sizeHi);
  uint64_t fileSize = ((uint64_t)sizeHi << 32) | sizeLo;
  
  size_t totalBytes = size * nmemb;
  if (handle->currentPosition >= fileSize) {
    return 0;
  }
  
  if (handle->currentPosition + totalBytes > fileSize) {
    totalBytes = fileSize - handle->currentPosition;
  }
  
  uint8_t *buffer = (uint8_t*)ptr;
  size_t bytesRead = 0;
  
  uint8_t *blockBuffer = (uint8_t*) malloc(state->blockSize);
  if (!blockBuffer) {
    return 0;
  }
  
  while (bytesRead < totalBytes) {
    uint32_t fileBlock = handle->currentPosition / state->blockSize;
    uint32_t blockOffset = handle->currentPosition % state->blockSize;
    uint32_t bytesToRead = state->blockSize - blockOffset;
    
    if (bytesRead + bytesToRead > totalBytes) {
      bytesToRead = totalBytes - bytesRead;
    }
    
    uint64_t physBlock = ext4GetBlockFromExtent(state, handle->inode, 
      fileBlock);
    if (physBlock == 0) {
      break;
    }
    
    if (ext4ReadBlock(state, physBlock, blockBuffer) != 0) {
      break;
    }
    
    copyBytes(buffer + bytesRead, blockBuffer + blockOffset, bytesToRead);
    
    bytesRead += bytesToRead;
    handle->currentPosition += bytesToRead;
  }
  
  free(blockBuffer);
  return bytesRead / size;
}

/// @brief Write to a file
///
/// @param state Pointer to the ext4 state structure
/// @param ptr Buffer to write from
/// @param size Size of each element
/// @param nmemb Number of elements
/// @param stream File handle
///
/// @return Number of elements written
size_t ext4Write(Ext4State *state, const void *ptr, size_t size, 
    size_t nmemb, FILE *stream) {
  if (!state || !ptr || !stream || size == 0 || nmemb == 0) {
    return 0;
  }
  
  Ext4FileHandle *handle = (Ext4FileHandle*)stream;
  
  if (!(handle->mode & EXT4_MODE_WRITE)) {
    return 0;
  }
  
  const uint8_t *buffer = (const uint8_t*)ptr;
  size_t totalBytes = size * nmemb;
  size_t bytesWritten = 0;
  
  uint8_t *blockBuffer = (uint8_t*) malloc(state->blockSize);
  if (!blockBuffer) {
    return 0;
  }
  
  while (bytesWritten < totalBytes) {
    uint32_t fileBlock = handle->currentPosition / state->blockSize;
    uint32_t blockOffset = handle->currentPosition % state->blockSize;
    uint32_t bytesToWrite = state->blockSize - blockOffset;
    
    if (bytesWritten + bytesToWrite > totalBytes) {
      bytesToWrite = totalBytes - bytesWritten;
    }
    
    uint64_t physBlock = ext4GetBlockFromExtent(state, handle->inode, 
      fileBlock);
    
    if (physBlock == 0) {
      // Allocate new block
      physBlock = ext4AllocateBlock(state);
      if (physBlock == 0) {
        break;
      }
      
      if (ext4SetBlockInExtent(state, handle->inode, fileBlock, 
          physBlock) != 0) {
        ext4FreeBlock(state, physBlock);
        break;
      }
    }
    
    // Read existing block if not writing full block
    if (blockOffset != 0 || bytesToWrite < state->blockSize) {
      if (ext4ReadBlock(state, physBlock, blockBuffer) != 0) {
        break;
      }
    }
    
    copyBytes(blockBuffer + blockOffset, buffer + bytesWritten, bytesToWrite);
    
    if (ext4WriteBlock(state, physBlock, blockBuffer) != 0) {
      break;
    }
    
    bytesWritten += bytesToWrite;
    handle->currentPosition += bytesToWrite;
  }
  
  // Update file size if needed
  uint32_t sizeLo, sizeHi;
  readBytes(&sizeLo, &handle->inode->sizeLo);
  readBytes(&sizeHi, &handle->inode->sizeHi);
  uint64_t fileSize = ((uint64_t)sizeHi << 32) | sizeLo;
  
  if (handle->currentPosition > fileSize) {
    sizeLo = handle->currentPosition & 0xFFFFFFFF;
    sizeHi = (handle->currentPosition >> 32) & 0xFFFFFFFF;
    writeBytes(&handle->inode->sizeLo, &sizeLo);
    writeBytes(&handle->inode->sizeHi, &sizeHi);
    
    // Update modification time
    uint32_t currentTime = 0;  // Would use real time
    writeBytes(&handle->inode->mtime, &currentTime);
    
    ext4WriteInode(state, handle->inodeNumber, handle->inode);
  }
  
  free(blockBuffer);
  return bytesWritten / size;
}

/// @brief Remove a file or directory
///
/// @param state Pointer to the ext4 state structure
/// @param pathname The path to remove
///
/// @return 0 on success, negative on error
int ext4Remove(Ext4State *state, const char *pathname) {
  if (!state || !pathname) {
    return -1;
  }
  
  // Find the file
  uint32_t inodeNum = ext4FindInodeByPath(state, pathname);
  if (inodeNum == 0) {
    return -1;
  }
  
  // Read the inode
  Ext4Inode inode;
  if (ext4ReadInode(state, inodeNum, &inode) != 0) {
    return -1;
  }
  
  // Check if it's a directory
  uint16_t mode;
  readBytes(&mode, &inode.mode);
  bool isDir = ((mode & EXT4_S_IFMT) == EXT4_S_IFDIR);
  
  if (isDir) {
    // Check if directory is empty
    uint32_t sizeLo;
    readBytes(&sizeLo, &inode.sizeLo);
    
    if (sizeLo > state->blockSize) {
      // Directory not empty (has more than just . and ..)
      return -1;
    }
  }
  
  // Find parent directory
  char *pathCopy = (char*) malloc(strlen(pathname) + 1);
  if (!pathCopy) {
    return -1;
  }
  strcpy(pathCopy, pathname);
  
  char *lastSlash = strrchr(pathCopy, '/');
  char *filename;
  uint32_t parentInode;
  
  if (lastSlash) {
    *lastSlash = '\0';
    filename = lastSlash + 1;
    parentInode = ext4FindInodeByPath(state, pathCopy);
  } else {
    filename = pathCopy;
    parentInode = EXT4_ROOT_INO;
  }
  
  if (parentInode == 0) {
    free(pathCopy);
    return -1;
  }
  
  // Remove directory entry
  if (ext4RemoveDirEntry(state, parentInode, filename) != 0) {
    free(pathCopy);
    return -1;
  }
  
  free(pathCopy);
  
  // Free all blocks used by the file
  uint32_t sizeLo;
  readBytes(&sizeLo, &inode.sizeLo);
  uint32_t blockCount = (sizeLo + state->blockSize - 1) / state->blockSize;
  
  for (uint32_t ii = 0; ii < blockCount; ii++) {
    uint64_t physBlock = ext4GetBlockFromExtent(state, &inode, ii);
    if (physBlock != 0) {
      ext4FreeBlock(state, physBlock);
    }
  }
  
  // Free the inode
  ext4FreeInode(state, inodeNum);
  
  return 0;
}

/// @brief Seek to a position in a file
///
/// @param state Pointer to the ext4 state structure
/// @param stream The file handle
/// @param offset The offset to seek
/// @param whence The seek mode
///
/// @return 0 on success, negative on error
int ext4Seek(Ext4State *state, FILE *stream, long offset, int whence) {
  if (!state || !stream) {
    return -1;
  }
  
  Ext4FileHandle *handle = (Ext4FileHandle*)stream;
  
  uint32_t sizeLo, sizeHi;
  readBytes(&sizeLo, &handle->inode->sizeLo);
  readBytes(&sizeHi, &handle->inode->sizeHi);
  uint64_t fileSize = ((uint64_t)sizeHi << 32) | sizeLo;
  
  int64_t newPosition;
  
  switch (whence) {
    case SEEK_SET:
      newPosition = offset;
      break;
    
    case SEEK_CUR:
      newPosition = handle->currentPosition + offset;
      break;
    
    case SEEK_END:
      newPosition = fileSize + offset;
      break;
    
    default:
      return -1;
  }
  
  if (newPosition < 0) {
    return -1;
  }
  
  handle->currentPosition = newPosition;
  return 0;
}

/// @brief Create a directory
///
/// @param state Pointer to the ext4 state structure
/// @param pathname The path of the directory to create
///
/// @return 0 on success, negative on error
int ext4MkDir(Ext4State *state, const char *pathname) {
  if (!state || !pathname) {
    return -1;
  }
  
  // Check if already exists
  if (ext4FindInodeByPath(state, pathname) != 0) {
    return -1;
  }
  
  // Find parent directory
  char *pathCopy = (char*) malloc(strlen(pathname) + 1);
  if (!pathCopy) {
    return -1;
  }
  strcpy(pathCopy, pathname);
  
  char *lastSlash = strrchr(pathCopy, '/');
  char *dirname;
  uint32_t parentInode;
  
  if (lastSlash) {
    *lastSlash = '\0';
    dirname = lastSlash + 1;
    parentInode = ext4FindInodeByPath(state, pathCopy);
  } else {
    dirname = pathCopy;
    parentInode = EXT4_ROOT_INO;
  }
  
  if (parentInode == 0) {
    free(pathCopy);
    return -1;
  }
  
  // Allocate new inode
  uint32_t inodeNum = ext4AllocateInode(state);
  if (inodeNum == 0) {
    free(pathCopy);
    return -1;
  }
  
  // Initialize directory inode
  Ext4Inode newInode;
  memset(&newInode, 0, sizeof(Ext4Inode));
  
  uint16_t inodeMode = EXT4_S_IFDIR | EXT4_S_IRUSR | EXT4_S_IWUSR | 
    EXT4_S_IXUSR;
  writeBytes(&newInode.mode, &inodeMode);
  
  uint32_t currentTime = 0;  // Would use real time
  writeBytes(&newInode.atime, &currentTime);
  writeBytes(&newInode.ctime, &currentTime);
  writeBytes(&newInode.mtime, &currentTime);
  
  uint16_t linksCount = 2;  // . and parent's link
  writeBytes(&newInode.linksCount, &linksCount);
  
  uint32_t inodeFlags = EXT4_INODE_FLAG_EXTENTS;
  writeBytes(&newInode.flags, &inodeFlags);
  
  // Initialize extent header
  Ext4ExtentHeader header;
  uint16_t magic = EXT4_EXTENT_MAGIC;
  uint16_t entries = 0;
  uint16_t max = 4;
  uint16_t depth = 0;
  writeBytes(&header.magic, &magic);
  writeBytes(&header.entries, &entries);
  writeBytes(&header.max, &max);
  writeBytes(&header.depth, &depth);
  
  copyBytes(newInode.block, &header, sizeof(Ext4ExtentHeader));
  
  // Allocate block for directory entries
  uint32_t dirBlock = ext4AllocateBlock(state);
  if (dirBlock == 0) {
    ext4FreeInode(state, inodeNum);
    free(pathCopy);
    return -1;
  }
  
  // Create . and .. entries
  uint8_t *dirBuffer = (uint8_t*) malloc(state->blockSize);
  if (!dirBuffer) {
    ext4FreeBlock(state, dirBlock);
    ext4FreeInode(state, inodeNum);
    free(pathCopy);
    return -1;
  }
  memset(dirBuffer, 0, state->blockSize);
  
  // . entry
  Ext4DirEntry dotEntry;
  writeBytes(&dotEntry.inode, &inodeNum);
  uint16_t dotRecLen = 12;
  writeBytes(&dotEntry.recLen, &dotRecLen);
  uint8_t dotNameLen = 1;
  writeBytes(&dotEntry.nameLen, &dotNameLen);
  uint8_t dirType = EXT4_FT_DIR;
  writeBytes(&dotEntry.fileType, &dirType);
  
  copyBytes(dirBuffer, &dotEntry, 8);
  dirBuffer[8] = '.';
  
  // .. entry
  Ext4DirEntry dotDotEntry;
  writeBytes(&dotDotEntry.inode, &parentInode);
  uint16_t dotDotRecLen = state->blockSize - 12;
  writeBytes(&dotDotEntry.recLen, &dotDotRecLen);
  uint8_t dotDotNameLen = 2;
  writeBytes(&dotDotEntry.nameLen, &dotDotNameLen);
  writeBytes(&dotDotEntry.fileType, &dirType);
  
  copyBytes(dirBuffer + 12, &dotDotEntry, 8);
  dirBuffer[20] = '.';
  dirBuffer[21] = '.';
  
  if (ext4WriteBlock(state, dirBlock, dirBuffer) != 0) {
    free(dirBuffer);
    ext4FreeBlock(state, dirBlock);
    ext4FreeInode(state, inodeNum);
    free(pathCopy);
    return -1;
  }
  
  free(dirBuffer);
  
  // Update inode with block
  uint32_t dirSize = state->blockSize;
  writeBytes(&newInode.sizeLo, &dirSize);
  
  if (ext4SetBlockInExtent(state, &newInode, 0, dirBlock) != 0) {
    ext4FreeBlock(state, dirBlock);
    ext4FreeInode(state, inodeNum);
    free(pathCopy);
    return -1;
  }
  
  if (ext4WriteInode(state, inodeNum, &newInode) != 0) {
    ext4FreeBlock(state, dirBlock);
    ext4FreeInode(state, inodeNum);
    free(pathCopy);
    return -1;
  }
  
  // Add entry to parent directory
  if (ext4CreateDirEntry(state, parentInode, dirname, inodeNum, 
      EXT4_FT_DIR) != 0) {
    ext4FreeBlock(state, dirBlock);
    ext4FreeInode(state, inodeNum);
    free(pathCopy);
    return -1;
  }
  
  // Update parent's link count
  Ext4Inode parentInodeData;
  if (ext4ReadInode(state, parentInode, &parentInodeData) == 0) {
    uint16_t parentLinks;
    readBytes(&parentLinks, &parentInodeData.linksCount);
    parentLinks++;
    writeBytes(&parentInodeData.linksCount, &parentLinks);
    ext4WriteInode(state, parentInode, &parentInodeData);
  }
  
  free(pathCopy);
  return 0;
}