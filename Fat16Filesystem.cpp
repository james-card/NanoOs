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

// Custom includes
#include "Fat16Filesystem.h"

// Read a single block from the filesystem
static int fat16ReadBlock(FilesystemState *fs,
  uint32_t blockNumber, uint8_t *buffer
) {
  return fs->blockDevice->readBlocks(fs->blockDevice->context, 
    blockNumber, 1, fs->blockSize, buffer);
}

// Write a single block to the filesystem
static int fat16WriteBlock(FilesystemState *fs,
  uint32_t blockNumber, const uint8_t *buffer
) {
  return fs->blockDevice->writeBlocks(fs->blockDevice->context,
    blockNumber, 1, fs->blockSize, buffer);
}

// Read the boot sector to get filesystem parameters
static Fat16BootSector* fat16ReadBootSector(FilesystemState *fs) {
  if (fat16ReadBlock(fs, fs->startLba, fs->blockBuffer) != 0) {
    return NULL;
  }
  return (Fat16BootSector*) fs->blockBuffer;
}

// Compare two filenames in FAT16 8.3 format
static int fat16CompareFilenames(
  const uint8_t *dirEntry, const char *pathname
) {
  char filename[13];
  int ii;
  int jj = 0;

  // Copy filename and extension, removing spaces
  for (ii = 0; ii < FAT16_FILENAME_LENGTH; ii++) {
    if (dirEntry[ii] != ' ') {
      filename[jj++] = dirEntry[ii];
    }
  }
  
  if (dirEntry[FAT16_FILENAME_LENGTH] != ' ') {
    filename[jj++] = '.';
    for (ii = 0; ii < FAT16_EXTENSION_LENGTH; ii++) {
      if (dirEntry[FAT16_FILENAME_LENGTH + ii] != ' ') {
        filename[jj++] = dirEntry[FAT16_FILENAME_LENGTH + ii];
      }
    }
  }
  filename[jj] = '\0';

  int returnValue = 0;
  for (ii = 0; (returnValue == 0) && (filename[ii]) && (pathname[ii]); ii++) {
    returnValue = filename[ii] - toupper(pathname[ii]);
  }

  return returnValue;
}

// Find a file in the root directory
static Fat16DirectoryEntry* fat16FindFile(FilesystemState *fs,
  const char *pathname, Fat16BootSector *bootSector
) {
  uint32_t rootDirStart = fs->startLba + bootSector->reservedSectorCount + 
    (bootSector->numberOfFats * bootSector->sectorsPerFat);
  uint32_t rootDirBlocks = (bootSector->rootEntryCount * 
    FAT16_BYTES_PER_DIRECTORY_ENTRY) / fs->blockSize;
  
  for (uint32_t ii = 0; ii < rootDirBlocks; ii++) {
    if (fat16ReadBlock(fs, rootDirStart + ii, fs->blockBuffer) != 0) {
      return NULL;
    }
    
    Fat16DirectoryEntry *dirEntry = (Fat16DirectoryEntry*) fs->blockBuffer;
    for (uint32_t jj = 0; jj < FAT16_ENTRIES_PER_CLUSTER; jj++) {
      if (dirEntry[jj].filename[0] == 0) {
        // End of directory
        return NULL;
      }
      
      if (dirEntry[jj].filename[0] != 0xE5 && // Not deleted
          (dirEntry[jj].attributes & FAT16_ATTR_FILE) && // Is a file
          fat16CompareFilenames(dirEntry[jj].filename, pathname) == 0) {
        return &dirEntry[jj];
      }
    }
  }
  
  return NULL;
}

Fat16File* fat16Fopen(FilesystemState *fs,
  const char *pathname, const char *mode
) {
  if (fs == NULL || pathname == NULL || mode == NULL) {
    printString("Invalid parameters\n");
    return NULL;
  }
  
  // Only support "r" and "w" modes for now
  if (strcmp(mode, "r") != 0 && strcmp(mode, "w") != 0) {
    printString("Unsupported mode\n");
    return NULL;
  }
  
  Fat16BootSector *bootSector = fat16ReadBootSector(fs);
  if (bootSector == NULL) {
    printString("Failed to read boot sector\n");
    return NULL;
  }
  
  Fat16DirectoryEntry *dirEntry = fat16FindFile(fs, pathname, bootSector);
  if (dirEntry == NULL) {
    if (strcmp(mode, "r") == 0) {
      printString("File not found\n");
      return NULL;
    }
    // TODO: Implement file creation for write mode
    printString("File creation not implemented\n");
    return NULL;
  }
  
  Fat16File *file = (Fat16File*) malloc(sizeof(Fat16File));
  if (file == NULL) {
    printString("Memory allocation failed\n");
    return NULL;
  }
  
  file->currentCluster = dirEntry->firstClusterLow;
  file->currentPosition = 0;
  file->fileSize = dirEntry->fileSize;
  file->firstCluster = dirEntry->firstClusterLow;
  
  return file;
}

static uint32_t fat16GetNextCluster(FilesystemState *fs,
  uint32_t reservedSectorCount, uint16_t bytesPerSector,
  uint32_t currentCluster
) {
  uint32_t fatOffset = currentCluster * 2;
  uint32_t fatBlock = reservedSectorCount + (fatOffset / bytesPerSector);
  uint32_t fatIndex = fatOffset % bytesPerSector;
  
  // Read the FAT block
  if (fat16ReadBlock(fs, fatBlock, fs->blockBuffer) != 0) {
    return 0xFFFF;  // Error indicator
  }
  
  return *((uint16_t *) &fs->blockBuffer[fatIndex]);
}

int fat16Read(FilesystemState *fs, Fat16File *file,
  void *buffer, uint32_t length
) {
  if ((fs == NULL) || (file == NULL) || (buffer == NULL) || (length == 0)) {
    return -EINVAL;
  }

  // Don't read beyond EOF
  uint32_t remainingBytes = file->fileSize - file->currentPosition;
  if (length > remainingBytes) {
    length = remainingBytes;
  }

  if (length == 0) {
    return 0;
  }

  // Read boot sector and store needed parameters
  Fat16BootSector *bootSector = fat16ReadBootSector(fs);
  if (bootSector == NULL) {
    return -EIO;
  }
  
  // Store all parameters we need from boot sector before buffer is reused
  uint16_t bytesPerSector = bootSector->bytesPerSector;
  uint8_t sectorsPerCluster = bootSector->sectorsPerCluster;
  uint16_t reservedSectorCount = bootSector->reservedSectorCount;
  uint8_t numberOfFats = bootSector->numberOfFats;
  uint16_t sectorsPerFat = bootSector->sectorsPerFat;
  uint16_t rootEntryCount = bootSector->rootEntryCount;
  
  uint32_t bytesRead = 0;
  uint8_t *outputBuffer = (uint8_t *) buffer;
  uint32_t bytesPerCluster = bytesPerSector * sectorsPerCluster;
  
  while (bytesRead < length) {
    // Calculate position within current cluster
    uint32_t clusterOffset = file->currentPosition % bytesPerCluster;
    uint32_t blockInCluster = clusterOffset / bytesPerSector;
    uint32_t offsetInBlock = clusterOffset % bytesPerSector;
    
    // Calculate absolute block number
    uint32_t firstDataBlock = fs->startLba + reservedSectorCount +
      (numberOfFats * sectorsPerFat) +
      ((rootEntryCount * 32) / bytesPerSector);
    uint32_t currentBlock = firstDataBlock +
      ((file->currentCluster - 2) * sectorsPerCluster) +
      blockInCluster;

    // Read the block containing our data
    if (fat16ReadBlock(fs, currentBlock, fs->blockBuffer) != 0) {
      return -EIO;
    }

    // Calculate how many bytes we can read from this block
    uint32_t bytesThisBlock = bytesPerSector - offsetInBlock;
    if (bytesThisBlock > (length - bytesRead)) {
      bytesThisBlock = length - bytesRead;
    }

    // Copy the data to the output buffer
    memcpy(&outputBuffer[bytesRead],
      &fs->blockBuffer[offsetInBlock], bytesThisBlock);
    bytesRead += bytesThisBlock;
    file->currentPosition += bytesThisBlock;

    // Check if we need to move to the next cluster
    if ((file->currentPosition % bytesPerCluster) == 0 &&
        bytesRead < length) {
      uint32_t nextCluster = fat16GetNextCluster(fs, reservedSectorCount,
        bytesPerSector, file->currentCluster);
      if (nextCluster >= 0xFFF8) {  // End of chain
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
  // Validate parameters
  if (!fs || !file || !buffer || length == 0) {
    return -EINVAL;
  }

  const Fat16BootSector *bootSector = fat16ReadBootSector(fs);
  if (!bootSector) {
    return -EIO;
  }

  uint32_t bytesWritten = 0;
  uint32_t remainingBytes = length;
  uint32_t blockSize = fs->blockSize;
  uint32_t blockOffset = file->currentPosition % blockSize;
  uint32_t sectorsPerCluster = bootSector->sectorsPerCluster;
  uint32_t clusterSize = blockSize * sectorsPerCluster;
  uint32_t fatStartBlock = fs->startLba + bootSector->reservedSectorCount;
  
  // Read the FAT table into the block buffer
  if (fat16ReadBlock(fs, fatStartBlock, fs->blockBuffer) != 0) {
    return -EIO;
  }
  uint16_t *fatTable = (uint16_t *) fs->blockBuffer;
  
  while (remainingBytes > 0) {
    // If we're at the end of a cluster, we need to find/allocate next one
    if (file->currentCluster != 0 && 
        (file->currentPosition % clusterSize) == 0) {
      uint16_t nextCluster = fatTable[file->currentCluster];
      
      // If no next cluster or at end of chain, allocate a new one
      if (nextCluster >= 0xFFF8) {
        uint16_t newCluster = 0;
        // Find a free cluster
        for (uint16_t ii = 2; ii < bootSector->sectorsPerFat * 256; ii++) {
          if (fatTable[ii] == 0) {
            newCluster = ii;
            break;
          }
        }
        
        // Check if disk is full
        if (newCluster == 0) {
          // Update directory entry with current file size before returning
          Fat16DirectoryEntry *dirEntry = fat16FindFile(fs,
            NULL, // We'd need filename here - simplified
            (Fat16BootSector *)bootSector);
          if (dirEntry) {
            dirEntry->fileSize = file->fileSize;
            // Write directory entry block back
            if (fat16WriteBlock(fs, fs->startLba +
                bootSector->reservedSectorCount +
                (bootSector->numberOfFats * bootSector->sectorsPerFat),
                fs->blockBuffer) != 0) {
              return -EIO;
            }
          }
          return bytesWritten;
        }
        
        // Update FAT table
        fatTable[file->currentCluster] = newCluster;
        fatTable[newCluster] = 0xFFFF;  // End of chain marker
        
        // Write FAT table back to all FAT copies
        for (uint8_t ii = 0; ii < bootSector->numberOfFats; ii++) {
          if (fat16WriteBlock(fs, fatStartBlock + 
              (ii * bootSector->sectorsPerFat),
              fs->blockBuffer) != 0) {
            return -EIO;
          }
        }
        
        file->currentCluster = newCluster;
      } else {
        file->currentCluster = nextCluster;
      }
    }
    
    // Calculate current block number from cluster and position
    uint32_t currentBlock = fs->startLba +
      bootSector->reservedSectorCount +
      (bootSector->numberOfFats * bootSector->sectorsPerFat) +
      (bootSector->rootEntryCount * 32 / blockSize) +
      ((file->currentCluster - 2) * sectorsPerCluster);
    
    // If we're not at a block boundary, need to read-modify-write
    if (blockOffset > 0) {
      if (fat16ReadBlock(fs, currentBlock, fs->blockBuffer) != 0) {
        return -EIO;
      }
    }
    
    // Calculate how many bytes we can write in this block
    uint32_t blockBytesAvailable = blockSize - blockOffset;
    uint32_t bytesToWrite = (remainingBytes < blockBytesAvailable) ?
      remainingBytes : blockBytesAvailable;
    
    // Copy the bytes to write into the block buffer
    memcpy(&fs->blockBuffer[blockOffset],
      &buffer[bytesWritten], bytesToWrite);
    
    // Write the block back to storage
    if (fat16WriteBlock(fs, currentBlock, fs->blockBuffer) != 0) {
      return -EIO;
    }
    
    // Update our position tracking
    bytesWritten += bytesToWrite;
    remainingBytes -= bytesToWrite;
    file->currentPosition += bytesToWrite;
    if (file->currentPosition > file->fileSize) {
      file->fileSize = file->currentPosition;
    }
    
    // Reset block offset since future iterations will be block-aligned
    blockOffset = 0;
  }
  
  // Update directory entry with new file size
  Fat16DirectoryEntry *dirEntry = fat16FindFile(fs,
    NULL, // We'd need filename here - simplified
    (Fat16BootSector *)bootSector);
  if (dirEntry) {
    dirEntry->fileSize = file->fileSize;
    // Update modification time/date (simplified - should use RTC if available)
    dirEntry->lastModifiedTime = 0;  // Would set from RTC
    dirEntry->lastModifiedDate = 0;  // Would set from RTC
    
    // Write directory entry block back
    if (fat16WriteBlock(fs, fs->startLba +
        bootSector->reservedSectorCount +
        (bootSector->numberOfFats * bootSector->sectorsPerFat),
        fs->blockBuffer) != 0) {
      return -EIO;
    }
  }
  
  return bytesWritten;
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
  printDebug("Handling FILESYSTEM_OPEN_FILE command.\n");

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
  printDebug("Exiting FILESYSTEM_OPEN_FILE command.\n");
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
  free(nanoOsFile->file);
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

/// @fn void handleFilesystemMessages(FilesystemState *filesystemState)
///
/// @brief Handle filesystem messages from the process's queue until there are
/// no more waiting.
///
/// @param filesystemState A pointer to the FilesystemState structure
///   maintained by the filesystem process.
///
/// @return This function returns no value.
void handleFilesystemMessages(FilesystemState *filesystemState) {
  ProcessMessage *processMessage = processMessageQueuePop();
  while (processMessage != NULL) {
    FilesystemCommandResponse messageType
      = (FilesystemCommandResponse) processMessageType(processMessage);
    if (messageType >= NUM_FILESYSTEM_COMMANDS) {
      processMessage = processMessageQueuePop();
      continue;
    }
    
    filesystemCommandHandlers[messageType](filesystemState, processMessage);
    processMessage = processMessageQueuePop();
  }
  
  return;
}

/// @fn int getPartitionInfo(FilesystemState *filesystemState)
///
/// @brief Get the start and end LBA information from the partition specified
/// by the blockDevice->partitionNumber in the provided FilesystemState.
///
/// @param filesystemState A pointer to the FilesystemState object maintained
///   by the filesystem process.
///
/// @return Returns 0 on success, negative error code on failure.
int getPartitionInfo(FilesystemState *filesystemState) {
  if (filesystemState->blockDevice->partitionNumber == 0) {
    printString("ERROR! Partition number not set\n");
    return -1;
  }

  // Read block 0 (MBR)
  if (filesystemState->blockDevice->readBlocks(
      filesystemState->blockDevice->context,
      0,  // start block
      1,  // number of blocks
      filesystemState->blockSize,
      filesystemState->blockBuffer) != 0
  ) {
    printString("Error reading MBR\n");
    return -2;
  }

  // Partition table starts at offset 0x1BE
  uint8_t *partitionTable = filesystemState->blockBuffer + 0x1BE;

  // Each entry is 16 bytes
  uint8_t *entry = partitionTable
    + ((filesystemState->blockDevice->partitionNumber - 1) * 16);
  uint8_t type = entry[4];
  
  // (type == 0x04 /* Up to 32 MB FAT16 using CHS addressing */)
  // (type == 0x06 /* Over 32 MB FAT16 using CHS addressing */)
  // (type == 0x0e /* FAT16 using LBA addressing */)
  // (type == 0x14 /* Up to 32 MB hidden FAT16 using CHS addressing */)
  // (type == 0x16 /* Over 32 MB hidden FAT16 using CHS addressing */)
  // (type == 0x1e /* Hidden FAT16 using LBA addressing */)
  // We're only supporting FAT16 using LBA addressing.
  if (
       (type == 0x0e /* FAT16 using LBA addressing */)
    || (type == 0x1e /* Hidden FAT16 using LBA addressing */)
  ) {
    filesystemState->startLba
      = (((uint32_t) entry[11]) << 24)
      | (((uint32_t) entry[10]) << 16)
      | (((uint32_t) entry[ 9]) <<  8)
      | (((uint32_t) entry[ 8]) <<  0);
    uint32_t numSectors
      = (((uint32_t) entry[15]) << 24)
      | (((uint32_t) entry[14]) << 16)
      | (((uint32_t) entry[13]) <<  8)
      | (((uint32_t) entry[12]) <<  0);
    filesystemState->endLba = filesystemState->startLba + numSectors - 1;
  } else {
    printString("ERROR: Filesystem partition type is ");
    printInt(type);
    printString("\n");
    return -3;
  }

  return 0;
}

/// @fn void* runFat16Filesystem(void *args)
///
/// @brief Process entry-point for the filesystem process.  Enters an infinite
/// loop for processing commands.
///
/// @param args Any arguments to this function, cast to a void*.  Currently
///   ignored by this function.
///
/// @return This function never returns, but would return NULL if it did.
void* runFat16Filesystem(void *args) {
  FilesystemState filesystemState;
  memset(&filesystemState, 0, sizeof(filesystemState));
  filesystemState.blockDevice = (BlockStorageDevice*) args;
  filesystemState.blockSize = 512;
  // Yield before we make any more calls.
  ProcessMessage *schedulerMessage = (ProcessMessage*) coroutineYield(NULL);

  filesystemState.blockBuffer = (uint8_t*) malloc(filesystemState.blockSize);
  getPartitionInfo(&filesystemState);

  printDebug("Entering runFat16Filesystem while loop\n");
  while (1) {
    if (schedulerMessage != NULL) {
      // We have a message from the scheduler that we need to process.  This
      // is not the expected case, but it's the priority case, so we need to
      // list it first.
      FilesystemCommandResponse messageType
        = (FilesystemCommandResponse) processMessageType(schedulerMessage);
      printDebug("Received message ");
      printDebug(messageType);
      printDebug(" from scheduler\n");
      if (messageType < NUM_FILESYSTEM_COMMANDS) {
        filesystemCommandHandlers[messageType](
          &filesystemState, schedulerMessage);
      } else {
        printString("ERROR!!!  Received unknown filesystem command ");
        printInt(messageType);
        printString(" from scheduler.\n");
      }
    } else {
      handleFilesystemMessages(&filesystemState);
    }
    schedulerMessage = (ProcessMessage*) coroutineYield(NULL);
  }

  return NULL;
}

/// @fn FILE* filesystemFOpen(const char *pathname, const char *mode)
///
/// @brief NanoOs implementation of standard fopen command.
///
/// @param pathname Pointer to a C string with the full path to the file to
///   open.
/// @param mode Pointer to a C string containing one of the standard file mode
///   combinations.
///
/// @return Returns a pointer to a newly-allocated and initialized FILE object
/// on success, NULL on failure.
FILE* filesystemFOpen(const char *pathname, const char *mode) {
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_OPEN_FILE,
    /* func= */ (intptr_t) mode, /* data= */ (intptr_t) pathname, true);
  processMessageWaitForDone(processMessage, NULL);
  FILE *returnValue = nanoOsMessageDataPointer(processMessage, FILE*);
  processMessageRelease(processMessage);
  return returnValue;
}

/// @fn int filesystemFClose(FILE *stream)
///
/// @brief Close a file previously opened with filesystemFOpen.
///
/// @param stream A pointer to the FILE object that was previously opened.
///
/// @return Returns 0 on success, EOF on failure.  On failure, the value of
/// errno is also set to the appropriate error.
int filesystemFClose(FILE *stream) {
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_CLOSE_FILE,
    /* func= */ 0, /* data= */ (intptr_t) stream, true);
  processMessageWaitForDone(processMessage, NULL);
  processMessageRelease(processMessage);
  return 0;
}

