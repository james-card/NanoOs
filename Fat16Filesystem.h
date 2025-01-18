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
  uint16_t currentCluster;
  uint32_t currentPosition;
  uint32_t fileSize;
  uint16_t firstCluster;
  char *pathname;
} Fat16File;

// Exported functionality
void* runFat16Filesystem(void *args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FAT16_FILESYSTEM_H
