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
Fat16BootSector* fat16ReadBootSector(FilesystemState *fs) {
  return (fat16ReadBlock(fs, fs->startLba, fs->blockBuffer) == 0) ?
    (Fat16BootSector*) fs->blockBuffer : NULL;
}

// Filename handling
void fat16ParsePathname(const char *pathname,
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
int fat16WriteDirectoryEntry(FilesystemState *fs,
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

int fat16FindFreeDirectoryEntry(FilesystemState *fs,
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

// Combined directory entry operations
int fat16CreateOrFindFile(FilesystemState *fs, const char *pathname,
  const char *mode, Fat16DirectoryEntry *outEntry, uint32_t *outOffset
) {
  char createFile = '\0';
  for (int ii = 0; mode[ii] != '\0'; ii++) {
    // 'a' = 0x61
    // 'b' = 0x62
    // 'r' = 0x72
    // 'w' = 0x77
    // '+' = 0x2b
    //
    // '+' is always a read and some kind of write operation.
    // Anything that involves writing must create the file.
    // The presence of a 'b' does nothing.
    //
    // Given the ASCII character codes above, all of the characters that involve
    // writing operations have odd values.  None of the other ones do.  So, if
    // there is a mode character that has an odd value, we need to create the
    // file.
    createFile |= mode[ii] & 1;
  }

  // Read boot sector directly
  if (fs->blockDevice->readBlocks(fs->blockDevice->context, fs->startLba, 1,
      fs->blockSize, fs->blockBuffer) != 0) {
    return -1;
  }
  Fat16BootSector *bootSector = (Fat16BootSector*) fs->blockBuffer;
  
  // Parse filename once
  char filename[9], extension[4];
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

  // Calculate root directory location
  uint32_t rootDirStart = fs->startLba + bootSector->reservedSectorCount + 
    (bootSector->numberOfFats * bootSector->sectorsPerFat);
  uint32_t entriesPerSector = fs->blockSize / sizeof(Fat16DirectoryEntry);
  uint32_t rootDirSectors = (bootSector->rootEntryCount * 32 +
    fs->blockSize - 1) / fs->blockSize;

  // Search directory
  for (uint32_t sector = 0; sector < rootDirSectors; sector++) {
    if (fs->blockDevice->readBlocks(fs->blockDevice->context,
        rootDirStart + sector, 1, fs->blockSize, fs->blockBuffer) != 0) {
      return -1;
    }

    Fat16DirectoryEntry *entries = (Fat16DirectoryEntry*) fs->blockBuffer;
    for (uint32_t entry = 0; entry < entriesPerSector; entry++) {
      uint8_t firstChar = entries[entry].filename[0];
      
      // Found existing file
      if (firstChar != 0xE5 && firstChar != 0 &&
          memcmp(entries[entry].filename, filename, 8) == 0 &&
          memcmp(entries[entry].extension, extension, 3) == 0) {
        *outEntry = entries[entry];
        *outOffset = (sector * entriesPerSector + entry) * 32;
        return 1;  // File exists
      }
      
      // Found free entry
      if (createFile) {
        if (firstChar == 0xE5 || firstChar == 0) {
          memset(outEntry, 0, sizeof(Fat16DirectoryEntry));
          memcpy(outEntry->filename, filename, 8);
          memcpy(outEntry->extension, extension, 3);
          outEntry->attributes = FAT16_ATTR_FILE;
          *outOffset = (sector * entriesPerSector + entry) * 32;
          return 0;  // New file location
        }
      }
    }
  }
  return -1;  // No space or not found
}

// Optimized file operations
Fat16File* fat16Fopen(FilesystemState *fs, const char *pathname,
  const char *mode
) {
  Fat16DirectoryEntry entry;
  uint32_t entryOffset;
  char createFile = '\0';
  char append = '\0';
  for (int ii = 0; mode[ii] != '\0'; ii++) {
    // See the comment in fat16CreateOrFindFile for the rationale behind this
    // logic.
    createFile |= mode[ii] & 1;
    append |= mode[ii] & 2;
  }
  append = !append;

  int result = fat16CreateOrFindFile(fs, pathname, mode, &entry, &entryOffset);
  
  if (result < 0 || ((result == 0) && (!createFile))) {
    return NULL;
  }

  Fat16File *file = (Fat16File*) malloc(sizeof(Fat16File));
  if (!file) {
    return NULL;
  }

  file->currentCluster = entry.firstClusterLow;
  file->fileSize = entry.fileSize;
  file->firstCluster = entry.firstClusterLow;
  file->currentPosition = (append) ? file->fileSize : 0;
  file->pathname = strdup(pathname);

  return file;
}

uint32_t fat16GetNextCluster(FilesystemState *fs,
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
  // Quick bounds check
  uint32_t remainingBytes = file->fileSize - file->currentPosition;
  if (!remainingBytes) {
    return 0;
  }
  length = (length > remainingBytes) ? remainingBytes : length;

  // Read boot sector directly
  if (fs->blockDevice->readBlocks(fs->blockDevice->context, fs->startLba, 1,
      fs->blockSize, fs->blockBuffer) != 0) {
    return -1;
  }
  Fat16BootSector *bootSector = (Fat16BootSector*) fs->blockBuffer;

  // Calculate key offsets once
  uint32_t fatStart = fs->startLba + bootSector->reservedSectorCount;
  uint32_t bytesPerCluster = bootSector->bytesPerSector *
    bootSector->sectorsPerCluster;
  uint32_t dataStart = fatStart + (bootSector->numberOfFats *
    bootSector->sectorsPerFat) + ((bootSector->rootEntryCount * 32) /
    bootSector->bytesPerSector);
  uint32_t bytesRead = 0;

  while (bytesRead < length) {
    // Calculate current block position
    uint32_t clusterOffset = file->currentPosition % bytesPerCluster;
    uint32_t blockInCluster = clusterOffset / bootSector->bytesPerSector;
    uint32_t offsetInBlock = clusterOffset % bootSector->bytesPerSector;
    uint32_t currentBlock = dataStart +
      ((file->currentCluster - 2) * bootSector->sectorsPerCluster) +
      blockInCluster;

    // Read current block
    if (fs->blockDevice->readBlocks(fs->blockDevice->context, currentBlock, 1,
        fs->blockSize, fs->blockBuffer) != 0) {
      return bytesRead ? bytesRead : -1;
    }

    // Copy data from block
    uint32_t bytesToCopy = bootSector->bytesPerSector - offsetInBlock;
    if (bytesToCopy > (length - bytesRead)) {
      bytesToCopy = length - bytesRead;
    }
    memcpy((uint8_t*) buffer + bytesRead, fs->blockBuffer + offsetInBlock,
      bytesToCopy);

    // Update counters
    bytesRead += bytesToCopy;
    file->currentPosition += bytesToCopy;

    // Check if we need to move to next cluster
    if ((file->currentPosition % bytesPerCluster) == 0 &&
        bytesRead < length) {
      // Read FAT block
      uint32_t fatBlockNum = fatStart + ((file->currentCluster * 2) /
        fs->blockSize);
      if (fs->blockDevice->readBlocks(fs->blockDevice->context, fatBlockNum, 1,
          fs->blockSize, fs->blockBuffer) != 0) {
        return bytesRead;
      }

      // Get next cluster number
      uint16_t fatOffset = (file->currentCluster * 2) % fs->blockSize;
      uint16_t nextCluster = *((uint16_t*) &fs->blockBuffer[fatOffset]);
      
      if (nextCluster >= 0xFFF8) {
        break;
      }
      file->currentCluster = nextCluster;
    }
  }

  return bytesRead;
}

uint32_t fat16AllocateCluster(FilesystemState *fs,
  uint32_t fatStart, uint32_t currentCluster
) {
  if (fat16ReadBlock(fs, fatStart, fs->blockBuffer) != 0) {
    return 0;
  }
  
  uint16_t *fat = (uint16_t*) fs->blockBuffer;
  uint32_t newCluster = 0;
  
  // Find free cluster
  for (uint32_t ii = 2; ii < 0xFF0; ii++) {
    if (fat[ii] == 0) {
      newCluster = ii;
      break;
    }
  }
  
  if (newCluster) {
    fat[currentCluster] = newCluster;
    fat[newCluster] = 0xFFFF;
    // Write all FAT copies
    Fat16BootSector *bootSector = fat16ReadBootSector(fs);
    for (uint8_t ii = 0; ii < bootSector->numberOfFats; ii++) {
      fat16WriteBlock(fs, fatStart + (ii * bootSector->sectorsPerFat),
        fs->blockBuffer);
    }
  }
  
  return newCluster;
}

int fat16Write(FilesystemState *fs, Fat16File *file,
  const uint8_t *buffer, uint32_t length
) {
  // Read boot sector directly
  if (fs->blockDevice->readBlocks(fs->blockDevice->context, fs->startLba, 1,
      fs->blockSize, fs->blockBuffer) != 0) {
    return -1;
  }
  Fat16BootSector *bootSector = (Fat16BootSector*) fs->blockBuffer;

  // Calculate key offsets once
  uint32_t fatStart = fs->startLba + bootSector->reservedSectorCount;
  uint32_t bytesPerCluster = bootSector->bytesPerSector *
    bootSector->sectorsPerCluster;
  uint32_t dataStart = fatStart + (bootSector->numberOfFats *
    bootSector->sectorsPerFat) + ((bootSector->rootEntryCount * 32) /
    bootSector->bytesPerSector);
  uint32_t bytesWritten = 0;

  while (bytesWritten < length) {
    // Allocate new cluster if needed
    if (!file->currentCluster || 
        !(file->currentPosition % bytesPerCluster)) {
      // Read FAT block
      if (fs->blockDevice->readBlocks(fs->blockDevice->context, fatStart, 1,
          fs->blockSize, fs->blockBuffer) != 0) {
        return -1;
      }
      
      // Find free cluster
      uint16_t *fat = (uint16_t*) fs->blockBuffer;
      uint16_t newCluster = 0;
      for (uint16_t ii = 2; ii < 0xFF0; ii++) {
        if (fat[ii] == 0) {
          newCluster = ii;
          break;
        }
      }
      
      if (!newCluster) {
        return -1;
      }

      // Update FAT entries
      if (file->currentCluster) {
        fat[file->currentCluster] = newCluster;
      }
      fat[newCluster] = 0xFFFF;
      
      // Write FAT copies
      for (uint8_t ii = 0; ii < bootSector->numberOfFats; ii++) {
        if (fs->blockDevice->writeBlocks(fs->blockDevice->context,
            fatStart + (ii * bootSector->sectorsPerFat), 1,
            fs->blockSize, fs->blockBuffer) != 0) {
          return -1;
        }
      }
      
      file->currentCluster = newCluster;
    }

    // Calculate current block and write data
    uint32_t currentBlock = dataStart +
      ((file->currentCluster - 2) * bootSector->sectorsPerCluster);
    
    if (fs->blockDevice->writeBlocks(fs->blockDevice->context,
        currentBlock, 1, fs->blockSize, buffer + bytesWritten) != 0) {
      return -1;
    }

    bytesWritten += bootSector->bytesPerSector;
    file->currentPosition += bootSector->bytesPerSector;
    file->fileSize = file->currentPosition;
  }

  // Update directory entry
  Fat16DirectoryEntry entry;
  uint32_t entryOffset;
  if (fat16CreateOrFindFile(fs, file->pathname, "r", &entry,
      &entryOffset) == 1) {
    entry.fileSize = file->fileSize;
    if (!file->firstCluster) {
      file->firstCluster = entry.firstClusterLow = file->currentCluster;
    }
    
    uint32_t entrySector = fs->startLba + bootSector->reservedSectorCount +
      (bootSector->numberOfFats * bootSector->sectorsPerFat) +
      (entryOffset / fs->blockSize);
    entryOffset = entryOffset % fs->blockSize;
    
    if (fs->blockDevice->readBlocks(fs->blockDevice->context,
        entrySector, 1, fs->blockSize, fs->blockBuffer) == 0) {
      memcpy(fs->blockBuffer + entryOffset, &entry,
        sizeof(Fat16DirectoryEntry));
      fs->blockDevice->writeBlocks(fs->blockDevice->context,
        entrySector, 1, fs->blockSize, fs->blockBuffer);
    }
  }

  return bytesWritten;
}

int getPartitionInfo(FilesystemState *fs) {
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

