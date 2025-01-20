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

static int fat16BlockOperation(FilesystemState *fs, uint32_t block, 
    uint8_t *buffer, bool isWrite) {
  return isWrite ? 
    fs->blockDevice->writeBlocks(fs->blockDevice->context, block, 1,
      fs->blockSize, buffer) :
    fs->blockDevice->readBlocks(fs->blockDevice->context, block, 1, 
      fs->blockSize, buffer);
}

static inline void fat16FormatFilename(
  const char *pathname, char *formattedName
) {
  memset(formattedName, ' ', 11);
  const char *dot = strrchr(pathname, '.');
  size_t nameLen = dot ? (dot - pathname) : strlen(pathname);
  
  for (uint8_t ii = 0; ii < 8 && ii < nameLen; ii++) {
    formattedName[ii] = toupper(pathname[ii]);
  }
  
  if (dot) {
    for (uint8_t ii = 0; ii < 3 && dot[ii + 1]; ii++) {
      formattedName[8 + ii] = toupper(dot[ii + 1]);
    }
  }
}

Fat16File* fat16Fopen(FilesystemState *fs, const char *pathname, 
    const char *mode) {
  bool createFile = (mode[0] & 1);
  bool append = (mode[0] == 'a');
  uint8_t *buffer = fs->blockBuffer;
  
  // Read boot sector
  if (fat16BlockOperation(fs, fs->startLba, buffer, false)) {
    return NULL;
  }
  
  // Create file structure to hold common values
  Fat16File *file = (Fat16File*) malloc(sizeof(Fat16File));
  if (!file) {
    return NULL;
  }
  
  // Store common boot sector values
  file->bytesPerSector = *((uint16_t*) &buffer[FAT16_BOOT_BYTES_PER_SECTOR]);
  file->sectorsPerCluster = buffer[FAT16_BOOT_SECTORS_PER_CLUSTER];
  file->reservedSectors = *((uint16_t*) &buffer[FAT16_BOOT_RESERVED_SECTORS]);
  file->numberOfFats = buffer[FAT16_BOOT_NUMBER_OF_FATS];
  file->rootEntries = *((uint16_t*) &buffer[FAT16_BOOT_ROOT_ENTRIES]);
  file->sectorsPerFat = *((uint16_t*) &buffer[FAT16_BOOT_SECTORS_PER_FAT]);
  file->bytesPerCluster = file->bytesPerSector * file->sectorsPerCluster;
  file->fatStart = fs->startLba + file->reservedSectors;
  file->rootStart = file->fatStart + (file->numberOfFats * file->sectorsPerFat);
  file->dataStart = file->rootStart
    + (((file->rootEntries << 5) + file->bytesPerSector - 1)
        / file->bytesPerSector);
  
  // Format filename
  char upperName[12];
  fat16FormatFilename(pathname, upperName);
  upperName[11] = '\0';
  
  // Search directory
  for (uint16_t ii = 0; ii < file->rootEntries; ii++) {
    uint32_t block = file->rootStart + (ii / (file->bytesPerSector >> 5));
    if (fat16BlockOperation(fs, block, buffer, false)) {
      free(file);
      return NULL;
    }
    
    uint8_t *entry = buffer + ((ii % (file->bytesPerSector >> 5)) << 5);
    uint8_t firstChar = entry[FAT16_DIR_FILENAME];
    
    if (firstChar != 0xE5 && firstChar != 0 &&
        memcmp(entry + FAT16_DIR_FILENAME, upperName, 11) == 0) {
      // Found existing file
      file->currentCluster = *((uint16_t*) &entry[FAT16_DIR_FIRST_CLUSTER_LOW]);
      file->fileSize = *((uint32_t*) &entry[FAT16_DIR_FILE_SIZE]);
      file->firstCluster = file->currentCluster;
      file->currentPosition = append ? file->fileSize : 0;
      file->pathname = strdup(pathname);
      return file;
    }
    
    // Handle file creation if needed
    if (createFile && (firstChar == 0xE5 || firstChar == 0)) {
      memcpy(entry + FAT16_DIR_FILENAME, upperName, 11);
      entry[FAT16_DIR_ATTRIBUTES] = 0x20;  // Normal file
      memset(entry + FAT16_DIR_ATTRIBUTES + 1, 0, 
        32 - (FAT16_DIR_ATTRIBUTES + 1));
        
      if (fat16BlockOperation(fs, block, buffer, true)) {
        free(file);
        return NULL;
      }
      
      file->currentCluster = 0;
      file->fileSize = 0;
      file->firstCluster = 0;
      file->currentPosition = 0;
      file->pathname = strdup(pathname);
      return file;
    }
  }
  
  free(file);
  return NULL;
}

int fat16Read(FilesystemState *fs, Fat16File *file, void *buffer, 
    uint32_t length) {
  if (!length || file->currentPosition >= file->fileSize) {
    return 0;
  }
  
  uint32_t bytesRead = 0;
  while (bytesRead < length) {
    uint32_t block = file->dataStart + ((file->currentCluster - 2) * 
      file->sectorsPerCluster);
      
    if (fat16BlockOperation(fs, block, fs->blockBuffer, false)) {
      break;
    }
    
    uint16_t toCopy = min(file->bytesPerSector, length - bytesRead);
    memcpy((uint8_t*) buffer + bytesRead, fs->blockBuffer, toCopy);
    bytesRead += toCopy;
    file->currentPosition += toCopy;
    
    // Get next cluster if needed
    if (!(file->currentPosition % (file->bytesPerSector * 
        file->sectorsPerCluster))) {
      // Read FAT
      uint32_t fatBlock = fs->startLba + file->reservedSectors +
        ((file->currentCluster * 2) / file->bytesPerSector);
      
      if (fat16BlockOperation(fs, fatBlock, fs->blockBuffer, false)) {
        break;
      }
      
      uint16_t nextCluster = *((uint16_t*) &fs->blockBuffer
        [(file->currentCluster * 2) % file->bytesPerSector]);
      if (nextCluster >= 0xFFF8) {
        break;
      }
      file->currentCluster = nextCluster;
    }
  }
  
  return bytesRead;
}

int fat16Write(FilesystemState *fs, Fat16File *file, const uint8_t *buffer,
    uint32_t length) {
  uint32_t bytesWritten = 0;
  while (bytesWritten < length) {
    // Allocate new cluster if needed
    if (!file->currentCluster || 
        !(file->currentPosition % (file->bytesPerSector * 
        file->sectorsPerCluster))) {
      // Find free cluster
      if (fat16BlockOperation(fs, file->fatStart, fs->blockBuffer, false)) {
        return -1;
      }
      
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
      fat[newCluster] = FAT16_CLUSTER_CHAIN_END;  // Using defined constant
      
      // Write FAT copies
      for (uint8_t ii = 0; ii < file->numberOfFats; ii++) {
        if (fat16BlockOperation(fs,
            file->fatStart + (ii * file->sectorsPerFat),
            fs->blockBuffer, true)) {
          return -1;
        }
      }
      
      if (!file->firstCluster) {
        file->firstCluster = newCluster;
      }
      file->currentCluster = newCluster;
    }

    // Calculate current sector and offset
    uint32_t block = file->dataStart + ((file->currentCluster - 2) * 
      file->sectorsPerCluster);
    uint32_t sectorOffset = file->currentPosition % file->bytesPerSector;
    
    // If not writing a full sector, read existing data first
    if ((sectorOffset != 0) || (length - bytesWritten < file->bytesPerSector)) {
      if (fat16BlockOperation(fs, block, fs->blockBuffer, false)) {
        return -1;
      }
    }
    
    // Calculate how many bytes to write in this sector
    uint32_t bytesThisWrite = file->bytesPerSector - sectorOffset;
    if (bytesThisWrite > (length - bytesWritten)) {
      bytesThisWrite = length - bytesWritten;
    }
    
    // Copy data to buffer at correct offset
    memcpy(fs->blockBuffer + sectorOffset, buffer + bytesWritten, bytesThisWrite);
    
    if (fat16BlockOperation(fs, block, fs->blockBuffer, true)) {
      return -1;
    }

    bytesWritten += bytesThisWrite;
    file->currentPosition += bytesThisWrite;
    if (file->currentPosition > file->fileSize) {
      file->fileSize = file->currentPosition;
    }
  }

  // Update directory entry with new size
  uint32_t rootDirStart = file->fatStart +
    (file->numberOfFats * file->sectorsPerFat);
  
  char upperName[12];
  fat16FormatFilename(file->pathname, upperName);

  for (uint16_t ii = 0; ii < file->rootEntries; ii++) {
    uint32_t block = rootDirStart + (ii / (file->bytesPerSector >> 5));
    if (fat16BlockOperation(fs, block, fs->blockBuffer, false)) {
      return -1;
    }
    
    uint8_t *entry
      = fs->blockBuffer + ((ii % (file->bytesPerSector >> 5)) << 5);
    if (memcmp(entry + FAT16_DIR_FILENAME, upperName, 11) == 0) {
      *((uint32_t*) &entry[FAT16_DIR_FILE_SIZE]) = file->fileSize;
      if (!*((uint16_t*) &entry[FAT16_DIR_FIRST_CLUSTER_LOW])) {
        *((uint16_t*) &entry[FAT16_DIR_FIRST_CLUSTER_LOW]) = file->firstCluster;
      }
      
      if (fat16BlockOperation(fs, block, fs->blockBuffer, true)) {
        return -1;
      }
      break;
    }
  }

  return bytesWritten;
}

int fat16Remove(FilesystemState *fs, const char *pathname) {
  // We need a file handle to access the cached values
  Fat16File *file = fat16Fopen(fs, pathname, "r");
  if (!file) {
    return -1;
  }
  
  // Format filename
  char upperName[12];
  fat16FormatFilename(pathname, upperName);
  upperName[11] = '\0';
  
  // Search for file in directory
  for (uint16_t ii = 0; ii < file->rootEntries; ii++) {
    uint32_t block = file->rootStart + (ii / (file->bytesPerSector >> 5));
    if (fat16BlockOperation(fs, block, fs->blockBuffer, false)) {
      free(file);
      return -1;
    }
    
    uint8_t *entry = fs->blockBuffer + 
      ((ii % (file->bytesPerSector >> 5)) << 5);
    uint8_t firstChar = entry[FAT16_DIR_FILENAME];
    
    if (firstChar != 0xE5 && firstChar != 0 &&
        memcmp(entry + FAT16_DIR_FILENAME, upperName, 11) == 0) {
      // Found file - mark entry as deleted
      entry[FAT16_DIR_FILENAME] = 0xE5;
      int result = fat16BlockOperation(fs, block, fs->blockBuffer, true);
      free(file);
      return result;
    }
  }
  
  free(file);
  return -1;  // File not found
}

int fat16Seek(FilesystemState *fs, Fat16File *file, int32_t offset,
    uint8_t whence) {
  // Calculate target position
  uint32_t newPosition = file->currentPosition;
  switch (whence) {
    case SEEK_SET:
      newPosition = offset;
      break;
    case SEEK_CUR:
      newPosition += offset;
      break;
    case SEEK_END:
      newPosition = file->fileSize + offset;
      break;
    default:
      return -1;
  }
  
  // Check bounds
  if (newPosition > file->fileSize) {
    return -1;
  }
  
  // If seek target is before current position, reset to start
  if (newPosition < file->currentPosition) {
    file->currentPosition = 0;
    file->currentCluster = file->firstCluster;
  }
  
  // If no seeking needed, we're done
  if (newPosition == file->currentPosition) {
    return 0;
  }
  
  // Skip through cluster chain until we reach target position
  while (file->currentPosition + file->bytesPerCluster <= newPosition) {
    // Read FAT entry
    uint32_t fatBlock = file->fatStart + 
      ((file->currentCluster * 2) / file->bytesPerSector);
    if (fat16BlockOperation(fs, fatBlock, fs->blockBuffer, false)) {
      return -1;
    }
    
    uint16_t nextCluster = *((uint16_t*) &fs->blockBuffer
      [(file->currentCluster * 2) % file->bytesPerSector]);
    
    if (nextCluster >= 0xFFF8) {
      return -1;  // End of chain reached too soon
    }
    
    file->currentCluster = nextCluster;
    file->currentPosition += file->bytesPerCluster;
  }
  
  // Final position adjustment
  file->currentPosition = newPosition;
  return 0;
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

/// @fn int fat16FilesystemRemoveFileCommandHandler(
///   FilesystemState *filesystemState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_REMOVE_FILE command.
///
/// @param filesystemState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemRemoveFileCommandHandler(
  FilesystemState *filesystemState, ProcessMessage *processMessage
) {
  const char *pathname = nanoOsMessageDataPointer(processMessage, char*);
  int returnValue = fat16Remove(filesystemState, pathname);

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = (intptr_t) returnValue;
  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int fat16FilesystemSeekFileCommandHandler(
///   FilesystemState *filesystemState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_SEEK_FILE command.
///
/// @param filesystemState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int fat16FilesystemSeekFileCommandHandler(
  FilesystemState *filesystemState, ProcessMessage *processMessage
) {
  FilesystemSeekParameters *filesystemSeekParameters
    = nanoOsMessageDataPointer(processMessage, FilesystemSeekParameters*);
  int returnValue = fat16Seek(filesystemState,
    (Fat16File*) filesystemSeekParameters->stream->file,
    filesystemSeekParameters->offset,
    filesystemSeekParameters->whence);

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = (intptr_t) returnValue;
  processMessageSetDone(processMessage);
  return 0;
}

/// @var filesystemCommandHandlers
///
/// @brief Array of FilesystemCommandHandler function pointers.
const FilesystemCommandHandler filesystemCommandHandlers[] = {
  fat16FilesystemOpenFileCommandHandler,   // FILESYSTEM_OPEN_FILE
  fat16FilesystemCloseFileCommandHandler,  // FILESYSTEM_CLOSE_FILE
  fat16FilesystemReadFileCommandHandler,   // FILESYSTEM_READ_FILE
  fat16FilesystemWriteFileCommandHandler,  // FILESYSTEM_WRITE_FILE
  fat16FilesystemRemoveFileCommandHandler, // FILESYSTEM_REMOVE_FILE
  fat16FilesystemSeekFileCommandHandler,   // FILESYSTEM_SEEK_FILE
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

int filesystemRemove(const char *pathname) {
  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_REMOVE_FILE,
    /* func= */ 0, (intptr_t) pathname, true);
  processMessageWaitForDone(msg, NULL);
  int returnValue = nanoOsMessageDataValue(msg, int);
  processMessageRelease(msg);
  return returnValue;
}

int filesystemFSeek(FILE *stream, long offset, int whence) {
 FilesystemSeekParameters filesystemSeekParameters = {
    .stream = stream,
    .offset = offset,
    .whence = whence,
  };
  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_REMOVE_FILE,
    /* func= */ 0, (intptr_t) &filesystemSeekParameters, true);
  processMessageWaitForDone(msg, NULL);
  int returnValue = nanoOsMessageDataValue(msg, int);
  processMessageRelease(msg);
  return returnValue;
}

