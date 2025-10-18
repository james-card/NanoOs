///////////////////////////////////////////////////////////////////////////////
///
/// @file              Filesystem.cpp
///
/// @brief             Common filesystem functionality.
///
///////////////////////////////////////////////////////////////////////////////

#include "Filesystem.h"

// Partition table constants
#define PARTITION_TABLE_OFFSET 0x1BE
#define PARTITION_ENTRY_SIZE 16

#define PARTITION_TYPE_NTFS_EXFAT 0x07
#define PARTITION_TYPE_FAT16_LBA 0x0E
#define PARTITION_TYPE_FAT16_LBA_EXTENDED 0x1E
#define PARTITION_TYPE_LINUX 0x83

#define PARTITION_LBA_OFFSET 8
#define PARTITION_SECTORS_OFFSET 12

/// @fn int getPartitionInfo(FilesystemState *fs)
///
/// @brief Get information about the partition for the provided filesystem.
///
/// @param fs Pointer to the filesystem state structure maintained by the
///   filesystem process.
///
/// @return Returns 0 on success, negative error code on failure.
int getPartitionInfo(FilesystemState *fs) {
  if (fs->blockDevice->partitionNumber == 0) {
    return -1;
  }

  if (fs->blockDevice->readBlocks(fs->blockDevice->context, 0, 1, 
      fs->blockSize, fs->blockBuffer) != 0
  ) {
    return -2;
  }

  uint8_t *partitionTable = fs->blockBuffer + PARTITION_TABLE_OFFSET;
  uint8_t *entry
    = partitionTable
    + ((fs->blockDevice->partitionNumber - 1)
    * PARTITION_ENTRY_SIZE);
  uint8_t type = entry[4];
  
  if ((type == PARTITION_TYPE_FAT16_LBA)
    || (type == PARTITION_TYPE_FAT16_LBA_EXTENDED)
    || (type == PARTITION_TYPE_NTFS_EXFAT)
    || (type == PARTITION_TYPE_LINUX)
  ) {
    uint32_t lbaValue, sectorsValue;
    
    // Read LBA offset using readBytes for alignment safety
    readBytes(&lbaValue, &entry[PARTITION_LBA_OFFSET]);
    fs->startLba = lbaValue;
      
    // Read number of sectors using readBytes for alignment safety  
    readBytes(&sectorsValue, &entry[PARTITION_SECTORS_OFFSET]);
      
    fs->endLba = fs->startLba + sectorsValue - 1;
    return 0;
  }
  
  return -3;
}

/// @fn FILE* filesystemFOpen(const char *pathname, const char *mode)
///
/// @brief Implementation of the standard C fopen call.
///
/// @param pathname The full pathname to the file.  NOTE:  This implementation
///   can only open files in the root directory.  Subdirectories are NOT
///   supported.
/// @param mode The standard C file mode to open the file as.
///
/// @return Returns a pointer to an initialized FILE object on success, NULL on
/// failure.
FILE* filesystemFOpen(const char *pathname, const char *mode) {
  if ((pathname == NULL) || (*pathname == '\0')
    || (mode == NULL) || (*mode == '\0')
  ) {
    return NULL;
  }

  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_OPEN_FILE,
    (intptr_t) mode, (intptr_t) pathname, true);
  processMessageWaitForDone(msg, NULL);
  FILE *file = nanoOsMessageDataPointer(msg, FILE*);
  processMessageRelease(msg);
  return file;
}

/// @fn int filesystemFClose(FILE *stream)
///
/// @brief Implementation of the standard C fclose call.
///
/// @param stream A pointer to a previously-opened FILE object.
///
/// @return This function always succeeds and always returns 0.
int filesystemFClose(FILE *stream) {
  int returnValue = 0;

  if (stream != NULL) {
    FilesystemFcloseParameters fcloseParameters;
    fcloseParameters.stream = stream;
    fcloseParameters.returnValue = 0;

    ProcessMessage *msg = sendNanoOsMessageToPid(
      NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_CLOSE_FILE,
      0, (intptr_t) &fcloseParameters, true);
    processMessageWaitForDone(msg, NULL);

    if (fcloseParameters.returnValue != 0) {
      errno = -fcloseParameters.returnValue;
      returnValue = EOF;
    }

    processMessageRelease(msg);
  }

  return returnValue;
}

/// @fn int filesystemRemove(const char *pathname)
///
/// @brief Implementation of the standard C remove call.
///
/// @param pathname The full pathname to the file.  NOTE:  This implementation
///   can only open files in the root directory.  Subdirectories are NOT
///   supported.
///
/// @return Returns 0 on success, -1 and sets the value of errno on failure.
int filesystemRemove(const char *pathname) {
  int returnValue = 0;
  if ((pathname != NULL) && (*pathname != '\0')) {
    ProcessMessage *msg = sendNanoOsMessageToPid(
      NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_REMOVE_FILE,
      /* func= */ 0, (intptr_t) pathname, true);
    processMessageWaitForDone(msg, NULL);
    returnValue = nanoOsMessageDataValue(msg, int);
    if (returnValue != 0) {
      // returnValue holds a negative errno.  Set errno for the current process
      // and return -1 like we're supposed to.
      errno = -returnValue;
      returnValue = -1;
    }
    processMessageRelease(msg);
  }
  return returnValue;
}

/// @fn int filesystemFSeek(FILE *stream, long offset, int whence)
///
/// @brief Implementation of the standard C fseek call.
///
/// @param stream A pointer to a previously-opened FILE object.
/// @param offset A signed integer value that will be added to the specified
///   position.
/// @param whence The location within the file to apply the offset to.  Valid
///   values are SEEK_SET (the beginning of the file), SEEK_CUR (the current
///   file positon), and SEEK_END (the end of the file).
///
/// @return Returns 0 on success, -1 on failure.
int filesystemFSeek(FILE *stream, long offset, int whence) {
  if (stream == NULL) {
    return -1;
  }

  FilesystemSeekParameters filesystemSeekParameters = {
    .stream = stream,
    .offset = offset,
    .whence = whence,
  };
  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_REMOVE_FILE,
    /* func= */ 0, (intptr_t) &filesystemSeekParameters, true);
  processMessageWaitForDone(msg, NULL);
  int returnValue = nanoOsMessageDataValue(msg, int);
  processMessageRelease(msg);
  return returnValue;
}

/// @fn size_t filesystemFRead(
///   void *ptr, size_t size, size_t nmemb, FILE *stream)
///
/// @brief Read data from a previously-opened file.
///
/// @param ptr A pointer to the memory to read data into.
/// @param size The size, in bytes, of each element that is to be read from the
///   file.
/// @param nmemb The number of elements that are to be read from the file.
/// @param stream A pointer to the previously-opened file.
///
/// @return Returns the total number of objects successfully read from the
/// file.
size_t filesystemFRead(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  size_t returnValue = 0;
  if ((ptr == NULL) || (size == 0) || (nmemb == 0) || (stream == NULL)) {
    // Nothing to do.
    return returnValue; // 0
  }

  FilesystemIoCommandParameters filesystemIoCommandParameters = {
    .file = stream,
    .buffer = ptr,
    .length = (uint32_t) (size * nmemb)
  };
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID,
    FILESYSTEM_READ_FILE,
    /* func= */ 0,
    /* data= */ (intptr_t) &filesystemIoCommandParameters,
    true);
  processMessageWaitForDone(processMessage, NULL);
  returnValue = (filesystemIoCommandParameters.length / size);
  processMessageRelease(processMessage);

  return returnValue;
}

/// @fn size_t filesystemFWrite(
///   const void *ptr, size_t size, size_t nmemb, FILE *stream)
///
/// @brief Write data to a previously-opened file.
///
/// @param ptr A pointer to the memory to write data from.
/// @param size The size, in bytes, of each element that is to be written to
///   the file.
/// @param nmemb The number of elements that are to be written to the file.
/// @param stream A pointer to the previously-opened file.
///
/// @return Returns the total number of objects successfully written to the
/// file.
size_t filesystemFWrite(
  const void *ptr, size_t size, size_t nmemb, FILE *stream
) {
  size_t returnValue = 0;
  if ((ptr == NULL) || (size == 0) || (nmemb == 0) || (stream == NULL)) {
    // Nothing to do.
    return returnValue; // 0
  }

  FilesystemIoCommandParameters filesystemIoCommandParameters = {
    .file = stream,
    .buffer = (void*) ptr,
    .length = (uint32_t) (size * nmemb)
  };
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID,
    FILESYSTEM_WRITE_FILE,
    /* func= */ 0,
    /* data= */ (intptr_t) &filesystemIoCommandParameters,
    true);
  processMessageWaitForDone(processMessage, NULL);
  returnValue = (filesystemIoCommandParameters.length / size);
  processMessageRelease(processMessage);

  return returnValue;
}

// .lnk file format support

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

