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
///                   Copyright (c) 2012-2024 James Card
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

#define FAT16_BYTES_PER_DIRECTORY_ENTRY 32
#define FAT16_ENTRIES_PER_CLUSTER 16
#define FAT16_CLUSTER_CHAIN_END 0xFFF8
#define FAT16_FILENAME_LENGTH 8
#define FAT16_EXTENSION_LENGTH 3
#define FAT16_ATTR_DIRECTORY 0x10
#define FAT16_ATTR_FILE 0x20

typedef struct Fat16BootSector {
  uint8_t jumpBoot[3];
  uint8_t oemName[8];
  uint16_t bytesPerSector;
  uint8_t sectorsPerCluster;
  uint16_t reservedSectorCount;
  uint8_t numberOfFats;
  uint16_t rootEntryCount;
  uint16_t totalSectors16;
  uint8_t mediaType;
  uint16_t sectorsPerFat;
  uint16_t sectorsPerTrack;
  uint16_t numberOfHeads;
  uint32_t hiddenSectors;
  uint32_t totalSectors32;
  uint8_t driveNumber;
  uint8_t reserved1;
  uint8_t bootSignature;
  uint32_t volumeId;
  uint8_t volumeLabel[11];
  uint8_t fileSystemType[8];
} Fat16BootSector;

typedef struct Fat16DirectoryEntry {
  uint8_t filename[FAT16_FILENAME_LENGTH];
  uint8_t extension[FAT16_EXTENSION_LENGTH];
  uint8_t attributes;
  uint8_t reserved;
  uint8_t creationTimeMs;
  uint16_t creationTime;
  uint16_t creationDate;
  uint16_t lastAccessDate;
  uint16_t firstClusterHigh;
  uint16_t lastModifiedTime;
  uint16_t lastModifiedDate;
  uint16_t firstClusterLow;
  uint32_t fileSize;
} Fat16DirectoryEntry;

typedef struct Fat16File {
  uint32_t currentCluster;
  uint32_t currentPosition;
  uint32_t fileSize;
  uint32_t firstCluster;
  char mode;  // 'r' for read, 'w' for write
  FilesystemState *fs;
} Fat16File;

// Exported functionality
void* runFat16Filesystem(void *args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FAT16_FILESYSTEM_H
