///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              12.02.2024
///
/// @file              Fat16Filesystem.h
///
/// @brief             FAT16 filesystem functionality for NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
/// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///                                James Card
///                         http://www.jamescard.org
///
///////////////////////////////////////////////////////////////////////////////

#ifndef FAT16_FILESYSTEM_H
#define FAT16_FILESYSTEM_H

// Custom includes
#include "NanoOs.h"
#include "Filesystem.h"

#ifdef __cplusplus
extern "C"
{
#endif

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

// Partition table constants
#define FAT16_PARTITION_TABLE_OFFSET 0x1BE
#define FAT16_PARTITION_ENTRY_SIZE 16
#define FAT16_PARTITION_TYPE_FAT16_LBA 0x0E
#define FAT16_PARTITION_TYPE_FAT16_LBA_EXTENDED 0x1E
#define FAT16_PARTITION_LBA_OFFSET 8
#define FAT16_PARTITION_SECTORS_OFFSET 12

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

// Exported functionality
void* runFat16Filesystem(void *args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FAT16_FILESYSTEM_H
