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

#include "Ext4FilesystemClaude.h"
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
  FilesystemState *fs;
  Ext4Superblock *superblock;
  uint32_t inodeSize;
  uint32_t groupDescSize;
  uint32_t groupsCount;
  uint32_t inodesPerGroup;
  uint32_t blocksPerGroup;
  uint32_t gdtStartBlock;     // Starting block of the GDT
  uint32_t gdtBlocks;          // Number of blocks in the GDT
  Ext4GroupDesc *groupDescCache; // Cache for a single group descriptor
  uint32_t cachedGroupIndex;  // Which group is currently cached (-1 if none)
  Ext4FileHandle *openFiles;
  bool driverStateValid; // Whether or not this is a valid state
} Ext4State;

// Forward declarations
static int ext4ReadBlock(Ext4State *state, uint32_t blockNum, void *buffer);
static int ext4WriteBlock(Ext4State *state, uint32_t blockNum, 
  const void *buffer);
static int ext4ReadInode(Ext4State *state, uint32_t inodeNum, 
  Ext4Inode *inode);
static int ext4WriteInode(Ext4State *state, uint32_t inodeNum, 
  const Ext4Inode *inode);
static int ext4ReadGroupDesc(Ext4State *state, uint32_t groupIndex,
  Ext4GroupDesc *groupDesc);
static int ext4WriteGroupDesc(Ext4State *state, uint32_t groupIndex,
  const Ext4GroupDesc *groupDesc);
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
  if (!state || !state->fs) {
    return -1;
  }
  
  // Allocate superblock
  state->superblock = (Ext4Superblock*) malloc(sizeof(Ext4Superblock));
  if (!state->superblock) {
    return -2;
  }
  
  // Read superblock from block 1 (or block 0 if blocksize > 1024)
  uint32_t sbBlock = (state->fs->blockSize > 1024) ? 0 : 1;
  uint32_t sbOffset = 1024 % state->fs->blockSize;
  
  uint8_t *tempBuffer = (uint8_t*) malloc(state->fs->blockSize);
  if (!tempBuffer) {
    free(state->superblock);
    return -3;
  }
  
  printDebug("state->fs->startLba = ");
  printDebug(state->fs->startLba);
  printDebug("\n");
  printDebug("sbBlock = ");
  printDebug(sbBlock);
  printDebug("\n");
  if (state->fs->blockDevice->readBlocks(
      state->fs->blockDevice->context,
      state->fs->startLba + sbBlock,
      1,
      state->fs->blockSize,
      tempBuffer) != 0) {
    free(tempBuffer);
    free(state->superblock);
    return -4;
  }
  
  // Copy superblock data
  memcpy(state->superblock, tempBuffer + sbOffset, sizeof(Ext4Superblock));
  free(tempBuffer);
  
  // Verify magic number
  uint16_t magic;
  readBytes(&magic, &state->superblock->magic);
  if (magic != EXT4_SUPER_MAGIC) {
    printString("ERROR: Expected ext4 super magic to be 0x");
    printHex(EXT4_SUPER_MAGIC);
    printString(", got 0x");
    printHex(magic);
    printString("\n");
    free(state->superblock);
    return -5;
  }
  
  // Calculate filesystem parameters
  uint32_t logBlockSize;
  readBytes(&logBlockSize, &state->superblock->logBlockSize);
  state->fs->blockSize = EXT4_MIN_BLOCK_SIZE << logBlockSize;
  state->fs->blockBuffer = (uint8_t*) malloc(state->fs->blockSize);
  
  state->fs->blockDevice->blockBitShift = 0;
  for (uint16_t blockSize = state->fs->blockSize;
    blockSize != state->fs->blockDevice->blockSize;
    blockSize >>= 1
  ) {
    state->fs->blockDevice->blockBitShift++;
  }
  
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
  
  // Calculate GDT location and size
  state->gdtStartBlock = (state->fs->blockSize > 1024) ? 1 : 2;
  state->gdtBlocks = (state->groupsCount * state->groupDescSize + 
    state->fs->blockSize - 1) / state->fs->blockSize;
  
  printDebug("gdtBlocks = ");
  printDebug(state->gdtBlocks);
  printDebug("\n");
  printDebug("state->fs->blockSize = ");
  printDebug(state->fs->blockSize);
  printDebug("\n");
  
  // Allocate cache for a single group descriptor
  printDebug("sizeof(Ext4GroupDesc) = ");
  printDebug(sizeof(Ext4GroupDesc));
  printDebug("\n");
  state->groupDescCache = (Ext4GroupDesc*) malloc(sizeof(Ext4GroupDesc));
  if (!state->groupDescCache) {
    printDebug("ERROR: Allocation of state->groupDescCache failed.\n");
    printDebug("Freeing state->fs->blockBuffer.\n");
    free(state->fs->blockBuffer);
    printDebug("Freeing state->superblock.\n");
    free(state->superblock);
    printDebug("Returning -6\n");
    return -6;
  }
  printDebug("Successfully allocated group descriptor cache.\n");
  
  state->cachedGroupIndex = (uint32_t) -1; // No group cached initially
  state->openFiles = NULL;
  state->driverStateValid = true;
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
  
  if (state->groupDescCache) {
    free(state->groupDescCache);
    state->groupDescCache = NULL;
  }
  if (state->superblock) {
    free(state->superblock);
    state->superblock = NULL;
  }
  if (state->fs->blockBuffer) {
    free(state->fs->blockBuffer);
    state->fs->blockBuffer = NULL;
  }
  state->driverStateValid = false;
}

/// @brief Read a group descriptor from the filesystem
///
/// @param state Pointer to the ext4 state structure
/// @param groupIndex The index of the group descriptor to read
/// @param groupDesc Buffer to read the group descriptor into
///
/// @return 0 on success, negative on error
static int ext4ReadGroupDesc(Ext4State *state, uint32_t groupIndex,
    Ext4GroupDesc *groupDesc
) {
  if (!state || !groupDesc || groupIndex >= state->groupsCount) {
    return -1;
  }
  
  // Check if we have this group cached
  if (groupIndex == state->cachedGroupIndex) {
    printDebug("groupIndex == state->cachedGroupIndex\n");
    memcpy(groupDesc, state->groupDescCache, sizeof(Ext4GroupDesc));
    return 0;
  }
  
  // Calculate which block and offset contains this group descriptor
  uint32_t gdOffset = groupIndex * state->groupDescSize;
  printDebug("gdOffset = ");
  printDebug(gdOffset);
  printDebug("\n");
  uint32_t gdBlock = state->gdtStartBlock + 
    (gdOffset / state->fs->blockSize);
  printDebug("gdBlock = ");
  printDebug(gdBlock);
  printDebug("\n");
  uint32_t gdBlockOffset = gdOffset % state->fs->blockSize;
  printDebug("gdBlockOffset = ");
  printDebug(gdBlockOffset);
  printDebug("\n");
  
  // Read the block containing the group descriptor
  uint8_t *buffer = state->fs->blockBuffer;
  if (ext4ReadBlock(state, gdBlock, buffer) != 0) {
    return -1;
  }

#ifdef NANO_OS_DEBUG
  uint32_t ii = 0;
  for (; ii < state->fs->blockSize; ii++) {
    if (buffer[ii] != '\0') {
      break;
    }
  }
  if (ii < state->fs->blockSize) {
    printDebug("Non-zero byte found in buffer.\n");
  } else {
    printDebug("WARNING: Block read in was all zero bytes!\n");
  }
#endif // NANO_OS_DEBUG
  
  // Copy the group descriptor data
  copyBytes(groupDesc, buffer + gdBlockOffset, sizeof(Ext4GroupDesc));
  
  // Update cache
  memcpy(state->groupDescCache, groupDesc, sizeof(Ext4GroupDesc));
  state->cachedGroupIndex = groupIndex;
  
  return 0;
}

/// @brief Write a group descriptor to the filesystem
///
/// @param state Pointer to the ext4 state structure
/// @param groupIndex The index of the group descriptor to write
/// @param groupDesc The group descriptor data to write
///
/// @return 0 on success, negative on error
static int ext4WriteGroupDesc(Ext4State *state, uint32_t groupIndex,
    const Ext4GroupDesc *groupDesc) {
  if (!state || !groupDesc || groupIndex >= state->groupsCount) {
    return -1;
  }
  
  // Calculate which block and offset contains this group descriptor
  uint32_t gdOffset = groupIndex * state->groupDescSize;
  uint32_t gdBlock = state->gdtStartBlock + 
    (gdOffset / state->fs->blockSize);
  uint32_t gdBlockOffset = gdOffset % state->fs->blockSize;
  
  // Read the block containing the group descriptor
  uint8_t *buffer = state->fs->blockBuffer;
  if (ext4ReadBlock(state, gdBlock, buffer) != 0) {
    return -1;
  }
  
  // Update the group descriptor data
  copyBytes(buffer + gdBlockOffset, groupDesc, sizeof(Ext4GroupDesc));
  
  // Write the block back
  if (ext4WriteBlock(state, gdBlock, buffer) != 0) {
    return -1;
  }
  
  // Update cache if this is the cached group
  if (groupIndex == state->cachedGroupIndex) {
    memcpy(state->groupDescCache, groupDesc, sizeof(Ext4GroupDesc));
  }
  
  return 0;
}

/// @brief Read a block from the filesystem
///
/// @param state Pointer to the ext4 state structure
/// @param blockNum The block number to read
/// @param buffer Buffer to read the block into
///
/// @return 0 on success, negative on error
static int ext4ReadBlock(Ext4State *state, uint32_t blockNum, void *buffer) {
  if (!state || !state->fs || !buffer) {
    return -1;
  }
  
  return state->fs->blockDevice->readBlocks(
    state->fs->blockDevice->context,
    state->fs->startLba + blockNum,
    1,
    state->fs->blockSize,
    (uint8_t*) buffer);
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
  if (!state || !state->fs || !buffer) {
    return -1;
  }
  
  return state->fs->blockDevice->writeBlocks(
    state->fs->blockDevice->context,
    state->fs->startLba + blockNum,
    1,
    state->fs->blockSize,
    (uint8_t*) buffer);
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
  printDebug("Reading inode ");
  printDebug(inodeNum);
  printDebug("\n");
  
  uint32_t group = (inodeNum - 1) / state->inodesPerGroup;
  uint32_t index = (inodeNum - 1) % state->inodesPerGroup;
  uint8_t *buffer = state->fs->blockBuffer;
  
  if (group >= state->groupsCount) {
    printDebug("group >= state->groupsCount\n");
    return -1;
  }
  printDebug("Reading inode group ");
  printDebug(group);
  printDebug("\n");
  printDebug("Reading inode index ");
  printDebug(index);
  printDebug("\n");
  
  // Get inode table location from group descriptor
  Ext4GroupDesc gd;
  if (ext4ReadGroupDesc(state, group, &gd) != 0) {
    printDebug("ext4ReadGroupDesc failed\n");
    return -1;
  }
  
  uint32_t inodeTableLo;
  readBytes(&inodeTableLo, &gd.inodeTableLo);
  printDebug("gd.inodeTableLo = ");
  printDebug(inodeTableLo);
  printDebug("\n");
  
  uint32_t inodeBlock = inodeTableLo + 
    ((index * state->inodeSize) / state->fs->blockSize);
  uint32_t inodeOffset
    = (index * state->inodeSize) % state->fs->blockSize;
  
  printDebug("Reading inodeBlock ");
  printDebug(inodeBlock);
  printDebug("\n");
  if (ext4ReadBlock(state, inodeBlock, buffer) != 0) {
    printDebug("ext4ReadBlock failed\n");
    return -1;
  }
  
  copyBytes(inode, buffer + inodeOffset, sizeof(Ext4Inode));
  
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
  uint8_t *buffer = state->fs->blockBuffer;
  
  if (group >= state->groupsCount) {
    return -1;
  }
  
  // Get inode table location from group descriptor
  Ext4GroupDesc gd;
  if (ext4ReadGroupDesc(state, group, &gd) != 0) {
    return -1;
  }
  
  uint32_t inodeTableLo;
  readBytes(&inodeTableLo, &gd.inodeTableLo);
  
  uint32_t inodeBlock = inodeTableLo + 
    ((index * state->inodeSize) / state->fs->blockSize);
  uint32_t inodeOffset
    = (index * state->inodeSize) % state->fs->blockSize;
  
  if (ext4ReadBlock(state, inodeBlock, buffer) != 0) {
    return -1;
  }
  
  memcpy(state->fs->blockBuffer + inodeOffset, inode,
    sizeof(Ext4Inode));
  
  int result = ext4WriteBlock(state, inodeBlock, buffer);
  
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

/// @brief Allocate a new block from the filesystem
///
/// @param state Pointer to the ext4 state structure
///
/// @return Block number, or 0 if no blocks available
static uint32_t ext4AllocateBlock(Ext4State *state) {
  if (!state) {
    return 0;
  }
  
  uint16_t blockSize = state->fs->blockSize;
  uint8_t *bitmap = (uint8_t*) malloc(blockSize);
  if (!bitmap) {
    return 0;
  }
  
  for (uint32_t group = 0; group < state->groupsCount; group++) {
    Ext4GroupDesc gd;
    if (ext4ReadGroupDesc(state, group, &gd) != 0) {
      continue;
    }
    
    uint16_t freeBlocks;
    readBytes(&freeBlocks, &gd.freeBlocksCountLo);
    
    if (freeBlocks == 0) {
      continue;
    }
    
    uint32_t bitmapBlock;
    readBytes(&bitmapBlock, &gd.blockBitmapLo);
    
    if (ext4ReadBlock(state, bitmapBlock, bitmap) != 0) {
      continue;
    }
    
    // Find free bit in bitmap
    for (uint32_t byte = 0; byte < blockSize; byte++) {
      if (bitmap[byte] != 0xFF) {
        for (uint32_t bit = 0; bit < 8; bit++) {
          if (!(bitmap[byte] & (1 << bit))) {
            // Found free block
            bitmap[byte] |= (1 << bit);
            
            // Write updated bitmap
            if (ext4WriteBlock(state, bitmapBlock, bitmap) == 0) {
              // Update group descriptor
              freeBlocks--;
              writeBytes(&gd.freeBlocksCountLo, &freeBlocks);
              ext4WriteGroupDesc(state, group, &gd);
              
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
  
  uint16_t blockSize = state->fs->blockSize;
  uint8_t *bitmap = (uint8_t*) malloc(blockSize);
  if (!bitmap) {
    return;
  }
  
  Ext4GroupDesc gd;
  if (ext4ReadGroupDesc(state, group, &gd) != 0) {
    free(bitmap);
    return;
  }
  
  uint32_t bitmapBlock;
  readBytes(&bitmapBlock, &gd.blockBitmapLo);
  
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
      readBytes(&freeBlocks, &gd.freeBlocksCountLo);
      freeBlocks++;
      writeBytes(&gd.freeBlocksCountLo, &freeBlocks);
      ext4WriteGroupDesc(state, group, &gd);
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
  
  uint16_t blockSize = state->fs->blockSize;
  uint8_t *bitmap = (uint8_t*) malloc(blockSize);
  if (!bitmap) {
    return 0;
  }
  
  for (uint32_t group = 0; group < state->groupsCount; group++) {
    Ext4GroupDesc gd;
    if (ext4ReadGroupDesc(state, group, &gd) != 0) {
      continue;
    }
    
    uint16_t freeInodes;
    readBytes(&freeInodes, &gd.freeInodesCountLo);
    
    if (freeInodes == 0) {
      continue;
    }
    
    uint32_t bitmapBlock;
    readBytes(&bitmapBlock, &gd.inodeBitmapLo);
    
    if (ext4ReadBlock(state, bitmapBlock, bitmap) != 0) {
      continue;
    }
    
    // Find free bit in bitmap
    for (uint32_t byte = 0; byte < blockSize; byte++) {
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
              writeBytes(&gd.freeInodesCountLo, &freeInodes);
              ext4WriteGroupDesc(state, group, &gd);
              
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
  
  uint8_t *bitmap = (uint8_t*) malloc(state->fs->blockSize);
  if (!bitmap) {
    return;
  }
  
  Ext4GroupDesc gd;
  if (ext4ReadGroupDesc(state, group, &gd) != 0) {
    free(bitmap);
    return;
  }
  
  uint32_t bitmapBlock;
  readBytes(&bitmapBlock, &gd.inodeBitmapLo);
  
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
      readBytes(&freeInodes, &gd.freeInodesCountLo);
      freeInodes++;
      writeBytes(&gd.freeInodesCountLo, &freeInodes);
      ext4WriteGroupDesc(state, group, &gd);
    }
  }
  
  free(bitmap);
}

/// @brief Find an inode number by path
///
/// @param state Pointer to the ext4 state structure
/// @param path The path to search for
///
/// @return Inode number, or 0 if not found
static uint32_t ext4FindInodeByPath(Ext4State *state, const char *path) {
  int returnValue = 0;
  // Start at root inode
  uint32_t currentInode = EXT4_ROOT_INO;
  char *pathCopy = NULL;
  char *token = NULL;
  uint16_t blockSize = state->fs->blockSize;
  Ext4Inode *dirInode = NULL;
  uint8_t *dirBuffer = state->fs->blockBuffer;
  Ext4DirEntry *entry = NULL;

  if (!state || !path) {
    printDebug("NULL state or path provided to ext4FindInodeByPath\n");
    return returnValue; // 0
  }
  
  if (path[0] == '/') {
    path++;
  }
  
  if (strlen(path) == 0) {
    return currentInode;
  }
  
  pathCopy = (char*) malloc(strlen(path) + 1);
  if (!pathCopy) {
    return returnValue; // 0
  }
  strcpy(pathCopy, path);
  printDebug("Looking for path \"");
  printDebug(pathCopy);
  printDebug("\"\n");
  token = strtok(pathCopy, "/");
  printDebug("First token is \"");
  printDebug(token);
  printDebug("\"\n");
  
  dirInode = (Ext4Inode*) malloc(sizeof(Ext4Inode));
  if (dirInode == NULL) {
    printDebug("Could not allocate dirInode\n");
    goto cleanup;
  }
  
  if (!dirBuffer) {
    // This should be impossible if we initialized the filesystem correctly, but
    // don't chance it.
    printDebug("dirBuffer is NULL\n");
    goto cleanup;
  }
  
  entry = (Ext4DirEntry*) malloc(sizeof(Ext4DirEntry));
  if (entry == NULL) {
    printDebug("Could not allocate entry\n");
    goto cleanup;
  }

  while (token != NULL) {
    // Read current directory inode
    if (ext4ReadInode(state, currentInode, dirInode) != 0) {
      printDebug("Could not read dirInode\n");
      goto cleanup;
    }
    
    // Check if it's a directory
    uint16_t mode = 0;
    readBytes(&mode, &dirInode->mode);
    printDebug("dirInode->mode = 0x");
    printDebug(mode, HEX);
    printDebug("\n");
    if ((mode & EXT4_S_IFMT) != EXT4_S_IFDIR) {
      printDebug("mode does not include EXT4_S_IFDIR\n");
      goto cleanup;
    }
    
    // Search directory for entry
    uint32_t sizeLo;
    readBytes(&sizeLo, &dirInode->sizeLo);
    uint32_t blockCount = (sizeLo + blockSize - 1) / blockSize;
    
    bool found = false;
    for (uint32_t ii = 0; ii < blockCount && !found; ii++) {
      uint64_t blockNum = ext4GetBlockFromExtent(state, dirInode, ii);
      if (blockNum == 0) {
        continue;
      }
      
      if (ext4ReadBlock(state, blockNum, dirBuffer) != 0) {
        continue;
      }
      
      uint32_t offset = 0;
      while (offset < blockSize) {
        copyBytes(entry, dirBuffer + offset, 8);  // Read fixed part
        
        uint32_t inodeNum;
        uint16_t recLen;
        uint8_t nameLen;
        readBytes(&inodeNum, &entry->inode);
        readBytes(&recLen, &entry->recLen);
        readBytes(&nameLen, &entry->nameLen);
        
        if (recLen == 0) {
          break;
        }
        
        if (inodeNum != 0 && nameLen == strlen(token)) {
          copyBytes(entry->name, dirBuffer + offset + 8, nameLen);
          entry->name[nameLen] = '\0';
          
          if (strcmp(entry->name, token) == 0) {
            currentInode = inodeNum;
            found = true;
            break;
          }
        }
        
        offset += recLen;
      }
    }
    
    if (!found) {
      printDebug("Could not find entry ");
      printDebug(token);
      printDebug("\n");
      goto cleanup;
    }
    
    token = strtok(NULL, "/");
    if (token != NULL) {
      printDebug("Next token is \"");
      printDebug(token);
      printDebug("\"\n");
    } else {
      printDebug("No next token.\n");
    }
  }
  
  returnValue = currentInode;

cleanup:
  free(entry);
  free(dirInode);
  free(pathCopy);

  return returnValue;
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
    const char *name, uint32_t inodeNum, uint8_t fileType
) {
  int returnValue = -1;
  uint32_t nameLen = strlen(name);
  uint32_t requiredLen = ((8 + nameLen + 3) / 4) * 4;  // Align to 4 bytes
  
  uint32_t sizeLo = 0;
  uint16_t blockSize = state->fs->blockSize;
  uint32_t blockCount = 0;
  uint16_t newRecLen = blockSize;
  uint32_t newBlock = 0;
  uint8_t *dirBuffer = NULL;
  Ext4DirEntry *entry = NULL;
  Ext4DirEntry *newEntry = NULL;
  Ext4Inode *dirInode = NULL;
  
  if (!state || !name || (strlen(name) > EXT4_NAME_LEN)) {
    return returnValue; // -1
  }
  
  dirInode = (Ext4Inode*) malloc(sizeof(Ext4Inode));
  if (dirInode == NULL) {
    return returnValue; // -1
  }
  if (ext4ReadInode(state, parentInode, dirInode) != 0) {
    goto cleanup;
  }
  
  readBytes(&sizeLo, &dirInode->sizeLo);
  blockCount = (sizeLo + blockSize - 1) / blockSize;
  
  dirBuffer = (uint8_t*) malloc(blockSize);
  if (!dirBuffer) {
    goto cleanup;
  }
  
  // Try to find space in existing blocks
  entry = (Ext4DirEntry*) malloc(sizeof(Ext4DirEntry));
  if (entry == NULL) {
    goto cleanup;
  }
  newEntry = (Ext4DirEntry*) malloc(sizeof(Ext4DirEntry));
  if (newEntry == NULL) {
    goto cleanup;
  }
  
  for (uint32_t ii = 0; ii < blockCount; ii++) {
    uint64_t blockNum = ext4GetBlockFromExtent(state, dirInode, ii);
    if (blockNum == 0) {
      continue;
    }
    
    if (ext4ReadBlock(state, blockNum, dirBuffer) != 0) {
      continue;
    }
    
    uint32_t offset = 0;
    uint32_t lastOffset = 0;
    
    // Find the last entry in this block
    while (offset < blockSize) {
      copyBytes(entry, dirBuffer + offset, 8);
      
      uint16_t currentRecLen;
      readBytes(&currentRecLen, &entry->recLen);
      
      if (currentRecLen == 0) {
        break;
      }
      
      lastOffset = offset;
      offset += currentRecLen;
    }
    
    // Check if the last entry can be split
    copyBytes(entry, dirBuffer + lastOffset, 8);
    uint16_t lastRecLen;
    uint8_t lastNameLen;
    readBytes(&lastRecLen, &entry->recLen);
    readBytes(&lastNameLen, &entry->nameLen);
    
    uint32_t actualLastLen = ((8 + lastNameLen + 3) / 4) * 4;
    
    if (lastOffset + lastRecLen == blockSize && 
        lastRecLen >= actualLastLen + requiredLen) {
      // We can split the last entry
      uint16_t newLastRecLen = actualLastLen;
      writeBytes(&entry->recLen, &newLastRecLen);
      copyBytes(dirBuffer + lastOffset, entry, 8);
      
      // Create new entry
      writeBytes(&newEntry->inode, &inodeNum);
      uint16_t remainingLen = lastRecLen - actualLastLen;
      writeBytes(&newEntry->recLen, &remainingLen);
      uint8_t nameLenByte = nameLen;
      writeBytes(&newEntry->nameLen, &nameLenByte);
      writeBytes(&newEntry->fileType, &fileType);
      
      copyBytes(dirBuffer + lastOffset + actualLastLen, newEntry, 8);
      copyBytes(dirBuffer + lastOffset + actualLastLen + 8, name, nameLen);
      
      if (ext4WriteBlock(state, blockNum, dirBuffer) == 0) {
        returnValue = 0;
        goto cleanup;
      }
    }
  }
  
  // Need to allocate new block
  newBlock = ext4AllocateBlock(state);
  if (newBlock == 0) {
    goto cleanup;
  }
  
  // Initialize new block with zeros
  memset(dirBuffer, 0, blockSize);
  
  // Create entry in new block
  writeBytes(&newEntry->inode, &inodeNum);
  writeBytes(&newEntry->recLen, &newRecLen);
  uint8_t nameLenByte = nameLen;
  writeBytes(&newEntry->nameLen, &nameLenByte);
  writeBytes(&newEntry->fileType, &fileType);
  
  copyBytes(dirBuffer, newEntry, 8);
  copyBytes(dirBuffer + 8, name, nameLen);
  
  if (ext4WriteBlock(state, newBlock, dirBuffer) != 0) {
    ext4FreeBlock(state, newBlock);
    goto cleanup;
  }
  
  // Update directory size
  sizeLo += blockSize;
  writeBytes(&dirInode->sizeLo, &sizeLo);
  
  // Add block to extent tree
  if (ext4SetBlockInExtent(state, dirInode, blockCount, newBlock) != 0) {
    ext4FreeBlock(state, newBlock);
    goto cleanup;
  }
  
  if (ext4WriteInode(state, parentInode, dirInode) != 0) {
    ext4FreeBlock(state, newBlock);
    goto cleanup;
  }
  
  returnValue = 0;

cleanup:
  free(newEntry);
  free(entry);
  free(dirBuffer);
  free(dirInode);
  return returnValue;
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
  
  // Handle extent tree
  Ext4ExtentHeader header;
  copyBytes(&header, inode->block, sizeof(Ext4ExtentHeader));
  
  uint16_t entries;
  readBytes(&entries, &header.entries);
  
  // Check if we can merge with existing extent
  if (entries > 0) {
    Ext4Extent *extents = (Ext4Extent*)(inode->block + 
      sizeof(Ext4ExtentHeader));
    
    // Check last extent for possible merge
    Ext4Extent *lastExtent = &extents[entries - 1];
    uint32_t lastBlock;
    uint16_t lastLen;
    uint32_t lastStartLo;
    uint16_t lastStartHi;
    
    readBytes(&lastBlock, &lastExtent->block);
    readBytes(&lastLen, &lastExtent->len);
    readBytes(&lastStartLo, &lastExtent->startLo);
    readBytes(&lastStartHi, &lastExtent->startHi);
    
    uint64_t lastPhysBlock = ((uint64_t)lastStartHi << 32) | lastStartLo;
    
    // Check if this block extends the last extent
    if (fileBlock == lastBlock + lastLen && 
        physBlock == lastPhysBlock + lastLen) {
      // Extend the last extent
      lastLen++;
      writeBytes(&lastExtent->len, &lastLen);
      copyBytes(inode->block + sizeof(Ext4ExtentHeader) + 
        (entries - 1) * sizeof(Ext4Extent), lastExtent, sizeof(Ext4Extent));
      return 0;
    }
  }
  
  // Add new extent if there's room
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
    const char *name
) {
  int returnValue = -1;
  uint32_t sizeLo = 0;
  uint16_t blockSize = state->fs->blockSize;
  uint32_t blockCount = 0;
  uint8_t *dirBuffer = NULL;
  Ext4DirEntry *entry = NULL;
  Ext4Inode *dirInode = NULL;
  
  if (!state || !name) {
    return returnValue; // -1
  }
  
  dirInode = (Ext4Inode*) malloc(sizeof(Ext4Inode));
  if (dirInode == NULL) {
    return returnValue; // -1
  }
  if (ext4ReadInode(state, parentInode, dirInode) != 0) {
    goto cleanup;
  }
  
  readBytes(&sizeLo, &dirInode->sizeLo);
  blockCount = (sizeLo + blockSize - 1) / blockSize;
  
  dirBuffer = (uint8_t*) malloc(blockSize);
  if (!dirBuffer) {
    goto cleanup;
  }
  
  entry = (Ext4DirEntry*) malloc(sizeof(Ext4DirEntry));
  if (entry == NULL) {
    goto cleanup;
  }
  
  for (uint32_t ii = 0; ii < blockCount; ii++) {
    uint64_t blockNum = ext4GetBlockFromExtent(state, dirInode, ii);
    if (blockNum == 0) {
      continue;
    }
    
    if (ext4ReadBlock(state, blockNum, dirBuffer) != 0) {
      continue;
    }
    
    uint32_t offset = 0;
    uint32_t prevOffset = 0;
    Ext4DirEntry *prevEntry = NULL;
    
    while (offset < blockSize) {
      copyBytes(entry, dirBuffer + offset, 8);
      
      uint32_t inodeNum;
      uint16_t recLen;
      uint8_t nameLen;
      readBytes(&inodeNum, &entry->inode);
      readBytes(&recLen, &entry->recLen);
      readBytes(&nameLen, &entry->nameLen);
      
      if (recLen == 0) {
        break;
      }
      
      if (inodeNum != 0 && nameLen == strlen(name)) {
        copyBytes(entry->name, dirBuffer + offset + 8, nameLen);
        entry->name[nameLen] = '\0';
        
        if (strcmp(entry->name, name) == 0) {
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
            writeBytes(&entry->inode, &zero);
            copyBytes(dirBuffer + offset, &entry->inode, 4);
          }
          
          if (ext4WriteBlock(state, blockNum, dirBuffer) == 0) {
            returnValue = 0;
            goto cleanup;
          }
        }
      }
      
      prevEntry = entry;
      prevOffset = offset;
      offset += recLen;
    }
  }
  
cleanup:
  free(entry);
  free(dirBuffer);
  free(dirInode);
  return returnValue;
}

/// @brief Open a file
///
/// @param state Pointer to the ext4 state structure
/// @param pathname The path to the file
/// @param mode The mode to open the file in
///
/// @return A pointer to a Ext4FileHandle on success, or NULL on error
Ext4FileHandle* ext4Open(Ext4State *state,
  const char *pathname, const char *mode
) {
  if (!state || !pathname || !mode) {
    return NULL;
  }
  
  // Parse mode
  uint32_t openMode = 0;
  bool create = false;
  bool truncate = false;
  
  if (mode[0] == 'r') {
    openMode |= EXT4_MODE_READ;
  }
  if (mode[0] == 'w') {
    openMode |= EXT4_MODE_WRITE;
    create = true;
    truncate = true;
  }
  if (mode[0] == 'a') {
    openMode |= EXT4_MODE_WRITE | EXT4_MODE_APPEND;
  }
  if (openMode == 0) {
    // Invalid mode.
    printDebug("Invalid open mode \"");
    printDebug(mode);
    printDebug("\" for file \"");
    printDebug(pathname);
    printDebug("\"\n");
    return NULL;
  }
  if (mode[1] == '+') {
    openMode |= EXT4_MODE_READ | EXT4_MODE_WRITE;
  }
  // Find or create file
  uint32_t inodeNum = ext4FindInodeByPath(state, pathname);
  
  Ext4Inode *newInode = (Ext4Inode*) malloc(sizeof(Ext4Inode));
  if (newInode == NULL) {
    printDebug("Could not allocate newInode.\n");
    return NULL;
  }
  Ext4FileHandle *handle = NULL;
  if ((inodeNum == 0) && create) {
    // Need to create file
    // Find parent directory
    char *pathCopy = (char*) malloc(strlen(pathname) + 1);
    if (!pathCopy) {
      printDebug("Could not allocate pathCopy.\n");
      goto cleanup;
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
      printDebug("parentInode is 0.\n");
      free(pathCopy);
      goto cleanup;
    }
    
    // Allocate new inode
    inodeNum = ext4AllocateInode(state);
    if (inodeNum == 0) {
      printDebug("inodeNum is 0.\n");
      free(pathCopy);
      goto cleanup;
    }
    
    // Initialize new inode
    memset(newInode, 0, sizeof(Ext4Inode));
    
    uint16_t inodeMode = EXT4_S_IFREG | EXT4_S_IRUSR | EXT4_S_IWUSR;
    writeBytes(&newInode->mode, &inodeMode);
    
    uint32_t currentTime = 0;  // Would use real time
    writeBytes(&newInode->atime, &currentTime);
    writeBytes(&newInode->ctime, &currentTime);
    writeBytes(&newInode->mtime, &currentTime);
    
    uint16_t linksCount = 1;
    writeBytes(&newInode->linksCount, &linksCount);
    
    uint32_t inodeFlags = EXT4_INODE_FLAG_EXTENTS;
    writeBytes(&newInode->flags, &inodeFlags);
    
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
    
    copyBytes(newInode->block, &header, sizeof(Ext4ExtentHeader));
    
    if (ext4WriteInode(state, inodeNum, newInode) != 0) {
      printDebug("Could not write inode.\n");
      ext4FreeInode(state, inodeNum);
      free(pathCopy);
      goto cleanup;
    }
    
    // Add directory entry
    if (ext4CreateDirEntry(state, parentInode, filename, inodeNum, 
        EXT4_FT_REG_FILE) != 0
    ) {
      printDebug("Could not create directory entry.\n");
      ext4FreeInode(state, inodeNum);
      free(pathCopy);
      goto cleanup;
    }
    
    free(pathCopy);
  } else if (inodeNum == 0) {
    printDebug("inodeNum is 0 and we're not creating.\n");
    goto cleanup;
  }
  
  // Allocate file handle
  handle = (Ext4FileHandle*) malloc(sizeof(Ext4FileHandle));
  if (!handle) {
    printDebug("Could not allocate handle.\n");
    goto cleanup;
  }
  
  handle->inodeNumber = inodeNum;
  handle->mode = openMode;
  handle->currentPosition = 0;
  
  // Read inode
  handle->inode = (Ext4Inode*) malloc(sizeof(Ext4Inode));
  if (!handle->inode) {
    printDebug("Could not allocate handle->inode.\n");
    free(handle); handle = NULL;
    goto cleanup;
  }
  
  if (ext4ReadInode(state, inodeNum, handle->inode) != 0) {
    printDebug("Could not read handle->inode.\n");
    free(handle->inode);
    free(handle); handle = NULL;
    goto cleanup;
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
  
  state->fs->numOpenFiles++;
  
cleanup:
  free(newInode);
  // Return handle
  return handle;
}

/// @brief Close a file
///
/// @param state Pointer to the ext4 state structure
/// @param stream The file handle to close
///
/// @return 0 on success, negative on error
int ext4Close(Ext4State *state, Ext4FileHandle *handle) {
  if (!state || !handle) {
    return -1;
  }
  
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
  
  if (state->fs->numOpenFiles > 0) {
    state->fs->numOpenFiles--;
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
/// @return Number of bytes read
size_t ext4Read(Ext4State *state,
  void *ptr, uint32_t totalBytes, Ext4FileHandle *handle
) {
  if (!state || !ptr || !handle || totalBytes == 0) {
    return 0;
  }
  
  if (!(handle->mode & EXT4_MODE_READ)) {
    return 0;
  }
  
  uint32_t sizeLo, sizeHi;
  readBytes(&sizeLo, &handle->inode->sizeLo);
  readBytes(&sizeHi, &handle->inode->sizeHi);
  uint64_t fileSize = ((uint64_t)sizeHi << 32) | sizeLo;
  
  if (handle->currentPosition >= fileSize) {
    return 0;
  }
  
  if (handle->currentPosition + totalBytes > fileSize) {
    totalBytes = fileSize - handle->currentPosition;
  }
  
  uint8_t *buffer = (uint8_t*)ptr;
  size_t bytesRead = 0;
  
  uint8_t *blockBuffer = state->fs->blockBuffer;
  
  uint16_t blockSize = state->fs->blockSize;
  while (bytesRead < totalBytes) {
    uint32_t fileBlock = handle->currentPosition / blockSize;
    uint32_t blockOffset = handle->currentPosition % blockSize;
    uint32_t bytesToRead = blockSize - blockOffset;
    
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
  
  return bytesRead;
}

/// @brief Write to a file
///
/// @param state Pointer to the ext4 state structure
/// @param ptr Buffer to write from
/// @param size Size of each element
/// @param nmemb Number of elements
/// @param stream File handle
///
/// @return Number of bytes written
size_t ext4Write(Ext4State *state,
  const void *ptr, uint32_t totalBytes, Ext4FileHandle *handle
) {
  if (!state || !ptr || !handle || totalBytes == 0) {
    return 0;
  }
  
  if (!(handle->mode & EXT4_MODE_WRITE)) {
    return 0;
  }
  
  const uint8_t *buffer = (const uint8_t*)ptr;
  size_t bytesWritten = 0;
  
  uint8_t *blockBuffer = state->fs->blockBuffer;
  
  uint16_t blockSize = state->fs->blockSize;
  while (bytesWritten < totalBytes) {
    uint32_t fileBlock = handle->currentPosition / blockSize;
    uint32_t blockOffset = handle->currentPosition % blockSize;
    uint32_t bytesToWrite = blockSize - blockOffset;
    
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
    if (blockOffset != 0 || bytesToWrite < blockSize) {
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
  
  return bytesWritten;
}

/// @brief Remove a file or directory
///
/// @param state Pointer to the ext4 state structure
/// @param pathname The path to remove
///
/// @return 0 on success, negative on error
int ext4Remove(Ext4State *state, const char *pathname) {
  int returnValue = -1;
  uint16_t mode = 0;
  bool isDir = false;
  uint16_t blockSize = state->fs->blockSize;
  char *pathCopy = NULL;
  char *lastSlash = NULL;
  char *filename = NULL;
  uint32_t parentInode = 0;
  uint32_t sizeLo = 0;
  uint32_t blockCount = 0;
  
  if (!state || !pathname) {
    return returnValue; // -1
  }
  
  // Find the file
  uint32_t inodeNum = ext4FindInodeByPath(state, pathname);
  if (inodeNum == 0) {
    return returnValue; // -1
  }
  
  // Read the inode
  Ext4Inode *inode = (Ext4Inode*) malloc(sizeof(Ext4Inode));
  if (inode == NULL) {
    return returnValue; // -1
  }
  if (ext4ReadInode(state, inodeNum, inode) != 0) {
    goto cleanup;
  }
  
  // Check if it's a directory
  readBytes(&mode, &inode->mode);
  isDir = ((mode & EXT4_S_IFMT) == EXT4_S_IFDIR);
  
  if (isDir) {
    // Check if directory is empty
    readBytes(&sizeLo, &inode->sizeLo);
    
    if (sizeLo > blockSize) {
      // Directory not empty (has more than just . and ..)
      goto cleanup;
    }
  }
  
  // Find parent directory
  pathCopy = (char*) malloc(strlen(pathname) + 1);
  if (!pathCopy) {
    goto cleanup;
  }
  strcpy(pathCopy, pathname);
  
  lastSlash = strrchr(pathCopy, '/');
  
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
    goto cleanup;
  }
  
  // Remove directory entry
  if (ext4RemoveDirEntry(state, parentInode, filename) != 0) {
    free(pathCopy);
    goto cleanup;
  }
  
  free(pathCopy);
  
  // Free all blocks used by the file
  readBytes(&sizeLo, &inode->sizeLo);
  blockCount = (sizeLo + blockSize - 1) / blockSize;
  
  for (uint32_t ii = 0; ii < blockCount; ii++) {
    uint64_t physBlock = ext4GetBlockFromExtent(state, inode, ii);
    if (physBlock != 0) {
      ext4FreeBlock(state, physBlock);
    }
  }
  
  // Free the inode
  ext4FreeInode(state, inodeNum);
  returnValue = 0;
  
cleanup:
  free(inode);
  return returnValue;
}

/// @brief Seek to a position in a file
///
/// @param state Pointer to the ext4 state structure
/// @param stream The file handle
/// @param offset The offset to seek
/// @param whence The seek mode
///
/// @return 0 on success, negative on error
int ext4Seek(Ext4State *state, Ext4FileHandle *handle, long offset, int whence) {
  if (!state || !handle) {
    return -1;
  }
  
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
  int returnValue = -1;
  Ext4Inode *parentInodeData = NULL;
  if (!state || !pathname) {
    return returnValue; // -1
  }
  
  // Check if already exists
  if (ext4FindInodeByPath(state, pathname) != 0) {
    return returnValue; // -1
  }
  
  // Find parent directory
  char *pathCopy = (char*) malloc(strlen(pathname) + 1);
  if (!pathCopy) {
    return returnValue; // -1
  }
  strcpy(pathCopy, pathname);
  
  char *lastSlash = strrchr(pathCopy, '/');
  char *dirname = NULL;
  uint32_t parentInode = 0;
  uint32_t inodeNum = 0;
  Ext4Inode *newInode = NULL;
  uint16_t inodeMode = EXT4_S_IFDIR | EXT4_S_IRUSR | EXT4_S_IWUSR | 
    EXT4_S_IXUSR;
  uint32_t currentTime = 0;  // Would use real time
  uint16_t linksCount = 2;  // . and parent's link
  uint32_t inodeFlags = EXT4_INODE_FLAG_EXTENTS;
  
  Ext4ExtentHeader header;
  uint16_t magic = EXT4_EXTENT_MAGIC;
  uint16_t entries = 0;
  uint16_t max = 4;
  uint16_t depth = 0;
  
  uint32_t dirBlock = 0;
  uint16_t blockSize = state->fs->blockSize;
  uint8_t *dirBuffer = state->fs->blockBuffer;
  
  Ext4DirEntry *dotEntry = NULL;
  uint16_t dotRecLen = 12;
  uint8_t dotNameLen = 1;
  uint8_t dirType = EXT4_FT_DIR;
  Ext4DirEntry *dotDotEntry = NULL;
  uint16_t dotDotRecLen = blockSize - 12;
  uint8_t dotDotNameLen = 2;
  uint32_t dirSize = blockSize;
  
  if (lastSlash) {
    *lastSlash = '\0';
    dirname = lastSlash + 1;
    parentInode = ext4FindInodeByPath(state, pathCopy);
  } else {
    dirname = pathCopy;
    parentInode = EXT4_ROOT_INO;
  }
  
  if (parentInode == 0) {
    goto cleanup;
  }
  
  // Allocate new inode
  inodeNum = ext4AllocateInode(state);
  if (inodeNum == 0) {
    goto cleanup;
  }
  
  // Initialize directory inode
  newInode = (Ext4Inode*) malloc(sizeof(Ext4Inode));
  if (newInode == NULL) {
    goto cleanup;
  }
  memset(newInode, 0, sizeof(Ext4Inode));
  
  writeBytes(&newInode->mode, &inodeMode);
  
  writeBytes(&newInode->atime, &currentTime);
  writeBytes(&newInode->ctime, &currentTime);
  writeBytes(&newInode->mtime, &currentTime);
  
  writeBytes(&newInode->linksCount, &linksCount);
  
  writeBytes(&newInode->flags, &inodeFlags);
  
  // Initialize extent header
  writeBytes(&header.magic, &magic);
  writeBytes(&header.entries, &entries);
  writeBytes(&header.max, &max);
  writeBytes(&header.depth, &depth);
  
  copyBytes(newInode->block, &header, sizeof(Ext4ExtentHeader));
  
  // Allocate block for directory entries
  dirBlock = ext4AllocateBlock(state);
  if (dirBlock == 0) {
    ext4FreeInode(state, inodeNum);
    goto cleanup;
  }
  
  // Create . and .. entries
  if (!dirBuffer) {
    ext4FreeBlock(state, dirBlock);
    ext4FreeInode(state, inodeNum);
    goto cleanup;
  }
  memset(dirBuffer, 0, blockSize);
  
  // . entry
  dotEntry = (Ext4DirEntry*) malloc(sizeof(Ext4DirEntry));
  if (dotEntry == NULL) {
    ext4FreeBlock(state, dirBlock);
    ext4FreeInode(state, inodeNum);
    goto cleanup;
  }
  writeBytes(&dotEntry->inode, &inodeNum);
  writeBytes(&dotEntry->recLen, &dotRecLen);
  writeBytes(&dotEntry->nameLen, &dotNameLen);
  writeBytes(&dotEntry->fileType, &dirType);
  
  copyBytes(dirBuffer, dotEntry, 8);
  dirBuffer[8] = '.';
  
  // .. entry
  dotDotEntry = dotEntry; dotEntry = NULL;
  writeBytes(&dotDotEntry->inode, &parentInode);
  writeBytes(&dotDotEntry->recLen, &dotDotRecLen);
  writeBytes(&dotDotEntry->nameLen, &dotDotNameLen);
  writeBytes(&dotDotEntry->fileType, &dirType);
  
  copyBytes(dirBuffer + 12, dotDotEntry, 8);
  dirBuffer[20] = '.';
  dirBuffer[21] = '.';
  free(dotDotEntry); dotEntry = NULL;
  
  if (ext4WriteBlock(state, dirBlock, dirBuffer) != 0) {
    ext4FreeBlock(state, dirBlock);
    ext4FreeInode(state, inodeNum);
    goto cleanup;
  }
  
  // Update inode with block
  writeBytes(&newInode->sizeLo, &dirSize);
  
  if (ext4SetBlockInExtent(state, newInode, 0, dirBlock) != 0) {
    ext4FreeBlock(state, dirBlock);
    ext4FreeInode(state, inodeNum);
    goto cleanup;
  }
  
  if (ext4WriteInode(state, inodeNum, newInode) != 0) {
    ext4FreeBlock(state, dirBlock);
    ext4FreeInode(state, inodeNum);
    goto cleanup;
  }
  
  // Add entry to parent directory
  if (ext4CreateDirEntry(state, parentInode, dirname, inodeNum, 
      EXT4_FT_DIR) != 0) {
    ext4FreeBlock(state, dirBlock);
    ext4FreeInode(state, inodeNum);
    goto cleanup;
  }
  
  // Update parent's link count
  parentInodeData = (Ext4Inode*) malloc(sizeof(Ext4Inode));
  if (ext4ReadInode(state, parentInode, parentInodeData) == 0) {
    uint16_t parentLinks;
    readBytes(&parentLinks, &parentInodeData->linksCount);
    parentLinks++;
    writeBytes(&parentInodeData->linksCount, &parentLinks);
    ext4WriteInode(state, parentInode, parentInodeData);
  }
  free(parentInodeData); parentInodeData = NULL;
  
  returnValue = 0;
  
cleanup:
  free(newInode);
  free(pathCopy);
  return returnValue;
}

/// @typedef Ext4CommandHandler
///
/// @brief Definition of a filesystem command handler function.
typedef int (*Ext4CommandHandler)(Ext4State*, ProcessMessage*);

/// @fn int ext4FilesystemOpenFileCommandHandler(
///   Ext4State *driverState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_OPEN_FILE command.
///
/// @param filesystemState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int ext4FilesystemOpenFileCommandHandler(
  Ext4State *driverState, ProcessMessage *processMessage
) {
  NanoOsFile *nanoOsFile = NULL;
  const char *pathname = nanoOsMessageDataPointer(processMessage, char*);
  const char *mode = nanoOsMessageFuncPointer(processMessage, char*);
  if (driverState->driverStateValid) {
    Ext4FileHandle *ext4File = ext4Open(driverState, pathname, mode);
    if (ext4File != NULL) {
      nanoOsFile = (NanoOsFile*) malloc(sizeof(NanoOsFile));
      nanoOsFile->file = ext4File;
    }
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = (intptr_t) nanoOsFile;
  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int ext4FilesystemCloseFileCommandHandler(
///   Ext4State *driverState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_CLOSE_FILE command.
///
/// @param driverState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int ext4FilesystemCloseFileCommandHandler(
  Ext4State *driverState, ProcessMessage *processMessage
) {
  (void) driverState;

  NanoOsFile *nanoOsFile
    = nanoOsMessageDataPointer(processMessage, NanoOsFile*);
  Ext4FileHandle *ext4File = (Ext4FileHandle*) nanoOsFile->file;
  free(nanoOsFile);
  if (driverState->driverStateValid) {
    ext4Close(driverState, ext4File);
  }

  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int ext4FilesystemReadFileCommandHandler(
///   Ext4State *driverState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_READ_FILE command.
///
/// @param driverState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int ext4FilesystemReadFileCommandHandler(
  Ext4State *driverState, ProcessMessage *processMessage
) {
  FilesystemIoCommandParameters *filesystemIoCommandParameters
    = nanoOsMessageDataPointer(processMessage, FilesystemIoCommandParameters*);
  int32_t returnValue = 0;
  if (driverState->driverStateValid) {
    returnValue = ext4Read(driverState,
      filesystemIoCommandParameters->buffer,
      filesystemIoCommandParameters->length, 
      (Ext4FileHandle*) filesystemIoCommandParameters->file->file);
    if (returnValue >= 0) {
      // Return value is the number of bytes read.  Set the length variable to
      // it and set it to 0 to indicate good status.
      filesystemIoCommandParameters->length = returnValue;
      returnValue = 0;
    } else {
      // Return value is a negative error code.  Negate it.
      returnValue = -returnValue;
      // Tell the caller that we read nothing.
      filesystemIoCommandParameters->length = 0;
    }
  }

  processMessageSetDone(processMessage);
  return returnValue;
}

/// @fn int ext4FilesystemWriteFileCommandHandler(
///   Ext4State *driverState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_WRITE_FILE command.
///
/// @param driverState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int ext4FilesystemWriteFileCommandHandler(
  Ext4State *driverState, ProcessMessage *processMessage
) {
  FilesystemIoCommandParameters *filesystemIoCommandParameters
    = nanoOsMessageDataPointer(processMessage, FilesystemIoCommandParameters*);
  int returnValue = 0;
  if (driverState->driverStateValid) {
    returnValue = ext4Write(driverState,
      (uint8_t*) filesystemIoCommandParameters->buffer,
      filesystemIoCommandParameters->length,
      (Ext4FileHandle*) filesystemIoCommandParameters->file->file);
    if (returnValue >= 0) {
      // Return value is the number of bytes written.  Set the length variable
      // to it and set it to 0 to indicate good status.
      filesystemIoCommandParameters->length = returnValue;
      returnValue = 0;
    } else {
      // Return value is a negative error code.  Negate it.
      returnValue = -returnValue;
      // Tell the caller that we wrote nothing.
      filesystemIoCommandParameters->length = 0;
    }
  }

  processMessageSetDone(processMessage);
  return returnValue;
}

/// @fn int ext4FilesystemRemoveFileCommandHandler(
///   Ext4State *driverState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_REMOVE_FILE command.
///
/// @param driverState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int ext4FilesystemRemoveFileCommandHandler(
  Ext4State *driverState, ProcessMessage *processMessage
) {
  const char *pathname = nanoOsMessageDataPointer(processMessage, char*);
  int returnValue = 0;
  if (driverState->driverStateValid) {
    returnValue = ext4Remove(driverState, pathname);
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = (intptr_t) returnValue;
  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int ext4FilesystemSeekFileCommandHandler(
///   Ext4State *driverState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_SEEK_FILE command.
///
/// @param driverState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int ext4FilesystemSeekFileCommandHandler(
  Ext4State *driverState, ProcessMessage *processMessage
) {
  FilesystemSeekParameters *filesystemSeekParameters
    = nanoOsMessageDataPointer(processMessage, FilesystemSeekParameters*);
  int returnValue = 0;
  if (driverState->driverStateValid) {
    returnValue = ext4Seek(driverState,
      (Ext4FileHandle*) filesystemSeekParameters->stream->file,
      filesystemSeekParameters->offset,
      filesystemSeekParameters->whence);
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = (intptr_t) returnValue;
  processMessageSetDone(processMessage);
  return 0;
}

/// @var filesystemCommandHandlers
///
/// @brief Array of Ext4CommandHandler function pointers.
const Ext4CommandHandler filesystemCommandHandlers[] = {
  ext4FilesystemOpenFileCommandHandler,   // FILESYSTEM_OPEN_FILE
  ext4FilesystemCloseFileCommandHandler,  // FILESYSTEM_CLOSE_FILE
  ext4FilesystemReadFileCommandHandler,   // FILESYSTEM_READ_FILE
  ext4FilesystemWriteFileCommandHandler,  // FILESYSTEM_WRITE_FILE
  ext4FilesystemRemoveFileCommandHandler, // FILESYSTEM_REMOVE_FILE
  ext4FilesystemSeekFileCommandHandler,   // FILESYSTEM_SEEK_FILE
};


/// @fn static void ext4HandleFilesystemMessages(FilesystemState *fs)
///
/// @brief Pop and handle all messages in the filesystem process's message
/// queue until there are no more.
///
/// @param fs A pointer to the FilesystemState object maintained by the
///   filesystem process.
///
/// @return This function returns no value.
static void ext4HandleFilesystemMessages(Ext4State *driverState) {
  ProcessMessage *msg = processMessageQueuePop();
  while (msg != NULL) {
    FilesystemCommandResponse type = 
      (FilesystemCommandResponse) processMessageType(msg);
    if (type < NUM_FILESYSTEM_COMMANDS) {
      filesystemCommandHandlers[type](driverState, msg);
    }
    msg = processMessageQueuePop();
  }
}

/// @fn void* runExt4Filesystem(void *args)
///
/// @brief Main process entry point for the FAT16 filesystem process.
///
/// @param args A pointer to an initialized BlockStorageDevice structure cast
///   to a void*.
///
/// @return This function never returns, but would return NULL if it did.
void* runExt4Filesystem(void *args) {
  coroutineYield(NULL);
  FilesystemState *fs = (FilesystemState*) calloc(1, sizeof(FilesystemState));
  fs->blockDevice = (BlockStorageDevice*) args;
  fs->blockSize = fs->blockDevice->blockSize;
  Ext4State driverState;
  memset(&driverState, 0, sizeof(Ext4State));
  driverState.fs = fs;
  
  fs->blockBuffer = (uint8_t*) malloc(fs->blockSize);
  int returnValue = getPartitionInfo(fs);
  free(fs->blockBuffer); fs->blockBuffer = NULL;
  if (returnValue == 0) {
    if (fs->blockSize < 1024) {
      // ext4 expects a minimum block size of 1024, so make sure we satisfy the
      // minimum.
      uint64_t startBytes = ((uint64_t) fs->startLba)
        * ((uint64_t) fs->blockSize);
      fs->startLba = (uint32_t) (startBytes / ((uint64_t) 1024));
      fs->blockSize = 1024;
      fs->blockDevice->blockBitShift = 1;
    }
    returnValue = ext4Initialize(&driverState);
    if (returnValue != 0) {
      printString("ERROR: ext4Initialize returned status ");
      printInt(returnValue);
      printString("\n");
    }
  } else {
    printString("ERROR: getPartitionInfo returned status ");
    printInt(returnValue);
    printString("\n");
  }
  
  ProcessMessage *msg = NULL;
  while (1) {
    msg = (ProcessMessage*) coroutineYield(NULL);
    if (msg) {
      FilesystemCommandResponse type = 
        (FilesystemCommandResponse) processMessageType(msg);
      if (type < NUM_FILESYSTEM_COMMANDS) {
        filesystemCommandHandlers[type](&driverState, msg);
      }
    } else {
      ext4HandleFilesystemMessages(&driverState);
    }
  }
  return NULL;
}

/// @fn long ext4FilesystemFTell(FILE *stream)
///
/// @brief Get the current value of the position indicator of a
/// previously-opened file.
///
/// @param stream A pointer to a previously-opened file.
///
/// @return Returns the current position of the file on success, -1 on failure.
long ext4FilesystemFTell(FILE *stream) {
  if (stream == NULL) {
    return -1;
  }

  return (long) ((Ext4FileHandle*) stream->file)->currentPosition;
}

