///////////////////////////////////////////////////////////////////////////////
///
/// @file              ExFatFilesystem.c
///
/// @brief             Memory-efficient exFAT driver implementation for NanoOs.
///
///////////////////////////////////////////////////////////////////////////////

#include "ExFatFilesystem.h"
#include "Filesystem.h"

/// @brief Read a sector from the storage device
///
/// @param driverState Pointer to driver state
/// @param sectorNumber Sector number to read
/// @param buffer Buffer to read into
///
/// @return EXFAT_SUCCESS on success, EXFAT_ERROR on failure
static int readSector(
  ExFatDriverState* driverState, uint32_t sectorNumber, uint8_t* buffer
) {
  if (driverState == NULL || driverState->filesystemState == NULL || 
      buffer == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  int result = filesystemState->blockDevice->readBlocks(
    filesystemState->blockDevice->context,
    filesystemState->startLba + sectorNumber,
    1,
    filesystemState->blockSize,
    buffer
  );

  return (result == 0) ? EXFAT_SUCCESS : EXFAT_ERROR;
}

/// @brief Write a sector to the storage device
///
/// @param driverState Pointer to driver state
/// @param sectorNumber Sector number to write
/// @param buffer Buffer to write from
///
/// @return EXFAT_SUCCESS on success, EXFAT_ERROR on failure
static int writeSector(
  ExFatDriverState* driverState, uint32_t sectorNumber, const uint8_t* buffer
) {
  if (driverState == NULL || driverState->filesystemState == NULL || 
      buffer == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  int result = filesystemState->blockDevice->writeBlocks(
    filesystemState->blockDevice->context,
    filesystemState->startLba + sectorNumber,
    1,
    filesystemState->blockSize,
    buffer
  );

  return (result == 0) ? EXFAT_SUCCESS : EXFAT_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Initialize an exFAT driver state
///
/// @param driverState Pointer to allocated and zeroed ExFatDriverState struct
/// @param filesystemState Pointer to initialized FilesystemState struct
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int exFatInitialize(
  ExFatDriverState* driverState, FilesystemState* filesystemState
) {
  if (driverState == NULL || filesystemState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  if (filesystemState->blockDevice == NULL ||
      filesystemState->blockBuffer == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  // Use the existing blockBuffer to read boot sector
  uint8_t* buffer = filesystemState->blockBuffer;

  // Read the boot sector (sector 0)
  int result = filesystemState->blockDevice->readBlocks(
    filesystemState->blockDevice->context,
    filesystemState->startLba,
    1,
    filesystemState->blockSize,
    buffer
  );

  if (result != 0) {
    return EXFAT_ERROR;
  }

  // Validate boot signature
  uint16_t bootSignature = 0;
  readBytes(&bootSignature, &buffer[510]);
  if (bootSignature != 0xAA55) {
    return EXFAT_INVALID_FILESYSTEM;
  }

  // Validate filesystem name
  const char* expectedName = "EXFAT   ";
  for (uint8_t ii = 0; ii < 8; ii++) {
    if (buffer[3 + ii] != expectedName[ii]) {
      return EXFAT_INVALID_FILESYSTEM;
    }
  }

  // Extract boot sector values
  uint8_t bytesPerSectorShift = buffer[108];
  uint8_t sectorsPerClusterShift = buffer[109];
  uint32_t fatOffset = 0;
  uint32_t clusterHeapOffset = 0;
  uint32_t clusterCount = 0;
  uint32_t rootDirectoryCluster = 0;

  readBytes(&fatOffset, &buffer[80]);
  readBytes(&clusterHeapOffset, &buffer[88]);
  readBytes(&clusterCount, &buffer[92]);
  readBytes(&rootDirectoryCluster, &buffer[96]);

  // Calculate values
  uint32_t bytesPerSector = 1 << bytesPerSectorShift;
  uint32_t sectorsPerCluster = 1 << sectorsPerClusterShift;
  uint32_t bytesPerCluster = bytesPerSector * sectorsPerCluster;

  // Validate values
  if (bytesPerSector < EXFAT_SECTOR_SIZE) {
    return EXFAT_INVALID_FILESYSTEM;
  }

  if (bytesPerCluster < EXFAT_CLUSTER_SIZE_MIN ||
      bytesPerCluster > EXFAT_CLUSTER_SIZE_MAX) {
    return EXFAT_INVALID_FILESYSTEM;
  }

  if (rootDirectoryCluster < 2) {
    return EXFAT_INVALID_FILESYSTEM;
  }

  // Initialize driver state
  driverState->filesystemState = filesystemState;
  driverState->bytesPerSector = bytesPerSector;
  driverState->sectorsPerCluster = sectorsPerCluster;
  driverState->bytesPerCluster = bytesPerCluster;
  driverState->fatStartSector = fatOffset;
  driverState->clusterHeapStartSector = clusterHeapOffset;
  driverState->rootDirectoryCluster = rootDirectoryCluster;
  driverState->clusterCount = clusterCount;
  driverState->driverStateValid = true;

  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Convert a cluster number to a sector number
///
/// @param driverState Pointer to the exFAT driver state
/// @param cluster The cluster number to convert
///
/// @return The sector number corresponding to the cluster
///////////////////////////////////////////////////////////////////////////////
static uint32_t clusterToSector(
  ExFatDriverState* driverState, uint32_t cluster
) {
  if (cluster < 2) {
    return 0;
  }
  return driverState->clusterHeapStartSector +
    ((cluster - 2) * driverState->sectorsPerCluster);
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Read a FAT entry
///
/// @param driverState Pointer to the exFAT driver state
/// @param cluster The cluster whose FAT entry to read
/// @param nextCluster Pointer to store the next cluster number
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
static int readFatEntry(
  ExFatDriverState* driverState, uint32_t cluster, uint32_t* nextCluster
) {
  if (driverState == NULL || nextCluster == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  uint32_t fatOffset = cluster * 4;
  uint32_t fatSector = driverState->fatStartSector +
    (fatOffset / driverState->bytesPerSector);
  uint32_t entryOffset = fatOffset % driverState->bytesPerSector;

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;

  int result = readSector(driverState, fatSector, buffer);
  if (result != EXFAT_SUCCESS) {
    return result;
  }

  readBytes(nextCluster, &buffer[entryOffset]);
  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Write a FAT entry
///
/// @param driverState Pointer to the exFAT driver state
/// @param cluster The cluster whose FAT entry to write
/// @param value The value to write to the FAT entry
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
static int writeFatEntry(
  ExFatDriverState* driverState, uint32_t cluster, uint32_t value
) {
  if (driverState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  uint32_t fatOffset = cluster * 4;
  uint32_t fatSector = driverState->fatStartSector +
    (fatOffset / driverState->bytesPerSector);
  uint32_t entryOffset = fatOffset % driverState->bytesPerSector;

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;

  int result = readSector(driverState, fatSector, buffer);
  if (result != EXFAT_SUCCESS) {
    return result;
  }

  writeBytes(&buffer[entryOffset], &value);

  result = writeSector(driverState, fatSector, buffer);
  return result;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Find a free cluster
///
/// @param driverState Pointer to the exFAT driver state
/// @param freeCluster Pointer to store the free cluster number
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
static int findFreeCluster(
  ExFatDriverState* driverState, uint32_t* freeCluster
) {
  if (driverState == NULL || freeCluster == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  for (uint32_t ii = 2; ii < driverState->clusterCount + 2; ii++) {
    uint32_t fatValue = 0;
    int result = readFatEntry(driverState, ii, &fatValue);
    if (result != EXFAT_SUCCESS) {
      return result;
    }

    if (fatValue == 0) {
      *freeCluster = ii;
      return EXFAT_SUCCESS;
    }
  }

  return EXFAT_DISK_FULL;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Allocate a new cluster
///
/// @param driverState Pointer to the exFAT driver state
/// @param newCluster Pointer to store the allocated cluster number
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
static int allocateCluster(
  ExFatDriverState* driverState, uint32_t* newCluster
) {
  if (driverState == NULL || newCluster == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  int result = findFreeCluster(driverState, newCluster);
  if (result != EXFAT_SUCCESS) {
    return result;
  }

  // Mark as end of chain
  result = writeFatEntry(driverState, *newCluster, 0xFFFFFFFF);
  return result;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Calculate checksum for directory entry set
///
/// @param entries Pointer to the directory entries
/// @param numEntries Number of entries in the set
///
/// @return The calculated checksum
///////////////////////////////////////////////////////////////////////////////
static uint16_t calculateEntrySetChecksum(
  const uint8_t* entries, uint8_t numEntries
) {
  uint16_t checksum = 0;
  uint32_t totalBytes = numEntries * EXFAT_DIRECTORY_ENTRY_SIZE;

  for (uint32_t ii = 0; ii < totalBytes; ii++) {
    if (ii == 2 || ii == 3) {
      // Skip checksum field itself
      continue;
    }
    checksum = ((checksum << 15) | (checksum >> 1)) + (uint16_t) entries[ii];
  }

  return checksum;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Parse a filename component and convert to UTF-16
///
/// @param name The ASCII filename component
/// @param utf16Name Buffer to store UTF-16 name
/// @param maxLength Maximum length of the UTF-16 buffer
///
/// @return The length of the UTF-16 name
///////////////////////////////////////////////////////////////////////////////
static uint8_t asciiToUtf16(
  const char* name, uint16_t* utf16Name, uint8_t maxLength
) {
  uint8_t length = 0;
  while (*name != '\0' && length < maxLength) {
    utf16Name[length++] = (uint16_t) *name;
    name++;
  }
  return length;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Calculate hash for a filename
///
/// @param utf16Name The UTF-16 filename
/// @param nameLength Length of the filename
///
/// @return The calculated hash
///////////////////////////////////////////////////////////////////////////////
static uint16_t calculateNameHash(
  const uint16_t* utf16Name, uint8_t nameLength
) {
  uint16_t hash = 0;
  for (uint8_t ii = 0; ii < nameLength; ii++) {
    uint16_t character = utf16Name[ii];
    // Simple hash function
    hash = ((hash << 15) | (hash >> 1)) + (character & 0xFF);
    hash = ((hash << 15) | (hash >> 1)) + (character >> 8);
  }
  return hash;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Compare two filenames (case-insensitive)
///
/// @param name1 First filename (UTF-16)
/// @param length1 Length of first filename
/// @param name2 Second filename (UTF-16)
/// @param length2 Length of second filename
///
/// @return 0 if equal, non-zero if not equal
///////////////////////////////////////////////////////////////////////////////
static int compareFilenames(
  const uint16_t* name1, uint8_t length1,
  const uint16_t* name2, uint8_t length2
) {
  if (length1 != length2) {
    return 1;
  }

  for (uint8_t ii = 0; ii < length1; ii++) {
    uint16_t c1 = name1[ii];
    uint16_t c2 = name2[ii];
    // Simple case conversion for ASCII
    if (c1 >= 'a' && c1 <= 'z') {
      c1 -= 32;
    }
    if (c2 >= 'a' && c2 <= 'z') {
      c2 -= 32;
    }
    if (c1 != c2) {
      return 1;
    }
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Create a new file in a directory
///
/// @param driverState Pointer to the exFAT driver state
/// @param directoryCluster First cluster of the directory
/// @param fileName Name of the file to create
/// @param fileEntry Pointer to store created file directory entry
/// @param streamEntry Pointer to store created stream extension entry
/// @param dirCluster Pointer to store directory cluster where file was created
/// @param dirOffset Pointer to store offset in directory where file was
///                  created
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
static int createFileEntry(
  ExFatDriverState* driverState, uint32_t directoryCluster,
  const char* fileName, ExFatFileDirectoryEntry* fileEntry,
  ExFatStreamExtensionEntry* streamEntry, uint32_t* dirCluster,
  uint32_t* dirOffset
) {
  if ((driverState == NULL)
    || (fileName == NULL) || (*fileName == '\0')
    || (fileEntry == NULL)
    || (streamEntry == NULL)
  ) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;

  // Allocate UTF-16 name buffer
  uint16_t* utf16Name = (uint16_t*) malloc(
    EXFAT_MAX_FILENAME_LENGTH * sizeof(uint16_t)
  );
  if (utf16Name == NULL) {
    return EXFAT_NO_MEMORY;
  }

  // Convert filename to UTF-16
  uint8_t nameLength = asciiToUtf16(
    fileName, utf16Name, EXFAT_MAX_FILENAME_LENGTH
  );

  uint8_t numNameEntries = (nameLength + 14) / 15;
  uint8_t totalEntries = 2 + numNameEntries;

  // Find space for entries
  uint32_t currentCluster = directoryCluster;
  uint32_t targetSector = 0;
  uint32_t targetOffset = 0;
  bool foundSpace = false;
  int returnValue = EXFAT_SUCCESS;
  uint32_t entriesPerSector =
    driverState->bytesPerSector / EXFAT_DIRECTORY_ENTRY_SIZE;

  while (currentCluster != 0xFFFFFFFF && !foundSpace) {
    // Check each sector in the cluster
    for (uint32_t ss = 0; ss < driverState->sectorsPerCluster; ss++) {
      uint32_t sector = clusterToSector(driverState, currentCluster) + ss;
      int result = readSector(driverState, sector, buffer);
      if (result != EXFAT_SUCCESS) {
        returnValue = result;
        goto cleanup;
      }

      uint8_t consecutiveFree = 0;
      uint32_t firstFreeSector = 0;
      uint32_t firstFreeOffset = 0;
      
      for (
        uint32_t ii = 0;
        ii < driverState->bytesPerSector;
        ii += EXFAT_DIRECTORY_ENTRY_SIZE
      ) {
        uint8_t entryType = buffer[ii];

        if (entryType == EXFAT_ENTRY_UNUSED ||
            entryType == EXFAT_ENTRY_END_OF_DIR) {
          if (consecutiveFree == 0) {
            firstFreeSector = sector;
            firstFreeOffset = ii;
          }
          consecutiveFree++;

          if (consecutiveFree >= totalEntries) {
            targetSector = firstFreeSector;
            targetOffset = firstFreeOffset;
            foundSpace = true;
            break;
          }
        } else {
          consecutiveFree = 0;
        }
      }

      if (foundSpace) {
        break;
      }
    }

    if (!foundSpace) {
      uint32_t nextCluster = 0;
      int result = readFatEntry(driverState, currentCluster, &nextCluster);
      if (result != EXFAT_SUCCESS) {
        returnValue = result;
        goto cleanup;
      }
      currentCluster = nextCluster;
    }
  }

  if (!foundSpace) {
    returnValue = EXFAT_DISK_FULL;
    goto cleanup;
  }

  // Allocate first cluster for the file
  uint32_t firstCluster = 0;
  int result = allocateCluster(driverState, &firstCluster);
  if (result != EXFAT_SUCCESS) {
    returnValue = result;
    goto cleanup;
  }

  // Create the file directory entry structure
  ExFatFileDirectoryEntry *newFileEntry
    = (ExFatFileDirectoryEntry*) calloc(1, sizeof(ExFatFileDirectoryEntry));
  if (newFileEntry == NULL) {
    free(utf16Name);
    return EXFAT_NO_MEMORY;
  }
  
  newFileEntry->entryType = EXFAT_ENTRY_FILE;
  newFileEntry->secondaryCount = totalEntries - 1;
  newFileEntry->fileAttributes = EXFAT_ATTR_ARCHIVE;
  // Leave timestamps as zero (valid for exFAT)

  // Create the stream extension entry structure
  ExFatStreamExtensionEntry *newStreamEntry
    = (ExFatStreamExtensionEntry*) calloc(1, sizeof(ExFatStreamExtensionEntry));
  if (newStreamEntry == NULL) {
    free(newFileEntry);
    free(utf16Name);
    return EXFAT_NO_MEMORY;
  }
  
  newStreamEntry->entryType = EXFAT_ENTRY_STREAM;
  newStreamEntry->generalSecondaryFlags = 0x01;  // Name hash valid
  newStreamEntry->nameLength = nameLength;
  newStreamEntry->nameHash = calculateNameHash(utf16Name, nameLength);
  newStreamEntry->validDataLength = 0;  // Empty file
  newStreamEntry->firstCluster = firstCluster;
  newStreamEntry->dataLength = 0;  // Empty file

  // Allocate array for filename entries
  ExFatFileNameEntry* nameEntries = (ExFatFileNameEntry*) malloc(
    sizeof(ExFatFileNameEntry) * numNameEntries
  );
  if (nameEntries == NULL) {
    free(newStreamEntry);
    free(newFileEntry);
    free(utf16Name);
    return EXFAT_NO_MEMORY;
  }

  // Create filename entry structures
  uint8_t nameIndex = 0;
  for (uint8_t ii = 0; ii < numNameEntries; ii++) {
    for (uint32_t jj = 0; jj < sizeof(ExFatFileNameEntry); jj++) {
      ((uint8_t*) &nameEntries[ii])[jj] = 0;
    }
    
    nameEntries[ii].entryType = EXFAT_ENTRY_FILENAME;
    nameEntries[ii].generalSecondaryFlags = 0x00;

    for (uint8_t jj = 0; jj < 15 && nameIndex < nameLength; jj++) {
      nameEntries[ii].fileName[jj] = utf16Name[nameIndex];
      nameIndex++;
    }
  }

  // Allocate temporary buffer for entry set
  uint8_t* entrySetBuffer = (uint8_t*) malloc(
    EXFAT_DIRECTORY_ENTRY_SIZE * totalEntries
  );
  if (entrySetBuffer == NULL) {
    free(newStreamEntry);
    free(newFileEntry);
    free(nameEntries);
    free(utf16Name);
    return EXFAT_NO_MEMORY;
  }

  // Copy structures into entry set buffer using memcpy
  memcpy(
    &entrySetBuffer[0],
    newFileEntry,
    sizeof(ExFatFileDirectoryEntry)
  );
  
  memcpy(
    &entrySetBuffer[EXFAT_DIRECTORY_ENTRY_SIZE],
    newStreamEntry,
    sizeof(ExFatStreamExtensionEntry)
  );
  
  for (uint8_t ii = 0; ii < numNameEntries; ii++) {
    memcpy(
      &entrySetBuffer[(2 + ii) * EXFAT_DIRECTORY_ENTRY_SIZE],
      &nameEntries[ii],
      sizeof(ExFatFileNameEntry)
    );
  }

  // Calculate and write checksum
  uint16_t checksum = calculateEntrySetChecksum(entrySetBuffer, totalEntries);
  writeBytes(&entrySetBuffer[2], &checksum);

  // Write entries to directory (handling sector boundaries)
  uint32_t entriesWritten = 0;
  uint32_t sectorToWrite = targetSector;
  uint32_t writeOffset = targetOffset;

  while (entriesWritten < totalEntries) {
    // Read current sector
    result = readSector(driverState, sectorToWrite, buffer);
    if (result != EXFAT_SUCCESS) {
      returnValue = result;
      goto cleanup_all;
    }

    // Write as many entries as fit in this sector
    while (writeOffset < driverState->bytesPerSector &&
           entriesWritten < totalEntries) {
      memcpy(
        &buffer[writeOffset],
        &entrySetBuffer[entriesWritten * EXFAT_DIRECTORY_ENTRY_SIZE],
        EXFAT_DIRECTORY_ENTRY_SIZE
      );
      writeOffset += EXFAT_DIRECTORY_ENTRY_SIZE;
      entriesWritten++;
    }

    // Write sector back - THIS IS CRITICAL!
    result = writeSector(driverState, sectorToWrite, buffer);
    if (result != EXFAT_SUCCESS) {
      returnValue = result;
      goto cleanup_all;
    }

    // Move to next sector if needed
    if (entriesWritten < totalEntries) {
      sectorToWrite++;
      writeOffset = 0;
      
      // Verify we're still in the same cluster
      uint32_t sectorsFromStart = sectorToWrite -
        clusterToSector(driverState, currentCluster);
      if (sectorsFromStart >= driverState->sectorsPerCluster) {
        // ERROR: Entries span cluster boundary - shouldn't happen
        // with our search logic, but check anyway
        returnValue = EXFAT_ERROR;
        goto cleanup_all;
      }
    }
  }

  // Copy created entries back to output parameters
  copyBytes(fileEntry, newFileEntry, sizeof(ExFatFileDirectoryEntry));
  copyBytes(streamEntry, newStreamEntry, sizeof(ExFatStreamExtensionEntry));

  // Return directory location
  if (dirCluster != NULL) {
    *dirCluster = currentCluster;
  }
  if (dirOffset != NULL) {
    // Calculate the entry offset within the directory
    uint32_t sectorsFromClusterStart =
      (targetSector - clusterToSector(driverState, currentCluster));
    uint32_t entriesBeforeTargetSector =
      sectorsFromClusterStart * entriesPerSector;
    uint32_t entryOffsetInSector = targetOffset / EXFAT_DIRECTORY_ENTRY_SIZE;
    *dirOffset = entriesBeforeTargetSector + entryOffsetInSector;
  }

cleanup_all:
  free(newStreamEntry);
  free(newFileEntry);
  free(entrySetBuffer);
  free(nameEntries);
cleanup:
  free(utf16Name);
  return returnValue;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Search for a file in a directory
///
/// @param driverState Pointer to the exFAT driver state
/// @param directoryCluster First cluster of the directory
/// @param fileName Name of the file to search for
/// @param fileEntry Pointer to store file directory entry
/// @param streamEntry Pointer to store stream extension entry
/// @param dirCluster Pointer to store directory cluster
/// @param dirOffset Pointer to store offset in directory
///
/// @return EXFAT_SUCCESS if found, EXFAT_FILE_NOT_FOUND if not found
///////////////////////////////////////////////////////////////////////////////
static int searchDirectory(
  ExFatDriverState* driverState, uint32_t directoryCluster,
  const char* fileName, ExFatFileDirectoryEntry* fileEntry,
  ExFatStreamExtensionEntry* streamEntry, uint32_t* dirCluster,
  uint32_t* dirOffset
) {
  if (driverState == NULL || fileName == NULL || fileEntry == NULL ||
      streamEntry == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  // Validate cluster number
  if (directoryCluster < 2 ||
      directoryCluster >= driverState->clusterCount + 2) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;

  // Allocate search name buffer
  uint16_t* searchName = (uint16_t*) malloc(
    EXFAT_MAX_FILENAME_LENGTH * sizeof(uint16_t)
  );
  if (searchName == NULL) {
    return EXFAT_NO_MEMORY;
  }

  // Allocate full name buffer
  uint16_t* fullName = (uint16_t*) malloc(
    EXFAT_MAX_FILENAME_LENGTH * sizeof(uint16_t)
  );
  if (fullName == NULL) {
    free(searchName);
    return EXFAT_NO_MEMORY;
  }

  // Allocate temporary file entry
  ExFatFileDirectoryEntry* tempFileEntry =
    (ExFatFileDirectoryEntry*) malloc(sizeof(ExFatFileDirectoryEntry));
  if (tempFileEntry == NULL) {
    free(fullName);
    free(searchName);
    return EXFAT_NO_MEMORY;
  }

  // Allocate temporary stream entry
  ExFatStreamExtensionEntry* tempStreamEntry =
    (ExFatStreamExtensionEntry*) malloc(sizeof(ExFatStreamExtensionEntry));
  if (tempStreamEntry == NULL) {
    free(tempFileEntry);
    free(fullName);
    free(searchName);
    return EXFAT_NO_MEMORY;
  }

  // Allocate temporary name entry
  ExFatFileNameEntry* nameEntry =
    (ExFatFileNameEntry*) malloc(sizeof(ExFatFileNameEntry));
  if (nameEntry == NULL) {
    free(tempStreamEntry);
    free(tempFileEntry);
    free(fullName);
    free(searchName);
    return EXFAT_NO_MEMORY;
  }

  uint8_t searchNameLength = asciiToUtf16(
    fileName, searchName, EXFAT_MAX_FILENAME_LENGTH
  );

  uint32_t currentCluster = directoryCluster;
  uint32_t globalEntryOffset = 0;
  int returnValue = EXFAT_FILE_NOT_FOUND;

  uint32_t entriesPerSector =
    driverState->bytesPerSector / EXFAT_DIRECTORY_ENTRY_SIZE;

  while (currentCluster != 0xFFFFFFFF && currentCluster >= 2) {
    // Validate cluster is in range
    if (currentCluster >= driverState->clusterCount + 2) {
      returnValue = EXFAT_ERROR;
      goto cleanup;
    }

    uint32_t clusterStartSector = clusterToSector(driverState, currentCluster);
    uint32_t entriesPerCluster =
      entriesPerSector * driverState->sectorsPerCluster;

    // Process entries in this cluster
    for (uint32_t entryIndex = 0; entryIndex < entriesPerCluster;
         entryIndex++) {
      // Calculate sector and offset for this entry
      uint32_t sectorOffset = entryIndex / entriesPerSector;
      uint32_t entryOffset =
        (entryIndex % entriesPerSector) * EXFAT_DIRECTORY_ENTRY_SIZE;
      uint32_t sector = clusterStartSector + sectorOffset;

      // Read sector containing this entry
      int result = readSector(driverState, sector, buffer);
      if (result != EXFAT_SUCCESS) {
        returnValue = result;
        goto cleanup;
      }

      uint8_t entryType = buffer[entryOffset];

      if (entryType == EXFAT_ENTRY_END_OF_DIR) {
        goto cleanup;
      }

      if (entryType == EXFAT_ENTRY_FILE) {
        // Read file directory entry
        readBytes(tempFileEntry, &buffer[entryOffset]);

        uint8_t secondaryCount = 0;
        readBytes(&secondaryCount, &tempFileEntry->secondaryCount);

        if (secondaryCount < 2) {
          globalEntryOffset++;
          continue;
        }

        // Read stream extension entry (next entry)
        uint32_t streamIndex = entryIndex + 1;
        if (streamIndex >= entriesPerCluster) {
          // Stream entry is in next cluster - skip this file
          globalEntryOffset++;
          // FIX: Skip all secondary entries
          entryIndex += secondaryCount;
          continue;
        }

        uint32_t streamSectorOffset = streamIndex / entriesPerSector;
        uint32_t streamEntryOffset =
          (streamIndex % entriesPerSector) * EXFAT_DIRECTORY_ENTRY_SIZE;
        uint32_t streamSector = clusterStartSector + streamSectorOffset;

        if (streamSector != sector) {
          result = readSector(driverState, streamSector, buffer);
          if (result != EXFAT_SUCCESS) {
            returnValue = result;
            goto cleanup;
          }
        }

        readBytes(tempStreamEntry, &buffer[streamEntryOffset]);

        uint8_t streamEntryType = 0;
        readBytes(&streamEntryType, &tempStreamEntry->entryType);

        if (streamEntryType != EXFAT_ENTRY_STREAM) {
          globalEntryOffset++;
          // FIX: Skip all secondary entries
          entryIndex += secondaryCount;
          continue;
        }

        uint8_t nameLength = 0;
        readBytes(&nameLength, &tempStreamEntry->nameLength);

        if (nameLength == 0) {
          globalEntryOffset++;
          // FIX: Skip all secondary entries
          entryIndex += secondaryCount;
          continue;
        }

        // Read filename entries
        uint8_t nameIndex = 0;
        uint8_t numNameEntries = (nameLength + 14) / 15;
        uint32_t lastSectorRead = streamSector;
        bool nameReadComplete = true;

        for (uint8_t jj = 0;
          (jj < numNameEntries) && (nameIndex < nameLength);
          jj++
        ) {
          uint32_t nameEntryIndex = entryIndex + 2 + jj;
          if (nameEntryIndex >= entriesPerCluster) {
            // Filename entries span beyond cluster - skip this file
            nameReadComplete = false;
            break;
          }

          uint32_t nameSectorOffset = nameEntryIndex / entriesPerSector;
          uint32_t nameEntryOffset =
            (nameEntryIndex % entriesPerSector) * EXFAT_DIRECTORY_ENTRY_SIZE;
          uint32_t nameSector = clusterStartSector + nameSectorOffset;

          if (nameSector != lastSectorRead) {
            result = readSector(driverState, nameSector, buffer);
            if (result != EXFAT_SUCCESS) {
              returnValue = result;
              goto cleanup;
            }
            lastSectorRead = nameSector;
          }

          readBytes(nameEntry, &buffer[nameEntryOffset]);

          uint8_t nameEntryType = 0;
          readBytes(&nameEntryType, &nameEntry->entryType);

          if (nameEntryType != EXFAT_ENTRY_FILENAME) {
            nameReadComplete = false;
            break;
          }

          // Extract characters from this entry
          for (uint8_t kk = 0; kk < 15 && nameIndex < nameLength; kk++) {
            uint16_t character = 0;
            readBytes(&character, &nameEntry->fileName[kk]);
            fullName[nameIndex] = character;
            nameIndex++;
          }
        }

        // Compare names if we read all characters
        if (nameReadComplete && nameIndex == nameLength &&
            compareFilenames(
              fullName, nameLength, searchName, searchNameLength
            ) == 0) {
          // Found a match - copy to output parameters
          copyBytes(
            fileEntry, tempFileEntry, sizeof(ExFatFileDirectoryEntry)
          );
          copyBytes(
            streamEntry, tempStreamEntry, sizeof(ExFatStreamExtensionEntry)
          );

          if (dirCluster != NULL) {
            *dirCluster = currentCluster;
          }
          if (dirOffset != NULL) {
            *dirOffset = globalEntryOffset;
          }
          returnValue = EXFAT_SUCCESS;
          goto cleanup;
        }

        // FIX: Skip all secondary entries (file + stream + name entries)
        entryIndex += secondaryCount;
        globalEntryOffset += (secondaryCount + 1);
      } else {
        globalEntryOffset++;
      }
    }

    // Get next cluster in directory chain
    uint32_t nextCluster = 0;
    int result = readFatEntry(driverState, currentCluster, &nextCluster);
    if (result != EXFAT_SUCCESS) {
      returnValue = result;
      goto cleanup;
    }
    currentCluster = nextCluster;
  }

cleanup:
  free(nameEntry);
  free(tempStreamEntry);
  free(tempFileEntry);
  free(fullName);
  free(searchName);
  return returnValue;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Parse path and navigate through directories
///
/// @param driverState Pointer to the exFAT driver state
/// @param filePath Full path to the file
/// @param finalDirectory Pointer to store the final directory cluster
/// @param fileNameBuffer Buffer to store the filename (must be at least
///                       EXFAT_MAX_FILENAME_LENGTH + 1 bytes)
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
static int navigateToDirectory(
  ExFatDriverState* driverState, const char* filePath,
  uint32_t* finalDirectory, char* fileNameBuffer
) {
  if (driverState == NULL || filePath == NULL || finalDirectory == NULL ||
      fileNameBuffer == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  // Allocate component buffer
  char* component = (char*) malloc(EXFAT_MAX_FILENAME_LENGTH + 1);
  if (component == NULL) {
    return EXFAT_NO_MEMORY;
  }

  // Allocate directory entry
  ExFatFileDirectoryEntry* dirEntry =
    (ExFatFileDirectoryEntry*) malloc(sizeof(ExFatFileDirectoryEntry));
  if (dirEntry == NULL) {
    free(component);
    return EXFAT_NO_MEMORY;
  }

  // Allocate stream entry
  ExFatStreamExtensionEntry* streamEntry =
    (ExFatStreamExtensionEntry*) malloc(sizeof(ExFatStreamExtensionEntry));
  if (streamEntry == NULL) {
    free(dirEntry);
    free(component);
    return EXFAT_NO_MEMORY;
  }

  // Start at root directory
  uint32_t currentDirectory = driverState->rootDirectoryCluster;
  const char* pathPtr = filePath;
  int returnValue = EXFAT_SUCCESS;

  // Skip leading slash
  if (*pathPtr == '/') {
    pathPtr++;
  }

  // If empty path, return root
  if (*pathPtr == '\0') {
    *finalDirectory = currentDirectory;
    fileNameBuffer[0] = '\0';
    goto cleanup;
  }

  // Parse path component by component
  while (*pathPtr != '\0') {
    // Extract next path component
    uint8_t componentLength = 0;
    while (*pathPtr != '\0' && *pathPtr != '/' &&
           componentLength < EXFAT_MAX_FILENAME_LENGTH) {
      component[componentLength++] = *pathPtr++;
    }
    component[componentLength] = '\0';

    // Skip trailing slash
    if (*pathPtr == '/') {
      pathPtr++;
    }

    // If this is the last component, it's the filename
    if (*pathPtr == '\0') {
      *finalDirectory = currentDirectory;

      // Copy component to caller's buffer
      for (uint8_t ii = 0; ii <= componentLength; ii++) {
        fileNameBuffer[ii] = component[ii];
      }
      goto cleanup;
    }

    // Not the last component, so it should be a directory
    int result = searchDirectory(
      driverState, currentDirectory, component, dirEntry, streamEntry,
      NULL, NULL
    );

    if (result != EXFAT_SUCCESS) {
      returnValue = result;
      goto cleanup;
    }

    // Verify it's actually a directory using readBytes
    uint16_t attributes = 0;
    readBytes(&attributes, &dirEntry->fileAttributes);
    if ((attributes & EXFAT_ATTR_DIRECTORY) == 0) {
      returnValue = EXFAT_ERROR;  // Not a directory
      goto cleanup;
    }

    // Move to this directory using readBytes
    uint32_t nextDirectory = 0;
    readBytes(&nextDirectory, &streamEntry->firstCluster);
    currentDirectory = nextDirectory;
  }

  *finalDirectory = currentDirectory;
  fileNameBuffer[0] = '\0';

cleanup:
  free(streamEntry);
  free(dirEntry);
  free(component);
  return returnValue;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Open or create an exFAT file
///
/// @param driverState Pointer to the exFAT driver state
/// @param filePath Path to the file
/// @param mode File open mode (same as fopen: "r", "w", "a", etc.)
///
/// @return Pointer to ExFatFileHandle on success, NULL on failure
///////////////////////////////////////////////////////////////////////////////
ExFatFileHandle* exFatOpenFile(
  ExFatDriverState* driverState, const char* filePath, const char* mode
) {
  if ((driverState == NULL) || (!driverState->driverStateValid)
    || (filePath == NULL) || (*filePath == '\0')
    || (mode == NULL) || (*mode == '\0')
  ) {
    return NULL;
  }

  // Parse mode
  bool read = false;
  bool write = false;
  bool append = false;
  bool mustExist = false;
  bool truncate = false;

  if (mode[0] == 'r') {
    read = true;
    mustExist = true;
    if (mode[1] == '+') {
      write = true;
    }
  } else if (mode[0] == 'w') {
    write = true;
    truncate = true;
    if (mode[1] == '+') {
      read = true;
    }
  } else if (mode[0] == 'a') {
    write = true;
    append = true;
    if (mode[1] == '+') {
      read = true;
    }
  } else {
    return NULL;
  }

  // Allocate filename buffer
  char* fileName = (char*) malloc(EXFAT_MAX_FILENAME_LENGTH + 1);
  if (fileName == NULL) {
    return NULL;
  }

  // Allocate file entry
  ExFatFileDirectoryEntry* fileEntry =
    (ExFatFileDirectoryEntry*) malloc(sizeof(ExFatFileDirectoryEntry));
  if (fileEntry == NULL) {
    free(fileName);
    return NULL;
  }

  // Allocate stream entry
  ExFatStreamExtensionEntry* streamEntry =
    (ExFatStreamExtensionEntry*) malloc(sizeof(ExFatStreamExtensionEntry));
  if (streamEntry == NULL) {
    free(fileEntry);
    free(fileName);
    return NULL;
  }

  // Navigate to the directory containing the file
  uint32_t directoryCluster = 0;

  int result = navigateToDirectory(
    driverState, filePath, &directoryCluster, fileName
  );

  if (result != EXFAT_SUCCESS) {
    free(streamEntry);
    free(fileEntry);
    free(fileName);
    return NULL;
  }

  // Search for the file
  uint32_t dirCluster = 0;
  uint32_t dirOffset = 0;

  result = searchDirectory(
    driverState, directoryCluster, fileName, fileEntry, streamEntry,
    &dirCluster, &dirOffset
  );

  if (result == EXFAT_FILE_NOT_FOUND) {
    if (mustExist) {
      free(streamEntry);
      free(fileEntry);
      free(fileName);
      return NULL;
    }

    // Create the file
    result = createFileEntry(
      driverState, directoryCluster, fileName, fileEntry, streamEntry,
      &dirCluster, &dirOffset
    );
    if (result != EXFAT_SUCCESS) {
      free(streamEntry);
      free(fileEntry);
      free(fileName);
      return NULL;
    }

    // ###########################
    // DEBUG: Verify the file was written
    printString("File created at cluster ");
    printULongLong(dirCluster);
    printString(" offset ");
    printULongLong(dirOffset);
    printString("\n");
    
    // Dump directory to verify
    dumpDirectoryEntries(driverState, directoryCluster, 20);
    
    // Try to find it again - allocate verification structures
    ExFatFileDirectoryEntry* verifyEntry =
      (ExFatFileDirectoryEntry*) malloc(sizeof(ExFatFileDirectoryEntry));
    ExFatStreamExtensionEntry* verifyStream =
      (ExFatStreamExtensionEntry*) malloc(sizeof(ExFatStreamExtensionEntry));
    
    if (verifyEntry != NULL && verifyStream != NULL) {
      int verifyResult = searchDirectory(
        driverState, directoryCluster, fileName,
        verifyEntry, verifyStream, NULL, NULL
      );
      
      if (verifyResult == EXFAT_SUCCESS) {
        printString("Verification: File found after creation!\n");
      } else {
        printString("Verification FAILED: File not found! Error: ");
        printLongLong(verifyResult);
        printString("\n");
      }
    }
    
    free(verifyStream);
    free(verifyEntry);
  } else if (result != EXFAT_SUCCESS) {
    free(streamEntry);
    free(fileEntry);
    free(fileName);
    return NULL;
  }
  printString("Found file \"");
  printString(filePath);
  printString("\"\n");

  // Check if file is read-only when trying to write
  uint16_t fileAttributes = 0;
  readBytes(&fileAttributes, &fileEntry->fileAttributes);
  if ((write || append) &&
      ((fileAttributes & EXFAT_ATTR_READ_ONLY) != 0)) {
    free(streamEntry);
    free(fileEntry);
    free(fileName);
    return NULL;  // Cannot open read-only file for writing
  }

  // Allocate file handle
  ExFatFileHandle* handle = (ExFatFileHandle*) malloc(
    sizeof(ExFatFileHandle)
  );
  if (handle == NULL) {
    free(streamEntry);
    free(fileEntry);
    free(fileName);
    return NULL;
  }

  // Initialize file handle using readBytes for all packed struct accesses
  uint32_t firstCluster = 0;
  readBytes(&firstCluster, &streamEntry->firstCluster);
  handle->firstCluster = firstCluster;
  handle->currentCluster = firstCluster;
  printString("Allocated cluster: ");
  printULongLong(firstCluster);
  printString("\n");

  uint64_t fileSize = 0;
  readBytes(&fileSize, &streamEntry->dataLength);
  handle->fileSize = fileSize;

  uint16_t attributes = 0;
  readBytes(&attributes, &fileEntry->fileAttributes);
  handle->attributes = attributes;

  handle->directoryCluster = dirCluster;
  handle->directoryOffset = dirOffset;

  // Store open mode flags
  handle->canRead = read;
  handle->canWrite = write;
  handle->appendMode = append;

  // Copy filename
  uint8_t nameLength = 0;
  for (uint8_t ii = 0; fileName[ii] != '\0' &&
       ii < EXFAT_MAX_FILENAME_LENGTH; ii++) {
    handle->fileName[ii] = fileName[ii];
    nameLength = ii + 1;
  }
  handle->fileName[nameLength] = '\0';

  // Set position based on mode
  if (append) {
    handle->currentPosition = (uint32_t) handle->fileSize;
    // Navigate to last cluster
    uint32_t cluster = handle->firstCluster;
    uint32_t position = 0;
    while (position + driverState->bytesPerCluster < handle->fileSize) {
      uint32_t nextCluster = 0;
      if (readFatEntry(driverState, cluster, &nextCluster) !=
          EXFAT_SUCCESS) {
        free(streamEntry);
        free(fileEntry);
        free(fileName);
        free(handle);
        return NULL;
      }
      if (nextCluster == 0xFFFFFFFF) {
        break;
      }
      cluster = nextCluster;
      position += driverState->bytesPerCluster;
    }
    handle->currentCluster = cluster;
  } else {
    handle->currentPosition = 0;
  }

  // Truncate if needed
  if (truncate && handle->fileSize > 0) {
    handle->fileSize = 0;
    handle->currentPosition = 0;
    // TODO: Free all clusters and update directory entry
    // This requires implementing cluster freeing logic
  }

  free(streamEntry);
  free(fileEntry);
  free(fileName);
  return handle;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Debug function to dump directory entries
///
/// @param driverState Pointer to the exFAT driver state
/// @param directoryCluster First cluster of the directory to dump
/// @param maxEntries Maximum number of entries to dump
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int dumpDirectoryEntries(
  ExFatDriverState* driverState, uint32_t directoryCluster,
  uint32_t maxEntries
) {
  if (driverState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;
  
  uint32_t currentCluster = directoryCluster;
  uint32_t entriesRead = 0;

  printString("=== Directory Dump ===\n");
  
  while (currentCluster != 0xFFFFFFFF && entriesRead < maxEntries) {
    for (uint32_t ss = 0; ss < driverState->sectorsPerCluster; ss++) {
      uint32_t sector = clusterToSector(driverState, currentCluster) + ss;
      int result = readSector(driverState, sector, buffer);
      if (result != EXFAT_SUCCESS) {
        return result;
      }

      for (
        uint32_t ii = 0;
        ii < driverState->bytesPerSector && entriesRead < maxEntries;
        ii += EXFAT_DIRECTORY_ENTRY_SIZE
      ) {
        uint8_t entryType = buffer[ii];
        
        printString("Entry ");
        printULongLong(entriesRead);
        printString(": Type=0x");
        printHex(entryType);
        printString("\n");
        
        if (entryType == EXFAT_ENTRY_END_OF_DIR) {
          printString(" (END)\n");
          return EXFAT_SUCCESS;
        } else if (entryType == EXFAT_ENTRY_FILE) {
          uint8_t secondaryCount = buffer[ii + 1];
          printString(" (FILE) Secondary=");
          printULongLong(secondaryCount);
          printString("\n");
        } else if (entryType == EXFAT_ENTRY_STREAM) {
          uint8_t nameLen = buffer[ii + 3];
          uint32_t cluster = 0;
          readBytes(&cluster, &buffer[ii + 20]);
          printString(" (STREAM) NameLen=");
          printULongLong(nameLen);
          printString(" Cluster=");
          printULongLong(cluster);
          printString("\n");
        } else if (entryType == EXFAT_ENTRY_FILENAME) {
          printString(" (NAME)\n");
        } else if (entryType == EXFAT_ENTRY_UNUSED) {
          printString(" (UNUSED)\n");
        } else {
          printString(" (UNKNOWN)\n");
        }
        
        entriesRead++;
      }
    }

    uint32_t nextCluster = 0;
    int result = readFatEntry(driverState, currentCluster, &nextCluster);
    if (result != EXFAT_SUCCESS) {
      return result;
    }
    currentCluster = nextCluster;
  }

  return EXFAT_SUCCESS;
}

