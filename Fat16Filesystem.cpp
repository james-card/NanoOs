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
  return strcmp(filename, pathname);
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
    printString("Invalid parameters");
    return NULL;
  }
  
  // Only support "r" and "w" modes for now
  if (strcmp(mode, "r") != 0 && strcmp(mode, "w") != 0) {
    printString("Unsupported mode");
    return NULL;
  }
  
  Fat16BootSector *bootSector = fat16ReadBootSector(fs);
  if (bootSector == NULL) {
    printString("Failed to read boot sector");
    return NULL;
  }
  
  Fat16DirectoryEntry *dirEntry = fat16FindFile(fs, pathname, bootSector);
  if (dirEntry == NULL) {
    if (strcmp(mode, "r") == 0) {
      printString("File not found");
      return NULL;
    }
    // TODO: Implement file creation for write mode
    printString("File creation not implemented");
    return NULL;
  }
  
  Fat16File *file = (Fat16File*) malloc(sizeof(Fat16File));
  if (file == NULL) {
    printString("Memory allocation failed");
    return NULL;
  }
  
  file->currentCluster = dirEntry->firstClusterLow;
  file->currentPosition = 0;
  file->fileSize = dirEntry->fileSize;
  file->firstCluster = dirEntry->firstClusterLow;
  file->mode = mode[0];
  file->fs = fs;
  
  return file;
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
  (void) filesystemState;

  const char *pathname = nanoOsMessageDataPointer(processMessage, char*);
  const char *mode = nanoOsMessageFuncPointer(processMessage, char*);
  Fat16File *fat16File = fat16Fopen(filesystemState, pathname, mode);
  NanoOsFile *nanoOsFile = (NanoOsFile*) malloc(sizeof(NanoOsFile));
  nanoOsFile->file = fat16File;

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
  free(nanoOsFile->file);
  free(nanoOsFile);

  processMessageSetDone(processMessage);
  return 0;
}

/// @var filesystemCommandHandlers
///
/// @brief Array of FilesystemCommandHandler function pointers.
const FilesystemCommandHandler filesystemCommandHandlers[] = {
  fat16FilesystemOpenFileCommandHandler,  // FILESYSTEM_OPEN_FILE
  fat16FilesystemCloseFileCommandHandler, // FILESYSTEM_CLOSE_FILE
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
  processYield();

  filesystemState.blockBuffer = (uint8_t*) malloc(filesystemState.blockSize);
  getPartitionInfo(&filesystemState);

  ProcessMessage *schedulerMessage;
  while (1) {
    schedulerMessage = (ProcessMessage*) coroutineYield(NULL);
    if (schedulerMessage != NULL) {
      // We have a message from the scheduler that we need to process.  This
      // is not the expected case, but it's the priority case, so we need to
      // list it first.
      FilesystemCommandResponse messageType
        = (FilesystemCommandResponse) processMessageType(schedulerMessage);
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

