////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2024 James Card                     //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included    //
// in all copies or substantial portions of the Software.                     //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//                                 James Card                                 //
//                          http://www.jamescard.org                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// Doxygen marker
/// @file

#include "Fat16Filesystem.h"

// Core block operations
static inline int fat16ReadBlock(FilesystemState *fs,
  uint32_t blockNumber, uint8_t *buffer
) {
  return fs->blockDevice->readBlocks(fs->blockDevice->context, 
    blockNumber, 1, fs->blockSize, buffer);
}

static inline int fat16WriteBlock(FilesystemState *fs,
  uint32_t blockNumber, const uint8_t *buffer
) {
  return fs->blockDevice->writeBlocks(fs->blockDevice->context,
    blockNumber, 1, fs->blockSize, buffer);
}

// Boot sector operations
static Fat16BootSector* fat16ReadBootSector(FilesystemState *fs) {
  return (fat16ReadBlock(fs, fs->startLba, fs->blockBuffer) == 0) ?
    (Fat16BootSector*) fs->blockBuffer : NULL;
}

// Filename handling
static void fat16ParsePathname(const char *pathname,
  char *filename, char *extension
) {
  memset(filename, ' ', 8);
  memset(extension, ' ', 3);
  filename[8] = extension[3] = '\0';
  
  const char *dot = strrchr(pathname, '.');
  size_t nameLength = dot ? (dot - pathname) : strlen(pathname);
  
  for (size_t ii = 0; ii < 8 && ii < nameLength; ii++) {
    filename[ii] = toupper((unsigned char) pathname[ii]);
  }
  
  if (dot) {
    for (size_t ii = 0; ii < 3 && dot[ii + 1]; ii++) {
      extension[ii] = toupper((unsigned char) dot[ii + 1]);
    }
  }
}

// Directory entry operations
static int fat16WriteDirectoryEntry(FilesystemState *fs,
  const Fat16DirectoryEntry *entry, uint32_t entryOffset
) {
  Fat16BootSector *bootSector = fat16ReadBootSector(fs);
  if (!bootSector) {
    return -1;
  }

  uint32_t rootDirStartSector = fs->startLba + bootSector->reservedSectorCount + 
    (bootSector->numberOfFats * bootSector->sectorsPerFat);
  uint32_t entrySector = rootDirStartSector + 
    (entryOffset / bootSector->bytesPerSector);
  uint32_t entryOffsetInSector = entryOffset % bootSector->bytesPerSector;

  if (fat16ReadBlock(fs, entrySector, fs->blockBuffer) != 0) {
    return -1;
  }

  memcpy(fs->blockBuffer + entryOffsetInSector, entry, 
    sizeof(Fat16DirectoryEntry));

  return fat16WriteBlock(fs, entrySector, fs->blockBuffer);
}

static int fat16FindFreeDirectoryEntry(FilesystemState *fs,
  Fat16BootSector *bootSector, uint32_t *entryOffset
) {
  uint32_t rootDirStartSector = fs->startLba + bootSector->reservedSectorCount + 
    (bootSector->numberOfFats * bootSector->sectorsPerFat);
  uint32_t entriesPerSector = bootSector->bytesPerSector / 
    sizeof(Fat16DirectoryEntry);
  uint32_t rootDirSectors = (bootSector->rootEntryCount * 
    sizeof(Fat16DirectoryEntry) + bootSector->bytesPerSector - 1) / 
    bootSector->bytesPerSector;

  for (uint32_t sector = 0; sector < rootDirSectors; sector++) {
    if (fat16ReadBlock(fs, rootDirStartSector + sector, fs->blockBuffer) != 0) {
      return -1;
    }

    Fat16DirectoryEntry *dirSector = (Fat16DirectoryEntry *) fs->blockBuffer;
    for (uint32_t entry = 0; entry < entriesPerSector; entry++) {
      uint8_t firstChar = dirSector[entry].filename[0];
      if (firstChar == 0x00 || firstChar == 0xE5) {
        *entryOffset = (sector * entriesPerSector + entry) * 
          sizeof(Fat16DirectoryEntry);
        return 0;
      }
    }
  }
  return -1;
}

static Fat16DirectoryEntry* fat16FindFile(FilesystemState *fs,
  const char *pathname, Fat16BootSector *bootSector
) {
  uint32_t rootDirStart = fs->startLba + bootSector->reservedSectorCount + 
    (bootSector->numberOfFats * bootSector->sectorsPerFat);
  uint32_t rootDirBlocks = (bootSector->rootEntryCount * 32) / fs->blockSize;
  
  char filename[9], extension[4];
  fat16ParsePathname(pathname, filename, extension);
  
  for (uint32_t ii = 0; ii < rootDirBlocks; ii++) {
    if (fat16ReadBlock(fs, rootDirStart + ii, fs->blockBuffer) != 0) {
      return NULL;
    }
    
    Fat16DirectoryEntry *dirEntry = (Fat16DirectoryEntry*) fs->blockBuffer;
    for (uint32_t jj = 0; jj < FAT16_ENTRIES_PER_CLUSTER; jj++) {
      if (dirEntry[jj].filename[0] == 0) {
        return NULL;
      }
      
      if (dirEntry[jj].filename[0] != 0xE5 && 
          (dirEntry[jj].attributes & FAT16_ATTR_FILE) &&
          memcmp(dirEntry[jj].filename, filename, 8) == 0 &&
          memcmp(dirEntry[jj].extension, extension, 3) == 0) {
        return &dirEntry[jj];
      }
    }
  }
  return NULL;
}

// File operations
Fat16File* fat16Fopen(FilesystemState *fs,
  const char *pathname, const char *mode
) {
  if (!fs || !pathname || !mode) {
    return NULL;
  }

  Fat16BootSector *bootSector = fat16ReadBootSector(fs);
  if (!bootSector) {
    return NULL;
  }

  Fat16DirectoryEntry *dirEntry = fat16FindFile(fs, pathname, bootSector);
  bool isWrite = (strchr(mode, 'w') != NULL);
  
  if (!dirEntry && !isWrite) {
    return NULL;
  }

  Fat16File *file = (Fat16File*) malloc(sizeof(Fat16File));
  if (!file) {
    return NULL;
  }

  if (!dirEntry) {
    // Create new file
    uint32_t entryOffset;
    if (fat16FindFreeDirectoryEntry(fs, bootSector, &entryOffset) != 0) {
      free(file);
      return NULL;
    }
    dirEntry = (Fat16DirectoryEntry*) fs->blockBuffer;
    memset(dirEntry, 0, sizeof(Fat16DirectoryEntry));
    
    char filename[9], extension[4];
    fat16ParsePathname(pathname, filename, extension);
    memcpy(dirEntry->filename, filename, 8);
    memcpy(dirEntry->extension, extension, 3);
    dirEntry->attributes = FAT16_ATTR_FILE;
    
    if (fat16WriteDirectoryEntry(fs, dirEntry, entryOffset) != 0) {
      free(file);
      return NULL;
    }
  }

  file->currentCluster = dirEntry->firstClusterLow;
  file->fileSize = dirEntry->fileSize;
  file->firstCluster = dirEntry->firstClusterLow;
  file->currentPosition = (strchr(mode, 'a') != NULL) ? file->fileSize : 0;
  file->pathname = strdup(pathname);

  return file;
}

static uint32_t fat16GetNextCluster(FilesystemState *fs,
  Fat16BootSector *bootSector, uint32_t currentCluster
) {
  uint32_t fatOffset = currentCluster * 2;
  uint32_t fatBlock = fs->startLba + bootSector->reservedSectorCount +
    (fatOffset / fs->blockSize);
  
  if (fat16ReadBlock(fs, fatBlock, fs->blockBuffer) != 0) {
    return 0xFFFF;
  }
  
  return *((uint16_t*) &fs->blockBuffer[fatOffset % fs->blockSize]);
}

int fat16Read(FilesystemState *fs, Fat16File *file,
  void *buffer, uint32_t length
) {
  if (!fs || !file || !buffer || !length) {
    return -EINVAL;
  }

  uint32_t remainingBytes = file->fileSize - file->currentPosition;
  length = (length > remainingBytes) ? remainingBytes : length;
  if (!length) {
    return 0;
  }

  Fat16BootSector *bootSector = fat16ReadBootSector(fs);
  if (!bootSector) {
    return -EIO;
  }

  uint32_t bytesRead = 0;
  uint32_t bytesPerCluster = bootSector->bytesPerSector *
    bootSector->sectorsPerCluster;
  uint32_t dataStart = fs->startLba + bootSector->reservedSectorCount +
    (bootSector->numberOfFats * bootSector->sectorsPerFat) +
    ((bootSector->rootEntryCount * 32) / bootSector->bytesPerSector);

  while (bytesRead < length) {
    uint32_t clusterOffset = file->currentPosition % bytesPerCluster;
    uint32_t blockInCluster = clusterOffset / bootSector->bytesPerSector;
    uint32_t offsetInBlock = clusterOffset % bootSector->bytesPerSector;
    uint32_t currentBlock = dataStart +
      ((file->currentCluster - 2) * bootSector->sectorsPerCluster) +
      blockInCluster;

    if (fat16ReadBlock(fs, currentBlock, fs->blockBuffer) != 0) {
      return bytesRead ? bytesRead : -EIO;
    }

    uint32_t bytesToCopy = bootSector->bytesPerSector - offsetInBlock;
    if (bytesToCopy > (length - bytesRead)) {
      bytesToCopy = length - bytesRead;
    }

    memcpy((uint8_t*)buffer + bytesRead,
      fs->blockBuffer + offsetInBlock, bytesToCopy);
    bytesRead += bytesToCopy;
    file->currentPosition += bytesToCopy;

    if ((file->currentPosition % bytesPerCluster) == 0 &&
        bytesRead < length) {
      uint32_t nextCluster = fat16GetNextCluster(fs, bootSector,
        file->currentCluster);
      if (nextCluster >= 0xFFF8) {
        break;
      }
      file->currentCluster = nextCluster;
    }
  }

  return bytesRead;
}

int fat16Write(FilesystemState *fs, Fat16File *file,
  const uint8_t *buffer, uint32_t length
) {
  if (!fs || !file || !buffer || !length) {
    return -EINVAL;
  }

  Fat16BootSector *bootSector = fat16ReadBootSector(fs);
  if (!bootSector) {
    return -EIO;
  }

  uint32_t bytesWritten = 0;
  uint32_t bytesPerCluster = bootSector->bytesPerSector *
    bootSector->sectorsPerCluster;
  uint32_t dataStart = fs->startLba + bootSector->reservedSectorCount +
    (bootSector->numberOfFats * bootSector->sectorsPerFat) +
    ((bootSector->rootEntryCount * 32) / bootSector->bytesPerSector);

  while (bytesWritten < length) {
    if (file->currentCluster == 0 ||
        (file->currentPosition % bytesPerCluster) == 0) {
      // Need new cluster
      uint32_t newCluster = 0;
      if (fat16ReadBlock(fs, fs->startLba + bootSector->reservedSectorCount,
          fs->blockBuffer) != 0) {
        return bytesWritten ? bytesWritten : -EIO;
      }
      uint16_t *fat = (uint16_t*) fs->blockBuffer;

      // Find free cluster
      for (uint32_t ii = 2; ii < (bootSector->sectorsPerFat * 256); ii++) {
        if (fat[ii] == 0) {
          newCluster = ii;
          break;
        }
      }
      if (!newCluster) {
        return bytesWritten ? bytesWritten : -ENOSPC;
      }

      // Update FAT
      if (file->currentCluster) {
        fat[file->currentCluster] = newCluster;
      } else {
        file->firstCluster = newCluster;
      }
      fat[newCluster] = 0xFFFF;
      file->currentCluster = newCluster;

      // Write FAT copies
      for (uint8_t ii = 0; ii < bootSector->numberOfFats; ii++) {
        if (fat16WriteBlock(fs, fs->startLba + bootSector->reservedSectorCount +
            (ii * bootSector->sectorsPerFat), fs->blockBuffer) != 0) {
          return bytesWritten ? bytesWritten : -EIO;
        }
      }
    }

    uint32_t clusterOffset = file->currentPosition % bytesPerCluster;
    uint32_t blockInCluster = clusterOffset / bootSector->bytesPerSector;
    uint32_t offsetInBlock = clusterOffset % bootSector->bytesPerSector;
    uint32_t currentBlock = dataStart +
      ((file->currentCluster - 2) * bootSector->sectorsPerCluster) +
      blockInCluster;

    // Read-modify-write if not aligned
    if (offsetInBlock && fat16ReadBlock(fs, currentBlock, fs->blockBuffer) != 0) {
      return bytesWritten ? bytesWritten : -EIO;
    }

    uint32_t bytesToWrite = bootSector->bytesPerSector - offsetInBlock;
    if (bytesToWrite > (length - bytesWritten)) {
      bytesToWrite = length - bytesWritten;
    }

    memcpy(fs->blockBuffer + offsetInBlock,
      buffer + bytesWritten, bytesToWrite);

    if (fat16WriteBlock(fs, currentBlock, fs->blockBuffer) != 0) {
      return bytesWritten ? bytesWritten : -EIO;
    }

    bytesWritten += bytesToWrite;
    file->currentPosition += bytesToWrite;
    if (file->currentPosition > file->fileSize) {
      file->fileSize = file->currentPosition;
    }
  }

  // Update directory entry with new file size
  Fat16DirectoryEntry *dirEntry = fat16FindFile(fs,
    file->pathname, bootSector);
  if (dirEntry) {
    dirEntry->fileSize = file->fileSize;
    dirEntry->lastModifiedTime = 0;  // Would set from RTC
    dirEntry->lastModifiedDate = 0;  // Would set from RTC

    if (fat16WriteBlock(fs, fs->startLba +
        bootSector->reservedSectorCount +
        (bootSector->numberOfFats * bootSector->sectorsPerFat),
        fs->blockBuffer) != 0) {
      return -EIO;
    }
  }

  return bytesWritten;
}

static int getPartitionInfo(FilesystemState *fs) {
  if (fs->blockDevice->partitionNumber == 0) {
    return -1;
  }

  if (fs->blockDevice->readBlocks(fs->blockDevice->context, 0, 1,
      fs->blockSize, fs->blockBuffer) != 0) {
    return -2;
  }

  uint8_t *partitionTable = fs->blockBuffer + 0x1BE;
  uint8_t *entry = partitionTable +
    ((fs->blockDevice->partitionNumber - 1) * 16);
  uint8_t type = entry[4];
  
  if ((type == 0x0e) || (type == 0x1e)) {
    fs->startLba = (((uint32_t) entry[11]) << 24) |
      (((uint32_t) entry[10]) << 16) |
      (((uint32_t) entry[9]) << 8) |
      ((uint32_t) entry[8]);
    uint32_t numSectors = (((uint32_t) entry[15]) << 24) |
      (((uint32_t) entry[14]) << 16) |
      (((uint32_t) entry[13]) << 8) |
      ((uint32_t) entry[12]);
    fs->endLba = fs->startLba + numSectors - 1;
    return 0;
  }
  
  return -3;
}

/// @fn int fat16FilesystemOpenFileCommandHandler(
///   FilesystemState *filesystemState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_OPEN_FILE command.
///
/// @param filesystemState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemOpenFileCommandHandler(
  FilesystemState *filesystemState, ProcessMessage *processMessage
) {
  const char *pathname = nanoOsMessageDataPointer(processMessage, char*);
  const char *mode = nanoOsMessageFuncPointer(processMessage, char*);
  Fat16File *fat16File = fat16Fopen(filesystemState, pathname, mode);
  NanoOsFile *nanoOsFile = NULL;
  if (fat16File != NULL) {
    nanoOsFile = (NanoOsFile*) malloc(sizeof(NanoOsFile));
    nanoOsFile->file = fat16File;
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = (intptr_t) nanoOsFile;
  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int fat16FilesystemCloseFileCommandHandler(
///   FilesystemState *filesystemState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_CLOSE_FILE command.
///
/// @param filesystemState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemCloseFileCommandHandler(
  FilesystemState *filesystemState, ProcessMessage *processMessage
) {
  (void) filesystemState;

  NanoOsFile *nanoOsFile
    = nanoOsMessageDataPointer(processMessage, NanoOsFile*);
  Fat16File *fat16File = (Fat16File*) nanoOsFile->file;
  free(fat16File->pathname);
  free(fat16File);
  free(nanoOsFile);

  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int fat16FilesystemReadFileCommandHandler(
///   FilesystemState *filesystemState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_READ_FILE command.
///
/// @param filesystemState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemReadFileCommandHandler(
  FilesystemState *filesystemState, ProcessMessage *processMessage
) {
  FilesystemIoCommandParameters *filesystemIoCommandParameters
    = nanoOsMessageDataPointer(processMessage, FilesystemIoCommandParameters*);
  int returnValue = fat16Read(filesystemState,
    (Fat16File*) filesystemIoCommandParameters->file->file,
    filesystemIoCommandParameters->buffer,
    filesystemIoCommandParameters->length);
  if (returnValue >= 0) {
    // Return value is the number of bytes read.  Set the length variable to it
    // and set it to 0 to indicate good status.
    filesystemIoCommandParameters->length = returnValue;
    returnValue = 0;
  } else {
    // Return value is a negative error code.  Negate it.
    returnValue = -returnValue;
    // Tell the caller that we read nothing.
    filesystemIoCommandParameters->length = 0;
  }

  processMessageSetDone(processMessage);
  return returnValue;
}

/// @fn int fat16FilesystemWriteFileCommandHandler(
///   FilesystemState *filesystemState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_WRITE_FILE command.
///
/// @param filesystemState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemWriteFileCommandHandler(
  FilesystemState *filesystemState, ProcessMessage *processMessage
) {
  FilesystemIoCommandParameters *filesystemIoCommandParameters
    = nanoOsMessageDataPointer(processMessage, FilesystemIoCommandParameters*);
  int returnValue = fat16Write(filesystemState,
    (Fat16File*) filesystemIoCommandParameters->file->file,
    (uint8_t*) filesystemIoCommandParameters->buffer,
    filesystemIoCommandParameters->length);
  if (returnValue >= 0) {
    // Return value is the number of bytes written.  Set the length variable to
    // it and set it to 0 to indicate good status.
    filesystemIoCommandParameters->length = returnValue;
    returnValue = 0;
  } else {
    // Return value is a negative error code.  Negate it.
    returnValue = -returnValue;
    // Tell the caller that we wrote nothing.
    filesystemIoCommandParameters->length = 0;
  }

  processMessageSetDone(processMessage);
  return returnValue;
}

/// @var filesystemCommandHandlers
///
/// @brief Array of FilesystemCommandHandler function pointers.
const FilesystemCommandHandler filesystemCommandHandlers[] = {
  fat16FilesystemOpenFileCommandHandler,  // FILESYSTEM_OPEN_FILE
  fat16FilesystemCloseFileCommandHandler, // FILESYSTEM_CLOSE_FILE
  fat16FilesystemReadFileCommandHandler,  // FILESYSTEM_READ_FILE
  fat16FilesystemWriteFileCommandHandler, // FILESYSTEM_WRITE_FILE
};


// Filesystem process
static void handleFilesystemMessages(FilesystemState *fs) {
  ProcessMessage *msg = processMessageQueuePop();
  while (msg != NULL) {
    FilesystemCommandResponse type = 
      (FilesystemCommandResponse) processMessageType(msg);
    if (type < NUM_FILESYSTEM_COMMANDS) {
      filesystemCommandHandlers[type](fs, msg);
    }
    msg = processMessageQueuePop();
  }
}

void* runFat16Filesystem(void *args) {
  FilesystemState fs;
  memset(&fs, 0, sizeof(fs));
  fs.blockDevice = (BlockStorageDevice*) args;
  fs.blockSize = 512;
  coroutineYield(NULL);
  fs.blockBuffer = (uint8_t*) malloc(fs.blockSize);
  
  getPartitionInfo(&fs);
  
  ProcessMessage *msg = NULL;
  while (1) {
    msg = (ProcessMessage*) coroutineYield(NULL);
    if (msg) {
      FilesystemCommandResponse type = 
        (FilesystemCommandResponse) processMessageType(msg);
      if (type < NUM_FILESYSTEM_COMMANDS) {
        filesystemCommandHandlers[type](&fs, msg);
      }
    } else {
      handleFilesystemMessages(&fs);
    }
  }
  return NULL;
}

// Standard file operations
FILE* filesystemFOpen(const char *pathname, const char *mode) {
  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_OPEN_FILE,
    (intptr_t) mode, (intptr_t) pathname, true);
  processMessageWaitForDone(msg, NULL);
  FILE *file = nanoOsMessageDataPointer(msg, FILE*);
  processMessageRelease(msg);
  return file;
}

int filesystemFClose(FILE *stream) {
  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_CLOSE_FILE,
    0, (intptr_t) stream, true);
  processMessageWaitForDone(msg, NULL);
  processMessageRelease(msg);
  return 0;
}

