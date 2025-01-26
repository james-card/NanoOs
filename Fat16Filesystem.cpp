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

// Directory search result codes
#define FAT16_DIR_SEARCH_ERROR -1
#define FAT16_DIR_SEARCH_FOUND 0
#define FAT16_DIR_SEARCH_DELETED 1
#define FAT16_DIR_SEARCH_NOT_FOUND 2

/// @fn int fat16ReadBlock(FilesystemState *fs, uint32_t block,
///   uint8_t *buffer)
///
/// @brief Read a single block from the underlying storage device for the
/// filesystem.
///
/// @param fs A pointer to the FilesystemState object maintained by the
///   filesystem process that contains information about how to access the
///   underlying block storage device.
/// @param block The logical block number of the storage device to read from.
/// @param buffer A pointer to a character buffer that is one block in size to
///   read the data into.
///
/// @return Returns 0 on success, a standard POSIX error on failure.
static inline int fat16ReadBlock(FilesystemState *fs, uint32_t block, 
    uint8_t *buffer
) {
  return
    fs->blockDevice->readBlocks(fs->blockDevice->context, block, 1, 
      fs->blockSize, buffer);
}

/// @fn int fat16WriteBlock(FilesystemState *fs, uint32_t block,
///   uint8_t *buffer)
///
/// @brief Write a single block to the underlying storage device for the
/// filesystem.
///
/// @param fs A pointer to the FilesystemState object maintained by the
///   filesystem process that contains information about how to access the
///   underlying block storage device.
/// @param block The logical block number of the storage device to write to.
/// @param buffer A pointer to a character buffer that is one block in size to
///   write the data from.
///
/// @return Returns 0 on success, a standard POSIX error on failure.
static int fat16WriteBlock(FilesystemState *fs, uint32_t block, 
    uint8_t *buffer
) {
  return
    fs->blockDevice->writeBlocks(fs->blockDevice->context, block, 1,
      fs->blockSize, buffer);
}

/// @fn void fat16FormatFilename(const char *pathname, char *formattedName)
///
/// @brief Format a user-supplied pathname into a name formatted for easy
/// comparison in a directory search.
///
/// @param pathname A pointer to the user-supplied path to format.
/// @param formattedName A pointer to the character buffer that is to be
///   populated by this function.
///
/// @return This function returns no value.
static void fat16FormatFilename(const char *pathname, char *formattedName) {
  memset(formattedName, ' ', FAT16_FULL_NAME_LENGTH);
  const char *dot = strrchr(pathname, '.');
  size_t nameLen = dot ? (dot - pathname) : strlen(pathname);
  
  for (uint8_t ii = 0; ii < FAT16_FILENAME_LENGTH && ii < nameLen; ii++) {
    formattedName[ii] = toupper(pathname[ii]);
  }
  
  if (dot) {
    for (uint8_t ii = 0; ii < FAT16_EXTENSION_LENGTH && dot[ii + 1]; ii++) {
      formattedName[FAT16_FILENAME_LENGTH + ii] = toupper(dot[ii + 1]);
    }
  }
}

/// @brief Search the root directory for a file entry
///
/// @param fs Pointer to the filesystem state structure
/// @param file Pointer to the FAT16 file structure containing common values
/// @param pathname The pathname to search for
/// @param entry Pointer to where the directory entry buffer should be stored
/// @param block Pointer to where the block number containing the entry should
///   be stored
/// @param entryIndex Pointer to where the entry index within the block should
///   be stored
///
/// @return FAT16_DIR_SEARCH_ERROR on error, FAT16_DIR_SEARCH_FOUND if entry
///   found, FAT16_DIR_SEARCH_DELETED if a deleted entry with matching name
///   found, FAT16_DIR_SEARCH_NOT_FOUND if no matching entry found
static int fat16FindDirectoryEntry(FilesystemState *fs, Fat16File *file,
    const char *pathname, uint8_t **entry, uint32_t *block,
    uint16_t *entryIndex)
{
  char upperName[FAT16_FULL_NAME_LENGTH + 1];
  fat16FormatFilename(pathname, upperName);
  upperName[FAT16_FULL_NAME_LENGTH] = '\0';
  
  for (uint16_t ii = 0; ii < file->rootEntries; ii++) {
    *block = file->rootStart + (ii / (file->bytesPerSector >>
      FAT16_DIR_ENTRIES_PER_SECTOR_SHIFT));
    if (fat16ReadBlock(fs, *block, fs->blockBuffer)) {
      return FAT16_DIR_SEARCH_ERROR;
    }
    
    *entry = fs->blockBuffer + ((ii % (file->bytesPerSector >>
      FAT16_DIR_ENTRIES_PER_SECTOR_SHIFT)) * FAT16_BYTES_PER_DIRECTORY_ENTRY);
    uint8_t firstChar = (*entry)[FAT16_DIR_FILENAME];
    
    if (memcmp(*entry + FAT16_DIR_FILENAME, upperName,
        FAT16_FULL_NAME_LENGTH) == 0) {
      if (entryIndex) {
        *entryIndex = ii;
      }
      if (firstChar == FAT16_DELETED_MARKER) {
        return FAT16_DIR_SEARCH_DELETED;
      } else if (firstChar != FAT16_EMPTY_ENTRY) {
        return FAT16_DIR_SEARCH_FOUND;
      }
    } else if (firstChar == FAT16_EMPTY_ENTRY) {
      // Once we hit an empty entry, there are no more entries to check
      break;
    }
  }
  
  return FAT16_DIR_SEARCH_NOT_FOUND;
}

/// @brief Open a file in the FAT16 filesystem
///
/// @param fs Pointer to the filesystem state structure containing the block
///   device and buffer information
/// @param pathname The name of the file to open
/// @param mode The mode to open the file in ("r" for read, "w" for write,
///   "a" for append)
///
/// @return Pointer to a newly allocated Fat16File structure on success,
///   NULL on failure. The caller is responsible for freeing the returned
///   structure when finished with it.
Fat16File* fat16Fopen(FilesystemState *fs, const char *pathname,
    const char *mode
) {
  bool createFile = (mode[0] & 1); // 'w' and 'a' have LSB set
  bool append = (mode[0] == 'a');
  uint8_t *buffer = NULL;
  uint8_t *entry = NULL;
  uint32_t block = 0;
  Fat16File *file = NULL;
  int result = 0;
  char upperName[FAT16_FULL_NAME_LENGTH + 1];
  
  if (fs->numOpenFiles == 0) {
    fs->blockBuffer = (uint8_t*) malloc(fs->blockSize);
    if (fs->blockBuffer == NULL) {
      printDebug("ERROR: malloc of fs->blockBuffer returned NULL!\n");
      goto exit;
    }
  }
  buffer = fs->blockBuffer;

  // Read boot sector
  if (fat16ReadBlock(fs, fs->startLba, buffer)) {
    printDebug("ERROR: Reading boot sector failed!\n");
    goto exit;
  }
  
  // Create file structure to hold common values
  file = (Fat16File*) malloc(sizeof(Fat16File));
  if (!file) {
    printDebug("ERROR: malloc of Fat16File failed!\n");
    goto exit;
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
  file->dataStart = file->rootStart +
    (((file->rootEntries * FAT16_BYTES_PER_DIRECTORY_ENTRY) +
    file->bytesPerSector - 1) / file->bytesPerSector);
  
  result = fat16FindDirectoryEntry(fs, file, pathname, &entry, &block, NULL);
  
  if (result == FAT16_DIR_SEARCH_FOUND) {
    if (createFile && !append) {
      // File exists but we're in write mode - truncate it
      file->currentCluster = *((uint16_t*) &entry[FAT16_DIR_FIRST_CLUSTER_LOW]);
      file->firstCluster = file->currentCluster;
      file->fileSize = 0;
      file->currentPosition = 0;
    } else {
      // Opening existing file for read
      file->currentCluster = *((uint16_t*) &entry[FAT16_DIR_FIRST_CLUSTER_LOW]);
      file->fileSize = *((uint32_t*) &entry[FAT16_DIR_FILE_SIZE]);
      file->firstCluster = file->currentCluster;
      file->currentPosition = append ? file->fileSize : 0;
    }
    file->pathname = strdup(pathname);
    fs->numOpenFiles++;
  } else if (createFile && 
      (result == FAT16_DIR_SEARCH_DELETED || 
       result == FAT16_DIR_SEARCH_NOT_FOUND)) {
    // Create new file using the entry location we found
    fat16FormatFilename(pathname, upperName);
    memcpy(entry + FAT16_DIR_FILENAME, upperName, FAT16_FULL_NAME_LENGTH);
    entry[FAT16_DIR_ATTRIBUTES] = FAT16_ATTR_NORMAL_FILE;
    memset(entry + FAT16_DIR_ATTRIBUTES + 1, FAT16_EMPTY_ENTRY,
      FAT16_BYTES_PER_DIRECTORY_ENTRY - (FAT16_DIR_ATTRIBUTES + 1));
    
    if (fat16WriteBlock(fs, block, buffer)) {
      free(file); file = NULL;
      printDebug("ERROR: Writing name of new file failed!\n");
      goto exit;
    }
    
    file->currentCluster = FAT16_EMPTY_ENTRY;
    file->fileSize = 0;
    file->firstCluster = FAT16_EMPTY_ENTRY;
    file->currentPosition = 0;
    file->pathname = strdup(pathname);
    fs->numOpenFiles++;
  } else {
    free(file); file = NULL;
  }
  
exit:
  if (fs->numOpenFiles == 0) {
    free(fs->blockBuffer); fs->blockBuffer = NULL;
  }
  return file;
}

/// @fn int fat16Read(FilesystemState *fs, Fat16File *file, void *buffer,
///   uint32_t length)
///
/// @brief Read an opened FAT file into a provided buffer up to the a specified
/// number of bytes.
///
/// @param fs Pointer to the filesystem state structure containing the block
///   device and buffer information maintained by the filesystem process.
/// @param file A pointer to a previously-opened and initialized Fat16File
///   object.
/// @param buffer A pointer to the memory to read the file contents into.
/// @param length The maximum number of bytes to read from the file.
///
/// @return Returns the number of bytes read from the file.
int fat16Read(FilesystemState *fs, Fat16File *file, void *buffer,
    uint32_t length
) {
  if (!length || file->currentPosition >= file->fileSize) {
    return 0;
  }
  
  uint32_t bytesRead = 0;
  while (bytesRead < length) {
    // Calculate the block to read based on current cluster
    // Subtract FAT16_MIN_DATA_CLUSTER since data clusters start at 2
    uint32_t block = file->dataStart + ((file->currentCluster -
      FAT16_MIN_DATA_CLUSTER) * file->sectorsPerCluster);
      
    if (fat16ReadBlock(fs, block, fs->blockBuffer)) {
      break;
    }
    
    // Don't read beyond the requested length or file size
    uint16_t toCopy = min(file->bytesPerSector, length - bytesRead);
    memcpy((uint8_t*) buffer + bytesRead, fs->blockBuffer, toCopy);
    bytesRead += toCopy;
    file->currentPosition += toCopy;
    
    // Get next cluster if we've read a full cluster
    if ((file->currentPosition
        % (file->bytesPerSector * file->sectorsPerCluster)) == 0
    ) {
      // Calculate FAT sector containing our current cluster entry
      uint32_t fatBlock = fs->startLba + file->reservedSectors +
        ((file->currentCluster * sizeof(uint16_t)) / file->bytesPerSector);
      
      if (fat16ReadBlock(fs, fatBlock, fs->blockBuffer)) {
        break;
      }
      
      // Check if we've reached the end of the cluster chain
      uint16_t nextCluster = *((uint16_t*) &fs->blockBuffer
        [(file->currentCluster * sizeof(uint16_t)) % file->bytesPerSector]);
      if (nextCluster >= FAT16_CLUSTER_CHAIN_END) {
        break;
      }
      file->currentCluster = nextCluster;
    }
  }
  
  return bytesRead;
}

/// @fn int fat16Write(FilesystemState *fs, Fat16File *file,
///   const uint8_t *buffer, uint32_t length)
///
/// @brief Write data to a FAT16 file
///
/// @param fs Pointer to the filesystem state structure
/// @param file Pointer to the FAT16 file structure containing file state
/// @param buffer Pointer to the data to write
/// @param length Number of bytes to write from the buffer
///
/// @return Number of bytes written on success, -1 on failure
int fat16Write(FilesystemState *fs, Fat16File *file, const uint8_t *buffer,
    uint32_t length) {
  uint32_t bytesWritten = 0;
  while (bytesWritten < length) {
    // Allocate new cluster if needed
    if (!file->currentCluster || !(file->currentPosition %
        (file->bytesPerSector * file->sectorsPerCluster))
    ) {
      // Find free cluster
      if (fat16ReadBlock(fs, file->fatStart, fs->blockBuffer)) {
        return -1;
      }
      
      uint16_t *fat = (uint16_t*) fs->blockBuffer;
      uint16_t newCluster = 0;
      for (uint16_t ii = FAT16_MIN_DATA_CLUSTER;
          ii < FAT16_MAX_CLUSTER_NUMBER; ii++) {
        if (fat[ii] == FAT16_EMPTY_ENTRY) {
          newCluster = ii;
          break;
        }
      }
      
      if (!newCluster) {
        return -1; // No free clusters found
      }

      // Update FAT entries
      if (file->currentCluster) {
        fat[file->currentCluster] = newCluster;
      }
      fat[newCluster] = FAT16_CLUSTER_CHAIN_END;
      
      // Write FAT copies
      for (uint8_t ii = 0; ii < file->numberOfFats; ii++) {
        if (fat16WriteBlock(fs, file->fatStart +
            (ii * file->sectorsPerFat), fs->blockBuffer)) {
          return -1;
        }
      }
      
      if (!file->firstCluster) {
        file->firstCluster = newCluster;
      }
      file->currentCluster = newCluster;
    }

    // Calculate current sector and offset
    uint32_t block = file->dataStart + ((file->currentCluster -
      FAT16_MIN_DATA_CLUSTER) * file->sectorsPerCluster);
    uint32_t sectorOffset = file->currentPosition % file->bytesPerSector;
    
    // If not writing a full sector, read existing data first
    if ((sectorOffset != 0)
      || (length - bytesWritten < file->bytesPerSector)
    ) {
      if (fat16ReadBlock(fs, block, fs->blockBuffer)) {
        return -1;
      }
    }
    
    // Calculate how many bytes to write in this sector
    uint32_t bytesThisWrite = file->bytesPerSector - sectorOffset;
    if (bytesThisWrite > (length - bytesWritten)) {
      bytesThisWrite = length - bytesWritten;
    }
    
    // Copy data to buffer at correct offset
    memcpy(fs->blockBuffer + sectorOffset, buffer + bytesWritten,
      bytesThisWrite);
    
    if (fat16WriteBlock(fs, block, fs->blockBuffer)) {
      return -1;
    }

    bytesWritten += bytesThisWrite;
    file->currentPosition += bytesThisWrite;
    if (file->currentPosition > file->fileSize) {
      file->fileSize = file->currentPosition;
    }
  }

  // Update directory entry with new size and first cluster
  uint8_t *entry;
  uint32_t block;
  int result = fat16FindDirectoryEntry(fs, file, file->pathname, &entry,
    &block, NULL);
    
  if (result != FAT16_DIR_SEARCH_FOUND) {
    return -1;
  }

  *((uint32_t*) &entry[FAT16_DIR_FILE_SIZE]) = file->fileSize;
  if (!*((uint16_t*) &entry[FAT16_DIR_FIRST_CLUSTER_LOW])) {
    *((uint16_t*) &entry[FAT16_DIR_FIRST_CLUSTER_LOW]) = file->firstCluster;
  }
  
  if (fat16WriteBlock(fs, block, fs->blockBuffer)) {
    return -1;
  }

  return bytesWritten;
}

/// @fn int fat16Remove(FilesystemState *fs, const char *pathname)
///
/// @brief Remove (delete) a file from the FAT16 filesystem
///
/// @param fs Pointer to the filesystem state structure
/// @param pathname The name of the file to remove
///
/// @return 0 on success, -1 on failure
int fat16Remove(FilesystemState *fs, const char *pathname) {
  // We need a file handle to access the cached values
  Fat16File *file = fat16Fopen(fs, pathname, "r");
  if (!file) {
    return -1;
  }
  
  uint8_t *entry;
  uint32_t block;
  int result = fat16FindDirectoryEntry(fs, file, pathname, &entry, &block,
    NULL);
    
  if (result != FAT16_DIR_SEARCH_FOUND) {
    free(file);
    return -1;
  }
  
  // Mark the directory entry as deleted
  entry[FAT16_DIR_FILENAME] = FAT16_DELETED_MARKER;
  result = fat16WriteBlock(fs, block, fs->blockBuffer);
  
  free(file);
  return result;
}

/// @fn int fat16Seek(FilesystemState *fs, Fat16File *file, int32_t offset,
///   uint8_t whence)
///
/// @brief Move the current position of the file to the specified position.
///
/// @param fs Pointer to the filesystem state structure maintained by the
///   filesystem process.
/// @param file A pointer to a previously-opened and initialized Fat16File
///   object.
/// @param offset A signed integer value that will be added to the specified
///   position.
/// @param whence The location within the file to apply the offset to.  Valid
///   values are SEEK_SET (the beginning of the file), SEEK_CUR (the current
///   file positon), and SEEK_END (the end of the file).
///
/// @return Returns 0 on success, -1 on failure.
int fat16Seek(FilesystemState *fs, Fat16File *file, int32_t offset,
    uint8_t whence
) {
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
    uint32_t fatBlock = file->fatStart + ((file->currentCluster *
      sizeof(uint16_t)) / file->bytesPerSector);
    if (fat16ReadBlock(fs, fatBlock, fs->blockBuffer)) {
      return -1;
    }
    
    uint16_t nextCluster = *((uint16_t*) &fs->blockBuffer
      [(file->currentCluster * sizeof(uint16_t)) % file->bytesPerSector]);
    if (nextCluster >= FAT16_CLUSTER_CHAIN_END) {
      return -1;
    }
    
    file->currentCluster = nextCluster;
    file->currentPosition += file->bytesPerCluster;
  }
  
  // Final position adjustment
  file->currentPosition = newPosition;
  return 0;
}

/// @fn int getPartitionInfo(FilesystemState *fs)
///
/// @brief Get information about the partition for the provided filesystem.
///
/// @param fs Pointer to the filesystem state structure maintained by the
///   filesystem process.
///
/// @return Returns 0 on success, negative error code on failure.
int getPartitionInfo(FilesystemState *fs) {
  if (fs->blockDevice->partitionNumber == 0) {
    return -1;
  }

  if (fs->blockDevice->readBlocks(fs->blockDevice->context, 0, 1,
      fs->blockSize, fs->blockBuffer) != 0) {
    return -2;
  }

  uint8_t *partitionTable = fs->blockBuffer + FAT16_PARTITION_TABLE_OFFSET;
  uint8_t *entry = partitionTable +
    ((fs->blockDevice->partitionNumber - 1) * FAT16_PARTITION_ENTRY_SIZE);
  uint8_t type = entry[4];
  
  if ((type == FAT16_PARTITION_TYPE_FAT16_LBA) || 
      (type == FAT16_PARTITION_TYPE_FAT16_LBA_EXTENDED)) {
    fs->startLba = (((uint32_t) entry[FAT16_PARTITION_LBA_OFFSET + 3]) << 24) |
      (((uint32_t) entry[FAT16_PARTITION_LBA_OFFSET + 2]) << 16) |
      (((uint32_t) entry[FAT16_PARTITION_LBA_OFFSET + 1]) << 8) |
      ((uint32_t) entry[FAT16_PARTITION_LBA_OFFSET]);
      
    uint32_t numSectors = 
      (((uint32_t) entry[FAT16_PARTITION_SECTORS_OFFSET + 3]) << 24) |
      (((uint32_t) entry[FAT16_PARTITION_SECTORS_OFFSET + 2]) << 16) |
      (((uint32_t) entry[FAT16_PARTITION_SECTORS_OFFSET + 1]) << 8) |
      ((uint32_t) entry[FAT16_PARTITION_SECTORS_OFFSET]);
      
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
  if (filesystemState->numOpenFiles > 0) {
    filesystemState->numOpenFiles--;
    if (filesystemState->numOpenFiles == 0) {
      free(filesystemState->blockBuffer); filesystemState->blockBuffer = NULL;
    }
  }

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


/// @fn static void handleFilesystemMessages(FilesystemState *fs)
///
/// @brief Pop and handle all messages in the filesystem process's message
/// queue until there are no more.
///
/// @param fs A pointer to the FilesystemState object maintained by the
///   filesystem process.
///
/// @return This function returns no value.
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

/// @fn void* runFat16Filesystem(void *args)
///
/// @brief Main process entry point for the FAT16 filesystem process.
///
/// @param args A pointer to an initialized BlockStorageDevice structure cast
///   to a void*.
///
/// @return This function never returns, but would return NULL if it did.
void* runFat16Filesystem(void *args) {
  FilesystemState fs;
  memset(&fs, 0, sizeof(fs));
  fs.blockDevice = (BlockStorageDevice*) args;
  fs.blockSize = 512;
  coroutineYield(NULL);
  
  fs.blockBuffer = (uint8_t*) malloc(fs.blockSize);
  getPartitionInfo(&fs);
  free(fs.blockBuffer); fs.blockBuffer = NULL;
  
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

/// @fn FILE* filesystemFOpen(const char *pathname, const char *mode)
///
/// @brief Implementation of the standard C fopen call.
///
/// @param pathname The full pathname to the file.  NOTE:  This implementation
///   can only open files in the root directory.  Subdirectories are NOT
///   supported.
/// @param mode The standard C file mode to open the file as.
///
/// @return Returns a pointer to an initialized FILE object on success, NULL on
/// failure.
FILE* filesystemFOpen(const char *pathname, const char *mode) {
  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_OPEN_FILE,
    (intptr_t) mode, (intptr_t) pathname, true);
  processMessageWaitForDone(msg, NULL);
  FILE *file = nanoOsMessageDataPointer(msg, FILE*);
  processMessageRelease(msg);
  return file;
}

/// @fn int filesystemFClose(FILE *stream)
///
/// @brief Implementation of the standard C fclose call.
///
/// @param stream A pointer to a previously-opened FILE object.
///
/// @return This function always succeeds and always returns 0.
int filesystemFClose(FILE *stream) {
  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_CLOSE_FILE,
    0, (intptr_t) stream, true);
  processMessageWaitForDone(msg, NULL);
  processMessageRelease(msg);
  return 0;
}

/// @fn int filesystemRemove(const char *pathname)
///
/// @brief Implementation of the standard C remove call.
///
/// @param pathname The full pathname to the file.  NOTE:  This implementation
///   can only open files in the root directory.  Subdirectories are NOT
///   supported.
///
/// @return Returns 0 on success, -1 on failure.
int filesystemRemove(const char *pathname) {
  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_REMOVE_FILE,
    /* func= */ 0, (intptr_t) pathname, true);
  processMessageWaitForDone(msg, NULL);
  int returnValue = nanoOsMessageDataValue(msg, int);
  processMessageRelease(msg);
  return returnValue;
}

/// @fn int filesystemFSeek(FILE *stream, long offset, int whence)
///
/// @brief Implementation of the standard C fseek call.
///
/// @param stream A pointer to a previously-opened FILE object.
/// @param offset A signed integer value that will be added to the specified
///   position.
/// @param whence The location within the file to apply the offset to.  Valid
///   values are SEEK_SET (the beginning of the file), SEEK_CUR (the current
///   file positon), and SEEK_END (the end of the file).
///
/// @return Returns 0 on success, -1 on failure.
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

/// @fn size_t filesystemFRead(
///   void *ptr, size_t size, size_t nmemb, FILE *stream)
///
/// @brief Read data from a previously-opened file.
///
/// @param ptr A pointer to the memory to read data into.
/// @param size The size, in bytes, of each element that is to be read from the
///   file.
/// @param nmemb The number of elements that are to be read from the file.
/// @param stream A pointer to the previously-opened file.
///
/// @return Returns the total number of objects successfully read from the
/// file.
size_t filesystemFRead(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  size_t returnValue = 0;

  FilesystemIoCommandParameters filesystemIoCommandParameters = {
    .file = stream,
    .buffer = ptr,
    .length = (uint32_t) (size * nmemb)
  };
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID,
    FILESYSTEM_READ_FILE,
    /* func= */ 0,
    /* data= */ (intptr_t) &filesystemIoCommandParameters,
    true);
  processMessageWaitForDone(processMessage, NULL);
  returnValue = (filesystemIoCommandParameters.length / size);
  processMessageRelease(processMessage);

  return returnValue;
}

/// @fn size_t filesystemFWrite(
///   void *ptr, size_t size, size_t nmemb, FILE *stream)
///
/// @brief Write data to a previously-opened file.
///
/// @param ptr A pointer to the memory to write data from.
/// @param size The size, in bytes, of each element that is to be written to
///   the file.
/// @param nmemb The number of elements that are to be written to the file.
/// @param stream A pointer to the previously-opened file.
///
/// @return Returns the total number of objects successfully written to the
/// file.
size_t filesystemFWrite(
  void *ptr, size_t size, size_t nmemb, FILE *stream
) {
  size_t returnValue = 0;

  FilesystemIoCommandParameters filesystemIoCommandParameters = {
    .file = stream,
    .buffer = (void*) ptr,
    .length = (uint32_t) (size * nmemb)
  };
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID,
    FILESYSTEM_WRITE_FILE,
    /* func= */ 0,
    /* data= */ (intptr_t) &filesystemIoCommandParameters,
    true);
  processMessageWaitForDone(processMessage, NULL);
  returnValue = (filesystemIoCommandParameters.length / size);
  processMessageRelease(processMessage);

  return returnValue;
}

/// @fn long filesystemFTell(FILE *stream)
///
/// @brief Get the current value of the position indicator of a
/// previously-opened file.
///
/// @param stream A pointer to a previously-opened file.
///
/// @return Returns the current position of the file on success, -1 on failure.
long filesystemFTell(FILE *stream) {
  if (stream == NULL) {
    return 0;
  }

  return (long) ((Fat16File*) stream->file)->fileSize;
}

