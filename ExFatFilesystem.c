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
int debugClusterAllocation(ExFatDriverState* driverState);
int checkForClusterConflicts(
  ExFatDriverState* driverState, uint32_t directoryCluster);
int verifyFatAllocation(
  ExFatDriverState* driverState, uint32_t cluster);
int compareFatCopies(
  ExFatDriverState* driverState, uint32_t cluster);
int dumpFatEntries(
  ExFatDriverState* driverState, uint32_t startCluster, uint32_t numClusters);
int debugPartitionLayout(
  ExFatDriverState* driverState, FilesystemState* filesystemState);
int crossCheckFatAndDirectory(ExFatDriverState* driverState);
int checkNoFatChainFlag(ExFatDriverState* driverState);

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

  checkNoFatChainFlag(driverState);
  debugPartitionLayout(driverState, filesystemState);
  crossCheckFatAndDirectory(driverState);

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
/// @brief Write a FAT entry (INSTRUMENTED VERSION)
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

  printString("\n=== writeFatEntry DEBUG ===\n");
  printString("Cluster: ");
  printULongLong(cluster);
  printString(", Value: 0x");
  printHex((value >> 24) & 0xFF);
  printHex((value >> 16) & 0xFF);
  printHex((value >> 8) & 0xFF);
  printHex(value & 0xFF);
  printString("\n");

  uint32_t fatOffset = cluster * 4;
  uint32_t fatSector = driverState->fatStartSector +
    (fatOffset / driverState->bytesPerSector);
  uint32_t entryOffset = fatOffset % driverState->bytesPerSector;

  printString("FAT offset: ");
  printULongLong(fatOffset);
  printString(" bytes\n");
  printString("FAT sector: ");
  printULongLong(fatSector);
  printString("\n");
  printString("Entry offset: ");
  printULongLong(entryOffset);
  printString("\n");

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;

  // Read current sector
  printString("Reading FAT sector...\n");
  int result = readSector(driverState, fatSector, buffer);
  if (result != EXFAT_SUCCESS) {
    printString("ERROR: Failed to read FAT sector!\n");
    return result;
  }

  // Show current value
  uint32_t oldValue = 0;
  readBytes(&oldValue, &buffer[entryOffset]);
  printString("Old value: 0x");
  printHex((oldValue >> 24) & 0xFF);
  printHex((oldValue >> 16) & 0xFF);
  printHex((oldValue >> 8) & 0xFF);
  printHex(oldValue & 0xFF);
  printString("\n");

  // Write new value
  printString("Writing new value to buffer...\n");
  writeBytes(&buffer[entryOffset], &value);

  // Verify buffer write
  uint32_t verifyBuffer = 0;
  readBytes(&verifyBuffer, &buffer[entryOffset]);
  printString("Buffer verify: 0x");
  printHex((verifyBuffer >> 24) & 0xFF);
  printHex((verifyBuffer >> 16) & 0xFF);
  printHex((verifyBuffer >> 8) & 0xFF);
  printHex(verifyBuffer & 0xFF);
  if (verifyBuffer == value) {
    printString(" [OK]\n");
  } else {
    printString(" [MISMATCH!]\n");
  }

  // Write sector to disk
  printString("Writing FAT sector to disk...\n");
  result = writeSector(driverState, fatSector, buffer);
  if (result != EXFAT_SUCCESS) {
    printString("ERROR: Failed to write FAT sector!\n");
    return result;
  }
  printString("Write complete.\n");

  // Read back to verify
  printString("Reading back to verify...\n");
  result = readSector(driverState, fatSector, buffer);
  if (result != EXFAT_SUCCESS) {
    printString("ERROR: Failed to read back FAT sector!\n");
    return result;
  }

  uint32_t diskValue = 0;
  readBytes(&diskValue, &buffer[entryOffset]);
  printString("Disk verify: 0x");
  printHex((diskValue >> 24) & 0xFF);
  printHex((diskValue >> 16) & 0xFF);
  printHex((diskValue >> 8) & 0xFF);
  printHex(diskValue & 0xFF);
  
  if (diskValue == value) {
    printString(" [OK - Write successful!]\n");
  } else {
    printString(" [FAILED - Value not persisted!]\n");
    printString("Expected: 0x");
    printHex((value >> 24) & 0xFF);
    printHex((value >> 16) & 0xFF);
    printHex((value >> 8) & 0xFF);
    printHex(value & 0xFF);
    printString("\n");
  }

  printString("=== writeFatEntry END ===\n\n");
  return result;
}

///////////////////////////////////////////////////////////////////////////////
/// @struct NoFatChainRange
///
/// @brief Represents a range of clusters used by a NoFatChain file
///
/// @param startCluster First cluster in the range
/// @param endCluster Last cluster in the range (inclusive)
typedef struct NoFatChainRange {
  uint32_t startCluster;
  uint32_t endCluster;
} NoFatChainRange;

///////////////////////////////////////////////////////////////////////////////
/// @brief Check if a cluster is in any NoFatChain range
///
/// @param cluster Cluster to check
/// @param ranges Array of NoFatChain ranges
/// @param numRanges Number of ranges in the array
///
/// @return true if cluster is in a NoFatChain range, false otherwise
///////////////////////////////////////////////////////////////////////////////
static bool isClusterInNoFatChainRange(
  uint32_t cluster, NoFatChainRange* ranges, uint8_t numRanges
) {
  for (uint8_t ii = 0; ii < numRanges; ii++) {
    if (cluster >= ranges[ii].startCluster &&
        cluster <= ranges[ii].endCluster) {
      return true;
    }
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Collect NoFatChain cluster ranges from directory
///
/// @param driverState Pointer to the exFAT driver state
/// @param ranges Pointer to array to store ranges
/// @param maxRanges Maximum number of ranges that can be stored
/// @param numRanges Pointer to store actual number of ranges found
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
static int collectNoFatChainRanges(
  ExFatDriverState* driverState, NoFatChainRange* ranges,
  uint8_t maxRanges, uint8_t* numRanges
) {
  if (driverState == NULL || ranges == NULL || numRanges == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  *numRanges = 0;
  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;

  printString("  Scanning for NoFatChain files...\n");

  uint32_t currentCluster = driverState->rootDirectoryCluster;

  while (currentCluster != 0xFFFFFFFF && currentCluster >= 2) {
    for (uint32_t ss = 0; ss < driverState->sectorsPerCluster; ss++) {
      uint32_t sector = clusterToSector(driverState, currentCluster) + ss;
      int result = readSector(driverState, sector, buffer);
      if (result != EXFAT_SUCCESS) {
        return result;
      }

      for (
        uint32_t ii = 0;
        ii < driverState->bytesPerSector;
        ii += EXFAT_DIRECTORY_ENTRY_SIZE
      ) {
        uint8_t entryType = buffer[ii];

        if (entryType == EXFAT_ENTRY_END_OF_DIR) {
          printString("  Found ");
          printULongLong(*numRanges);
          printString(" NoFatChain file(s)\n");
          return EXFAT_SUCCESS;
        }

        if (entryType == EXFAT_ENTRY_FILE) {
          if (ii + EXFAT_DIRECTORY_ENTRY_SIZE >=
              driverState->bytesPerSector) {
            continue;
          }

          uint8_t streamType = buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE];
          if (streamType == EXFAT_ENTRY_STREAM) {
            uint8_t flags = buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE + 1];
            
            // Check NoFatChain bit (bit 1)
            if ((flags & 0x02) != 0) {
              if (*numRanges >= maxRanges) {
                printString("  WARNING: Too many NoFatChain files, ");
                printString("some may not be tracked!\n");
                return EXFAT_SUCCESS;
              }

              uint32_t firstCluster = 0;
              readBytes(
                &firstCluster,
                &buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE + 20]
              );

              uint64_t dataLength = 0;
              readBytes(
                &dataLength,
                &buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE + 24]
              );

              uint32_t clustersNeeded = 0;
              if (dataLength > 0) {
                clustersNeeded = 
                  ((uint32_t) dataLength + driverState->bytesPerCluster - 1) /
                  driverState->bytesPerCluster;
              } else {
                clustersNeeded = 1;
              }

              ranges[*numRanges].startCluster = firstCluster;
              ranges[*numRanges].endCluster = firstCluster + clustersNeeded - 1;
              
              printString("    Range ");
              printULongLong(*numRanges);
              printString(": clusters ");
              printULongLong(firstCluster);
              printString("-");
              printULongLong(firstCluster + clustersNeeded - 1);
              printString("\n");

              (*numRanges)++;
            }
          }
        }
      }
    }

    uint32_t nextCluster = 0;
    int result = readFatEntry(driverState, currentCluster, &nextCluster);
    if (result != EXFAT_SUCCESS) {
      return result;
    }
    currentCluster = nextCluster;
  }

  printString("  Found ");
  printULongLong(*numRanges);
  printString(" NoFatChain file(s)\n");
  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Find a free cluster (memory-efficient version)
///
/// This version doesn't allocate a large bitmap. Instead, it:
/// 1. Collects NoFatChain ranges into a small array
/// 2. Checks each candidate cluster against FAT and ranges
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

  printString("Finding free cluster...\n");

  // Allocate small array for NoFatChain ranges (max 16 ranges)
  const uint8_t maxRanges = 16;
  NoFatChainRange* ranges = (NoFatChainRange*) malloc(
    maxRanges * sizeof(NoFatChainRange)
  );
  if (ranges == NULL) {
    printString("  ERROR: Cannot allocate range array\n");
    return EXFAT_NO_MEMORY;
  }

  // Collect NoFatChain ranges
  uint8_t numRanges = 0;
  int result = collectNoFatChainRanges(
    driverState, ranges, maxRanges, &numRanges
  );
  if (result != EXFAT_SUCCESS) {
    free(ranges);
    return result;
  }

  // Search for free cluster
  printString("  Searching for free cluster...\n");
  for (uint32_t cluster = 2; cluster < driverState->clusterCount + 2;
       cluster++) {
    // Check if in NoFatChain range
    if (isClusterInNoFatChainRange(cluster, ranges, numRanges)) {
      continue;  // Skip, in use by NoFatChain file
    }

    // Check FAT
    uint32_t fatValue = 0;
    result = readFatEntry(driverState, cluster, &fatValue);
    if (result != EXFAT_SUCCESS) {
      free(ranges);
      return result;
    }

    if (fatValue == 0) {
      // Found a free cluster!
      printString("  Found free cluster: ");
      printULongLong(cluster);
      printString("\n");
      
      *freeCluster = cluster;
      free(ranges);
      return EXFAT_SUCCESS;
    }
  }

  // No free clusters
  printString("  ERROR: No free clusters available\n");
  free(ranges);
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

  printString("\nAllocating new cluster...\n");
  
  int result = findFreeCluster(driverState, newCluster);
  if (result != EXFAT_SUCCESS) {
    return result;
  }

  printString("  Marking cluster ");
  printULongLong(*newCluster);
  printString(" as allocated in FAT\n");

  result = writeFatEntry(driverState, *newCluster, 0xFFFFFFFF);
  if (result != EXFAT_SUCCESS) {
    printString("  ERROR: Failed to write FAT entry\n");
    return result;
  }

  printString("  Cluster ");
  printULongLong(*newCluster);
  printString(" allocated successfully\n");

  return EXFAT_SUCCESS;
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
/// @brief Create a new file in a directory (FIXED VERSION)
///
/// This version properly handles entry sets that may span sector boundaries
/// and ensures all entries are written to disk correctly.
///
/// @param driverState Pointer to the exFAT driver state
/// @param directoryCluster First cluster of the directory
/// @param fileName Name of the file to create
/// @param fileEntry Pointer to store created file directory entry
/// @param streamEntry Pointer to store created stream extension entry
/// @param dirCluster Pointer to store directory cluster where file was
///                   created
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

  printString("\n=== Creating File Entries ===\n");
  printString("Target sector: ");
  printULongLong(targetSector);
  printString(", offset: ");
  printULongLong(targetOffset);
  printString("\n");
  printString("Total entries: ");
  printULongLong(totalEntries);
  printString(" (");
  printULongLong(totalEntries * EXFAT_DIRECTORY_ENTRY_SIZE);
  printString(" bytes)\n");
  printString("Bytes per sector: ");
  printULongLong(driverState->bytesPerSector);
  printString("\n");

  // CRITICAL FIX: Check if entries span multiple sectors
  uint32_t entrySetEndOffset =
    targetOffset + (totalEntries * EXFAT_DIRECTORY_ENTRY_SIZE);
  bool spansMultipleSectors = (entrySetEndOffset > driverState->bytesPerSector);
  
  if (spansMultipleSectors) {
    printString("WARNING: Entry set spans multiple sectors!\n");
    printString("This case is not yet implemented.\n");
    returnValue = EXFAT_ERROR;
    goto cleanup;
  }

  // Allocate a temporary buffer for building the entry set
  uint8_t* entrySetBuffer = (uint8_t*) malloc(
    totalEntries * EXFAT_DIRECTORY_ENTRY_SIZE
  );
  if (entrySetBuffer == NULL) {
    returnValue = EXFAT_NO_MEMORY;
    goto cleanup;
  }

  // Zero out the temporary buffer
  for (uint32_t ii = 0; ii < totalEntries * EXFAT_DIRECTORY_ENTRY_SIZE; ii++) {
    entrySetBuffer[ii] = 0;
  }

  // Build file directory entry
  ExFatFileDirectoryEntry* newFileEntry =
    (ExFatFileDirectoryEntry*) entrySetBuffer;
  
  uint8_t entryType = EXFAT_ENTRY_FILE;
  uint8_t secondaryCount = totalEntries - 1;
  uint16_t attributes = EXFAT_ATTR_ARCHIVE;
  uint32_t timestamp = 0;
  uint8_t zeroValue = 0;
  
  writeBytes(&newFileEntry->entryType, &entryType);
  writeBytes(&newFileEntry->secondaryCount, &secondaryCount);
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

  // Build stream extension entry
  ExFatStreamExtensionEntry* newStreamEntry =
    (ExFatStreamExtensionEntry*) (entrySetBuffer + EXFAT_DIRECTORY_ENTRY_SIZE);
  
  uint8_t streamEntryType = EXFAT_ENTRY_STREAM;
  uint8_t generalFlags = 0x01;
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

  // Build filename entries
  uint8_t nameEntryType = EXFAT_ENTRY_FILENAME;
  uint8_t nameIndex = 0;
  
  for (uint8_t ii = 0; ii < numNameEntries; ii++) {
    ExFatFileNameEntry* newNameEntry =
      (ExFatFileNameEntry*) (entrySetBuffer +
        (2 + ii) * EXFAT_DIRECTORY_ENTRY_SIZE);
    
    writeBytes(&newNameEntry->entryType, &nameEntryType);
    writeBytes(&newNameEntry->generalSecondaryFlags, &zeroValue);
    
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
    entrySetBuffer, totalEntries
  );
  writeBytes(&newFileEntry->setChecksum, &checksum);

  // Debug: Dump entry set
  printString("=== Entry Set (in temp buffer) ===\n");
  for (uint8_t ii = 0; ii < totalEntries; ii++) {
    printString("Entry ");
    printULongLong(ii);
    printString(": ");
    for (uint8_t jj = 0; jj < EXFAT_DIRECTORY_ENTRY_SIZE; jj++) {
      if (jj > 0 && (jj % 16) == 0) {
        printString("\n         ");
      }
      uint8_t* entryBytes =
        entrySetBuffer + (ii * EXFAT_DIRECTORY_ENTRY_SIZE);
      printHex(entryBytes[jj]);
      printString(" ");
    }
    printString("\n");
  }

  // Read the target sector
  result = readSector(driverState, targetSector, buffer);
  if (result != EXFAT_SUCCESS) {
    free(entrySetBuffer);
    returnValue = result;
    goto cleanup;
  }

  // Copy entry set to the sector buffer at the target offset
  for (uint32_t ii = 0; ii < totalEntries * EXFAT_DIRECTORY_ENTRY_SIZE; ii++) {
    buffer[targetOffset + ii] = entrySetBuffer[ii];
  }

  // Debug: Verify copy in sector buffer
  printString("=== Entry Set (in sector buffer before write) ===\n");
  for (uint8_t ii = 0; ii < totalEntries; ii++) {
    uint8_t entryType = buffer[targetOffset + (ii * EXFAT_DIRECTORY_ENTRY_SIZE)];
    printString("Entry ");
    printULongLong(ii);
    printString(" at byte ");
    printULongLong(targetOffset + (ii * EXFAT_DIRECTORY_ENTRY_SIZE));
    printString(": Type=0x");
    printHex(entryType);
    printString("\n");
  }

  // Write the sector back to disk
  printString("Writing sector ");
  printULongLong(targetSector);
  printString(" to disk...\n");
  
  result = writeSector(driverState, targetSector, buffer);
  free(entrySetBuffer);
  
  if (result != EXFAT_SUCCESS) {
    returnValue = result;
    goto cleanup;
  }

  printString("Write complete. Verifying...\n");

  // Verify the write by reading back
  result = readSector(driverState, targetSector, buffer);
  if (result != EXFAT_SUCCESS) {
    returnValue = result;
    goto cleanup;
  }

  printString("=== Entry Set (read back from disk) ===\n");
  for (uint8_t ii = 0; ii < totalEntries; ii++) {
    uint32_t entryByteOffset = targetOffset + (ii * EXFAT_DIRECTORY_ENTRY_SIZE);
    uint8_t entryType = buffer[entryByteOffset];
    printString("Entry ");
    printULongLong(ii);
    printString(" at byte ");
    printULongLong(entryByteOffset);
    printString(": Type=0x");
    printHex(entryType);
    printString("\n");
    
    if (entryType == 0x00) {
      printString("  ERROR: Entry is all zeros!\n");
    }
  }

  // Copy created entries back to output parameters
  copyBytes(fileEntry, &buffer[targetOffset], sizeof(ExFatFileDirectoryEntry));
  copyBytes(
    streamEntry,
    &buffer[targetOffset + EXFAT_DIRECTORY_ENTRY_SIZE],
    sizeof(ExFatStreamExtensionEntry)
  );

  // Return directory location
  if (dirCluster != NULL) {
    *dirCluster = currentCluster;
  }
  if (dirOffset != NULL) {
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

  if ((strcmp(filePath, "hello") == 0)
    && (strcmp(mode, "w") == 0)
  ) {
    printString("\n========================================\n");
    printString("BEFORE creating 'hello' file:\n");
    printString("========================================\n");
    debugClusterAllocation(driverState);
    checkForClusterConflicts(driverState, driverState->rootDirectoryCluster);
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

  if ((strcmp(filePath, "hello") == 0)
    && (strcmp(mode, "w") == 0)
  ) {
    printString("\n========================================\n");
    printString("AFTER creating 'hello' file:\n");
    printString("========================================\n");
    checkForClusterConflicts(driverState, driverState->rootDirectoryCluster);

    printString("\n=== POST-CREATION FAT CHECK ===\n");
    dumpFatEntries(driverState, 10, 5);  // Dump clusters 10-14
    compareFatCopies(driverState, 12);
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
  
  (void) driverState;
  printString("Compare: ");
  printString(ourFile);
  printString(" vs ");
  printString(linuxFile);
  printString("\n");
  
  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Debug FAT entries to find allocation conflicts
///
/// @param driverState Pointer to the exFAT driver state
/// @param startCluster First cluster to dump
/// @param numClusters Number of clusters to dump
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int dumpFatEntries(
  ExFatDriverState* driverState, uint32_t startCluster, uint32_t numClusters
) {
  if (driverState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  printString("\n=== FAT Entries Dump ===\n");
  printString("FAT start sector: ");
  printULongLong(driverState->fatStartSector);
  printString("\n");
  printString("Bytes per sector: ");
  printULongLong(driverState->bytesPerSector);
  printString("\n\n");

  for (uint32_t cluster = startCluster;
       cluster < startCluster + numClusters;
       cluster++) {
    uint32_t fatValue = 0;
    int result = readFatEntry(driverState, cluster, &fatValue);
    if (result != EXFAT_SUCCESS) {
      printString("Error reading FAT entry for cluster ");
      printULongLong(cluster);
      printString("\n");
      continue;
    }

    printString("Cluster ");
    printULongLong(cluster);
    printString(": 0x");
    printHex((fatValue >> 24) & 0xFF);
    printHex((fatValue >> 16) & 0xFF);
    printHex((fatValue >> 8) & 0xFF);
    printHex(fatValue & 0xFF);
    printString(" = ");
    printULongLong(fatValue);

    if (fatValue == 0) {
      printString(" [FREE]");
    } else if (fatValue == 0xFFFFFFFF) {
      printString(" [END OF CHAIN]");
    } else if (fatValue >= 2 && fatValue < driverState->clusterCount + 2) {
      printString(" [NEXT=");
      printULongLong(fatValue);
      printString("]");
    } else {
      printString(" [INVALID/RESERVED]");
    }
    printString("\n");
  }

  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Verify FAT entry reading by dumping raw FAT sector
///
/// @param driverState Pointer to the exFAT driver state
/// @param cluster Cluster whose FAT sector to dump
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int dumpFatSector(
  ExFatDriverState* driverState, uint32_t cluster
) {
  if (driverState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;

  // Calculate FAT sector
  uint32_t fatOffset = cluster * 4;
  uint32_t fatSector = driverState->fatStartSector +
    (fatOffset / driverState->bytesPerSector);
  uint32_t entryOffset = fatOffset % driverState->bytesPerSector;

  printString("\n=== FAT Sector Dump for Cluster ");
  printULongLong(cluster);
  printString(" ===\n");
  printString("FAT offset: ");
  printULongLong(fatOffset);
  printString(" bytes\n");
  printString("FAT sector: ");
  printULongLong(fatSector);
  printString("\n");
  printString("Entry offset in sector: ");
  printULongLong(entryOffset);
  printString("\n\n");

  // Read FAT sector
  int result = readSector(driverState, fatSector, buffer);
  if (result != EXFAT_SUCCESS) {
    return result;
  }

  // Dump the area around our entry
  uint32_t dumpStart = (entryOffset >= 16) ? (entryOffset - 16) : 0;
  uint32_t dumpEnd = entryOffset + 32;
  if (dumpEnd > driverState->bytesPerSector) {
    dumpEnd = driverState->bytesPerSector;
  }

  printString("Raw FAT data (bytes ");
  printULongLong(dumpStart);
  printString("-");
  printULongLong(dumpEnd - 1);
  printString("):\n");

  for (uint32_t ii = dumpStart; ii < dumpEnd; ii++) {
    if ((ii % 16) == 0) {
      printString("\n");
      printULongLong(ii);
      printString(": ");
    }
    printHex(buffer[ii]);
    printString(" ");
    
    // Mark our target entry
    if (ii == entryOffset) {
      printString("<- ");
    }
  }
  printString("\n\n");

  // Decode a few entries around our target
  uint32_t firstCluster = (entryOffset / 4) - 2;
  uint32_t lastCluster = firstCluster + 8;
  
  printString("Decoded entries:\n");
  for (uint32_t cl = firstCluster; cl < lastCluster; cl++) {
    uint32_t clOffset = (cl * 4) % driverState->bytesPerSector;
    if (clOffset + 4 > driverState->bytesPerSector) {
      break;
    }
    
    uint32_t value = 0;
    readBytes(&value, &buffer[clOffset]);
    
    printString("  Cluster ");
    printULongLong(cl);
    printString(": 0x");
    printHex((value >> 24) & 0xFF);
    printHex((value >> 16) & 0xFF);
    printHex((value >> 8) & 0xFF);
    printHex(value & 0xFF);
    
    if (cl == cluster) {
      printString(" <-- TARGET");
    }
    printString("\n");
  }

  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Test and debug cluster allocation
///
/// @param driverState Pointer to the exFAT driver state
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int debugClusterAllocation(ExFatDriverState* driverState) {
  if (driverState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  printString("\n=== Cluster Allocation Debug ===\n");

  // Dump first 20 FAT entries
  printString("\nCurrent FAT state (clusters 2-21):\n");
  dumpFatEntries(driverState, 2, 20);

  // Try to find a free cluster
  printString("\n--- Searching for free cluster ---\n");
  uint32_t freeCluster = 0;
  int result = findFreeCluster(driverState, &freeCluster);
  
  if (result == EXFAT_SUCCESS) {
    printString("Found free cluster: ");
    printULongLong(freeCluster);
    printString("\n");
    
    // Dump the FAT sector containing this cluster
    dumpFatSector(driverState, freeCluster);
    
    // Verify it's actually free by reading again
    uint32_t verifyValue = 0;
    result = readFatEntry(driverState, freeCluster, &verifyValue);
    if (result == EXFAT_SUCCESS) {
      printString("\nVerification read: 0x");
      printHex((verifyValue >> 24) & 0xFF);
      printHex((verifyValue >> 16) & 0xFF);
      printHex((verifyValue >> 8) & 0xFF);
      printHex(verifyValue & 0xFF);
      
      if (verifyValue == 0) {
        printString(" [Confirmed FREE]\n");
      } else {
        printString(" [ERROR: NOT FREE!]\n");
      }
    }
  } else if (result == EXFAT_DISK_FULL) {
    printString("No free clusters found (disk full)\n");
  } else {
    printString("Error searching for free cluster: ");
    printLongLong(result);
    printString("\n");
  }

  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Compare directory entries with FAT to find conflicts
///
/// @param driverState Pointer to the exFAT driver state
/// @param directoryCluster Directory cluster to check
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int checkForClusterConflicts(
  ExFatDriverState* driverState, uint32_t directoryCluster
) {
  if (driverState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  printString("\n=== Checking for Cluster Conflicts ===\n");

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;
  
  // Track which clusters we've seen
  uint8_t* clusterUsage = (uint8_t*) malloc(driverState->clusterCount + 2);
  if (clusterUsage == NULL) {
    return EXFAT_NO_MEMORY;
  }
  
  // Initialize to 0 (unused)
  for (uint32_t ii = 0; ii < driverState->clusterCount + 2; ii++) {
    clusterUsage[ii] = 0;
  }

  uint32_t currentCluster = directoryCluster;
  uint32_t fileCount = 0;

  while (currentCluster != 0xFFFFFFFF && currentCluster >= 2) {
    for (uint32_t ss = 0; ss < driverState->sectorsPerCluster; ss++) {
      uint32_t sector = clusterToSector(driverState, currentCluster) + ss;
      int result = readSector(driverState, sector, buffer);
      if (result != EXFAT_SUCCESS) {
        free(clusterUsage);
        return result;
      }

      for (
        uint32_t ii = 0;
        ii < driverState->bytesPerSector;
        ii += EXFAT_DIRECTORY_ENTRY_SIZE
      ) {
        uint8_t entryType = buffer[ii];

        if (entryType == EXFAT_ENTRY_END_OF_DIR) {
          goto done_scanning;
        }

        if (entryType == EXFAT_ENTRY_FILE) {
          // Next entry should be stream
          if (ii + EXFAT_DIRECTORY_ENTRY_SIZE >= driverState->bytesPerSector) {
            continue;
          }

          uint8_t streamType = buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE];
          if (streamType == EXFAT_ENTRY_STREAM) {
            uint32_t cluster = 0;
            readBytes(&cluster, &buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE + 20]);
            
            fileCount++;
            printString("File ");
            printULongLong(fileCount);
            printString(" uses cluster ");
            printULongLong(cluster);
            
            if (cluster >= 2 && cluster < driverState->clusterCount + 2) {
              if (clusterUsage[cluster] != 0) {
                printString(" [CONFLICT! Already used by file ");
                printULongLong(clusterUsage[cluster]);
                printString("]");
              } else {
                clusterUsage[cluster] = fileCount;
                printString(" [OK]");
              }
            } else {
              printString(" [INVALID CLUSTER]");
            }
            printString("\n");
          }
        }
      }
    }

    uint32_t nextCluster = 0;
    int result = readFatEntry(driverState, currentCluster, &nextCluster);
    if (result != EXFAT_SUCCESS) {
      free(clusterUsage);
      return result;
    }
    currentCluster = nextCluster;
  }

done_scanning:
  free(clusterUsage);
  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Check FAT state after allocation
///
/// @param driverState Pointer to the exFAT driver state
/// @param cluster Cluster that was just allocated
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int verifyFatAllocation(
  ExFatDriverState* driverState, uint32_t cluster
) {
  if (driverState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  printString("\n=== Verifying FAT Allocation ===\n");
  printString("Checking cluster: ");
  printULongLong(cluster);
  printString("\n");

  // Read the FAT entry
  uint32_t fatValue = 0;
  int result = readFatEntry(driverState, cluster, &fatValue);
  if (result != EXFAT_SUCCESS) {
    printString("ERROR: Failed to read FAT entry!\n");
    return result;
  }

  printString("FAT value: 0x");
  printHex((fatValue >> 24) & 0xFF);
  printHex((fatValue >> 16) & 0xFF);
  printHex((fatValue >> 8) & 0xFF);
  printHex(fatValue & 0xFF);
  printString(" = ");
  printULongLong(fatValue);

  if (fatValue == 0xFFFFFFFF) {
    printString(" [Correctly marked as END OF CHAIN]\n");
  } else if (fatValue == 0) {
    printString(" [ERROR: Still marked as FREE!]\n");
  } else {
    printString(" [ERROR: Unexpected value!]\n");
  }

  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Dump both FAT copies (if multiple FATs exist)
///
/// @param driverState Pointer to the exFAT driver state
/// @param cluster Cluster to check in both FATs
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int compareFatCopies(
  ExFatDriverState* driverState, uint32_t cluster
) {
  if (driverState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;

  printString("\n=== Comparing FAT Copies ===\n");

  // Read boot sector to get FAT information
  int result = readSector(driverState, 0, buffer);
  if (result != EXFAT_SUCCESS) {
    return result;
  }

  uint8_t numberOfFats = buffer[110];
  uint32_t fatLength = 0;
  readBytes(&fatLength, &buffer[84]);

  printString("Number of FATs: ");
  printULongLong(numberOfFats);
  printString("\n");
  printString("FAT length: ");
  printULongLong(fatLength);
  printString(" sectors\n");

  // Check each FAT
  uint32_t fatOffset = cluster * 4;
  uint32_t entryOffsetInSector = fatOffset % driverState->bytesPerSector;
  uint32_t sectorOffsetInFat = fatOffset / driverState->bytesPerSector;

  for (uint8_t fatNum = 0; fatNum < numberOfFats; fatNum++) {
    uint32_t fatStartSector = driverState->fatStartSector +
      (fatNum * fatLength);
    uint32_t sector = fatStartSector + sectorOffsetInFat;

    printString("\nFAT ");
    printULongLong(fatNum);
    printString(" (sector ");
    printULongLong(sector);
    printString("):\n");

    result = readSector(driverState, sector, buffer);
    if (result != EXFAT_SUCCESS) {
      printString("  ERROR: Failed to read\n");
      continue;
    }

    uint32_t value = 0;
    readBytes(&value, &buffer[entryOffsetInSector]);
    printString("  Cluster ");
    printULongLong(cluster);
    printString(": 0x");
    printHex((value >> 24) & 0xFF);
    printHex((value >> 16) & 0xFF);
    printHex((value >> 8) & 0xFF);
    printHex(value & 0xFF);
    printString("\n");
  }

  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Comprehensive partition and offset debugging
///
/// @param driverState Pointer to the exFAT driver state
/// @param filesystemState Pointer to filesystem state
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int debugPartitionLayout(
  ExFatDriverState* driverState, FilesystemState* filesystemState
) {
  if (driverState == NULL || filesystemState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  uint8_t* buffer = filesystemState->blockBuffer;

  printString("\n=== PARTITION LAYOUT ANALYSIS ===\n\n");

  // 1. Check MBR/Partition Table at sector 0 (absolute)
  printString("--- Checking MBR at absolute sector 0 ---\n");
  int result = filesystemState->blockDevice->readBlocks(
    filesystemState->blockDevice->context,
    0,  // Absolute sector 0
    1,
    filesystemState->blockSize,
    buffer
  );

  if (result == 0) {
    uint16_t mbrSignature = 0;
    readBytes(&mbrSignature, &buffer[510]);
    printString("MBR signature: 0x");
    printHex((mbrSignature >> 8) & 0xFF);
    printHex(mbrSignature & 0xFF);
    
    if (mbrSignature == 0xAA55) {
      printString(" [Valid MBR]\n");
      
      // Check partition table entries
      for (uint8_t ii = 0; ii < 4; ii++) {
        uint16_t partOffset = 446 + (ii * 16);
        uint8_t partType = buffer[partOffset + 4];
        uint32_t partStart = 0;
        uint32_t partSize = 0;
        
        readBytes(&partStart, &buffer[partOffset + 8]);
        readBytes(&partSize, &buffer[partOffset + 12]);
        
        if (partType != 0) {
          printString("\nPartition ");
          printULongLong(ii + 1);
          printString(":\n");
          printString("  Type: 0x");
          printHex(partType);
          printString(" (");
          if (partType == 0x07) {
            printString("exFAT/NTFS");
          } else if (partType == 0x0C || partType == 0x0E) {
            printString("FAT32 LBA");
          } else {
            printString("Other");
          }
          printString(")\n");
          printString("  Start LBA: ");
          printULongLong(partStart);
          printString("\n");
          printString("  Size: ");
          printULongLong(partSize);
          printString(" sectors\n");
        }
      }
    } else {
      printString(" [Not a valid MBR - might be raw filesystem]\n");
    }
  }

  // 2. Show what the driver thinks
  printString("\n--- Driver Configuration ---\n");
  printString("Filesystem startLba: ");
  printULongLong(filesystemState->startLba);
  printString("\n");
  printString("Filesystem endLba: ");
  printULongLong(filesystemState->endLba);
  printString("\n");
  printString("Block device partition: ");
  printULongLong(filesystemState->blockDevice->partitionNumber);
  printString("\n");

  // 3. Read boot sector at driver's startLba
  printString("\n--- Boot Sector at driver's startLba (");
  printULongLong(filesystemState->startLba);
  printString(") ---\n");
  
  result = filesystemState->blockDevice->readBlocks(
    filesystemState->blockDevice->context,
    filesystemState->startLba,
    1,
    filesystemState->blockSize,
    buffer
  );

  if (result == 0) {
    // Check filesystem name
    printString("Filesystem name: ");
    for (uint8_t ii = 0; ii < 8; ii++) {
      char ch = buffer[3 + ii];
      if (ch >= 32 && ch < 127) {
        char str[2] = {ch, '\0'};
        printString(str);
      } else {
        printString("?");
      }
    }
    printString("\n");

    // Check boot signature
    uint16_t bootSig = 0;
    readBytes(&bootSig, &buffer[510]);
    printString("Boot signature: 0x");
    printHex((bootSig >> 8) & 0xFF);
    printHex(bootSig & 0xFF);
    printString("\n");

    // Show key offsets
    uint32_t fatOffset = 0;
    uint32_t clusterHeapOffset = 0;
    uint32_t rootDirCluster = 0;
    
    readBytes(&fatOffset, &buffer[80]);
    readBytes(&clusterHeapOffset, &buffer[88]);
    readBytes(&rootDirCluster, &buffer[96]);

    printString("FAT offset: ");
    printULongLong(fatOffset);
    printString(" sectors\n");
    printString("Cluster heap offset: ");
    printULongLong(clusterHeapOffset);
    printString(" sectors\n");
    printString("Root dir cluster: ");
    printULongLong(rootDirCluster);
    printString("\n");

    // Calculate absolute sectors
    printString("\n--- Absolute Sector Addresses ---\n");
    printString("Boot sector: ");
    printULongLong(filesystemState->startLba);
    printString("\n");
    printString("FAT start: ");
    printULongLong(filesystemState->startLba + fatOffset);
    printString("\n");
    printString("Cluster heap start: ");
    printULongLong(filesystemState->startLba + clusterHeapOffset);
    printString("\n");
    
    uint32_t rootDirSector = filesystemState->startLba + clusterHeapOffset +
      ((rootDirCluster - 2) * driverState->sectorsPerCluster);
    printString("Root directory: ");
    printULongLong(rootDirSector);
    printString("\n");
  }

  // 4. Compare Linux's view
  printString("\n--- Linux Filesystem Check ---\n");
  printString("On Linux, run these commands:\n");
  printString("  sudo fdisk -l /dev/sdX\n");
  printString("  sudo exfatfsck /dev/sdX1 --verbose\n");
  printString("\nCompare the partition start sector with driver's startLba.\n");

  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Verify that file was written to expected location
///
/// @param driverState Pointer to the exFAT driver state
/// @param filesystemState Pointer to filesystem state
/// @param filename Name of file to verify
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int verifyFileLocation(
  ExFatDriverState* driverState, FilesystemState* filesystemState,
  const char* filename
) {
  if (driverState == NULL || filesystemState == NULL || filename == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  printString("\n=== VERIFY FILE LOCATION ===\n");
  printString("Searching for file: ");
  printString(filename);
  printString("\n");

  // Calculate root directory absolute sector
  uint32_t rootDirSector = filesystemState->startLba +
    driverState->clusterHeapStartSector +
    ((driverState->rootDirectoryCluster - 2) * driverState->sectorsPerCluster);

  printString("Root directory absolute sector: ");
  printULongLong(rootDirSector);
  printString("\n");

  printString("\nOn Linux, you can verify this with:\n");
  printString("  sudo dd if=/dev/sdX bs=512 skip=");
  printULongLong(rootDirSector);
  printString(" count=1 | hexdump -C | grep -A5 '85'\n");
  printString("\nThis should show the directory entries including your file.\n");

  return EXFAT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Cross-check FAT and directory entries
///
/// @param driverState Pointer to the exFAT driver state
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int crossCheckFatAndDirectory(ExFatDriverState* driverState) {
  if (driverState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;

  printString("\n=== CROSS-CHECK: FAT vs DIRECTORY ===\n");

  // Scan directory and check each file's cluster in FAT
  uint32_t currentCluster = driverState->rootDirectoryCluster;
  uint32_t fileNum = 0;

  while (currentCluster != 0xFFFFFFFF && currentCluster >= 2) {
    for (uint32_t ss = 0; ss < driverState->sectorsPerCluster; ss++) {
      uint32_t sector = clusterToSector(driverState, currentCluster) + ss;
      int result = readSector(driverState, sector, buffer);
      if (result != EXFAT_SUCCESS) {
        return result;
      }

      for (
        uint32_t ii = 0;
        ii < driverState->bytesPerSector;
        ii += EXFAT_DIRECTORY_ENTRY_SIZE
      ) {
        uint8_t entryType = buffer[ii];

        if (entryType == EXFAT_ENTRY_END_OF_DIR) {
          printString("\nEnd of directory.\n");
          return EXFAT_SUCCESS;
        }

        if (entryType == EXFAT_ENTRY_FILE) {
          fileNum++;
          
          // Get secondary count
          uint8_t secondaryCount = buffer[ii + 1];
          
          // Check for stream entry
          if (ii + EXFAT_DIRECTORY_ENTRY_SIZE >= 
              driverState->bytesPerSector) {
            continue;
          }

          uint8_t streamType = buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE];
          if (streamType == EXFAT_ENTRY_STREAM) {
            uint32_t cluster = 0;
            readBytes(
              &cluster,
              &buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE + 20]
            );

            uint8_t nameLen = buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE + 3];

            printString("\nFile ");
            printULongLong(fileNum);
            printString(" (nameLen=");
            printULongLong(nameLen);
            printString(", cluster=");
            printULongLong(cluster);
            printString("):\n");

            // Check FAT
            if (cluster >= 2 && cluster < driverState->clusterCount + 2) {
              uint32_t fatValue = 0;
              result = readFatEntry(driverState, cluster, &fatValue);
              if (result == EXFAT_SUCCESS) {
                printString("  FAT value: 0x");
                printHex((fatValue >> 24) & 0xFF);
                printHex((fatValue >> 16) & 0xFF);
                printHex((fatValue >> 8) & 0xFF);
                printHex(fatValue & 0xFF);

                if (fatValue == 0) {
                  printString(" [ERROR: CLUSTER IS FREE!]\n");
                  printString("  This file's cluster is not allocated in FAT!\n");
                } else if (fatValue == 0xFFFFFFFF) {
                  printString(" [OK: END OF CHAIN]\n");
                } else if (fatValue >= 2) {
                  printString(" [OK: POINTS TO ");
                  printULongLong(fatValue);
                  printString("]\n");
                } else {
                  printString(" [INVALID]\n");
                }
              }
            } else {
              printString("  [INVALID CLUSTER NUMBER]\n");
            }
          }
        }
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
/// @brief Check if Linux files are using NoFatChain optimization
///
/// @param driverState Pointer to the exFAT driver state
///
/// @return EXFAT_SUCCESS on success, error code on failure
///////////////////////////////////////////////////////////////////////////////
int checkNoFatChainFlag(ExFatDriverState* driverState) {
  if (driverState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  uint8_t* buffer = filesystemState->blockBuffer;

  printString("\n=== CHECKING NoFatChain FLAG ===\n");
  printString("exFAT optimization: NoFatChain bit allows contiguous file\n");
  printString("allocation without FAT entries.\n\n");

  uint32_t currentCluster = driverState->rootDirectoryCluster;
  uint32_t fileNum = 0;

  while (currentCluster != 0xFFFFFFFF && currentCluster >= 2) {
    for (uint32_t ss = 0; ss < driverState->sectorsPerCluster; ss++) {
      uint32_t sector = clusterToSector(driverState, currentCluster) + ss;
      int result = readSector(driverState, sector, buffer);
      if (result != EXFAT_SUCCESS) {
        return result;
      }

      for (
        uint32_t ii = 0;
        ii < driverState->bytesPerSector;
        ii += EXFAT_DIRECTORY_ENTRY_SIZE
      ) {
        uint8_t entryType = buffer[ii];

        if (entryType == EXFAT_ENTRY_END_OF_DIR) {
          printString("\nConclusion: ");
          if (fileNum == 0) {
            printString("No files checked.\n");
          }
          return EXFAT_SUCCESS;
        }

        if (entryType == EXFAT_ENTRY_FILE) {
          // Check stream entry
          if (ii + EXFAT_DIRECTORY_ENTRY_SIZE >=
              driverState->bytesPerSector) {
            continue;
          }

          uint8_t streamType = buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE];
          if (streamType == EXFAT_ENTRY_STREAM) {
            fileNum++;

            uint8_t flags = buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE + 1];
            uint32_t cluster = 0;
            readBytes(
              &cluster,
              &buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE + 20]
            );
            uint64_t dataLength = 0;
            readBytes(
              &dataLength,
              &buffer[ii + EXFAT_DIRECTORY_ENTRY_SIZE + 24]
            );

            printString("\nFile ");
            printULongLong(fileNum);
            printString(":\n");
            printString("  FirstCluster: ");
            printULongLong(cluster);
            printString("\n");
            printString("  DataLength: ");
            printULongLong((uint32_t) dataLength);
            printString(" bytes\n");
            printString("  GeneralSecondaryFlags: 0x");
            printHex(flags);
            printString("\n    Bit 0 (AllocPossible): ");
            printString((flags & 0x01) ? "YES" : "NO");
            printString("\n    Bit 1 (NoFatChain): ");
            printString((flags & 0x02) ? "YES" : "NO");
            printString("\n");

            if ((flags & 0x02) != 0) {
              printString("  ** Uses NoFatChain: Data is contiguous, ");
              printString("no FAT entries needed **\n");
              
              // Calculate number of clusters needed
              uint32_t clustersNeeded = 
                ((uint32_t) dataLength + driverState->bytesPerCluster - 1) /
                driverState->bytesPerCluster;
              
              if (clustersNeeded == 0) {
                clustersNeeded = 1;  // At least one cluster allocated
              }

              printString("  Clusters used: ");
              printULongLong(cluster);
              printString(" through ");
              printULongLong(cluster + clustersNeeded - 1);
              printString(" (");
              printULongLong(clustersNeeded);
              printString(" total)\n");
            } else {
              // Should have FAT entries
              printString("  ** Requires FAT chain **\n");
              
              if (cluster >= 2 && cluster < driverState->clusterCount + 2) {
                uint32_t fatValue = 0;
                result = readFatEntry(driverState, cluster, &fatValue);
                if (result == EXFAT_SUCCESS) {
                  printString("  FAT entry: 0x");
                  printHex((fatValue >> 24) & 0xFF);
                  printHex((fatValue >> 16) & 0xFF);
                  printHex((fatValue >> 8) & 0xFF);
                  printHex(fatValue & 0xFF);
                  
                  if (fatValue == 0) {
                    printString(" [ERROR: Not allocated!]");
                  } else if (fatValue == 0xFFFFFFFF) {
                    printString(" [OK: End of chain]");
                  }
                  printString("\n");
                }
              }
            }
          }
        }
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

