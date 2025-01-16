////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2024 James Card                     //
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

// Doxygen marker
/// @file

// Custom includes
#include "Filesystem.h"

/// @struct FilesystemState
///
/// @brief State metadata the filesystem process uses to provide access to
/// files.
///
/// @param blockDevice A pointer to an allocated and initialized
///   BlockStorageDevice to use for reading and writing blocks.
/// @param blockSize The size of a block as it is known to the filesystem.
/// @param blockBuffer A pointer to the read/write buffer that is blockSize
///   bytes in length.
typedef struct FilesystemState {
  BlockStorageDevice *blockDevice;
  uint16_t blockSize;
  uint8_t *blockBuffer;
} FilesystemState;

/// @typedef FilesystemCommandHandler
///
/// @brief Definition of a filesystem command handler function.
typedef int (*FilesystemCommandHandler)(FilesystemState*, ProcessMessage*);

/// @def PIN_SD_CS
///
/// @brief Pin to use for the MicroSD card reader's SPI chip select line.
#define PIN_SD_CS 4

/// @fn int filesystemOpenFileCommandHandler(
///   FilesystemState *filesystemState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_OPEN_FILE command.
///
/// @param filesystemState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int filesystemOpenFileCommandHandler(
  FilesystemState *filesystemState, ProcessMessage *processMessage
) {
  (void) filesystemState;

  const char *pathname = nanoOsMessageDataPointer(processMessage, char*);
  const char *modeString = nanoOsMessageFuncPointer(processMessage, char*);
  int mode = 0;
  //// if ((strchr(modeString, 'r') && (strchr(modeString, 'w')))
  ////   || (strchr(modeString, '+'))
  //// ) {
  ////   mode |= O_RDWR;
  //// } else if (strchr(modeString, 'r')) {
  ////   mode |= O_RDONLY;
  //// } else if (strchr(modeString, 'w')) {
  ////   mode |= O_WRONLY;
  //// }
  //// if (strchr(modeString, 'w')) {
  ////   mode |= O_CREAT | O_TRUNC;
  //// } else if (strchr(modeString, 'a')) {
  ////   mode |= O_APPEND | O_CREAT;
  //// }
  (void) pathname;
  (void) modeString;
  (void) mode;

  NanoOsFile *nanoOsFile = (NanoOsFile*) malloc(sizeof(NanoOsFile));
  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = (intptr_t) nanoOsFile;
  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int filesystemCloseFileCommandHandler(
///   FilesystemState *filesystemState, ProcessMessage *processMessage)
///
/// @brief Command handler for FILESYSTEM_CLOSE_FILE command.
///
/// @param filesystemState A pointer to the FilesystemState object maintained
///   by the filesystem process.
/// @param processMessage A pointer to the ProcessMessage that was received by
///   the filesystem process.
///
/// @return Returns 0 on success, a standard POSIX error code on failure.
int filesystemCloseFileCommandHandler(
  FilesystemState *filesystemState, ProcessMessage *processMessage
) {
  (void) filesystemState;

  FILE *stream = nanoOsMessageDataPointer(processMessage, FILE*);
  free(stream); stream = NULL;

  processMessageSetDone(processMessage);
  return 0;
}

/// @var filesystemCommandHandlers
///
/// @brief Array of FilesystemCommandHandler function pointers.
const FilesystemCommandHandler filesystemCommandHandlers[] = {
  filesystemOpenFileCommandHandler,  // FILESYSTEM_OPEN_FILE
  filesystemCloseFileCommandHandler, // FILESYSTEM_CLOSE_FILE
};

/// @fn void handleFilesystemMessages(FilesystemState *filesystemState)
///
/// @brief Handle filesystem messages from the process's queue until there are
/// no more waiting.
///
/// @param filesystemState A pointer to the FilesystemState structure
///   maintained by the filesystem process.
///
/// @return This function returns no value.
void handleFilesystemMessages(FilesystemState *filesystemState) {
  ProcessMessage *processMessage = processMessageQueuePop();
  while (processMessage != NULL) {
    FilesystemCommandResponse messageType
      = (FilesystemCommandResponse) processMessageType(processMessage);
    if (messageType >= NUM_FILESYSTEM_COMMANDS) {
      processMessage = processMessageQueuePop();
      continue;
    }
    
    filesystemCommandHandlers[messageType](filesystemState, processMessage);
    processMessage = processMessageQueuePop();
  }
  
  return;
}

void printPartitionTable(FilesystemState *filesystemState) {
    // Read block 0 (MBR)
    if (filesystemState->blockDevice->readBlocks(
            filesystemState->blockDevice->context,
            0,  // start block
            1,  // number of blocks
            512,  // block size
            filesystemState->blockBuffer) != 0) {
        printString("Error reading MBR\n");
        return;
    }

    // Partition table starts at offset 0x1BE
    uint8_t *partitionTable = filesystemState->blockBuffer + 0x1BE;

    printString("Partition Table:\n");
    
    // Process all 4 primary partition entries
    for (int i = 0; i < 4; i++) {
        uint8_t *entry = partitionTable + (i * 16);  // Each entry is 16 bytes
        
        uint8_t status = entry[0];
        uint8_t type = entry[4];
        uint32_t startLBA
          = (((uint32_t) entry[11]) << 24)
          | (((uint32_t) entry[10]) << 16)
          | (((uint32_t) entry[ 9]) <<  8)
          | (((uint32_t) entry[ 8]) <<  0);
        uint32_t numSectors
          = (((uint32_t) entry[15]) << 24)
          | (((uint32_t) entry[14]) << 16)
          | (((uint32_t) entry[13]) <<  8)
          | (((uint32_t) entry[12]) <<  0);
        
        // Only print active partitions (type != 0)
        if (type != 0) {
            printString("Partition ");
            printInt(i + 1);
            printString(":\n");
            
            printString("  Status: 0x");
            printInt(status);
            printString("\n");
            
            printString("  Type: 0x");
            printInt(type);
            printString(" (");
            
            // Print partition type description
            switch(type) {
                case 0x01:
                    printString("FAT12");
                    break;
                case 0x04:
                case 0x06:
                    printString("FAT16");
                    break;
                case 0x07:
                    printString("NTFS/exFAT");
                    break;
                case 0x0B:
                case 0x0C:
                    printString("FAT32");
                    break;
                case 0x83:
                    printString("Linux");
                    break;
                default:
                    printString("Unknown");
            }
            printString(")\n");
            
            printString("  Start Block: ");
            printInt(startLBA);
            printString("\n");
            
            printString("  End Block: ");
            printLong(startLBA + numSectors - 1);
            printString("\n\n");
        }
    }
}

/// @fn void* runFilesystem(void *args)
///
/// @brief Process entry-point for the filesystem process.  Enters an infinite
/// loop for processing commands.
///
/// @param args Any arguments to this function, cast to a void*.  Currently
///   ignored by this function.
///
/// @return This function never returns, but would return NULL if it did.
void* runFilesystem(void *args) {
  FilesystemState filesystemState;
  filesystemState.blockDevice = (BlockStorageDevice*) args;
  filesystemState.blockSize = 512;
  // Yield before we make any more calls.
  processYield();

  filesystemState.blockBuffer = (uint8_t*) malloc(filesystemState.blockSize);
  printPartitionTable(&filesystemState);

  ProcessMessage *schedulerMessage = NULL;
  while (1) {
    schedulerMessage = (ProcessMessage*) coroutineYield(NULL);
    if (schedulerMessage != NULL) {
      // We have a message from the scheduler that we need to process.  This
      // is not the expected case, but it's the priority case, so we need to
      // list it first.
      FilesystemCommandResponse messageType
        = (FilesystemCommandResponse) processMessageType(schedulerMessage);
      if (messageType < NUM_FILESYSTEM_COMMANDS) {
        filesystemCommandHandlers[messageType](
          &filesystemState, schedulerMessage);
      } else {
        printString("ERROR!!!  Received unknown filesystem command ");
        printInt(messageType);
        printString(" from scheduler.\n");
      }
    } else {
      handleFilesystemMessages(&filesystemState);
    }
  }

  return NULL;
}

/// @fn FILE* filesystemFOpen(const char *pathname, const char *mode)
///
/// @brief NanoOs implementation of standard fopen command.
///
/// @param pathname Pointer to a C string with the full path to the file to
///   open.
/// @param mode Pointer to a C string containing one of the standard file mode
///   combinations.
///
/// @return Returns a pointer to a newly-allocated and initialized FILE object
/// on success, NULL on failure.
FILE* filesystemFOpen(const char *pathname, const char *mode) {
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_OPEN_FILE,
    /* func= */ (intptr_t) mode, /* data= */ (intptr_t) pathname, true);
  processMessageWaitForDone(processMessage, NULL);
  FILE *returnValue = nanoOsMessageDataPointer(processMessage, FILE*);
  processMessageRelease(processMessage);
  return returnValue;
}

/// @fn int filesystemFClose(FILE *stream)
///
/// @brief Close a file previously opened with filesystemFOpen.
///
/// @param stream A pointer to the FILE object that was previously opened.
///
/// @return Returns 0 on success, EOF on failure.  On failure, the value of
/// errno is also set to the appropriate error.
int filesystemFClose(FILE *stream) {
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, FILESYSTEM_CLOSE_FILE,
    /* func= */ 0, /* data= */ (intptr_t) stream, true);
  processMessageWaitForDone(processMessage, NULL);
  processMessageRelease(processMessage);
  return 0;
}

