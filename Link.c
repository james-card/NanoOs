////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2025 James Card                     //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included    //
// in all copies or substantial portions of the Software.                     //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//                                 James Card                                 //
//                          http://www.jamescard.org                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///
/// @file              Link.c
///
/// @brief             .lnk file format support
///
///////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__linux__) || defined(__linux) || defined(_WIN32)

// We're on a "real" OS.
#include <stdio.h>
#include <stdlib.h>

#else

// We're compiling from within NanoOs.
#include "Filesystem.h"

#endif // OS-specific imports

// Include our own header.
#include "Link.h"

// Header offsets and sizes
#define HEADER_SIZE 0x0000004C
#define OFFSET_HEADER_SIZE 0
#define OFFSET_LINK_CLSID 4
#define OFFSET_LINK_FLAGS 20
#define OFFSET_FILE_ATTRIBUTES 24
#define OFFSET_CREATION_TIME 28
#define OFFSET_ACCESS_TIME 36
#define OFFSET_WRITE_TIME 44
#define OFFSET_FILE_SIZE 52
#define OFFSET_ICON_INDEX 56
#define OFFSET_SHOW_COMMAND 60
#define OFFSET_HOTKEY 64
#define OFFSET_RESERVED1 66
#define OFFSET_RESERVED2 68
#define OFFSET_RESERVED3 72

// LinkInfo offsets
#define LINKINFO_OFFSET_SIZE 0
#define LINKINFO_OFFSET_HEADER_SIZE 4
#define LINKINFO_OFFSET_FLAGS 8
#define LINKINFO_OFFSET_VOLUME_ID_OFFSET 12
#define LINKINFO_OFFSET_LOCAL_BASE_PATH_OFFSET 16
#define LINKINFO_OFFSET_NETWORK_VOLUME_TABLE_OFFSET 20
#define LINKINFO_OFFSET_COMMON_PATH_SUFFIX_OFFSET 24
#define LINKINFO_HEADER_SIZE 28

// VolumeID offsets
#define VOLUMEID_OFFSET_SIZE 0
#define VOLUMEID_OFFSET_TYPE 4
#define VOLUMEID_OFFSET_SERIAL 8
#define VOLUMEID_OFFSET_LABEL_OFFSET 12
#define VOLUMEID_HEADER_SIZE 16
#define VOLUMEID_TOTAL_SIZE 17  // Including null terminator for empty label

// Link flags
#define HAS_LINK_TARGET_ID_LIST    0x00000001
#define HAS_LINK_INFO              0x00000002
#define HAS_NAME                   0x00000004
#define HAS_RELATIVE_PATH          0x00000008
#define HAS_WORKING_DIR            0x00000010
#define HAS_ARGUMENTS              0x00000020
#define HAS_ICON_LOCATION          0x00000040
#define IS_UNICODE                 0x00000080

// Other constants
#define LINK_CLSID_SIZE 16
#define SHOW_NORMAL 1
#define FILE_ATTRIBUTE_ARCHIVE 0x00000020
#define VOLUME_TYPE_FIXED 3
#define LINKINFO_FLAG_VOLUME_ID_AND_LOCAL_PATH 0x00000001

// Standard CLSID for shell links
static const uint8_t SHELL_LINK_CLSID[LINK_CLSID_SIZE] = {
  0x01, 0x14, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46
};

// Helper functions for writing little-endian values to buffer
static void writeUint32ToLeBuffer(uint8_t *buffer, size_t offset, uint32_t val) {
  buffer[offset] = val & 0xFF;
  buffer[offset + 1] = (val >> 8) & 0xFF;
  buffer[offset + 2] = (val >> 16) & 0xFF;
  buffer[offset + 3] = (val >> 24) & 0xFF;
}

// Helper functions for reading little-endian values from buffer
static uint16_t readLeUint16FromBuffer(const uint8_t *buffer, size_t offset) {
  return buffer[offset] | (buffer[offset + 1] << 8);
}

static uint32_t readLeUint32FromBuffer(const uint8_t *buffer, size_t offset) {
  return buffer[offset]
    | (buffer[offset + 1] << 8)
    | (buffer[offset + 2] << 16)
    | (buffer[offset + 3] << 24);
}

/// @fn static char* getFilename(const char *path)
///
/// @brief Helper function to get filename from path.
///
/// @param path Full path to a file.
///
/// @return Returns just the filename portion of the path.
static const char* getFilename(const char *path) {
  const char *lastSlash = strrchr(path, '/');
  const char *lastBackslash = strrchr(path, '\\');
  const char *filename = path;
  
  if (lastSlash && (!lastBackslash || lastSlash > lastBackslash)) {
    filename = lastSlash + 1;
  } else if (lastBackslash) {
    filename = lastBackslash + 1;
  }
  
  return filename;
}

/// @fn static void writeHeaderToBuffer(uint8_t *buffer)
///
/// @brief Populate shell link header in buffer.
///
/// @param buffer A pointer to a buffer of bytes to write the header into.
///
/// @return This function returns no value.
static void writeHeaderToBuffer(uint8_t *buffer) {
  // Header size
  writeUint32ToLeBuffer(buffer, OFFSET_HEADER_SIZE, HEADER_SIZE);
  
  // CLSID
  memcpy(buffer + OFFSET_LINK_CLSID, SHELL_LINK_CLSID, LINK_CLSID_SIZE);
  
  // Link flags
  writeUint32ToLeBuffer(buffer, OFFSET_LINK_FLAGS, HAS_LINK_INFO);
  
  // File attributes
  writeUint32ToLeBuffer(buffer, OFFSET_FILE_ATTRIBUTES, FILE_ATTRIBUTE_ARCHIVE);
  
  /*
   * No need to do this.  Buffer was allocated with calloc.
   * 
   * // Timestamps (all zeros)
   * writeUint64ToLeBuffer(buffer, OFFSET_CREATION_TIME, 0);
   * writeUint64ToLeBuffer(buffer, OFFSET_ACCESS_TIME, 0);
   * writeUint64ToLeBuffer(buffer, OFFSET_WRITE_TIME, 0);
   * 
   * // File size
   * writeUint32ToLeBuffer(buffer, OFFSET_FILE_SIZE, 0);
   * 
   * // Icon index
   * writeUint32ToLeBuffer(buffer, OFFSET_ICON_INDEX, 0);
   */
  
  // Show command
  writeUint32ToLeBuffer(buffer, OFFSET_SHOW_COMMAND, SHOW_NORMAL);
  
  /*
   * No need to do this either.
   * 
   * // Hotkey
   * writeUint16ToLeBuffer(buffer, OFFSET_HOTKEY, 0);
   * 
   * // Reserved fields
   * writeUint16ToLeBuffer(buffer, OFFSET_RESERVED1, 0);
   * writeUint32ToLeBuffer(buffer, OFFSET_RESERVED2, 0);
   * writeUint32ToLeBuffer(buffer, OFFSET_RESERVED3, 0);
   */
}

/// @fn static void writeLinkInfoToBuffer(
///   uint8_t *buffer, size_t offset, const char *path)
///
/// @brief Populate LinkInfo structure in buffer.
///
/// @param buffer A pointer to a buffer of bytes to write the LinkInfo data
///   into.
/// @param offset The offset into the buffer at which to begin the write.
/// @param path The path to the file to link to.
///
/// @return This function returns no value.
static void writeLinkInfoToBuffer(
  uint8_t *buffer, size_t offset, const char *path
) {
  size_t pathLen = strlen(path) + 1;  // Include null terminator
  
  // Calculate offsets
  uint32_t volumeIdOffset = LINKINFO_HEADER_SIZE;
  uint32_t localBasePathOffset = volumeIdOffset + VOLUMEID_TOTAL_SIZE;
  uint32_t totalSize = localBasePathOffset + pathLen;
  
  // Write LinkInfo header
  writeUint32ToLeBuffer(buffer,
    offset + LINKINFO_OFFSET_SIZE, totalSize);
  writeUint32ToLeBuffer(buffer,
    offset + LINKINFO_OFFSET_HEADER_SIZE, LINKINFO_HEADER_SIZE);
  writeUint32ToLeBuffer(buffer,
    offset + LINKINFO_OFFSET_FLAGS, LINKINFO_FLAG_VOLUME_ID_AND_LOCAL_PATH);
  writeUint32ToLeBuffer(buffer,
    offset + LINKINFO_OFFSET_VOLUME_ID_OFFSET, volumeIdOffset);
  writeUint32ToLeBuffer(buffer,
    offset + LINKINFO_OFFSET_LOCAL_BASE_PATH_OFFSET, localBasePathOffset);
  writeUint32ToLeBuffer(buffer,
    offset + LINKINFO_OFFSET_NETWORK_VOLUME_TABLE_OFFSET, 0);
  writeUint32ToLeBuffer(buffer,
    offset + LINKINFO_OFFSET_COMMON_PATH_SUFFIX_OFFSET, localBasePathOffset);
  
  // Write VolumeID
  size_t volumeIdStart = offset + volumeIdOffset;
  writeUint32ToLeBuffer(buffer,
    volumeIdStart + VOLUMEID_OFFSET_SIZE, VOLUMEID_TOTAL_SIZE);
  writeUint32ToLeBuffer(buffer,
    volumeIdStart + VOLUMEID_OFFSET_TYPE, VOLUME_TYPE_FIXED);
  writeUint32ToLeBuffer(buffer,
    volumeIdStart + VOLUMEID_OFFSET_SERIAL, 0);
  writeUint32ToLeBuffer(buffer,
    volumeIdStart + VOLUMEID_OFFSET_LABEL_OFFSET, VOLUMEID_HEADER_SIZE);
  buffer[volumeIdStart + VOLUMEID_HEADER_SIZE] = '\0';  // Empty volume label
  
  // Write local base path
  size_t pathStart = offset + localBasePathOffset;
  memcpy(buffer + pathStart, path, pathLen);
}

/// @fn int makeLink(const char *src, const char *dst)
///
/// @brief Make a link to a source file on the filesystem from a specified
/// location.
///
/// @param src The target of the link.
/// @param dst The path to the new link on the filesystem.
///
/// @return Returns 0 on success, -1 on error.
int makeLink(const char *src, const char *dst) {
  if (!src) return -1;
  
  // Determine output filename
  char *outputPath = NULL;
  size_t dstLength = 0;
  if (dst != NULL) {
    dstLength = strlen(dst);
  }
  
  if (dstLength == 0) {
    // Place in current directory with src filename + .lnk
    const char *filename = getFilename(src);
    if (!filename) return -1;
    
    outputPath = (char*) malloc(strlen(filename) + 5);  // +5 for ".lnk\0"
    if (!outputPath) {
      return -1;
    }
    strcpy(outputPath, filename);
    strcat(outputPath, ".lnk");
  } else if (dst[dstLength - 1] == '/' || dst[dstLength - 1] == '\\') {
    // Place in specified directory with src filename + .lnk
    const char *filename = getFilename(src);
    if (!filename) return -1;
    
    outputPath = (char*) malloc(strlen(dst) + strlen(filename) + 5);
    if (!outputPath) {
      return -1;
    }
    strcpy(outputPath, dst);
    strcat(outputPath, filename);
    strcat(outputPath, ".lnk");
  } else {
    // Use dst as-is
    outputPath = (char*) malloc(strlen(dst) + 1);
    if (!outputPath) return -1;
    strcpy(outputPath, dst);
  }
  
  // Calculate total buffer size needed
  size_t pathLen = strlen(src) + 1;
  size_t linkInfoSize = LINKINFO_HEADER_SIZE + VOLUMEID_TOTAL_SIZE + pathLen;
  size_t totalSize = HEADER_SIZE + linkInfoSize;
  
  // Allocate buffer
  uint8_t *buffer = (uint8_t*) calloc(1, totalSize);
  if (!buffer) {
    free(outputPath);
    return -1;
  }
  
  // Populate buffer
  writeHeaderToBuffer(buffer);
  writeLinkInfoToBuffer(buffer, HEADER_SIZE, src);
  
  // Write entire buffer to file
  FILE *fp = fopen(outputPath, "wb");
  if (!fp) {
    free(buffer);
    free(outputPath);
    return -1;
  }
  
  size_t written = fwrite(buffer, 1, totalSize, fp);
  fclose(fp);
  
  // Cleanup
  free(buffer);
  free(outputPath);
  
  return (written == totalSize) ? 0 : -1;
}

/// @fn char* getLink(const char *linkFile)
///
/// @brief Extract the linked file path from a link file.
///
/// @param linkFile The full path to the link file on the filesystem.
///
/// @return Returns a dynamically-allocated C string with the link target path
/// on success, NULL on failure.
char* getLink(const char *linkFile) {
  if (!linkFile) return NULL;
  
  // Open file and get size
  FILE *fp = fopen(linkFile, "rb");
  if (!fp) return NULL;
  
  // Get file size
  fseek(fp, 0, SEEK_END);
  long fileSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  
  if (fileSize < HEADER_SIZE) {
    fclose(fp);
    return NULL;
  }
  
  // Allocate buffer and read entire file
  uint8_t *buffer = (uint8_t*) malloc(fileSize);
  if (!buffer) {
    fclose(fp);
    return NULL;
  }
  
  size_t bytesRead = fread(buffer, 1, fileSize, fp);
  fclose(fp);
  
  if (bytesRead != (size_t)fileSize) {
    free(buffer);
    return NULL;
  }
  
  // Verify header size
  uint32_t headerSize = readLeUint32FromBuffer(buffer, OFFSET_HEADER_SIZE);
  if (headerSize != HEADER_SIZE) {
    free(buffer);
    return NULL;
  }
  
  // Read link flags
  uint32_t linkFlags = readLeUint32FromBuffer(buffer, OFFSET_LINK_FLAGS);
  
  size_t currentOffset = HEADER_SIZE;
  
  // Skip LinkTargetIDList if present
  if (linkFlags & HAS_LINK_TARGET_ID_LIST) {
    if (currentOffset + 2 > (size_t)fileSize) {
      free(buffer);
      return NULL;
    }
    uint16_t idListSize = readLeUint16FromBuffer(buffer, currentOffset);
    currentOffset += 2 + idListSize;
  }
  
  char *result = NULL;
  
  // Read LinkInfo if present
  if (linkFlags & HAS_LINK_INFO) {
    if (currentOffset + LINKINFO_HEADER_SIZE > (size_t)fileSize) {
      free(buffer);
      return NULL;
    }
    
    size_t linkInfoStart = currentOffset;
    
    // Read LinkInfo header
    uint32_t linkInfoFlags = readLeUint32FromBuffer(buffer,
      linkInfoStart + LINKINFO_OFFSET_FLAGS);
    uint32_t localBasePathOffset = readLeUint32FromBuffer(buffer,
      linkInfoStart + LINKINFO_OFFSET_LOCAL_BASE_PATH_OFFSET);
    
    // Check if we have a local base path
    if (linkInfoFlags & LINKINFO_FLAG_VOLUME_ID_AND_LOCAL_PATH) {
      size_t pathStart = linkInfoStart + localBasePathOffset;
      
      if (pathStart >= (size_t)fileSize) {
        free(buffer);
        return NULL;
      }
      
      // Find the length of the null-terminated string
      size_t pathLen = 0;
      while ((pathStart + pathLen < (size_t)fileSize)
        && (buffer[pathStart + pathLen] != '\0')
      ) {
        pathLen++;
      }
      
      // Allocate and copy the path
      result = (char*) malloc(pathLen + 1);
      if (result) {
        memcpy(result, buffer + pathStart, pathLen);
        result[pathLen] = '\0';
      }
    }
  }
  
  free(buffer);
  return result;
}

