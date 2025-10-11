///////////////////////////////////////////////////////////////////////////////
///
/// @file              ExFatFilesystem.c
///
/// @brief             Memory-efficient exFAT driver implementation for NanoOs.
///
///////////////////////////////////////////////////////////////////////////////

#include "ExFatFilesystem.h"
#include "Filesystem.h"

// Debug helpers
int verifyAndFixChecksum(
  ExFatDriverState* driverState, uint32_t directoryCluster,
  uint32_t entryOffset);
int compareEntryWithLinux(
  ExFatDriverState* driverState, uint32_t directoryCluster,
  uint32_t entryOffset);

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

  // Allocate UTF-16 name buffer (only temp allocation needed)
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

  // Read the target sector into buffer
  result = readSector(driverState, targetSector, buffer);
  if (result != EXFAT_SUCCESS) {
    returnValue = result;
    goto cleanup;
  }

  // Calculate where in the buffer our entries will be
  uint8_t* entrySetStart = &buffer[targetOffset];
  
  // Zero out the entry set area (in case it had old data)
  for (uint32_t ii = 0; ii < totalEntries * EXFAT_DIRECTORY_ENTRY_SIZE; ii++) {
    if (targetOffset + ii >= driverState->bytesPerSector) {
      break;  // Can't span sectors yet, will handle below
    }
    entrySetStart[ii] = 0;
  }

  // Cast to file directory entry and populate using writeBytes
  ExFatFileDirectoryEntry* newFileEntry =
    (ExFatFileDirectoryEntry*) entrySetStart;
  
  uint8_t entryType = EXFAT_ENTRY_FILE;
  uint8_t secondaryCount = totalEntries - 1;
  uint16_t attributes = EXFAT_ATTR_ARCHIVE;
  uint32_t timestamp = 0;
  uint8_t zeroValue = 0;
  
  writeBytes(&newFileEntry->entryType, &entryType);
  writeBytes(&newFileEntry->secondaryCount, &secondaryCount);
  // Skip checksum for now
  writeBytes(&newFileEntry->fileAttributes, &attributes);
  writeBytes(&newFileEntry->reserved1, &zeroValue);
  writeBytes(&newFileEntry->createTimestamp, &timestamp);
  writeBytes(&newFileEntry->lastModifiedTimestamp, &timestamp);
  writeBytes(&newFileEntry->lastAccessedTimestamp, &timestamp);
  writeBytes(&newFileEntry->create10msIncrement, &zeroValue);
  writeBytes(&newFileEntry->lastModified10msIncrement, &zeroValue);
  writeBytes(&newFileEntry->createUtcOffset, &zeroValue);
  writeBytes(&newFileEntry->lastModifiedUtcOffset, &zeroValue);
  writeBytes(&newFileEntry->lastAccessedUtcOffset, &zeroValue);

  // Cast to stream extension entry and populate using writeBytes
  ExFatStreamExtensionEntry* newStreamEntry =
    (ExFatStreamExtensionEntry*) (entrySetStart + EXFAT_DIRECTORY_ENTRY_SIZE);
  
  uint8_t streamEntryType = EXFAT_ENTRY_STREAM;
  uint8_t generalFlags = 0x01;  // Allocation possible
  uint16_t nameHash = calculateNameHash(utf16Name, nameLength);
  uint64_t validDataLength = 0;
  uint64_t dataLength = 0;
  uint32_t reserved = 0;
  
  writeBytes(&newStreamEntry->entryType, &streamEntryType);
  writeBytes(&newStreamEntry->generalSecondaryFlags, &generalFlags);
  writeBytes(&newStreamEntry->reserved1, &zeroValue);
  writeBytes(&newStreamEntry->nameLength, &nameLength);
  writeBytes(&newStreamEntry->nameHash, &nameHash);
  writeBytes(&newStreamEntry->reserved2, &zeroValue);
  writeBytes(&newStreamEntry->validDataLength, &validDataLength);
  writeBytes(&newStreamEntry->reserved3, &reserved);
  writeBytes(&newStreamEntry->firstCluster, &firstCluster);
  writeBytes(&newStreamEntry->dataLength, &dataLength);

  // Build filename entries using writeBytes
  uint8_t nameEntryType = EXFAT_ENTRY_FILENAME;
  uint8_t nameIndex = 0;
  
  for (uint8_t ii = 0; ii < numNameEntries; ii++) {
    ExFatFileNameEntry* newNameEntry =
      (ExFatFileNameEntry*) (entrySetStart + (2 + ii) * EXFAT_DIRECTORY_ENTRY_SIZE);
    
    writeBytes(&newNameEntry->entryType, &nameEntryType);
    writeBytes(&newNameEntry->generalSecondaryFlags, &zeroValue);
    
    // Write up to 15 UTF-16 characters
    for (uint8_t jj = 0; jj < 15; jj++) {
      uint16_t character = 0;
      if (nameIndex < nameLength) {
        character = utf16Name[nameIndex];
        nameIndex++;
      }
      writeBytes(&newNameEntry->fileName[jj], &character);
    }
  }

  // Calculate checksum over the entry set
  uint16_t checksum = calculateEntrySetChecksum(
    entrySetStart, totalEntries
  );
  writeBytes(&newFileEntry->setChecksum, &checksum);

  // DEBUG: Dump the entry set before writing
  printString("=== Entry Set Dump ===\n");
  for (uint8_t ii = 0; ii < totalEntries; ii++) {
    printString("Entry ");
    printULongLong(ii);
    printString(": ");
    for (uint8_t jj = 0; jj < EXFAT_DIRECTORY_ENTRY_SIZE; jj++) {
      if (jj > 0 && (jj % 16) == 0) {
        printString("\n         ");
      }
      uint8_t* entryBytes = entrySetStart + (ii * EXFAT_DIRECTORY_ENTRY_SIZE);
      printHex(entryBytes[jj]);
      printString(" ");
    }
    printString("\n");
  }
  printString("Checksum: 0x");
  printHex((checksum >> 8) & 0xFF);
  printHex(checksum & 0xFF);
  printString("\n");

  // Write the sector back
  result = writeSector(driverState, targetSector, buffer);
  if (result != EXFAT_SUCCESS) {
    returnValue = result;
    goto cleanup;
  }

  // Copy created entries back to output parameters using copyBytes
  memcpy(fileEntry, newFileEntry, sizeof(ExFatFileDirectoryEntry));
  memcpy(streamEntry, newStreamEntry, sizeof(ExFatStreamExtensionEntry));

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

    compareEntryWithLinux(driverState, directoryCluster, 10);
    verifyAndFixChecksum(driverState, directoryCluster, 10);

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

///////////////////////////////////////////////////////////////////////////////
/// @brief Compare entry set with a known-good file from Linux
///
/// @param driverState Pointer to the exFAT driver state
/// @param directoryCluster Directory cluster to dump
/// @param entryOffset Offset of the entry to examine
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int compareEntryWithLinux(
  ExFatDriverState* driverState, uint32_t directoryCluster,
  uint32_t entryOffset
) {
  if (driverState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;

  // Calculate sector and offset
  uint32_t entriesPerSector =
    driverState->bytesPerSector / EXFAT_DIRECTORY_ENTRY_SIZE;
  uint32_t sectorOffset = entryOffset / entriesPerSector;
  uint32_t byteOffset =
    (entryOffset % entriesPerSector) * EXFAT_DIRECTORY_ENTRY_SIZE;
  
  uint32_t sector = clusterToSector(driverState, directoryCluster) +
    sectorOffset;

  int result = readSector(driverState, sector, buffer);
  if (result != EXFAT_SUCCESS) {
    return result;
  }

  printString("\n=== Detailed Entry Analysis ===\n");
  
  // Read file entry
  uint8_t* fileEntryBytes = &buffer[byteOffset];
  printString("FILE Entry (hex dump):\n");
  for (uint8_t ii = 0; ii < 32; ii++) {
    if (ii > 0 && (ii % 16) == 0) {
      printString("\n");
    }
    printHex(fileEntryBytes[ii]);
    printString(" ");
  }
  printString("\n");

  // Decode file entry fields
  printString("\nFILE Entry Fields:\n");
  printString("  EntryType: 0x");
  printHex(fileEntryBytes[0]);
  printString(" (should be 0x85)\n");
  
  printString("  SecondaryCount: ");
  printULongLong(fileEntryBytes[1]);
  printString("\n");
  
  uint16_t storedChecksum = 0;
  readBytes(&storedChecksum, &fileEntryBytes[2]);
  printString("  Checksum: 0x");
  printHex((storedChecksum >> 8) & 0xFF);
  printHex(storedChecksum & 0xFF);
  printString("\n");
  
  uint16_t attributes = 0;
  readBytes(&attributes, &fileEntryBytes[4]);
  printString("  Attributes: 0x");
  printHex((attributes >> 8) & 0xFF);
  printHex(attributes & 0xFF);
  printString(" (");
  if ((attributes & EXFAT_ATTR_READ_ONLY) != 0) {
    printString("RO ");
  }
  if ((attributes & EXFAT_ATTR_DIRECTORY) != 0) {
    printString("DIR ");
  }
  if ((attributes & EXFAT_ATTR_ARCHIVE) != 0) {
    printString("ARC ");
  }
  printString(")\n");

  // Read stream entry
  uint8_t* streamEntryBytes = &buffer[byteOffset + 32];
  printString("\nSTREAM Entry (hex dump):\n");
  for (uint8_t ii = 0; ii < 32; ii++) {
    if (ii > 0 && (ii % 16) == 0) {
      printString("\n");
    }
    printHex(streamEntryBytes[ii]);
    printString(" ");
  }
  printString("\n");

  // Decode stream entry fields
  printString("\nSTREAM Entry Fields:\n");
  printString("  EntryType: 0x");
  printHex(streamEntryBytes[0]);
  printString(" (should be 0xC0)\n");
  
  printString("  GeneralSecondaryFlags: 0x");
  printHex(streamEntryBytes[1]);
  printString(" (bit 0=AllocPossible, bit 1=NoFatChain)\n");
  
  printString("  NameLength: ");
  printULongLong(streamEntryBytes[3]);
  printString("\n");
  
  uint16_t nameHash = 0;
  readBytes(&nameHash, &streamEntryBytes[4]);
  printString("  NameHash: 0x");
  printHex((nameHash >> 8) & 0xFF);
  printHex(nameHash & 0xFF);
  printString("\n");
  
  uint64_t validDataLength = 0;
  readBytes(&validDataLength, &streamEntryBytes[8]);
  printString("  ValidDataLength: ");
  printULongLong((uint32_t) validDataLength);
  printString("\n");
  
  uint32_t firstCluster = 0;
  readBytes(&firstCluster, &streamEntryBytes[20]);
  printString("  FirstCluster: ");
  printULongLong(firstCluster);
  printString("\n");
  
  uint64_t dataLength = 0;
  readBytes(&dataLength, &streamEntryBytes[24]);
  printString("  DataLength: ");
  printULongLong((uint32_t) dataLength);
  printString("\n");

  // Verify checksum
  printString("\n=== Checksum Verification ===\n");
  uint8_t secondaryCount = fileEntryBytes[1];
  uint8_t totalEntries = secondaryCount + 1;
  
  // Allocate temporary buffer for entry set
  uint8_t* entrySet = (uint8_t*) malloc(
    totalEntries * EXFAT_DIRECTORY_ENTRY_SIZE
  );
  if (entrySet == NULL) {
    return EXFAT_NO_MEMORY;
  }

  // Copy entries
  for (uint8_t ii = 0; ii < totalEntries; ii++) {
    uint8_t* src = &buffer[byteOffset + (ii * EXFAT_DIRECTORY_ENTRY_SIZE)];
    uint8_t* dst = &entrySet[ii * EXFAT_DIRECTORY_ENTRY_SIZE];
    for (uint8_t jj = 0; jj < EXFAT_DIRECTORY_ENTRY_SIZE; jj++) {
      dst[jj] = src[jj];
    }
  }

  // Calculate checksum
  uint16_t calculatedChecksum = 0;
  uint32_t totalBytes = totalEntries * EXFAT_DIRECTORY_ENTRY_SIZE;
  
  for (uint32_t ii = 0; ii < totalBytes; ii++) {
    if (ii == 2 || ii == 3) {
      continue;  // Skip checksum field
    }
    calculatedChecksum = ((calculatedChecksum << 15) |
      (calculatedChecksum >> 1)) + (uint16_t) entrySet[ii];
  }

  printString("Stored checksum:     0x");
  printHex((storedChecksum >> 8) & 0xFF);
  printHex(storedChecksum & 0xFF);
  printString("\n");
  
  printString("Calculated checksum: 0x");
  printHex((calculatedChecksum >> 8) & 0xFF);
  printHex(calculatedChecksum & 0xFF);
  printString("\n");
  
  if (storedChecksum == calculatedChecksum) {
    printString("✓ Checksum MATCHES\n");
  } else {
    printString("✗ Checksum MISMATCH!\n");
  }

  free(entrySet);
  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Manually verify checksum by recalculating from disk
///
/// @param driverState Pointer to the exFAT driver state
/// @param directoryCluster Directory cluster containing the file
/// @param entryOffset Offset of the file entry
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int verifyAndFixChecksum(
  ExFatDriverState* driverState, uint32_t directoryCluster,
  uint32_t entryOffset
) {
  if (driverState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;

  // Read the sector containing the entry
  uint32_t entriesPerSector =
    driverState->bytesPerSector / EXFAT_DIRECTORY_ENTRY_SIZE;
  uint32_t sectorOffset = entryOffset / entriesPerSector;
  uint32_t byteOffset =
    (entryOffset % entriesPerSector) * EXFAT_DIRECTORY_ENTRY_SIZE;
  
  uint32_t sector = clusterToSector(driverState, directoryCluster) +
    sectorOffset;

  int result = readSector(driverState, sector, buffer);
  if (result != EXFAT_SUCCESS) {
    return result;
  }

  // Get secondary count
  uint8_t secondaryCount = buffer[byteOffset + 1];
  uint8_t totalEntries = secondaryCount + 1;

  // Recalculate checksum
  uint16_t newChecksum = 0;
  uint32_t totalBytes = totalEntries * EXFAT_DIRECTORY_ENTRY_SIZE;
  
  for (uint32_t ii = 0; ii < totalBytes; ii++) {
    uint32_t globalByteOffset = byteOffset + ii;
    
    // Skip checksum field (bytes 2-3 of first entry)
    if (ii == 2 || ii == 3) {
      continue;
    }
    
    newChecksum = ((newChecksum << 15) | (newChecksum >> 1)) +
      (uint16_t) buffer[globalByteOffset];
  }

  // Read stored checksum
  uint16_t storedChecksum = 0;
  readBytes(&storedChecksum, &buffer[byteOffset + 2]);

  printString("Verification:\n");
  printString("  Stored:     0x");
  printHex((storedChecksum >> 8) & 0xFF);
  printHex(storedChecksum & 0xFF);
  printString("\n");
  printString("  Calculated: 0x");
  printHex((newChecksum >> 8) & 0xFF);
  printHex(newChecksum & 0xFF);
  printString("\n");

  if (storedChecksum != newChecksum) {
    printString("  Status: MISMATCH - Fixing...\n");
    
    // Write corrected checksum
    writeBytes(&buffer[byteOffset + 2], &newChecksum);
    
    // Write sector back
    result = writeSector(driverState, sector, buffer);
    if (result != EXFAT_SUCCESS) {
      return result;
    }
    
    printString("  Fixed checksum written to disk\n");
  } else {
    printString("  Status: OK\n");
  }

  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Compare our file creation with a Linux-created file
///
/// @param driverState Pointer to the exFAT driver state
/// @param ourFile Path to file we created
/// @param linuxFile Path to file Linux created
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int compareFileStructures(
  ExFatDriverState* driverState, const char* ourFile, const char* linuxFile
) {
  printString("\n=== Comparing File Structures ===\n");
  
  // This would search for both files and compare their
  // directory entries byte-by-byte
  // Implementation left as exercise since it uses existing functions
  
  printString("Compare: ");
  printString(ourFile);
  printString(" vs ");
  printString(linuxFile);
  printString("\n");
  
  return EXFAT_SUCCESS;
}
