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
  bool      inUse;                 // Whether this handle is in use
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
  ExFatFileHandle   openFiles[EXFAT_MAX_OPEN_FILES]; // Open file handles
  bool              driverStateValid; // Whether or not this is a valid state
} ExFatDriverState;

/// @typedef ExFatCommandHandler
///
/// @brief Definition of a filesystem command handler function.
typedef int (*ExFatCommandHandler)(ExFatDriverState*, ProcessMessage*);

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
          clusterNumber < driverState->bootSector.clusterCount + 2);
}

/// @brief Enhanced cluster to sector conversion with validation
///
/// @param driverState Pointer to driver state
/// @param clusterNumber Cluster number
///
/// @return Sector number, or 0 if invalid cluster
static uint32_t clusterToSector(ExFatDriverState* driverState, 
                                   uint32_t clusterNumber) {
  if (!isValidCluster(driverState, clusterNumber)) {
    return 0;
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
                               ExFatFileHandle* fileHandle) {
  if (driverState == NULL || driverState->filesystemState == NULL ||
      fileName == NULL || fileHandle == NULL) {
    return EXFAT_INVALID_PARAMETER;
  }

  // Validate starting cluster
  if (directoryCluster < 2 || 
      directoryCluster >= driverState->bootSector.clusterCount + 2) {
    return EXFAT_INVALID_PARAMETER;
  }

  FilesystemState* filesystemState = driverState->filesystemState;
  uint32_t currentCluster = directoryCluster;
  
  // Walk through the cluster chain
  while (currentCluster != 0xFFFFFFFF && currentCluster >= 2 &&
         currentCluster < driverState->bootSector.clusterCount + 2) {
    
    // Read all sectors in this cluster
    for (uint32_t sectorInCluster = 0; 
         sectorInCluster < driverState->sectorsPerCluster; 
         sectorInCluster++) {
      
      uint32_t sectorNumber = clusterToSector(driverState, currentCluster) + 
                              sectorInCluster;
      
      int result = readSector(driverState, sectorNumber, 
                              filesystemState->blockBuffer);
      if (result != EXFAT_SUCCESS) {
        return result;
      }

      // Process all directory entries in this sector
      for (uint32_t entryOffset = 0; 
           entryOffset < driverState->bytesPerSector; 
           entryOffset += EXFAT_DIRECTORY_ENTRY_SIZE) {
        
        uint8_t* entryPtr = filesystemState->blockBuffer + entryOffset;
        uint8_t entryType = entryPtr[0];
        
        // Check for end of directory
        if (entryType == EXFAT_ENTRY_END_OF_DIR) {
          return EXFAT_FILE_NOT_FOUND;
        }

        // Skip unused entries
        if (entryType == EXFAT_ENTRY_UNUSED) {
          continue;
        }

        // Process file entries
        if (entryType == EXFAT_ENTRY_FILE) {
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
            uint8_t tempBuffer[EXFAT_SECTOR_SIZE];
            result = readSector(driverState, nextSectorNumber, tempBuffer);
            if (result != EXFAT_SUCCESS) {
              continue;
            }
            streamEntryPtr = tempBuffer + (streamEntryOffset - driverState->bytesPerSector);
          }

          // Validate and read stream extension entry
          if (streamEntryPtr[0] != EXFAT_ENTRY_STREAM) {
            continue; // Invalid entry set
          }
          readBytes(&streamEntry, streamEntryPtr);

          // Extract filename from filename entries
          uint16_t fullName[EXFAT_MAX_FILENAME_LENGTH + 1] = {0};
          uint8_t nameCharIndex = 0;
          bool nameComplete = true;
          
          // Read filename entries (remaining secondary entries)
          uint8_t numNameEntries = fileEntry.secondaryCount - 1;
          for (uint8_t nameEntryIdx = 0; 
               nameEntryIdx < numNameEntries && nameComplete; 
               nameEntryIdx++) {
            
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
            
            uint8_t* nameEntryPtr = filesystemState->blockBuffer + nameEntryOffset;
            if (nameEntryPtr[0] != EXFAT_ENTRY_FILENAME) {
              nameComplete = false;
              break;
            }
            
            readBytes(&nameEntry, nameEntryPtr);
            
            // Extract up to 15 characters from this name entry
            for (uint8_t charIdx = 0; 
                 charIdx < 15 && nameCharIndex < EXFAT_MAX_FILENAME_LENGTH; 
                 charIdx++) {
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
            char utf8Name[EXFAT_MAX_FILENAME_LENGTH + 1];
            utf16ToUtf8(fullName, utf8Name, sizeof(utf8Name));
            
            if (strcmp(utf8Name, fileName) == 0) {
              // Found the file - populate file handle
              fileHandle->inUse = true;
              fileHandle->firstCluster = streamEntry.firstCluster;
              fileHandle->currentCluster = streamEntry.firstCluster;
              fileHandle->currentPosition = 0;
              fileHandle->fileSize = streamEntry.dataLength;
              fileHandle->attributes = fileEntry.fileAttributes;
              fileHandle->directoryCluster = directoryCluster;
              strncpy(fileHandle->fileName, fileName, EXFAT_MAX_FILENAME_LENGTH);
              fileHandle->fileName[EXFAT_MAX_FILENAME_LENGTH] = '\0';
              return EXFAT_SUCCESS;
            }
          }

          // Skip to the end of this entry set
          entryOffset += (fileEntry.secondaryCount * EXFAT_DIRECTORY_ENTRY_SIZE) 
                        - EXFAT_DIRECTORY_ENTRY_SIZE;
        }
      }
    }

    // Move to next cluster in the chain
    int result = readFatEntry(driverState, currentCluster, &currentCluster);
    if (result != EXFAT_SUCCESS) {
      break;
    }
  }

  return EXFAT_FILE_NOT_FOUND;
}

/// @brief Get free file handle
///
/// @param driverState Pointer to driver state
///
/// @return Pointer to free file handle, NULL if none available
static ExFatFileHandle* getFreeFileHandle(ExFatDriverState* driverState) {
  if (driverState == NULL) {
    return NULL;
  }

  for (int ii = 0; ii < EXFAT_MAX_OPEN_FILES; ii++) {
    if (!driverState->openFiles[ii].inUse) {
      memset(&driverState->openFiles[ii], 0, sizeof(ExFatFileHandle));
      return &driverState->openFiles[ii];
    }
  }

  return NULL;
}

/// @brief Get file handle from FILE pointer
///
/// @param driverState Pointer to driver state
/// @param stream FILE pointer
///
/// @return ExFatFileHandle pointer or NULL if invalid
static ExFatFileHandle* getFileHandle(ExFatDriverState* driverState, 
                                      FILE* stream) {
  if (stream == NULL || driverState == NULL) {
    return NULL;
  }

  ExFatFileHandle* fileHandle = (ExFatFileHandle*) stream->file;
  
  // Validate that this is actually one of our file handles
  for (int ii = 0; ii < EXFAT_MAX_OPEN_FILES; ii++) {
    if (&driverState->openFiles[ii] == fileHandle && fileHandle->inUse) {
      return fileHandle;
    }
  }

  return NULL;
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

/// @brief Close an open file
///
/// @param driverState Pointer to driver state
/// @param fileHandle File handle to close
///
/// @return 0 on success, EOF on failure
int exFatFclose(ExFatDriverState* driverState, ExFatFileHandle* fileHandle) {
  if (driverState == NULL || fileHandle == NULL) {
    return EOF;
  }

  if (!fileHandle->inUse) {
    return EOF;
  }

  fileHandle->inUse = false;
  return 0;
}

/// @brief Open a file on the exFAT filesystem
///
/// @param driverState Pointer to driver state
/// @param pathname Path to the file to open
/// @param mode File open mode (not fully implemented)
///
/// @return ExFatFileHandle pointer on success, NULL on failure
ExFatFileHandle* exFatFopen(ExFatDriverState* driverState, const char* pathname,
                const char* mode) {
  ExFatFileHandle* fileHandle = NULL;
  int result = 0;

  if (driverState == NULL || pathname == NULL || mode == NULL) {
    goto exit;
  }

  if (driverState->filesystemState->numOpenFiles == 0) {
    driverState->filesystemState->blockBuffer = (uint8_t*) malloc(
      driverState->filesystemState->blockSize);
    if (driverState->filesystemState->blockBuffer == NULL) {
      goto exit;
    }
  }

  fileHandle = getFreeFileHandle(driverState);
  if (fileHandle == NULL) {
    goto exit;
  }

  // For simplicity, assume all files are in root directory
  result = findFileInDirectory(driverState, 
                               driverState->rootDirectoryCluster,
                               pathname, fileHandle);
  if (result == EXFAT_SUCCESS) {
    driverState->filesystemState->numOpenFiles++;
    goto exit;
  }

  // If we got here then findFileInDirectory failed.  Close the handle we got.
  exFatFclose(driverState, fileHandle);
  fileHandle = NULL;
  if (driverState->filesystemState->numOpenFiles == 0) {
    // We just allocated the buffer above because nothing else is open.  Free
    // the buffer.
    free(driverState->filesystemState->blockBuffer);
    driverState->filesystemState->blockBuffer = NULL;
  }

exit:
  return fileHandle;
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
    return -1;
  }

  if (!fileHandle->inUse) {
    return -1;
  }

  uint32_t bytesRead = 0;
  uint8_t* buffer = (uint8_t*) ptr;

  while (bytesRead < totalBytes && 
         fileHandle->currentPosition < fileHandle->fileSize) {
    uint32_t clusterOffset = fileHandle->currentPosition % 
                             driverState->bytesPerCluster;
    uint32_t sectorInCluster = clusterOffset / driverState->bytesPerSector;
    uint32_t offsetInSector = clusterOffset % driverState->bytesPerSector;
    
    uint32_t sectorNumber = clusterToSector(driverState, 
                                            fileHandle->currentCluster) + 
                            sectorInCluster;
    
    int result = readSector(driverState, sectorNumber, 
                            driverState->filesystemState->blockBuffer);
    if (result != EXFAT_SUCCESS) {
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
        fileHandle->currentPosition < fileHandle->fileSize) {
      uint32_t nextCluster;
      result = readFatEntry(driverState, fileHandle->currentCluster, 
                            &nextCluster);
      if (result != EXFAT_SUCCESS || nextCluster == 0xFFFFFFFF) {
        break;
      }
      fileHandle->currentCluster = nextCluster;
    }
  }

  return bytesRead;
}

/// @brief Write data to a file
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
  if ((driverState == NULL) || (ptr == NULL)
    || (fileHandle == NULL) || (totalBytes == 0)
  ) {
    return -1;
  }

  if (!fileHandle->inUse) {
    return -1;
  }

  uint32_t bytesWritten = 0;
  const uint8_t* buffer = (const uint8_t*) ptr;

  while (bytesWritten < totalBytes) {
    uint32_t clusterOffset = fileHandle->currentPosition % 
                             driverState->bytesPerCluster;
    uint32_t sectorInCluster = clusterOffset / driverState->bytesPerSector;
    uint32_t offsetInSector = clusterOffset % driverState->bytesPerSector;
    
    // Check if we need to allocate a new cluster
    if (fileHandle->currentCluster == 0 || 
        (clusterOffset == 0 && fileHandle->currentPosition > 0)) {
      uint32_t newCluster;
      int result = findFreeCluster(driverState, &newCluster);
      if (result != EXFAT_SUCCESS) {
        break;
      }

      if (fileHandle->currentCluster == 0) {
        fileHandle->firstCluster = newCluster;
        fileHandle->currentCluster = newCluster;
      } else {
        result = writeFatEntry(driverState, fileHandle->currentCluster, 
                               newCluster);
        if (result != EXFAT_SUCCESS) {
          break;
        }
        fileHandle->currentCluster = newCluster;
      }

      result = writeFatEntry(driverState, newCluster, 0xFFFFFFFF);
      if (result != EXFAT_SUCCESS) {
        break;
      }
    }

    uint32_t sectorNumber = clusterToSector(driverState, 
                                            fileHandle->currentCluster) + 
                            sectorInCluster;
    
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
    }

    // Check if we need to move to next cluster
    if ((fileHandle->currentPosition % driverState->bytesPerCluster) == 0) {
      uint32_t nextCluster;
      result = readFatEntry(driverState, fileHandle->currentCluster, 
                            &nextCluster);
      if (result != EXFAT_SUCCESS) {
        break;
      }
      
      if (nextCluster == 0xFFFFFFFF && bytesWritten < totalBytes) {
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
      } else if (nextCluster != 0xFFFFFFFF) {
        fileHandle->currentCluster = nextCluster;
      }
    }
  }

  return bytesWritten;
}

/// @brief Remove a file from the filesystem
///
/// @param driverState Pointer to driver state
/// @param pathname Path to the file to remove
///
/// @return 0 on success, -1 on failure
int exFatRemove(ExFatDriverState* driverState, const char* pathname) {
  if (driverState == NULL || pathname == NULL) {
    return -1;
  }

  // Find the file in the directory
  ExFatFileHandle tempHandle;
  int result = findFileInDirectory(driverState, 
                                   driverState->rootDirectoryCluster,
                                   pathname, &tempHandle);
  if (result != EXFAT_SUCCESS) {
    return -1;
  }

  // Free all clusters used by the file
  uint32_t currentCluster = tempHandle.firstCluster;
  while (currentCluster != 0xFFFFFFFF && currentCluster != 0) {
    uint32_t nextCluster;
    result = readFatEntry(driverState, currentCluster, &nextCluster);
    if (result != EXFAT_SUCCESS) {
      return -1;
    }

    result = writeFatEntry(driverState, currentCluster, 0);
    if (result != EXFAT_SUCCESS) {
      return -1;
    }

    currentCluster = nextCluster;
  }

  // Mark directory entries as deleted
  // This is a simplified implementation - in a real implementation,
  // we would need to find and mark the actual directory entries as deleted
  // For now, we'll return success assuming the clusters were freed
  
  return 0;
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
  if ((driverState == NULL) || (fileHandle== NULL)) {
    return -1;
  }

  if (!fileHandle->inUse) {
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

  ExFatFileHandle* fileHandle = getFileHandle(driverState, stream);
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

  ExFatFileHandle* fileHandle = getFileHandle(driverState, stream);
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

  ExFatFileHandle* fileHandle = getFileHandle(driverState, stream);
  if (fileHandle == NULL) {
    return EOF;
  }

  return 0;
}

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
    ExFatFileHandle *exFatFile = exFatFopen(driverState, pathname, mode);
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
    returnValue = exFatRemove(driverState, pathname);
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
  fs->blockSize = 512;
  
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

  return (long) ((ExFatFileHandle*) stream->file)->fileSize;
}

