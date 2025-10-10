///////////////////////////////////////////////////////////////////////////////
///
/// @file              ExFatFilesystem.h
///
/// @brief             Base exFAT driver for NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
///
///////////////////////////////////////////////////////////////////////////////

#ifndef EXFAT_FILESYSTEM_H
#define EXFAT_FILESYSTEM_H


#ifdef __cplusplus
extern "C"
{
#endif

// exFAT constants
#define EXFAT_SIGNATURE              0x4146544658455845ULL // "EXFATFAT"
#define EXFAT_SECTOR_SIZE            512
#define EXFAT_CLUSTER_SIZE_MIN       512
#define EXFAT_CLUSTER_SIZE_MAX       (32 * 1024 * 1024)
#define EXFAT_MAX_FILENAME_LENGTH    255
#define EXFAT_DIRECTORY_ENTRY_SIZE   32
#define EXFAT_MAX_OPEN_FILES         8

// Directory entry types
#define EXFAT_ENTRY_UNUSED            0x00
#define EXFAT_ENTRY_END_OF_DIR        0x00
#define EXFAT_ENTRY_FILE              0x85
#define EXFAT_ENTRY_STREAM            0xC0
#define EXFAT_ENTRY_FILENAME          0xC1
#define EXFAT_ENTRY_ALLOCATION_BITMAP 0x81
#define EXFAT_ENTRY_UPCASE_TABLE      0x82
#define EXFAT_ENTRY_VOLUME_LABEL      0x83

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

// Forward declaration
typedef struct FilesystemState FilesystemState;

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

// Function declarations
ExFatFileHandle* exFatOpenFile(
  ExFatDriverState* driverState, const char* filePath, const char* mode);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // EXFAT_FILESYSTEM_H

