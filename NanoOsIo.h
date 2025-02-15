///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              12.02.2024
///
/// @file              Filesystem.h
///
/// @brief             Common filesystem functionality for NanoOs.
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

#ifndef NANO_OS_IO_H
#define NANO_OS_IO_H

// Custom includes
#include "NanoOs.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Standard seek mode definitions
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

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

// Directory search result codes
#define FAT16_DIR_SEARCH_ERROR -1
#define FAT16_DIR_SEARCH_FOUND 0
#define FAT16_DIR_SEARCH_DELETED 1
#define FAT16_DIR_SEARCH_NOT_FOUND 2

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

/// @def USB_SERIAL_PORT
///
/// Index into ConsoleState.conslePorts for the USB serial port.
#define USB_SERIAL_PORT 0

/// @def GPIO_SERIAL_PORT
///
/// Index into ConsoleState.conslePorts for the GPIO serial port.
#define GPIO_SERIAL_PORT 1

/// @struct NanoOsIoCommandParameters
///
/// @brief Parameters needed for an I/O command in a filesystem.
///
/// @param file A pointer to the FILE object returned from a call to fopen.
/// @param buffer A pointer to the memory that is either to be read from or
///   written to.
/// @param length The number of bytes to read into the buffer or write from the
///   buffer.
typedef struct NanoOsIoCommandParameters {
  FILE *file;
  void *buffer;
  uint32_t length;
} NanoOsIoCommandParameters;

/// @struct NanoOsIoSeekParameters
///
/// @brief Parameters needed for an fseek function call on a file.
///
/// @param stream A pointer to the FILE object to adjust the position indicator
///   of.
/// @param offset The offset to apply to the position specified by the whence
///   parameter.
/// @param whence The position the offset is understood to be relative to.  One
///   of SEEK_SET (beginning of the file), SEEK_CUR (the current position of
///   the file) or SEEK_END (the end of the file).
typedef struct NanoOsIoSeekParameters {
  FILE *stream;
  long offset;
  int whence;
} NanoOsIoSeekParameters;

/// @struct FcopyArgs
///
/// @brief Structure for holding the arguments needed for an fcopy call.  See
/// that function for descriptions of these member variables.
typedef struct FcopyArgs {
  FILE *srcFile;
  off_t srcStart;
  FILE *dstFile;
  off_t dstStart;
  size_t length;
} FcopyArgs;

/// @enum NanoOsIoCommandResponse
///
/// @brief Commands and responses understood by the NanoOs I/O inter-process
/// message handler.
typedef enum NanoOsIoCommandResponse {
  // Commands:
  NANO_OS_IO_OPEN_FILE,
  NANO_OS_IO_CLOSE_FILE,
  NANO_OS_IO_READ_FILE,
  NANO_OS_IO_WRITE_FILE,
  NANO_OS_IO_REMOVE_FILE,
  NANO_OS_IO_SEEK_FILE,
  NANO_OS_IO_COPY_FILE,
  NANO_OS_IO_WRITE_VALUE,
  NANO_OS_IO_GET_BUFFER,
  NANO_OS_IO_WRITE_BUFFER,
  NANO_OS_IO_SET_PORT_SHELL,
  NANO_OS_IO_ASSIGN_PORT,
  NANO_OS_IO_ASSIGN_PORT_INPUT,
  NANO_OS_IO_RELEASE_PORT,
  NANO_OS_IO_GET_OWNED_PORT,
  NANO_OS_IO_SET_ECHO_PORT,
  NANO_OS_IO_WAIT_FOR_INPUT,
  NANO_OS_IO_RELEASE_PID_PORT,
  NANO_OS_IO_RELEASE_BUFFER,
  NUM_NANO_OS_IO_COMMANDS,
  // Responses:
  NANO_OS_IO_RETURNING_BUFFER,
  NANO_OS_IO_RETURNING_PORT,
  NANO_OS_IO_RETURNING_INPUT,
} NanoOsIoCommandResponse;

/// @enum NanoOsIoValueType
///
/// @brief Types to be used with the NANO_OS_IO_WRITE_VALUE command.
typedef enum NanoOsIoValueType {
  NANO_OS_IO_VALUE_CHAR,
  NANO_OS_IO_VALUE_UCHAR,
  NANO_OS_IO_VALUE_INT,
  NANO_OS_IO_VALUE_UINT,
  NANO_OS_IO_VALUE_LONG_INT,
  NANO_OS_IO_VALUE_LONG_UINT,
  NANO_OS_IO_VALUE_FLOAT,
  NANO_OS_IO_VALUE_DOUBLE,
  NANO_OS_IO_VALUE_STRING,
  NUM_NANO_OS_IO_VALUES
} NanoOsIoValueType;

/// @struct FilesystemState
///
/// @brief State metadata the filesystem process uses to provide access to
/// files.
///
/// @param partitionNumber The one-based partition index to use for filesystem
///   access.
/// @param blockSize The size of a block as it is known to the filesystem.
/// @param blockBuffer A pointer to the read/write buffer that is blockSize
///   bytes in length.
/// @param startLba The address of the first block of the filesystem.
/// @param endLba The address of the last block of the filesystem.
/// @param numOpenFiles The number of files currently open by the filesystem.
///   If this number is zero then the blockBuffer pointer may be NULL.
typedef struct FilesystemState {
  uint8_t partitionNumber;
  uint16_t blockSize;
  uint8_t *blockBuffer;
  uint32_t startLba;
  uint32_t endLba;
  uint8_t  numOpenFiles;
} FilesystemState;

// Support functions
void releaseConsole(void);
int getOwnedConsolePort(void);
int setConsoleEcho(bool desiredEchoState);

// Main entry point for the process
void* runNanoOsIo(void *args);

#ifdef __cplusplus
} // extern "C"
#endif

int printConsole(char message);
int printConsole(unsigned char message);
int printConsole(int message);
int printConsole(unsigned int message);
int printConsole(long int message);
int printConsole(long unsigned int message);
int printConsole(float message);
int printConsole(double message);
int printConsole(const char *message);

#endif // NANO_OS_IO_H
