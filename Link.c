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

/// @enum LinkValueType
///
/// @brief Type values used in link TLV metadata.
typedef enum LinkValueType {
  LINK_VALUE_TYPE_INVALID = 0,
  LINK_VALUE_TYPE_PATH,
  NUM_LINK_VALUE_TYPES
} LinkValueType;

/// @def LINK_MAGIC
///
/// @brief Magic value at the begining of a link to designate it as a NanoOS
/// link.
#define LINK_MAGIC ((uint64_t*) "NanoOsLn")

/// @brief LINK_MAGIC_SIZE
///
/// @brief Size, in bytes, of the magic value of the beginning of a NanoOs link.
#define LINK_MAGIC_SIZE (sizeof(uint64_t))

/// @def LINK_TYPE_LENGTH_SIZE
///
/// @brief Size, in bytes, of type + length metadata for a value.
#define LINK_TYPE_LENGTH_SIZE 4

/// @def LINK_CHECKSUM_SIZE
///
/// @brief Size, in bytes, of a checksum for a value.
#define LINK_CHECKSUM_SIZE 2

/// @def LINK_MAGIC_INDEX
///
/// @brief The index for the LINK_MAGIC in a link header.  This is an 8-byte
/// field.
#define LINK_MAGIC_INDEX 0

/// @def LINK_HEADER_SIZE_INDEX
///
/// @brief The index within the header of the header length.  This is a 2-byte
/// field.
#define LINK_HEADER_SIZE_INDEX 8

/// @def LINK_VERSION_INDEX
///
/// @brief The index within the header of the link version number.  This is a
/// 2-byte field in version 1.
#define LINK_VERSION_INDEX 10

/// @def LINK_VERSION1_HEADER_SIZE
///
/// @brief The total size, in bytes, of the link header in version 1.
#define LINK_VERSION1_HEADER_SIZE 12

/// @def LINK_VERSION1_PATH_TYPE_INDEX
///
/// @brief The index within the link file of the path type value.  This is a
/// 2-byte field in version 1.
#define LINK_VERSION1_PATH_TYPE_INDEX 12

/// @def LINK_VERSION1_PATH_LENGTH_INDEX
///
/// @brief The index within the link file of the of the path length.  This is a
/// 2-byte field in version 1.
#define LINK_VERSION1_PATH_LENGTH_INDEX 14

/// @def LINK_VERSION1_PATH_INDEX
///
/// @brief The index within the link file of the target path.  This is a 2-byte
/// field in version 1.
#define LINK_VERSION1_PATH_INDEX 16

/// @fn static char* getFilename(const char *path)
///
/// @brief Helper function to get filename from path.
///
/// @param path Full path to a file.
///
/// @return Returns just the filename portion of the path.
const char* getFilename(const char *path) {
  const char *lastSlashAt = strrchr(path, '/');
  const char *filename = path;
  
  if (lastSlashAt != NULL) {
    filename = lastSlashAt + 1;
  }
  
  return filename;
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
  if (src == NULL) {
    return -1;
  }
  
  // Determine output filename
  char *outputPath = NULL;
  size_t dstLength = 0;
  if (dst != NULL) {
    dstLength = strlen(dst);
  }
  
  if (dstLength == 0) {
    // Place in current directory with src filename + .lnk
    const char *filename = getFilename(src);
    if (filename == NULL) {
      return -1;
    }
    
    outputPath = (char*) malloc(strlen(filename) + 5);  // +5 for ".lnk\0"
    if (outputPath == NULL) {
      return -1;
    }
    strcpy(outputPath, filename);
    strcat(outputPath, ".lnk");
  } else if (dst[dstLength - 1] == '/') {
    // Place in specified directory with src filename + .lnk
    const char *filename = getFilename(src);
    if (filename == NULL) {
      return -1;
    }
    
    outputPath = (char*) malloc(strlen(dst) + strlen(filename) + 5);
    if (outputPath == NULL) {
      return -1;
    }
    strcpy(outputPath, dst);
    strcat(outputPath, filename);
    strcat(outputPath, ".lnk");
  } else {
    // Use dst as-is
    outputPath = (char*) malloc(strlen(dst) + 1);
    if (outputPath == NULL) {
      return -1;
    }
    strcpy(outputPath, dst);
  }
  
  // Calculate total buffer size needed
  size_t pathLen = strlen(src) + 1;
  if (pathLen > 255) {
    // Path too long.  We can't store this in version 1.
    free(outputPath);
    return -1;
  }
  size_t totalSize = LINK_VERSION1_HEADER_SIZE + LINK_TYPE_LENGTH_SIZE
    + pathLen + LINK_CHECKSUM_SIZE;
  
  // Allocate buffer
  uint8_t *buffer = (uint8_t*) malloc(totalSize);
  if (!buffer) {
    free(outputPath);
    return -1;
  }
  
  // Populate buffer
  // For the header, everything is 16-bit aligned, so we don't need to use
  // memcpy, we can just direclty set the values.
  
  // Magic value.  This is at index 0, which is 64-bit aligned, so we can
  // directly set that too.
  *((uint64_t*) &buffer[LINK_MAGIC_INDEX]) = *LINK_MAGIC;
  
  // Header size.
  *((uint16_t*) &buffer[LINK_HEADER_SIZE_INDEX])
    = LINK_VERSION1_HEADER_SIZE;
  
  // Link version.
  *((uint16_t*) &buffer[LINK_VERSION_INDEX]) = 1;
  
  // Since the header was 16-bit aligned, the metadata for the first payload
  // element is too.
  // Link path value type.
  *((uint16_t*) &buffer[LINK_VERSION1_PATH_TYPE_INDEX])
    = LINK_VALUE_TYPE_PATH;
  
  // Path length, including terminating NULL byte and checksum.
  *((uint16_t*) &buffer[LINK_VERSION1_PATH_LENGTH_INDEX])
    = pathLen + LINK_CHECKSUM_SIZE;

  // Path contents.
  memcpy(&buffer[LINK_VERSION1_PATH_INDEX], src, pathLen);
  
  // Checksum.
  uint16_t checksum = 0;
  for (size_t ii = 0; ii < pathLen; ii++) {
    checksum += (uint16_t) src[ii];
  }
  memcpy(&buffer[LINK_VERSION1_PATH_INDEX + pathLen],
    &checksum, sizeof(checksum));
  
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

/// @fn char* getLink(const char *initialLink)
///
/// @brief Extract the linked file path from a link file.
///
/// @param initialLink The full path to the link file on the filesystem.
///
/// @return Returns a dynamically-allocated C string with the link target path
/// on success, NULL on failure.
char* getLink(const char *initialLink) {
  if (initialLink == NULL) {
    return NULL;
  }
  
  char *finalTarget = (char*) malloc(strlen(initialLink) + 1);
  strcpy(finalTarget, initialLink);
  const char *extension = strrchr(finalTarget, '.');
  uint8_t *buffer = NULL;
  
  while ((extension != NULL) && (strcmp(extension, ".lnk") == 0)) {
    // Open file and get size
    FILE *fp = fopen(finalTarget, "rb");
    if (fp == NULL) {
      free(finalTarget);
      return NULL;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (fileSize < LINK_VERSION1_HEADER_SIZE) {
      fclose(fp);
      free(finalTarget);
      return NULL;
    }
    
    // Allocate buffer and read entire file
    void *check = realloc(buffer, fileSize);
    if (check == NULL) {
      fclose(fp);
      free(finalTarget);
      return NULL;
    }
    buffer = (uint8_t*) check;
    
    size_t bytesRead = fread(buffer, 1, fileSize, fp);
    fclose(fp); fp = NULL;
    
    if (bytesRead != (size_t)fileSize) {
      free(buffer);
      free(finalTarget);
      return NULL;
    }
    
    if (*((uint64_t*) &buffer[LINK_MAGIC_INDEX]) != *LINK_MAGIC) {
      // Not our link.
      free(buffer);
      free(finalTarget);
      return NULL;
    }
    
    uint16_t linkHeaderSize = *((uint16_t*) &buffer[LINK_HEADER_SIZE_INDEX]);
    // We only understand version 1, so there's really no point in reading the
    // link version.
    // uint16_t linkVersion = *((uint16_t*) buffer[LINK_VERSION_INDEX]);
    
    const char *targetPath = NULL;
    uint16_t valueType = 0;
    uint16_t valueLength = 0;
    uint16_t ii = 0;
    // Search through the payload until we find the link path, which is the only
    // thing we understand in this version.  At this point, we can no longer
    // directly access elements in the buffer because we have no idea what the
    // alignment is now.
    for (ii = linkHeaderSize; (ii < bytesRead) && (targetPath == NULL);) {
      memcpy(&valueType, &buffer[ii], sizeof(valueType));
      ii += sizeof(valueType);
      memcpy(&valueLength, &buffer[ii], sizeof(valueLength));
      ii += sizeof(valueLength);
      if (valueType == LINK_VALUE_TYPE_PATH) {
        targetPath = (const char*) &buffer[ii];
      }
      
      ii += valueLength;
    }
    
    if (targetPath != NULL) {
      // The checksum is stored at the end of the value, so two bytes before our
      // current index.
      uint16_t storedChecksum = 0;
      memcpy(&storedChecksum, &buffer[ii - sizeof(uint16_t)], sizeof(uint16_t));

      uint16_t computedChecksum = 0;
      for (size_t jj = 0; targetPath[jj] != '\0'; jj++) {
        computedChecksum += (uint16_t) targetPath[jj];
      }
      
      if (computedChecksum != storedChecksum) {
        // Link corrupted.  No good.
        free(buffer);
        free(finalTarget);
        return NULL;
      }
    }
    
    check = realloc(finalTarget, strlen(targetPath) + 1);
    if (check == NULL) {
      // Out of memory.  Cannot continue.
      free(buffer);
      free(finalTarget);
      return NULL;
    }
    finalTarget = (char*) check;
    strcpy(finalTarget, targetPath);
    extension = strrchr(finalTarget, '.');
  }
  
  free(buffer);
  return finalTarget;
}

