////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2025 James Card                     //
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

// Arduino includes
#include <Arduino.h>
#include <HardwareSerial.h>

#include "Fat16Filesystem.h"

// Directory search result codes
#define FAT16_DIR_SEARCH_ERROR -1
#define FAT16_DIR_SEARCH_FOUND 0
#define FAT16_DIR_SEARCH_DELETED 1
#define FAT16_DIR_SEARCH_NOT_FOUND 2

// General FAT16 specification values
#define FAT16_BYTES_PER_DIRECTORY_ENTRY 32
#define FAT16_ENTRIES_PER_CLUSTER 16
#define FAT16_CLUSTER_CHAIN_END 0xFFF8
#define FAT16_FILENAME_LENGTH 8
#define FAT16_EXTENSION_LENGTH 3
#define FAT16_FULL_NAME_LENGTH (FAT16_FILENAME_LENGTH + FAT16_EXTENSION_LENGTH)
#define FAT16_DIR_ENTRIES_PER_SECTOR_SHIFT 5

// File system limits and special values
#define FAT16_MAX_CLUSTER_NUMBER 0xFF0
#define FAT16_DELETED_MARKER 0xE5
#define FAT16_EMPTY_ENTRY 0x00
#define FAT16_MIN_DATA_CLUSTER 2

// File attributes
#define FAT16_ATTR_READ_ONLY 0x01
#define FAT16_ATTR_HIDDEN 0x02
#define FAT16_ATTR_SYSTEM 0x04
#define FAT16_ATTR_VOLUME_ID 0x08
#define FAT16_ATTR_DIRECTORY 0x10
#define FAT16_ATTR_ARCHIVE 0x20
#define FAT16_ATTR_NORMAL_FILE FAT16_ATTR_ARCHIVE

// Boot sector offsets
#define FAT16_BOOT_BYTES_PER_SECTOR 0x0B
#define FAT16_BOOT_SECTORS_PER_CLUSTER 0x0D
#define FAT16_BOOT_RESERVED_SECTORS 0x0E
#define FAT16_BOOT_NUMBER_OF_FATS 0x10
#define FAT16_BOOT_ROOT_ENTRIES 0x11
#define FAT16_BOOT_SECTORS_PER_FAT 0x16

// Directory entry offsets
#define FAT16_DIR_FILENAME 0x00
#define FAT16_DIR_ATTRIBUTES 0x0B
#define FAT16_DIR_FIRST_CLUSTER_LOW 0x1A
#define FAT16_DIR_FILE_SIZE 0x1C

typedef struct __attribute__((packed)) Fat16File {
  uint16_t  currentCluster;
  uint32_t  currentPosition;
  uint32_t  fileSize;
  uint16_t  firstCluster;
  // Directory entry location info:
  uint32_t  directoryBlock;     // Block containing the directory entry
  uint16_t  directoryOffset;    // Offset within block to directory entry
  // Common values:
  uint16_t  bytesPerSector;
  uint8_t   sectorsPerCluster;
  uint16_t  reservedSectors;
  uint8_t   numberOfFats;
  uint16_t  rootEntries;
  uint16_t  sectorsPerFat;
  uint32_t  bytesPerCluster;
  uint32_t  fatStart;
  uint32_t  rootStart;
  uint32_t  dataStart;
} Fat16File;

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

/// @fn int fat16FindDirectoryEntry(FilesystemState *fs, Fat16File *file,
///   const char *pathname, uint8_t **entry, uint32_t *block,
///   uint16_t *entryIndex)
///
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

/// @fn Fat16File* fat16Fopen(FilesystemState *fs, const char *pathname,
///   const char *mode)
///
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
  uint16_t entryIndex = 0;
  Fat16File *file = NULL;
  int result = 0;
  char upperName[FAT16_FULL_NAME_LENGTH + 1];
  
  if (fs->numOpenFiles == 0) {
    fs->blockBuffer = (uint8_t*) malloc(
      fs->blockSize);
    if (fs->blockBuffer == NULL) {
      goto exit;
    }
  }
  buffer = fs->blockBuffer;

  // Read boot sector
  if (fat16ReadBlock(fs,
      fs->startLba, buffer)
  ) {
    goto exit;
  }
  
  // Create file structure to hold common values
  file = (Fat16File*) malloc(sizeof(Fat16File));
  if (!file) {
    goto exit;
  }
  
  // Store common boot sector values using readBytes for alignment safety
  readBytes(&file->bytesPerSector, &buffer[FAT16_BOOT_BYTES_PER_SECTOR]);
  file->sectorsPerCluster = buffer[FAT16_BOOT_SECTORS_PER_CLUSTER];
  readBytes(&file->reservedSectors, &buffer[FAT16_BOOT_RESERVED_SECTORS]);
  file->numberOfFats = buffer[FAT16_BOOT_NUMBER_OF_FATS];
  readBytes(&file->rootEntries, &buffer[FAT16_BOOT_ROOT_ENTRIES]);
  readBytes(&file->sectorsPerFat, &buffer[FAT16_BOOT_SECTORS_PER_FAT]);
  file->bytesPerCluster = (uint32_t) file->bytesPerSector *
    (uint32_t) file->sectorsPerCluster;
  file->fatStart = ((uint32_t) fs->startLba) +
    ((uint32_t) file->reservedSectors);
  file->rootStart = ((uint32_t) file->fatStart) +
    (((uint32_t) file->numberOfFats) * ((uint32_t) file->sectorsPerFat));
  file->dataStart = ((uint32_t) file->rootStart) +
    (((((uint32_t) file->rootEntries) * FAT16_BYTES_PER_DIRECTORY_ENTRY) +
    ((uint32_t) file->bytesPerSector) - 1) /
    ((uint32_t) file->bytesPerSector));
  
  result = fat16FindDirectoryEntry(fs, file, pathname, &entry,
    &block, &entryIndex);
  
  if (result == FAT16_DIR_SEARCH_FOUND) {
    uint16_t firstCluster;
    uint32_t fileSize;
    readBytes(&firstCluster, &entry[FAT16_DIR_FIRST_CLUSTER_LOW]);
    readBytes(&fileSize, &entry[FAT16_DIR_FILE_SIZE]);
    
    if (createFile && !append) {
      // File exists but we're in write mode - truncate it
      file->currentCluster = firstCluster;
      file->firstCluster = file->currentCluster;
      file->fileSize = 0;
      file->currentPosition = 0;
    } else {
      // Opening existing file for read
      file->currentCluster = firstCluster;
      file->fileSize = fileSize;
      file->firstCluster = file->currentCluster;
      file->currentPosition = append ? file->fileSize : 0;
    }
    // Store directory entry location
    file->directoryBlock = block;
    file->directoryOffset = (entryIndex % (file->bytesPerSector >>
      FAT16_DIR_ENTRIES_PER_SECTOR_SHIFT)) * FAT16_BYTES_PER_DIRECTORY_ENTRY;
    fs->numOpenFiles++;
  } else if (createFile && 
      (result == FAT16_DIR_SEARCH_DELETED || 
       result == FAT16_DIR_SEARCH_NOT_FOUND)
  ) {
    // Create new file using the entry location we found
    fat16FormatFilename(pathname, upperName);
    memcpy(entry + FAT16_DIR_FILENAME, upperName, FAT16_FULL_NAME_LENGTH);
    entry[FAT16_DIR_ATTRIBUTES] = FAT16_ATTR_NORMAL_FILE;
    memset(entry + FAT16_DIR_ATTRIBUTES + 1, FAT16_EMPTY_ENTRY,
      FAT16_BYTES_PER_DIRECTORY_ENTRY - (FAT16_DIR_ATTRIBUTES + 1));
    
    if (fat16WriteBlock(fs, block, buffer)) {
      free(file);
      file = NULL;
      goto exit;
    }
    
    file->currentCluster = FAT16_EMPTY_ENTRY;
    file->fileSize = 0;
    file->firstCluster = FAT16_EMPTY_ENTRY;
    file->currentPosition = 0;
    // Store directory entry location
    file->directoryBlock = block;
    file->directoryOffset = (entryIndex % (file->bytesPerSector >>
      FAT16_DIR_ENTRIES_PER_SECTOR_SHIFT)) * FAT16_BYTES_PER_DIRECTORY_ENTRY;
    fs->numOpenFiles++;
  } else {
    free(file);
    file = NULL;
  }
  
exit:
  if (fs->numOpenFiles == 0) {
    free(fs->blockBuffer);
    fs->blockBuffer = NULL;
  }
  return file;
}

/// @fn int fat16GetNextCluster(FilesystemState *fs, Fat16File *file, 
///   uint16_t *nextCluster)
///
/// @brief Get the next cluster in the FAT chain for a given file
///
/// @param fs Pointer to the filesystem state structure containing the block
///   device and buffer information
/// @param file A pointer to the FAT16 file structure
/// @param nextCluster Pointer to store the next cluster number
///
/// @return Returns 0 on success, -1 on error
static int fat16GetNextCluster(FilesystemState *fs, Fat16File *file,
    uint16_t *nextCluster
) {
  uint32_t fatBlock = file->fatStart + 
    ((file->currentCluster * sizeof(uint16_t)) / file->bytesPerSector);

  if (fat16ReadBlock(fs, fatBlock, fs->blockBuffer)) {
    return -1;
  }

  readBytes(nextCluster, &fs->blockBuffer
    [(file->currentCluster * sizeof(uint16_t)) % file->bytesPerSector]);
  return 0;
}

/// @fn int fat16HandleClusterTransition(FilesystemState *fs, Fat16File *file,
///   bool allocateIfNeeded)
///
/// @brief Handle transitioning to the next cluster, optionally allocating a new
/// one if needed
///
/// @param fs Pointer to the filesystem state structure containing the block
///   device and buffer information
/// @param file A pointer to the FAT16 file structure
/// @param allocateIfNeeded Whether to allocate a new cluster if at chain end
///
/// @return Returns 0 on success, -1 on error
static int fat16HandleClusterTransition(FilesystemState *fs,
  Fat16File *file, bool allocateIfNeeded
) {
  if ((file->currentPosition % (file->bytesPerSector * 
      file->sectorsPerCluster)) != 0) {
    return 0;
  }

  uint16_t nextCluster;
  if (fat16GetNextCluster(fs, file, &nextCluster)) {
    return -1;
  }

  if (nextCluster >= FAT16_CLUSTER_CHAIN_END) {
    if (!allocateIfNeeded) {
      return -1;
    }

    // Find free cluster
    if (fat16ReadBlock(fs, file->fatStart, fs->blockBuffer)) {
      return -1;
    }
    
    uint16_t fatEntry;
    nextCluster = 0;
    for (uint16_t ii = FAT16_MIN_DATA_CLUSTER;
        ii < FAT16_MAX_CLUSTER_NUMBER; ii++) {
      readBytes(&fatEntry, &fs->blockBuffer[ii * sizeof(uint16_t)]);
      if (fatEntry == FAT16_EMPTY_ENTRY) {
        nextCluster = ii;
        break;
      }
    }
    
    if (!nextCluster) {
      return -1;
    }

    // Update FAT entries using writeBytes for alignment safety
    writeBytes(&fs->blockBuffer[file->currentCluster * sizeof(uint16_t)],
      &nextCluster);
    uint16_t endMarker = FAT16_CLUSTER_CHAIN_END;
    writeBytes(&fs->blockBuffer[nextCluster * sizeof(uint16_t)], &endMarker);
    
    // Write FAT copies
    for (uint8_t ii = 0; ii < file->numberOfFats; ii++) {
      if (fat16WriteBlock(fs, file->fatStart +
          (ii * file->sectorsPerFat), fs->blockBuffer)) {
        return -1;
      }
    }
  }

  file->currentCluster = nextCluster;
  return 0;
}

/// @fn int fat16UpdateDirectoryEntry(FilesystemState *fs,
///   Fat16File *file)
///
/// @brief Update the directory entry for a file using stored location
///
/// @param fs Pointer to the filesystem state structure containing the block
///   device and buffer information
/// @param file A pointer to the FAT16 file structure
///
/// @return Returns 0 on success, -1 on error
static int fat16UpdateDirectoryEntry(FilesystemState *fs,
    Fat16File *file
) {
  if (fat16ReadBlock(fs, file->directoryBlock, fs->blockBuffer)) {
    return -1;
  }

  uint8_t *entry = fs->blockBuffer + file->directoryOffset;

  writeBytes(&entry[FAT16_DIR_FILE_SIZE], &file->fileSize);
  uint16_t currentFirstCluster;
  readBytes(&currentFirstCluster, &entry[FAT16_DIR_FIRST_CLUSTER_LOW]);
  if (currentFirstCluster == 0) {
    writeBytes(&entry[FAT16_DIR_FIRST_CLUSTER_LOW], &file->firstCluster);
  }
  
  return fat16WriteBlock(fs, file->directoryBlock, fs->blockBuffer);
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
  uint32_t startByte
    = file->currentPosition & (((uint32_t) file->bytesPerSector) - 1);
  while (bytesRead < length) {
    uint32_t sectorInCluster = (((uint32_t) file->currentPosition) / 
      ((uint32_t) file->bytesPerSector))
      % ((uint32_t) file->sectorsPerCluster);
    uint32_t block = ((uint32_t) file->dataStart) + 
      ((((uint32_t) file->currentCluster) - FAT16_MIN_DATA_CLUSTER) * 
      ((uint32_t) file->sectorsPerCluster)) + ((uint32_t) sectorInCluster);
      
    if (fat16ReadBlock(fs, block, fs->blockBuffer)) {
      break;
    }
    
    uint16_t toCopy = min(file->bytesPerSector - startByte, length - bytesRead);
    memcpy((uint8_t*) buffer + bytesRead, &fs->blockBuffer[startByte], toCopy);
    bytesRead += toCopy;
    file->currentPosition += toCopy;
    
    if (fat16HandleClusterTransition(fs, file, false)) {
      break;
    }
    startByte = 0;
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
    uint32_t length
) {
  uint32_t bytesWritten = 0;
  
  while (bytesWritten < length) {
    if (!file->currentCluster) {
      if (fat16HandleClusterTransition(fs, file, true)) {
        return -1;
      }
      if (!file->firstCluster) {
        file->firstCluster = file->currentCluster;
      }
    }

    uint32_t sectorInCluster = (((uint32_t) file->currentPosition) / 
      ((uint32_t) file->bytesPerSector))
      % ((uint32_t) file->sectorsPerCluster);
    uint32_t block = ((uint32_t) file->dataStart) + 
      ((((uint32_t) file->currentCluster) - FAT16_MIN_DATA_CLUSTER) * 
      ((uint32_t) file->sectorsPerCluster)) + ((uint32_t) sectorInCluster);
    uint32_t sectorOffset = file->currentPosition % file->bytesPerSector;
    
    if ((sectorOffset != 0) || 
        (length - bytesWritten < file->bytesPerSector)) {
      if (fat16ReadBlock(fs, block, fs->blockBuffer)) {
        return -1;
      }
    }
    
    uint32_t bytesToWrite = file->bytesPerSector - sectorOffset;
    if (bytesToWrite > (length - bytesWritten)) {
      bytesToWrite = length - bytesWritten;
    }
    
    memcpy(fs->blockBuffer + sectorOffset, buffer + bytesWritten,
      bytesToWrite);
    
    if (fat16WriteBlock(fs, block, fs->blockBuffer)) {
      return -1;
    }

    bytesWritten += bytesToWrite;
    file->currentPosition += bytesToWrite;
    if (file->currentPosition > file->fileSize) {
      file->fileSize = file->currentPosition;
    }

    if (fat16HandleClusterTransition(fs, file, true)) {
      return -1;
    }
  }

  return fat16UpdateDirectoryEntry(fs, file) == 0 ? bytesWritten : -1;
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
  int result = fat16FindDirectoryEntry(
    fs, file, pathname, &entry, &block, NULL);
    
  if (result != FAT16_DIR_SEARCH_FOUND) {
    free(file);
    return -1;
  }
  
  // Get the first cluster of the file's chain
  uint16_t currentCluster;
  readBytes(&currentCluster, &entry[FAT16_DIR_FIRST_CLUSTER_LOW]);

  // Mark the directory entry as deleted
  entry[FAT16_DIR_FILENAME] = FAT16_DELETED_MARKER;
  result = fat16WriteBlock(fs, block, fs->blockBuffer);
  if (result != 0) {
    free(file);
    return -1;
  }

  // Follow cluster chain to mark all clusters as free
  while (currentCluster != 0 && currentCluster < FAT16_CLUSTER_CHAIN_END) {
    // Calculate FAT sector containing current cluster entry
    uint32_t fatBlock = file->fatStart + 
      ((currentCluster * sizeof(uint16_t)) / file->bytesPerSector);

    // Read the FAT block
    if (fat16ReadBlock(fs, fatBlock, fs->blockBuffer)) {
      free(file);
      return -1;
    }

    // Get next cluster in chain before marking current as free
    uint16_t nextCluster;
    readBytes(&nextCluster, &fs->blockBuffer
      [(currentCluster * sizeof(uint16_t)) % file->bytesPerSector]);

    // Mark current cluster as free (0x0000)
    uint16_t freeMarker = 0;
    writeBytes(&fs->blockBuffer
      [(currentCluster * sizeof(uint16_t)) % file->bytesPerSector],
      &freeMarker);

    // Write updated FAT block
    for (uint8_t ii = 0; ii < file->numberOfFats; ii++) {
      if (fat16WriteBlock(fs, fatBlock + (ii * file->sectorsPerFat),
          fs->blockBuffer)) {
        free(file);
        return -1;
      }
    }

    currentCluster = nextCluster;
  }
  
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
  
  // If no movement needed, return early
  if (newPosition == file->currentPosition) {
    return 0;
  }
  
  // Calculate cluster positions
  uint32_t currentClusterIndex = file->currentPosition / file->bytesPerCluster;
  uint32_t targetClusterIndex = newPosition / file->bytesPerCluster;
  uint32_t clustersToTraverse;
  
  if (targetClusterIndex >= currentClusterIndex) {
    // Seeking forward or to the same cluster
    clustersToTraverse = targetClusterIndex - currentClusterIndex;
  } else {
    // Reset to start if seeking backwards
    file->currentPosition = 0;
    file->currentCluster = file->firstCluster;
    clustersToTraverse = targetClusterIndex;
  }
  
  // Fast path: no cluster traversal needed
  if (clustersToTraverse == 0) {
    file->currentPosition = newPosition;
    return 0;
  }
  
  // Read multiple FAT entries at once
  uint32_t currentFatBlock = (uint32_t) -1;
  
  while (clustersToTraverse > 0) {
    uint32_t fatBlock = file->fatStart + 
      ((file->currentCluster * sizeof(uint16_t)) / file->bytesPerSector);
    
    // Only read FAT block if different from current
    if (fatBlock != currentFatBlock) {
      if (fat16ReadBlock(fs, fatBlock, fs->blockBuffer)) {
        return -1;
      }
      currentFatBlock = fatBlock;
    }
    
    uint16_t nextCluster;
    readBytes(&nextCluster, &fs->blockBuffer[(file->currentCluster * 
      sizeof(uint16_t)) % file->bytesPerSector]);
      
    if (nextCluster >= FAT16_CLUSTER_CHAIN_END) {
      return -1;
    }
    
    file->currentCluster = nextCluster;
    file->currentPosition += file->bytesPerCluster;
    clustersToTraverse--;
  }
  
  // Final position adjustment within cluster
  file->currentPosition = newPosition;
  return 0;
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


/// @fn static void fat16HandleFilesystemMessages(FilesystemState *fs)
///
/// @brief Pop and handle all messages in the filesystem process's message
/// queue until there are no more.
///
/// @param fs A pointer to the FilesystemState object maintained by the
///   filesystem process.
///
/// @return This function returns no value.
static void fat16HandleFilesystemMessages(FilesystemState *fs) {
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
      fat16HandleFilesystemMessages(&fs);
    }
  }
  return NULL;
}

/// @fn long fat16FilesystemFTell(FILE *stream)
///
/// @brief Get the current value of the position indicator of a
/// previously-opened file.
///
/// @param stream A pointer to a previously-opened file.
///
/// @return Returns the current position of the file on success, -1 on failure.
long fat16FilesystemFTell(FILE *stream) {
  if (stream == NULL) {
    return -1;
  }

  return (long) ((Fat16File*) stream->file)->currentPosition;
}

