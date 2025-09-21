///////////////////////////////////////////////////////////////////////////////
///
/// @file              ExFatFilesystem.c
///
/// @brief             exFAT filesystem driver implementation for NanoOs.
///
///////////////////////////////////////////////////////////////////////////////

#include "ExFatFilesystem.h"

#include <string.h>

// exFAT constants
#define EXFAT_SIGNATURE              0x4146544658455845ULL // "EXFATFAT"
#define EXFAT_SECTOR_SIZE            512
#define EXFAT_CLUSTER_SIZE_MIN       512
#define EXFAT_CLUSTER_SIZE_MAX       (32 * 1024 * 1024)
#define EXFAT_MAX_FILENAME_LENGTH    255
#define EXFAT_DIRECTORY_ENTRY_SIZE   32
#define EXFAT_MAX_OPEN_FILES         8

// Directory entry types
#define EXFAT_ENTRY_UNUSED           0x00
#define EXFAT_ENTRY_END_OF_DIR       0x00
#define EXFAT_ENTRY_FILE             0x85
#define EXFAT_ENTRY_STREAM           0xC0
#define EXFAT_ENTRY_FILENAME         0xC1
#define EXFAT_ENTRY_ALLOCATION_BITMAP 0x81
#define EXFAT_ENTRY_UPCASE_TABLE     0x82
#define EXFAT_ENTRY_VOLUME_LABEL     0x83

// File attributes
#define EXFAT_ATTR_READ_ONLY         0x01
#define EXFAT_ATTR_HIDDEN            0x02
#define EXFAT_ATTR_SYSTEM            0x04
#define EXFAT_ATTR_DIRECTORY         0x10
#define EXFAT_ATTR_ARCHIVE           0x20

// Error codes
#define EXFAT_SUCCESS                0
#define EXFAT_ERROR                  -1
#define EXFAT_FILE_NOT_FOUND         -2
#define EXFAT_INVALID_PARAMETER      -3
#define EXFAT_NO_MEMORY              -4
#define EXFAT_DISK_FULL              -5
#define EXFAT_TOO_MANY_OPEN_FILES    -6

/// @struct ExFatBootSector
///
/// @brief exFAT boot sector structure
typedef struct __attribute__((packed)) ExFatBootSector {
  uint8_t   jumpBoot[3];           // Jump instruction
  uint8_t   fileSystemName[8];     // "EXFAT   "
  uint8_t   mustBeZero[53];        // Must be zero
  uint64_t  partitionOffset;       // Partition offset in sectors
  uint64_t  volumeLength;          // Volume length in sectors
  uint32_t  fatOffset;             // FAT offset in sectors
  uint32_t  fatLength;             // FAT length in sectors
  uint32_t  clusterHeapOffset;     // Cluster heap offset in sectors
  uint32_t  clusterCount;          // Number of clusters
  uint32_t  rootDirectoryCluster;  // Root directory first cluster
  uint32_t  volumeSerialNumber;    // Volume serial number
  uint16_t  fileSystemRevision;    // File system revision
  uint16_t  volumeFlags;           // Volume flags
  uint8_t   bytesPerSectorShift;   // Bytes per sector (2^n)
  uint8_t   sectorsPerClusterShift; // Sectors per cluster (2^n)
  uint8_t   numberOfFats;          // Number of FATs
  uint8_t   driveSelect;           // Drive select
  uint8_t   percentInUse;          // Percent in use
  uint8_t   reserved[7];           // Reserved
  uint8_t   bootCode[390];         // Boot code
  uint16_t  bootSignature;         // Boot signature (0xAA55)
} ExFatBootSector;

/// @struct ExFatFileDirectoryEntry
///
/// @brief exFAT file directory entry
typedef struct __attribute__((packed)) ExFatFileDirectoryEntry {
  uint8_t   entryType;             // Entry type (0x85)
  uint8_t   secondaryCount;        // Number of secondary entries
  uint16_t  setChecksum;           // Set checksum
  uint16_t  fileAttributes;        // File attributes
  uint16_t  reserved1;             // Reserved
  uint32_t  createTimestamp;       // Create timestamp
  uint32_t  lastModifiedTimestamp; // Last modified timestamp
  uint32_t  lastAccessedTimestamp; // Last accessed timestamp
  uint8_t   create10msIncrement;   // Create 10ms increment
  uint8_t   lastModified10msIncrement; // Last modified 10ms increment
  uint8_t   createUtcOffset;       // Create UTC offset
  uint8_t   lastModifiedUtcOffset; // Last modified UTC offset
  uint8_t   lastAccessedUtcOffset; // Last accessed UTC offset
  uint8_t   reserved2[7];          // Reserved
} ExFatFileDirectoryEntry;

/// @struct ExFatStreamExtensionEntry
///
/// @brief exFAT stream extension directory entry
typedef struct __attribute__((packed)) ExFatStreamExtensionEntry {
  uint8_t   entryType;             // Entry type (0xC0)
  uint8_t   generalSecondaryFlags; // General secondary flags
  uint8_t   reserved1;             // Reserved
  uint8_t   nameLength;            // Name length
  uint16_t  nameHash;              // Name hash
  uint16_t  reserved2;             // Reserved
  uint64_t  validDataLength;       // Valid data length
  uint32_t  reserved3;             // Reserved
  uint32_t  firstCluster;          // First cluster
  uint64_t  dataLength;            // Data length
} ExFatStreamExtensionEntry;

/// @struct ExFatFileNameEntry
///
/// @brief exFAT file name directory entry
typedef struct __attribute__((packed)) ExFatFileNameEntry {
  uint8_t   entryType;             // Entry type (0xC1)
  uint8_t   generalSecondaryFlags; // General secondary flags
  uint16_t  fileName[15];          // File name (UTF-16)
} ExFatFileNameEntry;

/// @struct ExFatFileHandle
///
/// @brief File handle for open exFAT files
typedef struct ExFatFileHandle {
  uint32_t  firstCluster;          // First cluster of file
  uint32_t  currentCluster;        // Current cluster
  uint32_t  currentPosition;       // Current position in file
  uint64_t  fileSize;              // File size in bytes
  uint16_t  attributes;            // File attributes
  char      fileName[EXFAT_MAX_FILENAME_LENGTH + 1]; // File name
  uint32_t  directoryCluster;      // Directory containing this file
  uint32_t  directoryOffset;       // Offset in directory
} ExFatFileHandle;

/// @struct ExFatDriverState
///
/// @brief Driver state for exFAT filesystem
typedef struct ExFatDriverState {
  FilesystemState*  filesystemState; // Pointer to filesystem state
  uint32_t          bytesPerSector; // Bytes per sector
  uint32_t          sectorsPerCluster; // Sectors per cluster
  uint32_t          bytesPerCluster; // Bytes per cluster
  uint32_t          fatStartSector; // FAT start sector
  uint32_t          clusterHeapStartSector; // Cluster heap start sector
  uint32_t          rootDirectoryCluster; // Root directory cluster
  uint32_t          clusterCount;          // Number of clusters
  bool              driverStateValid; // Whether or not this is a valid state
} ExFatDriverState;

/// @brief Read a sector from the storage device
///
/// @param driverState Pointer to driver state
/// @param sectorNumber Sector number to read
/// @param buffer Buffer to read into
///
/// @return EXFAT_SUCCESS on success, EXFAT_ERROR on failure
static int readSector(ExFatDriverState* driverState, uint32_t sectorNumber,
                      uint8_t* buffer) {
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
static int writeSector(ExFatDriverState* driverState, uint32_t sectorNumber,
                       const uint8_t* buffer) {
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

/// @brief Additional helper function to validate cluster numbers
///
/// @param driverState Pointer to driver state
/// @param clusterNumber Cluster number to validate
///
/// @return true if valid, false otherwise
static bool isValidCluster(ExFatDriverState* driverState, 
                          uint32_t clusterNumber) {
  if (driverState == NULL) {
    return false;
  }
  
  return (clusterNumber >= 2 && 
          clusterNumber < driverState->clusterCount + 2);
}

/// @brief Fixed cluster to sector conversion with validation
///
/// @param driverState Pointer to driver state
/// @param clusterNumber Cluster number
///
/// @return Sector number, or UINT32_MAX if invalid cluster
static uint32_t clusterToSector(ExFatDriverState* driverState, 
                                uint32_t clusterNumber) {
  if (!isValidCluster(driverState, clusterNumber)) {
    // Return an invalid sector number that will cause proper error handling
    return UINT32_MAX;
  }

  return driverState->clusterHeapStartSector + 
         ((clusterNumber - 2) * driverState->sectorsPerCluster);
}

/// @brief Read FAT entry for given cluster
///
/// @param driverState Pointer to driver state
/// @param clusterNumber Cluster number
/// @param nextCluster Pointer to store next cluster number
///
/// @return EXFAT_SUCCESS on success, EXFAT_ERROR on failure
static int readFatEntry(ExFatDriverState* driverState, 
                        uint32_t clusterNumber, uint32_t* nextCluster) {
  if (driverState == NULL || driverState->filesystemState == NULL ||
      nextCluster == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  uint32_t fatOffset = clusterNumber * 4; // 4 bytes per FAT entry
  uint32_t fatSector = driverState->fatStartSector + 
                       (fatOffset / driverState->bytesPerSector);
  uint32_t fatOffsetInSector = fatOffset % driverState->bytesPerSector;

  int result = readSector(driverState, fatSector, 
                          filesystemState->blockBuffer);
  if (result != EXFAT_SUCCESS) {
    return result;
  }

  readBytes(nextCluster, filesystemState->blockBuffer + fatOffsetInSector);
  return EXFAT_SUCCESS;
}

/// @brief Write FAT entry for given cluster
///
/// @param driverState Pointer to driver state
/// @param clusterNumber Cluster number
/// @param nextCluster Next cluster number to write
///
/// @return EXFAT_SUCCESS on success, EXFAT_ERROR on failure
static int writeFatEntry(ExFatDriverState* driverState, 
                         uint32_t clusterNumber, uint32_t nextCluster) {
  if (driverState == NULL || driverState->filesystemState == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  uint32_t fatOffset = clusterNumber * 4; // 4 bytes per FAT entry
  uint32_t fatSector = driverState->fatStartSector + 
                       (fatOffset / driverState->bytesPerSector);
  uint32_t fatOffsetInSector = fatOffset % driverState->bytesPerSector;

  int result = readSector(driverState, fatSector, 
                          filesystemState->blockBuffer);
  if (result != EXFAT_SUCCESS) {
    return result;
  }

  writeBytes(filesystemState->blockBuffer + fatOffsetInSector, &nextCluster);

  return writeSector(driverState, fatSector, filesystemState->blockBuffer);
}

/// @brief Find free cluster in FAT
///
/// @param driverState Pointer to driver state
/// @param freeCluster Pointer to store free cluster number
///
/// @return EXFAT_SUCCESS on success, EXFAT_ERROR on failure
static int findFreeCluster(ExFatDriverState* driverState, 
                           uint32_t* freeCluster) {
  if (driverState == NULL || freeCluster == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  for (uint32_t cluster = 2; cluster < driverState->clusterCount + 2;
       cluster++) {
    uint32_t nextCluster;
    int result = readFatEntry(driverState, cluster, &nextCluster);
    if (result != EXFAT_SUCCESS) {
      return result;
    }

    if (nextCluster == 0) {
      *freeCluster = cluster;
      return EXFAT_SUCCESS;
    }
  }

  return EXFAT_DISK_FULL;
}

/// @brief Calculate checksum for directory entry set
///
/// @param entries Pointer to directory entries
/// @param numEntries Number of entries
///
/// @return Calculated checksum
static uint16_t calculateDirectorySetChecksum(uint8_t* entries, 
                                              uint8_t numEntries) {
  uint16_t checksum = 0;
  uint32_t totalBytes = numEntries * EXFAT_DIRECTORY_ENTRY_SIZE;

  for (uint32_t ii = 0; ii < totalBytes; ii++) {
    if (ii == 2 || ii == 3) {
      // Skip checksum field in first entry
      continue;
    }
    checksum = ((checksum & 1) ? 0x8000 : 0) + (checksum >> 1) + entries[ii];
  }

  return checksum;
}

/// @brief Convert UTF-8 string to UTF-16
///
/// @param utf8 UTF-8 string
/// @param utf16 UTF-16 buffer
/// @param maxLength Maximum length of UTF-16 buffer
///
/// @return Length of converted string
static int utf8ToUtf16(const char* utf8, uint16_t* utf16, int maxLength) {
  int length = 0;
  const uint8_t* src = (const uint8_t*) utf8;

  while (*src && length < maxLength - 1) {
    if (*src < 0x80) {
      utf16[length++] = *src++;
    } else if ((*src & 0xE0) == 0xC0) {
      // 2-byte UTF-8 sequence
      if (*(src + 1)) {
        utf16[length++] = ((*src & 0x1F) << 6) | (*(src + 1) & 0x3F);
        src += 2;
      } else {
        break;
      }
    } else {
      // For simplicity, replace other sequences with '?'
      utf16[length++] = '?';
      src++;
    }
  }

  utf16[length] = 0;
  return length;
}

/// @brief Convert UTF-16 string to UTF-8
///
/// @param utf16 UTF-16 string
/// @param utf8 UTF-8 buffer
/// @param maxLength Maximum length of UTF-8 buffer
///
/// @return Length of converted string
static int utf16ToUtf8(const uint16_t* utf16, char* utf8, int maxLength) {
  int length = 0;
  
  while (*utf16 && length < maxLength - 1) {
    if (*utf16 < 0x80) {
      utf8[length++] = (char) *utf16;
    } else if (*utf16 < 0x800) {
      if (length + 1 < maxLength) {
        utf8[length++] = 0xC0 | (*utf16 >> 6);
        utf8[length++] = 0x80 | (*utf16 & 0x3F);
      } else {
        break;
      }
    } else {
      // For simplicity, replace with '?'
      utf8[length++] = '?';
    }
    utf16++;
  }

  utf8[length] = 0;
  return length;
}

/// @brief Find file in directory (corrected version)
///
/// @param driverState Pointer to driver state
/// @param directoryCluster Directory cluster to search
/// @param fileName File name to search for
/// @param fileHandle Pointer to file handle to populate
///
/// @return EXFAT_SUCCESS on success, EXFAT_FILE_NOT_FOUND if not found
static int findFileInDirectory(ExFatDriverState* driverState, 
                               uint32_t directoryCluster, 
                               const char* fileName,
                               ExFatFileHandle* fileHandle
) {
  int result = EXFAT_INVALID_PARAMETER;
  FilesystemState* filesystemState = driverState->filesystemState;
  uint32_t currentCluster = directoryCluster;
  uint8_t *blockBuffer = driverState->filesystemState->blockBuffer;
  uint16_t *fullName = NULL;
  char *utf8Name = NULL;

  if ((driverState == NULL) || (driverState->filesystemState == NULL) ||
      (fileName == NULL) || (fileHandle == NULL)
  ) {
    printDebug("Invalid parameter in findFileInDirectory.\n");
    goto exit; // return EXFAT_INVALID_PARAMETER;
  }

  // Validate starting cluster
  if ((directoryCluster < 2)
    || (directoryCluster >= driverState->clusterCount + 2)
  ) {
    printDebug("Invalid cluster size in findFileInDirectory.\n");
    goto exit; // return EXFAT_INVALID_PARAMETER;
  }

  fullName = (uint16_t*) malloc(
    (EXFAT_MAX_FILENAME_LENGTH + 1) * sizeof(uint16_t));
  if (fullName == NULL) {
    result = EXFAT_NO_MEMORY;
    goto exit;
  }
  utf8Name = (char*) malloc(EXFAT_MAX_FILENAME_LENGTH + 1);
  if (utf8Name == NULL) {
    result = EXFAT_NO_MEMORY;
    goto exit;
  }
  
  // Walk through the cluster chain
  while ((currentCluster != 0xFFFFFFFF)
    && (currentCluster >= 2)
    && (currentCluster < driverState->clusterCount + 2)
  ) {
    
    // Read all sectors in this cluster
    for (uint32_t sectorInCluster = 0; 
         sectorInCluster < driverState->sectorsPerCluster; 
         sectorInCluster++
    ) {
      
      uint32_t sectorNumber
        = clusterToSector(driverState, currentCluster)
        + sectorInCluster;
      
      result = readSector(driverState, sectorNumber, 
        filesystemState->blockBuffer);
      if (result != EXFAT_SUCCESS) {
        printDebug("Could not read sector ");
        printDebug(sectorNumber);
        printDebug(" when searching for \"");
        printDebug(fileName);
        printDebug("\" in findFileInDirectory.\n");
        printDebug("readSector returned status ");
        printDebug(result);
        printDebug("\n");
        goto exit; // return result;
      }

      // Process all directory entries in this sector
      for (uint32_t entryOffset = 0; 
           entryOffset < driverState->bytesPerSector; 
           entryOffset += EXFAT_DIRECTORY_ENTRY_SIZE
      ) {
        
        uint8_t* entryPtr = filesystemState->blockBuffer + entryOffset;
        uint8_t entryType = entryPtr[0];
        
        // Check for end of directory
        if (entryType == EXFAT_ENTRY_END_OF_DIR) {
          printDebug("entryType == EXFAT_ENTRY_END_OF_DIR\n");
          result = EXFAT_FILE_NOT_FOUND;
          goto exit;
        }

        // Skip unused entries
        if (entryType == EXFAT_ENTRY_UNUSED) {
          printDebug("entryType == EXFAT_ENTRY_UNUSED\n");
          continue;
        }

        // Process file entries
        if (entryType == EXFAT_ENTRY_FILE) {
          printDebug("entryType == EXFAT_ENTRY_FILE\n");
          ExFatFileDirectoryEntry fileEntry;
          ExFatStreamExtensionEntry streamEntry;
          
          // Read file entry with aligned access
          readBytes(&fileEntry, entryPtr);
          
          // Validate secondary count (must have at least stream extension)
          if (fileEntry.secondaryCount < 1) {
            continue;
          }

          // Read the stream extension entry (next entry)
          uint32_t streamEntryOffset = entryOffset + EXFAT_DIRECTORY_ENTRY_SIZE;
          uint8_t* streamEntryPtr = NULL;
          
          // Check if stream entry is in the same sector
          if (streamEntryOffset < driverState->bytesPerSector) {
            streamEntryPtr = filesystemState->blockBuffer + streamEntryOffset;
          } else {
            // Stream entry is in next sector - need to read it
            uint32_t nextSectorInCluster = sectorInCluster + 1;
            uint32_t nextSectorNumber;
            
            if (nextSectorInCluster < driverState->sectorsPerCluster) {
              nextSectorNumber = clusterToSector(driverState, currentCluster) + 
                                nextSectorInCluster;
            } else {
              // Need to get next cluster
              uint32_t nextCluster;
              result = readFatEntry(driverState, currentCluster, &nextCluster);
              if (result != EXFAT_SUCCESS || nextCluster == 0xFFFFFFFF) {
                continue; // Skip this malformed entry
              }
              nextSectorNumber = clusterToSector(driverState, nextCluster);
            }
            
            // Use a temporary buffer for the next sector
            result = readSector(driverState, nextSectorNumber, blockBuffer);
            if (result != EXFAT_SUCCESS) {
              continue;
            }
            streamEntryPtr
              = blockBuffer + (streamEntryOffset - driverState->bytesPerSector);
          }

          // Validate and read stream extension entry
          if (streamEntryPtr[0] != EXFAT_ENTRY_STREAM) {
            continue; // Invalid entry set
          }
          readBytes(&streamEntry, streamEntryPtr);

          // Extract filename from filename entries
          memset(fullName, 0,
            (EXFAT_MAX_FILENAME_LENGTH + 1) * sizeof(uint16_t));
          uint8_t nameCharIndex = 0;
          bool nameComplete = true;
          
          // Read filename entries (remaining secondary entries)
          uint8_t numNameEntries = fileEntry.secondaryCount - 1;
          for (uint8_t nameEntryIdx = 0; 
               nameEntryIdx < numNameEntries && nameComplete; 
               nameEntryIdx++
          ) {
            
            ExFatFileNameEntry nameEntry;
            uint32_t nameEntryOffset = entryOffset + 
              ((nameEntryIdx + 2) * EXFAT_DIRECTORY_ENTRY_SIZE);
            
            // Handle cross-sector/cross-cluster name entries
            // This is simplified - in a production system you'd need more
            // robust cross-boundary handling
            if (nameEntryOffset >= driverState->bytesPerSector) {
              nameComplete = false;
              break;
            }
            
            uint8_t* nameEntryPtr
              = filesystemState->blockBuffer + nameEntryOffset;
            if (nameEntryPtr[0] != EXFAT_ENTRY_FILENAME) {
              nameComplete = false;
              break;
            }
            
            readBytes(&nameEntry, nameEntryPtr);
            
            // Extract up to 15 characters from this name entry
            for (uint8_t charIdx = 0; 
                 charIdx < 15 && nameCharIndex < EXFAT_MAX_FILENAME_LENGTH; 
                 charIdx++
            ) {
              uint16_t nameChar;
              readBytes(&nameChar, &nameEntry.fileName[charIdx]);
              if (nameChar == 0) {
                nameComplete = true;
                break;
              }
              fullName[nameCharIndex++] = nameChar;
            }
          }

          if (nameComplete) {
            // Convert filename to UTF-8 and compare
            utf16ToUtf8(fullName, utf8Name, EXFAT_MAX_FILENAME_LENGTH + 1);
            
            if (strcmp(utf8Name, fileName) == 0) {
              // Found the file - populate file handle
              fileHandle->firstCluster = streamEntry.firstCluster;
              printDebug("fileHandle->firstCluster = ");
              printDebug(fileHandle->firstCluster);
              printDebug("\n");
              fileHandle->currentCluster = streamEntry.firstCluster;
              printDebug("fileHandle->currentCluster = ");
              printDebug(fileHandle->currentCluster);
              printDebug("\n");
              fileHandle->currentPosition = 0;
              fileHandle->fileSize = streamEntry.dataLength;
              printDebug("fileHandle->fileSize = ");
              printDebug(fileHandle->fileSize);
              printDebug("\n");
              fileHandle->attributes = fileEntry.fileAttributes;
              printDebug("fileHandle->attributes = ");
              printDebug(fileHandle->attributes);
              printDebug("\n");
              fileHandle->directoryCluster = directoryCluster;
              printDebug("fileHandle->directoryCluster = ");
              printDebug(fileHandle->directoryCluster);
              printDebug("\n");
              strncpy(fileHandle->fileName,
                fileName, EXFAT_MAX_FILENAME_LENGTH);
              fileHandle->fileName[EXFAT_MAX_FILENAME_LENGTH] = '\0';
              result = EXFAT_SUCCESS;
              goto exit;
            } else {
              printDebug("\"");
              printDebug(utf8Name);
              printDebug("\" != \"");
              printDebug(fileName);
              printDebug("\"\n");
            }
          }

          // Skip to the end of this entry set
          entryOffset
            += (fileEntry.secondaryCount * EXFAT_DIRECTORY_ENTRY_SIZE)
            - EXFAT_DIRECTORY_ENTRY_SIZE;
        } else {
          printDebug("entryType == 0x");
          printDebug(entryType, HEX);
          printDebug("\n");
        }
      }
    }

    // Move to next cluster in the chain
    result = readFatEntry(driverState, currentCluster, &currentCluster);
    if (result != EXFAT_SUCCESS) {
      printDebug("readFatEntry returned status ");
      printDebug(result);
      printDebug("\n");
      break;
    }
  }

  result = EXFAT_FILE_NOT_FOUND;

exit:
  free(utf8Name);
  free(fullName);
  return result;
}

/// @brief Get free file handle
///
/// @param driverState Pointer to driver state
///
/// @return Pointer to free file handle, NULL if none available
static ExFatFileHandle* getFreeFileHandle() {
  ExFatFileHandle *fileHandle
    = (ExFatFileHandle*) calloc(1, sizeof(ExFatFileHandle));

  return fileHandle;
}

/// @brief Release an ExFatFileHandle back to the pool of available file handles.
///
/// @param driverState Pointer to driver state.
/// @param fileHandle Pointer to the ExFatFileHandle to release.
///
/// @return This function always returns NULL.
static ExFatFileHandle* releaseFileHandle(ExFatFileHandle *fileHandle) {
  if (fileHandle == NULL) {
    return NULL;
  }

  free(fileHandle);

  return NULL;
}

/// @brief Get file handle from FILE pointer
///
/// @param driverState Pointer to driver state
/// @param stream FILE pointer
///
/// @return ExFatFileHandle pointer or NULL if invalid
static ExFatFileHandle* getFileHandle(FILE* stream) {
  if (stream == NULL) {
    return NULL;
  }

  return (ExFatFileHandle*) stream->file;
}

/// @brief Initialize exFAT driver
///
/// @param driverState Pointer to driver state
/// @param filesystemState Pointer to filesystem state
///
/// @return EXFAT_SUCCESS on success, EXFAT_ERROR on failure
int exFatInitialize(ExFatDriverState* driverState, 
  FilesystemState* filesystemState
) {
  if (driverState == NULL || filesystemState == NULL) {
    printString("ERROR: Invalid parameter passed to exFatInitialize.\n");
    return EXFAT_INVALID_PARAMETER;
  }

  // Initialize driver state
  memset(driverState, 0, sizeof(ExFatDriverState));
  driverState->filesystemState = filesystemState;

  // Read boot sector
  int result = readSector(driverState, 0, filesystemState->blockBuffer);
  if (result != EXFAT_SUCCESS) {
    printString("ERROR: Could not read boot sector.\n");
    return result;
  }

  ExFatBootSector *bootSector = (ExFatBootSector*) filesystemState->blockBuffer;

  // Validate exFAT signature
  if (memcmp(bootSector->fileSystemName, "EXFAT   ", 8) != 0) {
    printString("ERROR: Not an exFAT filesystem.\n");
    printDebug("bootSector->fileSystemName = '");
    for (int ii = 0; ii < 8; ii++) {
      printDebug((char) bootSector->fileSystemName[ii]);
    }
    printDebug("'\n");
    printDebug("Boot signature = 0x");
    printDebug(bootSector->bootSignature, HEX);
    printDebug("\n");
    return EXFAT_ERROR;
#ifdef NANO_OS_DEBUG
  } else {
    printDebug("Filesystem *IS* exFAT.\n");
#endif // NANO_OS_DEBUG
  }

  // Calculate filesystem parameters
  driverState->bytesPerSector = 
    1 << bootSector->bytesPerSectorShift;
  driverState->sectorsPerCluster = 
    1 << bootSector->sectorsPerClusterShift;
  driverState->bytesPerCluster = 
    driverState->bytesPerSector * driverState->sectorsPerCluster;
  driverState->fatStartSector = bootSector->fatOffset;
  driverState->clusterHeapStartSector = 
    bootSector->clusterHeapOffset;
  driverState->rootDirectoryCluster = 
    bootSector->rootDirectoryCluster;
  driverState->clusterCount = 
    bootSector->clusterCount;

  driverState->driverStateValid = true;
  return EXFAT_SUCCESS;
}

/// @brief Seek to a position in a file
///
/// @param driverState Pointer to driver state
/// @param fileHandle File handle to seek in
/// @param offset Offset to seek to
/// @param whence Reference point for offset (SEEK_SET, SEEK_CUR, SEEK_END)
///
/// @return 0 on success, -1 on failure
int exFatSeek(ExFatDriverState* driverState, ExFatFileHandle* fileHandle,
  long offset, int whence
) {
  if ((driverState == NULL) || (fileHandle == NULL)) {
    return -1;
  }

  uint64_t newPosition;

  switch (whence) {
    case SEEK_SET:
      newPosition = offset;
      break;
    case SEEK_CUR:
      newPosition = fileHandle->currentPosition + offset;
      break;
    case SEEK_END:
      newPosition = fileHandle->fileSize + offset;
      break;
    default:
      return -1;
  }

  // Check bounds
  if (newPosition > fileHandle->fileSize) {
    return -1;
  }

  // Calculate which cluster the new position is in
  uint32_t targetCluster
    = (uint32_t)(newPosition / driverState->bytesPerCluster);
  uint32_t currentClusterIndex
    = (uint32_t)(fileHandle->currentPosition / driverState->bytesPerCluster);

  // Navigate to the target cluster
  if (targetCluster != currentClusterIndex) {
    fileHandle->currentCluster = fileHandle->firstCluster;
    
    for (uint32_t ii = 0; ii < targetCluster; ii++) {
      uint32_t nextCluster;
      int result = readFatEntry(driverState, fileHandle->currentCluster, 
                                &nextCluster);
      if (result != EXFAT_SUCCESS || nextCluster == 0xFFFFFFFF) {
        return -1;
      }
      fileHandle->currentCluster = nextCluster;
    }
  }

  fileHandle->currentPosition = (uint32_t) newPosition;
  return 0;
}

/// @brief Fixed updateDirectoryEntry with cross-sector support
///
/// @param driverState Pointer to driver state
/// @param fileHandle File handle containing current file information
///
/// @return EXFAT_SUCCESS on success, error code on failure
static int updateDirectoryEntry(ExFatDriverState* driverState, 
  ExFatFileHandle* fileHandle
) {
  int result = EXFAT_SUCCESS;
  FilesystemState* filesystemState = driverState->filesystemState;
  uint32_t directoryCluster = fileHandle->directoryCluster;
  uint32_t currentCluster = directoryCluster;
  bool entryFound = false;
  uint16_t *utf16Name = NULL;
  char *utf8Name = NULL;
  uint32_t fileEntrySector = 0;
  uint32_t fileEntryOffset = 0;
  uint32_t streamEntrySector = 0;
  uint32_t streamEntryOffset = 0;
  ExFatFileDirectoryEntry fileEntry;
  ExFatStreamExtensionEntry streamEntry;
  uint16_t newChecksum = 0;
  uint8_t *entrySetBuffer = NULL;
  uint8_t *tempBuffer = NULL;

  printDebug("updateDirectoryEntry: Updating file \"");
  printDebug(fileHandle->fileName);
  printDebug("\" with size ");
  printDebug(fileHandle->fileSize);
  printDebug("\n");

  if ((driverState == NULL) || (driverState->filesystemState == NULL) ||
      (fileHandle == NULL)
  ) {
    printDebug("updateDirectoryEntry: Invalid parameter\n");
    return EXFAT_INVALID_PARAMETER;
  }

  // Convert filename to UTF-16 for comparison
  utf16Name = (uint16_t*) malloc(
    (EXFAT_MAX_FILENAME_LENGTH + 1) * sizeof(uint16_t));
  if (utf16Name == NULL) {
    printDebug("updateDirectoryEntry: No memory for utf16Name\n");
    return EXFAT_NO_MEMORY;
  }
  utf8Name = (char*) malloc(EXFAT_MAX_FILENAME_LENGTH + 1);
  if (utf8Name == NULL) {
    free(utf16Name);
    printDebug("updateDirectoryEntry: No memory for utf8Name\n");
    return EXFAT_NO_MEMORY;
  }

  // Allocate a temporary buffer for reading next sectors
  tempBuffer = (uint8_t*) malloc(driverState->bytesPerSector);
  if (tempBuffer == NULL) {
    free(utf16Name);
    free(utf8Name);
    return EXFAT_NO_MEMORY;
  }

  // Search for the file entry in the directory
  while ((currentCluster != 0xFFFFFFFF) && (currentCluster >= 2) &&
         (currentCluster < driverState->clusterCount + 2) && !entryFound
  ) {
    printDebug("updateDirectoryEntry: Searching cluster ");
    printDebug(currentCluster);
    printDebug("\n");
    
    for (uint32_t sectorInCluster = 0; 
         sectorInCluster < driverState->sectorsPerCluster && !entryFound; 
         sectorInCluster++
    ) {
      uint32_t sectorNumber = clusterToSector(driverState, currentCluster) + 
                              sectorInCluster;
      
      if (sectorNumber == UINT32_MAX) {
        printDebug("updateDirectoryEntry: Invalid cluster number\n");
        result = EXFAT_ERROR;
        goto cleanup;
      }
      
      result = readSector(driverState, sectorNumber, 
                          filesystemState->blockBuffer);
      if (result != EXFAT_SUCCESS) {
        printDebug("updateDirectoryEntry: Failed to read sector ");
        printDebug(sectorNumber);
        printDebug("\n");
        goto cleanup;
      }

      for (uint32_t entryOffset = 0; 
           entryOffset < driverState->bytesPerSector && !entryFound; 
           entryOffset += EXFAT_DIRECTORY_ENTRY_SIZE
      ) {
        uint8_t* entryPtr = filesystemState->blockBuffer + entryOffset;
        uint8_t entryType = entryPtr[0];
        
        // Check for end of directory
        if (entryType == EXFAT_ENTRY_END_OF_DIR) {
          printDebug("updateDirectoryEntry: Reached end of directory\n");
          result = EXFAT_FILE_NOT_FOUND;
          goto cleanup;
        }
        
        if (entryType == EXFAT_ENTRY_FILE) {
          ExFatFileDirectoryEntry tempFileEntry;
          readBytes(&tempFileEntry, entryPtr);
          
          // Handle stream extension that might be in next sector
          uint32_t nextEntryOffset = entryOffset + EXFAT_DIRECTORY_ENTRY_SIZE;
          uint8_t* streamPtr = NULL;
          uint32_t nextSectorNumber = sectorNumber;
          uint32_t nextCluster = currentCluster;
          uint32_t nextSectorInCluster = sectorInCluster;
          bool usingTempBuffer = false;
          
          if (nextEntryOffset >= driverState->bytesPerSector) {
            // Stream entry is in next sector
            nextSectorInCluster++;
            if (nextSectorInCluster >= driverState->sectorsPerCluster) {
              // Need to move to next cluster
              result = readFatEntry(driverState, currentCluster, &nextCluster);
              if (result != EXFAT_SUCCESS || nextCluster == 0xFFFFFFFF) {
                // Invalid directory structure
                continue;
              }
              nextSectorInCluster = 0;
            }
            
            nextSectorNumber = clusterToSector(driverState, nextCluster) + 
                              nextSectorInCluster;
            if (nextSectorNumber == UINT32_MAX) {
              continue;
            }
            
            result = readSector(driverState, nextSectorNumber, tempBuffer);
            if (result != EXFAT_SUCCESS) {
              continue;
            }
            streamPtr = tempBuffer;
            nextEntryOffset = 0;
            usingTempBuffer = true;
          } else {
            streamPtr = filesystemState->blockBuffer + nextEntryOffset;
          }
          
          if (streamPtr[0] != EXFAT_ENTRY_STREAM) {
            printDebug("updateDirectoryEntry: No stream extension found\n");
            continue;
          }
          
          // Extract filename from filename entries to compare
          memset(utf16Name, 0, 
            (EXFAT_MAX_FILENAME_LENGTH + 1) * sizeof(uint16_t));
          uint8_t nameCharIndex = 0;
          bool nameComplete = true;
          
          // Read filename entries (simplified - still assumes they're in same sector)
          uint8_t numNameEntries = tempFileEntry.secondaryCount - 1;
          for (uint8_t nameEntryIdx = 0; 
               nameEntryIdx < numNameEntries && nameComplete; 
               nameEntryIdx++
          ) {
            uint32_t nameEntryOffset = entryOffset + 
              ((nameEntryIdx + 2) * EXFAT_DIRECTORY_ENTRY_SIZE);
            
            // For simplicity, skip if name entries span sectors
            if (nameEntryOffset >= driverState->bytesPerSector) {
              nameComplete = false;
              break;
            }
            
            uint8_t* nameEntryPtr = 
              filesystemState->blockBuffer + nameEntryOffset;
            if (nameEntryPtr[0] != EXFAT_ENTRY_FILENAME) {
              nameComplete = false;
              break;
            }
            
            ExFatFileNameEntry nameEntry;
            readBytes(&nameEntry, nameEntryPtr);
            
            for (uint8_t charIdx = 0; 
                 charIdx < 15 && nameCharIndex < EXFAT_MAX_FILENAME_LENGTH; 
                 charIdx++
            ) {
              uint16_t nameChar;
              readBytes(&nameChar, &nameEntry.fileName[charIdx]);
              if (nameChar == 0) {
                nameComplete = true;
                break;
              }
              utf16Name[nameCharIndex++] = nameChar;
            }
          }

          if (nameComplete) {
            utf16ToUtf8(utf16Name, utf8Name, EXFAT_MAX_FILENAME_LENGTH + 1);
            
            printDebug("updateDirectoryEntry: Comparing \"");
            printDebug(utf8Name);
            printDebug("\" with \"");
            printDebug(fileHandle->fileName);
            printDebug("\"\n");
            
            if (strcmp(utf8Name, fileHandle->fileName) == 0) {
              // Found our file entry!
              printDebug("updateDirectoryEntry: Found matching entry!\n");
              entryFound = true;
              fileEntrySector = sectorNumber;
              fileEntryOffset = entryOffset;
              
              if (usingTempBuffer) {
                streamEntrySector = nextSectorNumber;
                streamEntryOffset = 0;
              } else {
                streamEntrySector = sectorNumber;
                streamEntryOffset = nextEntryOffset;
              }
              
              // Read the entries with aligned access
              readBytes(&fileEntry, entryPtr);
              readBytes(&streamEntry, streamPtr);
              break;
            }
          }

          // Skip to the end of this entry set
          entryOffset += 
            (tempFileEntry.secondaryCount * EXFAT_DIRECTORY_ENTRY_SIZE) - 
            EXFAT_DIRECTORY_ENTRY_SIZE;
        }
      }
    }
    
    if (!entryFound) {
      result = readFatEntry(driverState, currentCluster, &currentCluster);
      if (result != EXFAT_SUCCESS) {
        printDebug("updateDirectoryEntry: Failed to read FAT entry\n");
        goto cleanup;
      }
    }
  }

  if (!entryFound) {
    printDebug("updateDirectoryEntry: File entry not found\n");
    result = EXFAT_FILE_NOT_FOUND;
    goto cleanup;
  }

  printDebug("updateDirectoryEntry: Updating stream entry\n");
  printDebug("  Old dataLength: ");
  printDebug(streamEntry.dataLength);
  printDebug("\n");
  printDebug("  New dataLength: ");
  printDebug(fileHandle->fileSize);
  printDebug("\n");
  printDebug("  Old firstCluster: ");
  printDebug(streamEntry.firstCluster);
  printDebug("\n");
  printDebug("  New firstCluster: ");
  printDebug(fileHandle->firstCluster);
  printDebug("\n");

  // Update stream extension entry
  streamEntry.firstCluster = fileHandle->firstCluster;
  streamEntry.dataLength = fileHandle->fileSize;
  streamEntry.validDataLength = fileHandle->fileSize;

  // Calculate new checksum for the entire entry set
  entrySetBuffer = (uint8_t*) malloc(
    (fileEntry.secondaryCount + 1) * EXFAT_DIRECTORY_ENTRY_SIZE);
  if (entrySetBuffer == NULL) {
    printDebug("updateDirectoryEntry: No memory for entry set buffer\n");
    result = EXFAT_NO_MEMORY;
    goto cleanup;
  }

  // Build the complete entry set for checksum calculation
  memcpy(entrySetBuffer, &fileEntry, sizeof(fileEntry));
  memcpy(entrySetBuffer + EXFAT_DIRECTORY_ENTRY_SIZE, &streamEntry, 
         sizeof(streamEntry));
  
  // Copy filename entries (simplified - assumes they're in the file entry sector)
  if (fileEntrySector == streamEntrySector) {
    for (uint8_t ii = 2; ii <= fileEntry.secondaryCount; ii++) {
      uint32_t sourceOffset = fileEntryOffset + 
        (ii * EXFAT_DIRECTORY_ENTRY_SIZE);
      if (sourceOffset < driverState->bytesPerSector) {
        memcpy(entrySetBuffer + (ii * EXFAT_DIRECTORY_ENTRY_SIZE),
               filesystemState->blockBuffer + sourceOffset,
               EXFAT_DIRECTORY_ENTRY_SIZE);
      }
    }
  }

  // Calculate new checksum
  newChecksum = calculateDirectorySetChecksum(entrySetBuffer, 
                                              fileEntry.secondaryCount + 1);
  
  printDebug("updateDirectoryEntry: New checksum: 0x");
  printDebug(newChecksum, HEX);
  printDebug("\n");

  // Update the file entry with new checksum
  fileEntry.setChecksum = newChecksum;
  
  // Write back the file entry
  result = readSector(driverState, fileEntrySector, 
                      filesystemState->blockBuffer);
  if (result != EXFAT_SUCCESS) {
    goto cleanup;
  }
  
  writeBytes(filesystemState->blockBuffer + fileEntryOffset, &fileEntry);
  
  result = writeSector(driverState, fileEntrySector, 
                       filesystemState->blockBuffer);
  if (result != EXFAT_SUCCESS) {
    printDebug("updateDirectoryEntry: Failed to write file entry sector\n");
    goto cleanup;
  }
  
  // Write back the stream entry (might be in different sector)
  if (streamEntrySector != fileEntrySector) {
    result = readSector(driverState, streamEntrySector, 
                        filesystemState->blockBuffer);
    if (result != EXFAT_SUCCESS) {
      goto cleanup;
    }
  }
  
  writeBytes(filesystemState->blockBuffer + streamEntryOffset, &streamEntry);
  
  result = writeSector(driverState, streamEntrySector, 
                       filesystemState->blockBuffer);
  if (result != EXFAT_SUCCESS) {
    printDebug("updateDirectoryEntry: Failed to write stream entry sector\n");
  } else {
    printDebug("updateDirectoryEntry: Successfully updated directory entry\n");
  }

cleanup:
  if (tempBuffer != NULL) {
    free(tempBuffer);
  }
  if (entrySetBuffer != NULL) {
    free(entrySetBuffer);
  }
  if (utf16Name != NULL) {
    free(utf16Name);
  }
  if (utf8Name != NULL) {
    free(utf8Name);
  }
  return result;
}

/// @brief Enhanced exFatWrite with proper cluster validation
///
/// @param driverState Pointer to driver state
/// @param ptr Buffer containing data to write
/// @param totalBytes The total number of bytes to write
/// @param fileHandle A pointer to the ExFatFileHandle to write to
///
/// @return Number of bytes written on success, -1 on failure.
int32_t exFatWrite(ExFatDriverState* driverState, const void* ptr, 
  uint32_t totalBytes, ExFatFileHandle *fileHandle
) {
  if ((driverState == NULL) || (ptr == NULL) || 
      (fileHandle == NULL) || (totalBytes == 0)
  ) {
    return -1;
  }

  uint32_t bytesWritten = 0;
  const uint8_t* buffer = (const uint8_t*) ptr;
  bool needDirectoryUpdate = false;
  uint32_t originalFirstCluster = fileHandle->firstCluster;

  while (bytesWritten < totalBytes) {
    // Check if we need to allocate a cluster
    if (fileHandle->currentCluster == 0) {
      // Need to allocate first cluster or seek brought us to unallocated area
      uint32_t newCluster;
      int result = findFreeCluster(driverState, &newCluster);
      if (result != EXFAT_SUCCESS) {
        break;
      }

      if (fileHandle->firstCluster == 0) {
        // This is the first cluster for the file
        fileHandle->firstCluster = newCluster;
        fileHandle->currentCluster = newCluster;
        needDirectoryUpdate = true;
      } else {
        // Need to find the previous cluster and link it
        // This is complex - for now just set it as current
        fileHandle->currentCluster = newCluster;
      }

      result = writeFatEntry(driverState, newCluster, 0xFFFFFFFF);
      if (result != EXFAT_SUCCESS) {
        break;
      }
    }

    uint32_t clusterOffset = fileHandle->currentPosition % 
                             driverState->bytesPerCluster;
    uint32_t sectorInCluster = clusterOffset / driverState->bytesPerSector;
    uint32_t offsetInSector = clusterOffset % driverState->bytesPerSector;
    
    // Check if we're at a cluster boundary and need next cluster
    if (clusterOffset == 0 && fileHandle->currentPosition > 0) {
      uint32_t nextCluster;
      int result = readFatEntry(driverState, fileHandle->currentCluster, 
                                &nextCluster);
      if (result == EXFAT_SUCCESS && nextCluster != 0xFFFFFFFF) {
        fileHandle->currentCluster = nextCluster;
      } else {
        // Need to allocate and link a new cluster
        uint32_t newCluster;
        result = findFreeCluster(driverState, &newCluster);
        if (result != EXFAT_SUCCESS) {
          break;
        }

        result = writeFatEntry(driverState, fileHandle->currentCluster, 
                               newCluster);
        if (result != EXFAT_SUCCESS) {
          break;
        }

        result = writeFatEntry(driverState, newCluster, 0xFFFFFFFF);
        if (result != EXFAT_SUCCESS) {
          break;
        }

        fileHandle->currentCluster = newCluster;
      }
    }

    // FIX: Get base sector and check for validity BEFORE adding offset
    uint32_t baseSectorNumber = clusterToSector(driverState, 
                                                fileHandle->currentCluster);
    
    // Check for invalid sector number
    if (baseSectorNumber == UINT32_MAX) {
      printDebug("exFatWrite: Invalid cluster ");
      printDebug(fileHandle->currentCluster);
      printDebug("\n");
      break;
    }
    
    uint32_t sectorNumber = baseSectorNumber + sectorInCluster;
    
    // Read sector if we're not writing the entire sector
    if (offsetInSector != 0 || 
        (totalBytes - bytesWritten) < driverState->bytesPerSector) {
      int result = readSector(driverState, sectorNumber, 
                              driverState->filesystemState->blockBuffer);
      if (result != EXFAT_SUCCESS) {
        break;
      }
    }

    size_t bytesInThisSector = driverState->bytesPerSector - offsetInSector;
    size_t bytesToWrite = (totalBytes - bytesWritten < bytesInThisSector) ?
                          totalBytes - bytesWritten : bytesInThisSector;

    memcpy(driverState->filesystemState->blockBuffer + offsetInSector, 
           buffer + bytesWritten, bytesToWrite);
    
    int result = writeSector(driverState, sectorNumber, 
                             driverState->filesystemState->blockBuffer);
    if (result != EXFAT_SUCCESS) {
      break;
    }

    bytesWritten += bytesToWrite;
    fileHandle->currentPosition += bytesToWrite;

    // Update file size if we've extended the file
    if (fileHandle->currentPosition > fileHandle->fileSize) {
      fileHandle->fileSize = fileHandle->currentPosition;
      needDirectoryUpdate = true;
    }

    // Check if we need to move to next cluster
    if ((fileHandle->currentPosition % driverState->bytesPerCluster) == 0 &&
        bytesWritten < totalBytes) {
      uint32_t nextCluster;
      result = readFatEntry(driverState, fileHandle->currentCluster, 
                            &nextCluster);
      if (result != EXFAT_SUCCESS) {
        break;
      }
      
      if (nextCluster == 0xFFFFFFFF) {
        // Need to allocate another cluster
        uint32_t newCluster;
        result = findFreeCluster(driverState, &newCluster);
        if (result != EXFAT_SUCCESS) {
          break;
        }

        result = writeFatEntry(driverState, fileHandle->currentCluster, 
                               newCluster);
        if (result != EXFAT_SUCCESS) {
          break;
        }

        result = writeFatEntry(driverState, newCluster, 0xFFFFFFFF);
        if (result != EXFAT_SUCCESS) {
          break;
        }

        fileHandle->currentCluster = newCluster;
      } else {
        fileHandle->currentCluster = nextCluster;
      }
    }
  }

  // Update directory entry if file size or first cluster changed
  if (needDirectoryUpdate || 
      originalFirstCluster != fileHandle->firstCluster) {
    int result = updateDirectoryEntry(driverState, fileHandle);
    if (result != EXFAT_SUCCESS) {
      printDebug("Warning: Failed to update directory entry after write\n");
      printDebug("updateDirectoryEntry returned ");
      printDebug(result);
      printDebug("\n");
    }
  }

  return bytesWritten;
}

/// @brief Read data from a file
///
/// @param driverState Pointer to driver state
/// @param ptr Buffer to read data into
/// @param totalBytes The total number of bytes to read
/// @param fileHandle A pointer to the ExFatFileHandle to read from
///
/// @return Number of bytes read on success, -1 on failure.
int32_t exFatRead(ExFatDriverState* driverState, void* ptr, uint32_t totalBytes,
  ExFatFileHandle* fileHandle
) {
  if ((driverState == NULL) || (ptr == NULL)
    || (fileHandle == NULL) || (totalBytes == 0)
  ) {
    printDebug("Invalid parameter in exFatRead.\n");
    return -1;
  }

  uint32_t bytesRead = 0;
  uint8_t* buffer = (uint8_t*) ptr;

  printDebug("Reading ");
  printDebug(totalBytes);
  printDebug(" bytes from \"");
  printDebug(fileHandle->fileName);
  printDebug("\".\n");
  printDebug("fileHandle->currentPosition = ");
  printDebug(fileHandle->currentPosition);
  printDebug("\n");
  printDebug("fileHandle->fileSize = ");
  printDebug(fileHandle->fileSize);
  printDebug("\n");
  
  // FIX: Check if file has no allocated clusters
  if (fileHandle->currentCluster == 0) {
    printDebug("exFatRead: File has no allocated clusters\n");
    return 0;  // EOF - no data to read
  }
  
  while (bytesRead < totalBytes && 
         fileHandle->currentPosition < fileHandle->fileSize
  ) {
    uint32_t clusterOffset = fileHandle->currentPosition % 
                             driverState->bytesPerCluster;
    uint32_t sectorInCluster = clusterOffset / driverState->bytesPerSector;
    uint32_t offsetInSector = clusterOffset % driverState->bytesPerSector;
    
    // FIX: Get base sector and check for validity BEFORE adding offset
    uint32_t baseSectorNumber = clusterToSector(driverState, 
                                                fileHandle->currentCluster);
    
    if (baseSectorNumber == UINT32_MAX) {
      printDebug("exFatRead: Invalid cluster ");
      printDebug(fileHandle->currentCluster);
      printDebug("\n");
      break;
    }
    
    uint32_t sectorNumber = baseSectorNumber + sectorInCluster;
    
    int result = readSector(driverState, sectorNumber, 
                            driverState->filesystemState->blockBuffer);
    if (result != EXFAT_SUCCESS) {
      printDebug("readSector returned ");
      printDebug(result);
      printDebug(" in exFatRead.\n");
      break;
    }

    size_t bytesInThisSector = driverState->bytesPerSector - offsetInSector;
    size_t bytesToRead = (totalBytes - bytesRead < bytesInThisSector) ?
                         totalBytes - bytesRead : bytesInThisSector;
    
    // Don't read beyond file size
    if (fileHandle->currentPosition + bytesToRead > fileHandle->fileSize) {
      bytesToRead = fileHandle->fileSize - fileHandle->currentPosition;
    }

    memcpy(buffer + bytesRead, 
           driverState->filesystemState->blockBuffer + offsetInSector, 
           bytesToRead);
    
    bytesRead += bytesToRead;
    fileHandle->currentPosition += bytesToRead;

    // Check if we need to move to next cluster
    if ((fileHandle->currentPosition % driverState->bytesPerCluster) == 0 &&
        fileHandle->currentPosition < fileHandle->fileSize
    ) {
      uint32_t nextCluster;
      result = readFatEntry(driverState, fileHandle->currentCluster, 
                            &nextCluster);
      if (result != EXFAT_SUCCESS || nextCluster == 0xFFFFFFFF) {
        printDebug("readFatEntry returned ");
        printDebug(result);
        printDebug(" in exFatRead.\n");
        break;
      }
      fileHandle->currentCluster = nextCluster;
    }
  }

  return bytesRead;
}

/// @brief Enhanced file close function with directory entry updates
///
/// @param driverState Pointer to driver state
/// @param fileHandle File handle to close
///
/// @return 0 on success, EOF on failure
int exFatFclose(ExFatDriverState* driverState, ExFatFileHandle* fileHandle) {
  if ((driverState == NULL) || (fileHandle == NULL)) {
    return EOF;
  }

  // Update directory entry one final time to ensure consistency
  int result = updateDirectoryEntry(driverState, fileHandle);
  if (result != EXFAT_SUCCESS) {
    printDebug("Warning: Failed to update directory entry on close\n");
      printDebug("updateDirectoryEntry returned ");
      printDebug(result);
      printDebug("\n");
    // Continue with close anyway
  }

  fileHandle = releaseFileHandle(fileHandle);
  
  // Decrement open file count
  if (driverState->filesystemState->numOpenFiles > 0) {
    driverState->filesystemState->numOpenFiles--;
  }
  
  // Free block buffer if no files are open
  if (driverState->filesystemState->numOpenFiles == 0) {
    if (driverState->filesystemState->blockBuffer != NULL) {
      free(driverState->filesystemState->blockBuffer);
      driverState->filesystemState->blockBuffer = NULL;
    }
  }
  
  return 0;
}

/// @brief Create a file in a directory with proper cluster allocation
///
/// @param driverState Pointer to driver state
/// @param directoryCluster Directory cluster to create file in
/// @param fileName File name to create
/// @param fileHandle Pointer to file handle to populate
///
/// @return EXFAT_SUCCESS on success, error code on failure
static int createFileInDirectory(ExFatDriverState* driverState, 
  uint32_t directoryCluster, const char* fileName, ExFatFileHandle* fileHandle
) {
  int result = EXFAT_SUCCESS;
  FilesystemState* filesystemState = driverState->filesystemState;
  uint32_t currentCluster = directoryCluster;
  uint32_t firstFreeEntryCluster = 0;
  uint32_t firstFreeEntrySector = 0;
  uint32_t firstFreeEntryOffset = 0;
  bool foundFreeSpace = false;
  uint8_t entriesNeeded = 2; // File entry + Stream extension entry
  ExFatFileDirectoryEntry fileEntry;
  ExFatStreamExtensionEntry streamEntry;
  uint16_t nameHash = 0;
  uint8_t filenameEntriesNeeded = 0;
  uint16_t checksum = 0;
  uint16_t *utf16Name = NULL;
  int nameLength = 0;
  uint8_t *entryBuffer = NULL;
  uint8_t nameEntryIndex = 0;
  uint32_t currentWriteCluster = 0;
  uint32_t currentWriteSector = 0;
  uint32_t currentWriteOffset = 0;
  uint32_t bufferOffset = 0;
  uint8_t *zeroBuffer = NULL;
  
  if ((driverState == NULL) || (driverState->filesystemState == NULL) ||
      (fileName == NULL) || (fileHandle == NULL)
  ) {
    return EXFAT_INVALID_PARAMETER;
  }

  zeroBuffer = (uint8_t*) calloc(1, driverState->bytesPerSector);
  if (zeroBuffer == NULL) {
    return EXFAT_NO_MEMORY;
  }

  // Convert filename to UTF-16 and calculate entries needed
  utf16Name = (uint16_t*) malloc(
    (EXFAT_MAX_FILENAME_LENGTH + 1) * sizeof(uint16_t));
  if (utf16Name == NULL) {
    free(zeroBuffer);
    return EXFAT_NO_MEMORY;
  }

  nameLength = utf8ToUtf16(fileName, utf16Name, 
                           EXFAT_MAX_FILENAME_LENGTH + 1);
  filenameEntriesNeeded = (nameLength + 14) / 15; // Round up division
  entriesNeeded += filenameEntriesNeeded;

  // Find free space in directory for all entries
  while ((currentCluster != 0xFFFFFFFF) && (currentCluster >= 2) &&
         (currentCluster < driverState->clusterCount + 2)
  ) {
    for (uint32_t sectorInCluster = 0; 
         sectorInCluster < driverState->sectorsPerCluster; 
         sectorInCluster++
    ) {
      uint32_t sectorNumber = clusterToSector(driverState, currentCluster) + 
                              sectorInCluster;
      
      result = readSector(driverState, sectorNumber, 
                          filesystemState->blockBuffer);
      if (result != EXFAT_SUCCESS) {
        goto cleanup;
      }

      // Look for consecutive free entries
      uint8_t consecutiveFree = 0;
      for (uint32_t entryOffset = 0; 
           entryOffset < driverState->bytesPerSector; 
           entryOffset += EXFAT_DIRECTORY_ENTRY_SIZE
      ) {
        uint8_t* entryPtr = filesystemState->blockBuffer + entryOffset;
        uint8_t entryType = entryPtr[0];
        
        if (entryType == EXFAT_ENTRY_UNUSED || 
            entryType == EXFAT_ENTRY_END_OF_DIR) {
          if (consecutiveFree == 0) {
            // Mark the start of free space
            firstFreeEntryCluster = currentCluster;
            firstFreeEntrySector = sectorInCluster;
            firstFreeEntryOffset = entryOffset;
          }
          consecutiveFree++;
          
          if (consecutiveFree >= entriesNeeded) {
            foundFreeSpace = true;
            break;
          }
        } else {
          consecutiveFree = 0;
        }
      }
      
      if (foundFreeSpace) {
        break;
      }
    }
    
    if (foundFreeSpace) {
      break;
    }

    // Move to next cluster
    uint32_t nextCluster;
    result = readFatEntry(driverState, currentCluster, &nextCluster);
    if (result != EXFAT_SUCCESS) {
      goto cleanup;
    }
    
    // Check if we're at the end of the directory chain
    if (nextCluster == 0xFFFFFFFF) {
      // Need to allocate a new cluster for the directory
      uint32_t newCluster;
      result = findFreeCluster(driverState, &newCluster);
      if (result != EXFAT_SUCCESS) {
        // No free clusters available
        result = EXFAT_DISK_FULL;
        goto cleanup;
      }
      
      // Link the new cluster to the directory chain
      result = writeFatEntry(driverState, currentCluster, newCluster);
      if (result != EXFAT_SUCCESS) {
        goto cleanup;
      }
      
      // Mark the new cluster as end of chain
      result = writeFatEntry(driverState, newCluster, 0xFFFFFFFF);
      if (result != EXFAT_SUCCESS) {
        goto cleanup;
      }
      
      // Write zeros to all sectors in the cluster
      for (uint32_t ii = 0; ii < driverState->sectorsPerCluster; ii++) {
        uint32_t sectorNum = clusterToSector(driverState, newCluster) + ii;
        result = writeSector(driverState, sectorNum, zeroBuffer);
        if (result != EXFAT_SUCCESS) {
          goto cleanup;
        }
      }
      
      currentCluster = newCluster;
    } else {
      currentCluster = nextCluster;
    }
  }

  if (!foundFreeSpace) {
    result = EXFAT_DISK_FULL;
    goto cleanup;
  }

  // Allocate buffer for all directory entries
  entryBuffer = (uint8_t*) calloc(1, entriesNeeded * EXFAT_DIRECTORY_ENTRY_SIZE);
  if (entryBuffer == NULL) {
    result = EXFAT_NO_MEMORY;
    goto cleanup;
  }

  // Create file directory entry
  memset(&fileEntry, 0, sizeof(fileEntry));
  fileEntry.entryType = EXFAT_ENTRY_FILE;
  fileEntry.secondaryCount = entriesNeeded - 1;
  fileEntry.fileAttributes = EXFAT_ATTR_ARCHIVE;
  // Set timestamps to current time (simplified - using 0 for now)
  fileEntry.createTimestamp = 0;
  fileEntry.lastModifiedTimestamp = 0;
  fileEntry.lastAccessedTimestamp = 0;

  // Copy file entry to buffer
  memcpy(entryBuffer, &fileEntry, sizeof(fileEntry));

  // Create stream extension entry
  memset(&streamEntry, 0, sizeof(streamEntry));
  streamEntry.entryType = EXFAT_ENTRY_STREAM;
  streamEntry.nameLength = nameLength;
  streamEntry.validDataLength = 0;
  streamEntry.firstCluster = 0; // No clusters allocated initially
  streamEntry.dataLength = 0;

  // Calculate name hash
  for (int ii = 0; ii < nameLength; ii++) {
    nameHash = ((nameHash & 1) ? 0x8000 : 0) + (nameHash >> 1) + 
               (uint8_t) utf16Name[ii];
  }
  streamEntry.nameHash = nameHash;

  // Copy stream entry to buffer
  memcpy(entryBuffer + EXFAT_DIRECTORY_ENTRY_SIZE, &streamEntry, 
         sizeof(streamEntry));

  // Create filename entries
  nameEntryIndex = 0;
  for (uint8_t entryIdx = 0; entryIdx < filenameEntriesNeeded; entryIdx++) {
    ExFatFileNameEntry nameEntry;
    memset(&nameEntry, 0, sizeof(nameEntry));
    nameEntry.entryType = EXFAT_ENTRY_FILENAME;
    
    // FIX: Use direct assignment for local variables instead of writeBytes
    for (int charIdx = 0; charIdx < 15 && nameEntryIndex < nameLength; 
         charIdx++
    ) {
      nameEntry.fileName[charIdx] = utf16Name[nameEntryIndex];
      nameEntryIndex++;
    }

    // Copy name entry to buffer
    memcpy(entryBuffer + ((entryIdx + 2) * EXFAT_DIRECTORY_ENTRY_SIZE),
           &nameEntry, sizeof(nameEntry));
  }

  // Calculate checksum for the entry set
  checksum = calculateDirectorySetChecksum(entryBuffer, entriesNeeded);
  
  // Set checksum in file entry using aligned access
  writeBytes(entryBuffer + 2, &checksum);

  // Write all entries to disk
  currentWriteCluster = firstFreeEntryCluster;
  currentWriteSector = firstFreeEntrySector;
  currentWriteOffset = firstFreeEntryOffset;
  bufferOffset = 0;

  while (bufferOffset < entriesNeeded * EXFAT_DIRECTORY_ENTRY_SIZE) {
    uint32_t sectorNumber = clusterToSector(driverState, currentWriteCluster) +
                            currentWriteSector;
    
    // Read the current sector
    result = readSector(driverState, sectorNumber, 
                        filesystemState->blockBuffer);
    if (result != EXFAT_SUCCESS) {
      goto cleanup;
    }

    // Copy as many entries as fit in this sector
    uint32_t remainingInSector = driverState->bytesPerSector - 
                                 currentWriteOffset;
    uint32_t remainingInBuffer = (entriesNeeded * EXFAT_DIRECTORY_ENTRY_SIZE) -
                                 bufferOffset;
    uint32_t bytesToCopy = (remainingInSector < remainingInBuffer)
                         ? remainingInSector
                         : remainingInBuffer;

    memcpy(filesystemState->blockBuffer + currentWriteOffset,
           entryBuffer + bufferOffset, bytesToCopy);

    // Write the sector back
    result = writeSector(driverState, sectorNumber, 
                         filesystemState->blockBuffer);
    if (result != EXFAT_SUCCESS) {
      goto cleanup;
    }

    bufferOffset += bytesToCopy;

    // Move to next sector if needed
    if (bufferOffset < entriesNeeded * EXFAT_DIRECTORY_ENTRY_SIZE) {
      currentWriteSector++;
      if (currentWriteSector >= driverState->sectorsPerCluster) {
        // Move to next cluster - need to check if it exists
        uint32_t nextCluster;
        result = readFatEntry(driverState, currentWriteCluster, 
                              &nextCluster);
        if (result != EXFAT_SUCCESS) {
          goto cleanup;
        }
        
        // Check if we need to allocate a new cluster for the directory
        if (nextCluster == 0xFFFFFFFF) {
          // Allocate a new cluster for the directory
          uint32_t newCluster;
          result = findFreeCluster(driverState, &newCluster);
          if (result != EXFAT_SUCCESS) {
            goto cleanup;
          }
          
          // Link the new cluster to the directory chain
          result = writeFatEntry(driverState, currentWriteCluster, newCluster);
          if (result != EXFAT_SUCCESS) {
            goto cleanup;
          }
          
          // Mark the new cluster as end of chain
          result = writeFatEntry(driverState, newCluster, 0xFFFFFFFF);
          if (result != EXFAT_SUCCESS) {
            goto cleanup;
          }
          
          // Clear the new cluster
          for (uint32_t ii = 0; ii < driverState->sectorsPerCluster; ii++) {
            uint32_t sectorNum = clusterToSector(driverState, newCluster) + ii;
            result = writeSector(driverState, sectorNum, zeroBuffer);
            if (result != EXFAT_SUCCESS) {
              goto cleanup;
            }
          }
          
          currentWriteCluster = newCluster;
        } else {
          currentWriteCluster = nextCluster;
        }
        currentWriteSector = 0;
      }
      currentWriteOffset = 0;
    }
  }

  // Initialize file handle for new file
  memset(fileHandle, 0, sizeof(ExFatFileHandle));
  fileHandle->firstCluster = 0; // No clusters allocated yet
  fileHandle->currentCluster = 0;
  fileHandle->currentPosition = 0;
  fileHandle->fileSize = 0;
  fileHandle->attributes = EXFAT_ATTR_ARCHIVE;
  fileHandle->directoryCluster = directoryCluster;
  strncpy(fileHandle->fileName, fileName, EXFAT_MAX_FILENAME_LENGTH);
  fileHandle->fileName[EXFAT_MAX_FILENAME_LENGTH] = '\0';

cleanup:
  free(entryBuffer);
  free(utf16Name);
  free(zeroBuffer);

  return result;
}

/// @brief Parse a path and navigate to the target directory
///
/// @param driverState Pointer to driver state
/// @param path Full path to parse (e.g., "/dir1/dir2/file.txt")
/// @param targetDirectoryCluster Pointer to store the cluster of target directory
/// @param fileName Pointer to store just the filename portion
///
/// @return EXFAT_SUCCESS on success, error code on failure
static int parsePathAndNavigate(ExFatDriverState* driverState,
  const char* path, uint32_t* targetDirectoryCluster, char* fileName
) {
  int result = EXFAT_SUCCESS;
  char* pathCopy = NULL;
  char* currentToken = NULL;
  char* nextToken = NULL;
  char* lastSlash = NULL;
  uint32_t currentCluster = driverState->rootDirectoryCluster;
  int returnValue = EXFAT_SUCCESS;
  ExFatFileHandle *directoryHandle = getFreeFileHandle();
  if (directoryHandle == NULL) {
    return EXFAT_TOO_MANY_OPEN_FILES;
  }
  
  if ((driverState == NULL) || (path == NULL) || 
      (targetDirectoryCluster == NULL) || (fileName == NULL)
  ) {
    returnValue = EXFAT_INVALID_PARAMETER;
    goto exit;
  }
  
  // Make a copy of the path for manipulation
  pathCopy = (char*) malloc(strlen(path) + 1);
  if (pathCopy == NULL) {
    returnValue = EXFAT_NO_MEMORY;
    goto exit;
  }
  strcpy(pathCopy, path);
  
  // Handle absolute vs relative paths
  currentToken = pathCopy;
  if (currentToken[0] == '/') {
    // Absolute path - start from root
    currentCluster = driverState->rootDirectoryCluster;
    currentToken++; // Skip leading slash
  } else {
    // Relative path - would start from current directory
    // For now, we'll treat it as starting from root
    currentCluster = driverState->rootDirectoryCluster;
  }
  
  if (driverState->filesystemState->numOpenFiles == 0) {
    driverState->filesystemState->blockBuffer = (uint8_t*) malloc(
      driverState->filesystemState->blockSize);
    if (driverState->filesystemState->blockBuffer == NULL) {
      goto exit;
    }
  }
  
  // Find the last slash to separate directory path from filename
  lastSlash = strrchr(currentToken, '/');
  if (lastSlash != NULL) {
    // There are directories in the path
    *lastSlash = '\0'; // Temporarily terminate at last slash
    strcpy(fileName, lastSlash + 1);
    
    // Navigate through each directory in the path
    char* savePtr = NULL;
    nextToken = strtok_r(currentToken, "/", &savePtr);
    
    while (nextToken != NULL) {
      // Look for this directory in the current directory
      memset(directoryHandle, 0, sizeof(*directoryHandle));
      result = findFileInDirectory(driverState, currentCluster,
        nextToken, directoryHandle);
      
      if (result != EXFAT_SUCCESS) {
        printDebug("Directory not found: ");
        printDebug(nextToken);
        printDebug("\n");
        free(pathCopy);
        returnValue = EXFAT_FILE_NOT_FOUND;
        goto exit;
      }
      
      // Verify it's actually a directory
      if (!(directoryHandle->attributes & EXFAT_ATTR_DIRECTORY)) {
        printDebug("Path component is not a directory: ");
        printDebug(nextToken);
        printDebug("\n");
        free(pathCopy);
        returnValue = EXFAT_FILE_NOT_FOUND;
        goto exit;
      }
      
      // Move to this directory
      currentCluster = directoryHandle->firstCluster;
      
      // Get next directory component
      nextToken = strtok_r(NULL, "/", &savePtr);
    }
  } else {
    // No directories in path, just a filename
    strcpy(fileName, currentToken);
  }
  
  *targetDirectoryCluster = currentCluster;
  free(pathCopy);

exit:
  if (driverState->filesystemState->numOpenFiles == 0) {
    // We just allocated the buffer above because nothing else is open.
    // Free the buffer.
    printDebug("Freeing blockBuffer in parsePathAndNavigate.\n");
    free(driverState->filesystemState->blockBuffer);
    driverState->filesystemState->blockBuffer = NULL;
  }
  
  directoryHandle = releaseFileHandle(directoryHandle);
  return returnValue;
}

/// @brief Create a directory in the filesystem
///
/// @param driverState Pointer to driver state
/// @param parentCluster Parent directory cluster
/// @param directoryName Name of directory to create
///
/// @return EXFAT_SUCCESS on success, error code on failure
static int createDirectory(ExFatDriverState* driverState,
  uint32_t parentCluster, const char* directoryName
) {
  int result = EXFAT_SUCCESS;
  uint32_t newDirectoryCluster = 0;
  uint8_t* dotEntryBuffer = NULL;
  uint32_t firstSector = 0;
  ExFatFileHandle *tempHandle = getFreeFileHandle();
  if (tempHandle == NULL) {
    return EXFAT_TOO_MANY_OPEN_FILES;
  }
  
  if ((driverState == NULL) || (directoryName == NULL)) {
    result = EXFAT_INVALID_PARAMETER;
    goto exit;
  }
  
  // First create the directory entry in the parent directory
  // This is similar to createFileInDirectory but with DIRECTORY attribute
  memset(&tempHandle, 0, sizeof(tempHandle));
  result = createFileInDirectory(driverState, parentCluster,
    directoryName, tempHandle);
  if (result != EXFAT_SUCCESS) {
    goto exit;
  }
  
  // Allocate a cluster for the new directory
  result = findFreeCluster(driverState, &newDirectoryCluster);
  if (result != EXFAT_SUCCESS) {
    goto exit;
  }
  
  // Mark cluster as end of chain
  result = writeFatEntry(driverState, newDirectoryCluster, 0xFFFFFFFF);
  if (result != EXFAT_SUCCESS) {
    goto exit;
  }
  
  // Update the directory entry with the DIRECTORY attribute and cluster
  tempHandle->attributes |= EXFAT_ATTR_DIRECTORY;
  tempHandle->firstCluster = newDirectoryCluster;
  tempHandle->currentCluster = newDirectoryCluster;
  
  // Update the directory entry in the parent
  result = updateDirectoryEntry(driverState, tempHandle);
  if (result != EXFAT_SUCCESS) {
    goto exit;
  }
  
  // Initialize the new directory with "." and ".." entries
  // Note: exFAT doesn't actually require these, but we can add them
  // for compatibility. Many exFAT implementations omit them.
  
  // Clear the first sector of the new directory
  firstSector = clusterToSector(driverState, newDirectoryCluster);
  dotEntryBuffer = (uint8_t*) malloc(driverState->bytesPerSector);
  if (dotEntryBuffer == NULL) {
    result = EXFAT_NO_MEMORY;
    goto exit;
  }
  memset(dotEntryBuffer, 0, driverState->bytesPerSector);
  
  // Write the cleared sector (empty directory)
  result = writeSector(driverState, firstSector, dotEntryBuffer);
  
  free(dotEntryBuffer);
exit:
  tempHandle = releaseFileHandle(tempHandle);
  return result;
}

/// @brief Enhanced exFatFopen with full directory support
///
/// @param driverState Pointer to driver state
/// @param pathname Full path to the file (e.g., "/dir/subdir/file.txt")
/// @param mode File open mode ("r", "w", "a", "r+", "w+", "a+")
///
/// @return ExFatFileHandle pointer on success, NULL on failure
ExFatFileHandle* exFatFopenWithPath(ExFatDriverState* driverState,
  const char* pathname, const char* mode
) {
  ExFatFileHandle* fileHandle = NULL;
  int result = 0;
  bool writeMode = false;
  bool createMode = false;
  uint32_t targetDirectoryCluster = 0;
  char* fileName = NULL;
  
  if ((driverState == NULL) || (pathname == NULL) || (mode == NULL)) {
    goto exit;
  }
  
  // Allocate buffer for filename
  fileName = (char*) malloc(EXFAT_MAX_FILENAME_LENGTH + 1);
  if (fileName == NULL) {
    goto exit;
  }
  
  // Parse the path and navigate to target directory
  result = parsePathAndNavigate(driverState, pathname,
    &targetDirectoryCluster, fileName);
  if (result != EXFAT_SUCCESS) {
    printDebug("Failed to parse path: ");
    printDebug(pathname);
    printDebug("\n");
    goto exit;
  }
  
  // Parse mode string
  writeMode = ((mode[0] & 1) || (mode[1] == '+'));
  createMode = (mode[0] & 1); // Both 'a' and 'w' have bit 0 set.
  
  if (driverState->filesystemState->numOpenFiles == 0) {
    driverState->filesystemState->blockBuffer = (uint8_t*) malloc(
      driverState->filesystemState->blockSize);
    if (driverState->filesystemState->blockBuffer == NULL) {
      goto exit;
    }
  }
  
  fileHandle = getFreeFileHandle();
  if (fileHandle == NULL) {
    goto exit;
  }
  
  // Try to find existing file in the target directory
  result = findFileInDirectory(driverState, targetDirectoryCluster,
    fileName, fileHandle);
  
  if (result == EXFAT_SUCCESS) {
    // File exists
    if (mode[0] == 'w') {
      // "w" mode - truncate file to zero length
      fileHandle->fileSize = 0;
      fileHandle->currentPosition = 0;
      fileHandle->currentCluster = fileHandle->firstCluster;
      
      // Free all clusters except the first
      if (fileHandle->firstCluster != 0) {
        uint32_t currentCluster = fileHandle->firstCluster;
        uint32_t nextCluster;
        
        result = readFatEntry(driverState, currentCluster, &nextCluster);
        if (result == EXFAT_SUCCESS && nextCluster != 0xFFFFFFFF) {
          // Mark first cluster as end of chain
          writeFatEntry(driverState, currentCluster, 0xFFFFFFFF);
          
          // Free remaining clusters
          currentCluster = nextCluster;
          while (currentCluster != 0xFFFFFFFF && currentCluster != 0) {
            result = readFatEntry(driverState, currentCluster, &nextCluster);
            if (result != EXFAT_SUCCESS) {
              break;
            }
            writeFatEntry(driverState, currentCluster, 0);
            currentCluster = nextCluster;
          }
        }
      }
      
      // Update directory entry with new size
      result = updateDirectoryEntry(driverState, fileHandle);
    } else if (mode[0] == 'a') {
      // "a" mode - position at end of file
      if (fileHandle->fileSize > 0) {
        result = exFatSeek(driverState, fileHandle, 0, SEEK_END);
        if (result != 0) {
          printDebug("Failed to seek to end of file for append mode\n");
          exFatFclose(driverState, fileHandle);
          fileHandle = NULL;
          goto exit;
        }
      }
    }
    
    // Store the directory cluster for later updates
    fileHandle->directoryCluster = targetDirectoryCluster;
    driverState->filesystemState->numOpenFiles++;
    goto exit;
  } else {
    printDebug("Could not find file \"");
    printDebug(fileName);
    printDebug("\" in directory cluster ");
    printDebug(targetDirectoryCluster);
    printDebug("\n");
  }
  
  // File doesn't exist
  if (createMode && writeMode) {
    // Create new file in the target directory
    result = createFileInDirectory(driverState, targetDirectoryCluster,
      fileName, fileHandle);
    if (result == EXFAT_SUCCESS) {
      fileHandle->directoryCluster = targetDirectoryCluster;
      driverState->filesystemState->numOpenFiles++;
      goto exit;
    }
    
    printDebug("Failed to create new file: ");
    printDebug(fileName);
    printDebug(" in directory cluster ");
    printDebug(targetDirectoryCluster);
    printDebug(" (error ");
    printDebug(result);
    printDebug(")\n");
  } else {
    printDebug("File not found and not in create mode: ");
    printDebug(fileName);
    printDebug("\n");
  }
  
  // If we got here then either findFileInDirectory failed for read mode
  // or createFileInDirectory failed for write mode. Close the handle.
  if (fileHandle != NULL) {
    fileHandle = releaseFileHandle(fileHandle);
  }
  
  if (driverState->filesystemState->numOpenFiles == 0) {
    // We just allocated the buffer above because nothing else is open.
    // Free the buffer.
    printDebug("Freeing blockBuffer in exFatFopenWithPath.\n");
    free(driverState->filesystemState->blockBuffer);
    driverState->filesystemState->blockBuffer = NULL;
  }
  
exit:
  free(fileName);
  return fileHandle;
}

/// @brief Create a directory with full path support
///
/// @param driverState Pointer to driver state
/// @param pathname Full path to the directory to create
///
/// @return 0 on success, -1 on failure
int exFatMkdir(ExFatDriverState* driverState, const char* pathname) {
  int result = 0;
  uint32_t parentDirectoryCluster = 0;
  char* directoryName = NULL;
  
  if ((driverState == NULL) || (pathname == NULL)) {
    return -1;
  }
  
  // Allocate buffer for directory name
  directoryName = (char*) malloc(EXFAT_MAX_FILENAME_LENGTH + 1);
  if (directoryName == NULL) {
    return -1;
  }
  
  // Parse the path to get parent directory and new directory name
  result = parsePathAndNavigate(driverState, pathname,
    &parentDirectoryCluster, directoryName);
  if (result != EXFAT_SUCCESS) {
    free(directoryName);
    return -1;
  }
  
  // Create the directory
  result = createDirectory(driverState, parentDirectoryCluster,
    directoryName);
  
  free(directoryName);
  return (result == EXFAT_SUCCESS) ? 0 : -1;
}

/// @brief Mark directory entries as deleted
///
/// @param driverState Pointer to driver state
/// @param directoryCluster Directory containing the file
/// @param fileName Name of file whose entries to delete
///
/// @return EXFAT_SUCCESS on success, error code on failure
static int markDirectoryEntriesDeleted(ExFatDriverState* driverState,
  uint32_t directoryCluster, const char* fileName
) {
  int result = EXFAT_SUCCESS;
  FilesystemState* filesystemState = driverState->filesystemState;
  uint32_t currentCluster = directoryCluster;
  bool entryFound = false;
  uint16_t* utf16Name = NULL;
  char* utf8Name = NULL;
  
  if ((driverState == NULL) || (fileName == NULL)) {
    return EXFAT_INVALID_PARAMETER;
  }
  
  // Allocate buffers for name conversion
  utf16Name = (uint16_t*) malloc(
    (EXFAT_MAX_FILENAME_LENGTH + 1) * sizeof(uint16_t));
  if (utf16Name == NULL) {
    return EXFAT_NO_MEMORY;
  }
  utf8Name = (char*) malloc(EXFAT_MAX_FILENAME_LENGTH + 1);
  if (utf8Name == NULL) {
    free(utf16Name);
    return EXFAT_NO_MEMORY;
  }
  
  // Allocate block buffer if needed
  if (filesystemState->blockBuffer == NULL) {
    filesystemState->blockBuffer = (uint8_t*) malloc(
      filesystemState->blockSize);
    if (filesystemState->blockBuffer == NULL) {
      free(utf16Name);
      free(utf8Name);
      return EXFAT_NO_MEMORY;
    }
  }
  
  // Search for the file entry in the directory
  while ((currentCluster != 0xFFFFFFFF) && (currentCluster >= 2) &&
         (currentCluster < driverState->clusterCount + 2) && !entryFound
  ) {
    for (uint32_t sectorInCluster = 0;
         sectorInCluster < driverState->sectorsPerCluster && !entryFound;
         sectorInCluster++
    ) {
      uint32_t sectorNumber = clusterToSector(driverState, currentCluster) +
                              sectorInCluster;
      
      result = readSector(driverState, sectorNumber,
        filesystemState->blockBuffer);
      if (result != EXFAT_SUCCESS) {
        goto cleanup;
      }
      
      for (uint32_t entryOffset = 0;
           entryOffset < driverState->bytesPerSector && !entryFound;
           entryOffset += EXFAT_DIRECTORY_ENTRY_SIZE
      ) {
        uint8_t* entryPtr = filesystemState->blockBuffer + entryOffset;
        uint8_t entryType = entryPtr[0];
        
        if (entryType == EXFAT_ENTRY_END_OF_DIR) {
          result = EXFAT_FILE_NOT_FOUND;
          goto cleanup;
        }
        
        if (entryType == EXFAT_ENTRY_FILE) {
          ExFatFileDirectoryEntry tempFileEntry;
          readBytes(&tempFileEntry, entryPtr);
          
          // Extract and compare filename (simplified - assumes entries
          // don't span sectors)
          uint32_t nextEntryOffset = entryOffset +
            EXFAT_DIRECTORY_ENTRY_SIZE;
          if (nextEntryOffset >= driverState->bytesPerSector) {
            continue;
          }
          
          // Check filename entries and compare
          memset(utf16Name, 0,
            (EXFAT_MAX_FILENAME_LENGTH + 1) * sizeof(uint16_t));
          uint8_t nameCharIndex = 0;
          bool nameComplete = true;
          
          uint8_t numNameEntries = tempFileEntry.secondaryCount - 1;
          for (uint8_t nameEntryIdx = 0;
               nameEntryIdx < numNameEntries && nameComplete;
               nameEntryIdx++
          ) {
            uint32_t nameEntryOffset = entryOffset +
              ((nameEntryIdx + 2) * EXFAT_DIRECTORY_ENTRY_SIZE);
            
            if (nameEntryOffset >= driverState->bytesPerSector) {
              nameComplete = false;
              break;
            }
            
            uint8_t* nameEntryPtr =
              filesystemState->blockBuffer + nameEntryOffset;
            if (nameEntryPtr[0] != EXFAT_ENTRY_FILENAME) {
              nameComplete = false;
              break;
            }
            
            ExFatFileNameEntry nameEntry;
            readBytes(&nameEntry, nameEntryPtr);
            
            for (uint8_t charIdx = 0;
                 charIdx < 15 && nameCharIndex < EXFAT_MAX_FILENAME_LENGTH;
                 charIdx++
            ) {
              uint16_t nameChar;
              readBytes(&nameChar, &nameEntry.fileName[charIdx]);
              if (nameChar == 0) {
                nameComplete = true;
                break;
              }
              utf16Name[nameCharIndex++] = nameChar;
            }
          }
          
          if (nameComplete) {
            utf16ToUtf8(utf16Name, utf8Name, EXFAT_MAX_FILENAME_LENGTH + 1);
            
            if (strcmp(utf8Name, fileName) == 0) {
              // Found the entry - mark all entries as deleted
              entryFound = true;
              
              // Mark the file entry and all secondary entries as unused
              for (uint8_t ii = 0; ii <= tempFileEntry.secondaryCount; ii++) {
                uint32_t deleteOffset = entryOffset +
                  (ii * EXFAT_DIRECTORY_ENTRY_SIZE);
                if (deleteOffset < driverState->bytesPerSector) {
                  filesystemState->blockBuffer[deleteOffset] =
                    EXFAT_ENTRY_UNUSED;
                }
              }
              
              // Write the modified sector back
              result = writeSector(driverState, sectorNumber,
                filesystemState->blockBuffer);
              if (result != EXFAT_SUCCESS) {
                goto cleanup;
              }
              
              break;
            }
          }
          
          // Skip to end of this entry set
          entryOffset +=
            (tempFileEntry.secondaryCount * EXFAT_DIRECTORY_ENTRY_SIZE) -
            EXFAT_DIRECTORY_ENTRY_SIZE;
        }
      }
    }
    
    if (!entryFound) {
      result = readFatEntry(driverState, currentCluster, &currentCluster);
      if (result != EXFAT_SUCCESS) {
        goto cleanup;
      }
    }
  }
  
  if (!entryFound) {
    result = EXFAT_FILE_NOT_FOUND;
  }
  
cleanup:
  free(utf16Name);
  free(utf8Name);
  return result;
}

/// @brief Remove a file with full path support
///
/// @param driverState Pointer to driver state
/// @param pathname Full path to the file to remove
///
/// @return 0 on success, -1 on failure
int exFatRemoveWithPath(ExFatDriverState* driverState, const char* pathname) {
  int result = 0;
  uint32_t targetDirectoryCluster = 0;
  char* fileName = NULL;
  uint32_t currentCluster = 0;
  ExFatFileHandle *tempHandle = getFreeFileHandle();
  if (tempHandle == NULL) {
    return EXFAT_TOO_MANY_OPEN_FILES;
  }
  
  if ((driverState == NULL) || (pathname == NULL)) {
    result = -1;
    goto exit;
  }
  
  // Allocate buffer for filename
  fileName = (char*) malloc(EXFAT_MAX_FILENAME_LENGTH + 1);
  if (fileName == NULL) {
    result = -1;
    goto exit;
  }
  
  // Parse the path and navigate to target directory
  result = parsePathAndNavigate(driverState, pathname,
    &targetDirectoryCluster, fileName);
  if (result != EXFAT_SUCCESS) {
    free(fileName);
    result = -1;
    goto exit;
  }
  
  if (driverState->filesystemState->numOpenFiles == 0) {
    driverState->filesystemState->blockBuffer = (uint8_t*) malloc(
      driverState->filesystemState->blockSize);
    if (driverState->filesystemState->blockBuffer == NULL) {
      free(fileName);
      result = -1;
      goto exit;
    }
  }
  
  // Find the file in the target directory
  result = findFileInDirectory(driverState, targetDirectoryCluster,
    fileName, tempHandle);
  
  if (driverState->filesystemState->numOpenFiles == 0) {
    // We just allocated the buffer for our directory search. Free it now.
    free(driverState->filesystemState->blockBuffer);
    driverState->filesystemState->blockBuffer = NULL;
  }
  
  if (result != EXFAT_SUCCESS) {
    free(fileName);
    result = -1;
    goto exit;
  }
  
  // Free all clusters used by the file
  currentCluster = tempHandle->firstCluster;
  while ((currentCluster != 0xFFFFFFFF) && (currentCluster != 0)) {
    uint32_t nextCluster;
    result = readFatEntry(driverState, currentCluster, &nextCluster);
    if (result != EXFAT_SUCCESS) {
      free(fileName);
      result = -1;
      goto exit;
    }
    
    result = writeFatEntry(driverState, currentCluster, 0);
    if (result != EXFAT_SUCCESS) {
      free(fileName);
      result = -1;
      goto exit;
    }
    
    currentCluster = nextCluster;
  }
  
  // Mark directory entries as deleted
  // We need to find and mark the actual directory entries
  result = markDirectoryEntriesDeleted(driverState, targetDirectoryCluster,
    fileName);
  
  free(fileName);
exit:
  tempHandle = releaseFileHandle(tempHandle);
  return (result == EXFAT_SUCCESS) ? 0 : -1;
}

/// @brief Get current position in file
///
/// @param driverState Pointer to driver state
/// @param stream FILE pointer
///
/// @return Current position or -1 on error
long exFatTell(ExFatDriverState* driverState, FILE* stream) {
  if (driverState == NULL || stream == NULL) {
    return -1;
  }

  ExFatFileHandle* fileHandle = getFileHandle(stream);
  if (fileHandle == NULL) {
    return -1;
  }

  return (long) fileHandle->currentPosition;
}

/// @brief Check if end of file has been reached
///
/// @param driverState Pointer to driver state
/// @param stream FILE pointer
///
/// @return Non-zero if EOF, 0 otherwise
int exFatEof(ExFatDriverState* driverState, FILE* stream) {
  if (driverState == NULL || stream == NULL) {
    return 1;
  }

  ExFatFileHandle* fileHandle = getFileHandle(stream);
  if (fileHandle == NULL) {
    return 1;
  }

  return (fileHandle->currentPosition >= fileHandle->fileSize) ? 1 : 0;
}

/// @brief Flush file buffers
///
/// @param driverState Pointer to driver state
/// @param stream FILE pointer
///
/// @return 0 on success, EOF on error
int exFatFlush(ExFatDriverState* driverState, FILE* stream) {
  // In this implementation, all writes are immediate, so flush is a no-op
  if (driverState == NULL || stream == NULL) {
    return EOF;
  }

  ExFatFileHandle* fileHandle = getFileHandle(stream);
  if (fileHandle == NULL) {
    return EOF;
  }

  return 0;
}

/// @typedef ExFatCommandHandler
///
/// @brief Definition of a filesystem command handler function.
typedef int (*ExFatCommandHandler)(ExFatDriverState*, ProcessMessage*);

/// @fn int exFatFilesystemOpenFileCommandHandler(
///   ExFatDriverState *driverState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_OPEN_FILE command.
///
/// @param filesystemState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int exFatFilesystemOpenFileCommandHandler(
  ExFatDriverState *driverState, ProcessMessage *processMessage
) {
  NanoOsFile *nanoOsFile = NULL;
  const char *pathname = nanoOsMessageDataPointer(processMessage, char*);
  const char *mode = nanoOsMessageFuncPointer(processMessage, char*);
  if (driverState->driverStateValid) {
    ExFatFileHandle *exFatFile = exFatFopenWithPath(driverState, pathname, mode);
    if (exFatFile != NULL) {
      nanoOsFile = (NanoOsFile*) malloc(sizeof(NanoOsFile));
      nanoOsFile->file = exFatFile;
    }
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = (intptr_t) nanoOsFile;
  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int exFatFilesystemCloseFileCommandHandler(
///   ExFatDriverState *driverState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_CLOSE_FILE command.
///
/// @param driverState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int exFatFilesystemCloseFileCommandHandler(
  ExFatDriverState *driverState, ProcessMessage *processMessage
) {
  (void) driverState;

  NanoOsFile *nanoOsFile
    = nanoOsMessageDataPointer(processMessage, NanoOsFile*);
  ExFatFileHandle *exFatFile = (ExFatFileHandle*) nanoOsFile->file;
  free(nanoOsFile);
  if (driverState->driverStateValid) {
    exFatFclose(driverState, exFatFile);
    if (driverState->filesystemState->numOpenFiles > 0) {
      driverState->filesystemState->numOpenFiles--;
      if (driverState->filesystemState->numOpenFiles == 0) {
        free(driverState->filesystemState->blockBuffer);
        driverState->filesystemState->blockBuffer = NULL;
      }
    }
  }

  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int exFatFilesystemReadFileCommandHandler(
///   ExFatDriverState *driverState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_READ_FILE command.
///
/// @param driverState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int exFatFilesystemReadFileCommandHandler(
  ExFatDriverState *driverState, ProcessMessage *processMessage
) {
  FilesystemIoCommandParameters *filesystemIoCommandParameters
    = nanoOsMessageDataPointer(processMessage, FilesystemIoCommandParameters*);
  int32_t returnValue = 0;
  if (driverState->driverStateValid) {
    returnValue = exFatRead(driverState,
      filesystemIoCommandParameters->buffer,
      filesystemIoCommandParameters->length, 
      (ExFatFileHandle*) filesystemIoCommandParameters->file->file);
    if (returnValue >= 0) {
      // Return value is the number of bytes read.  Set the length variable to
      // it and set it to 0 to indicate good status.
      filesystemIoCommandParameters->length = returnValue;
      returnValue = 0;
    } else {
      // Return value is a negative error code.  Negate it.
      returnValue = -returnValue;
      // Tell the caller that we read nothing.
      filesystemIoCommandParameters->length = 0;
    }
  }

  processMessageSetDone(processMessage);
  return returnValue;
}

/// @fn int exFatFilesystemWriteFileCommandHandler(
///   ExFatDriverState *driverState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_WRITE_FILE command.
///
/// @param driverState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int exFatFilesystemWriteFileCommandHandler(
  ExFatDriverState *driverState, ProcessMessage *processMessage
) {
  FilesystemIoCommandParameters *filesystemIoCommandParameters
    = nanoOsMessageDataPointer(processMessage, FilesystemIoCommandParameters*);
  int returnValue = 0;
  if (driverState->driverStateValid) {
    returnValue = exFatWrite(driverState,
      (uint8_t*) filesystemIoCommandParameters->buffer,
      filesystemIoCommandParameters->length,
      (ExFatFileHandle*) filesystemIoCommandParameters->file->file);
    if (returnValue >= 0) {
      // Return value is the number of bytes written.  Set the length variable
      // to it and set it to 0 to indicate good status.
      filesystemIoCommandParameters->length = returnValue;
      returnValue = 0;
    } else {
      // Return value is a negative error code.  Negate it.
      returnValue = -returnValue;
      // Tell the caller that we wrote nothing.
      filesystemIoCommandParameters->length = 0;
    }
  }

  processMessageSetDone(processMessage);
  return returnValue;
}

/// @fn int exFatFilesystemRemoveFileCommandHandler(
///   ExFatDriverState *driverState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_REMOVE_FILE command.
///
/// @param driverState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int exFatFilesystemRemoveFileCommandHandler(
  ExFatDriverState *driverState, ProcessMessage *processMessage
) {
  const char *pathname = nanoOsMessageDataPointer(processMessage, char*);
  int returnValue = 0;
  if (driverState->driverStateValid) {
    returnValue = exFatRemoveWithPath(driverState, pathname);
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = (intptr_t) returnValue;
  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int exFatFilesystemSeekFileCommandHandler(
///   ExFatDriverState *driverState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_SEEK_FILE command.
///
/// @param driverState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int exFatFilesystemSeekFileCommandHandler(
  ExFatDriverState *driverState, ProcessMessage *processMessage
) {
  FilesystemSeekParameters *filesystemSeekParameters
    = nanoOsMessageDataPointer(processMessage, FilesystemSeekParameters*);
  int returnValue = 0;
  if (driverState->driverStateValid) {
    returnValue = exFatSeek(driverState,
      (ExFatFileHandle*) filesystemSeekParameters->stream->file,
      filesystemSeekParameters->offset,
      filesystemSeekParameters->whence);
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = (intptr_t) returnValue;
  processMessageSetDone(processMessage);
  return 0;
}

/// @var filesystemCommandHandlers
///
/// @brief Array of ExFatCommandHandler function pointers.
const ExFatCommandHandler filesystemCommandHandlers[] = {
  exFatFilesystemOpenFileCommandHandler,   // FILESYSTEM_OPEN_FILE
  exFatFilesystemCloseFileCommandHandler,  // FILESYSTEM_CLOSE_FILE
  exFatFilesystemReadFileCommandHandler,   // FILESYSTEM_READ_FILE
  exFatFilesystemWriteFileCommandHandler,  // FILESYSTEM_WRITE_FILE
  exFatFilesystemRemoveFileCommandHandler, // FILESYSTEM_REMOVE_FILE
  exFatFilesystemSeekFileCommandHandler,   // FILESYSTEM_SEEK_FILE
};


/// @fn static void exFatHandleFilesystemMessages(FilesystemState *fs)
///
/// @brief Pop and handle all messages in the filesystem process's message
/// queue until there are no more.
///
/// @param fs A pointer to the FilesystemState object maintained by the
///   filesystem process.
///
/// @return This function returns no value.
static void exFatHandleFilesystemMessages(ExFatDriverState *driverState) {
  ProcessMessage *msg = processMessageQueuePop();
  while (msg != NULL) {
    FilesystemCommandResponse type = 
      (FilesystemCommandResponse) processMessageType(msg);
    if (type < NUM_FILESYSTEM_COMMANDS) {
      filesystemCommandHandlers[type](driverState, msg);
    }
    msg = processMessageQueuePop();
  }
}

/// @fn void* runExFatFilesystem(void *args)
///
/// @brief Main process entry point for the FAT16 filesystem process.
///
/// @param args A pointer to an initialized BlockStorageDevice structure cast
///   to a void*.
///
/// @return This function never returns, but would return NULL if it did.
void* runExFatFilesystem(void *args) {
  coroutineYield(NULL);
  FilesystemState *fs = (FilesystemState*) calloc(1, sizeof(FilesystemState));
  ExFatDriverState *driverState
    = (ExFatDriverState*) calloc(1, sizeof(ExFatDriverState));
  fs->blockDevice = (BlockStorageDevice*) args;
  fs->blockSize = fs->blockDevice->blockSize;
  
  fs->blockBuffer = (uint8_t*) malloc(fs->blockSize);
  getPartitionInfo(fs);
  exFatInitialize(driverState, fs);
  free(fs->blockBuffer); fs->blockBuffer = NULL;
  
  ProcessMessage *msg = NULL;
  while (1) {
    msg = (ProcessMessage*) coroutineYield(NULL);
    if (msg) {
      FilesystemCommandResponse type = 
        (FilesystemCommandResponse) processMessageType(msg);
      if (type < NUM_FILESYSTEM_COMMANDS) {
        filesystemCommandHandlers[type](driverState, msg);
      }
    } else {
      exFatHandleFilesystemMessages(driverState);
    }
  }
  return NULL;
}

/// @fn long exFatFilesystemFTell(FILE *stream)
///
/// @brief Get the current value of the position indicator of a
/// previously-opened file.
///
/// @param stream A pointer to a previously-opened file.
///
/// @return Returns the current position of the file on success, -1 on failure.
long exFatFilesystemFTell(FILE *stream) {
  if (stream == NULL) {
    return -1;
  }

  return (long) ((ExFatFileHandle*) stream->file)->currentPosition;
}

