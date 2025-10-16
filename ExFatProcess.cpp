///////////////////////////////////////////////////////////////////////////////
///
/// @file              ExFatProcess.c
///
/// @brief             exFAT process implementation for NanoOs.
///
///////////////////////////////////////////////////////////////////////////////

// Arduino includes
#include <Arduino.h>
#include <HardwareSerial.h>

#include "ExFatProcess.h"
#include "ExFatFilesystem.h"

/// @typedef ExFatCommandHandler
///
/// @brief Definition of a filesystem command handler function.
typedef int (*ExFatCommandHandler)(ExFatDriverState*, ProcessMessage*);

/// @fn int exFatProcessOpenFileCommandHandler(
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
int exFatProcessOpenFileCommandHandler(
  ExFatDriverState *driverState, ProcessMessage *processMessage
) {
  NanoOsFile *nanoOsFile = NULL;
  const char *pathname = nanoOsMessageDataPointer(processMessage, char*);
  const char *mode = nanoOsMessageFuncPointer(processMessage, char*);
  if (driverState->driverStateValid) {
    ExFatFileHandle *exFatFile = exFatOpenFile(driverState, pathname, mode);
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

/// @fn int exFatProcessCloseFileCommandHandler(
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
int exFatProcessCloseFileCommandHandler(
  ExFatDriverState *driverState, ProcessMessage *processMessage
) {
  (void) driverState;

  FilesystemFcloseParameters *fcloseParameters
    = nanoOsMessageDataPointer(processMessage, FilesystemFcloseParameters*);
  ExFatFileHandle *exFatFile = (ExFatFileHandle*) fcloseParameters->stream->file;
  free(fcloseParameters->stream);
  if (driverState->driverStateValid) {
    fcloseParameters->returnValue = exFatFclose(driverState, exFatFile);
    if (driverState->filesystemState->numOpenFiles > 0) {
      driverState->filesystemState->numOpenFiles--;
    }
  }

  processMessageSetDone(processMessage);
  return 0;
}

/// @fn int exFatProcessReadFileCommandHandler(
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
int exFatProcessReadFileCommandHandler(
  ExFatDriverState *driverState, ProcessMessage *processMessage
) {
  FilesystemIoCommandParameters *filesystemIoCommandParameters
    = nanoOsMessageDataPointer(processMessage, FilesystemIoCommandParameters*);
  int32_t returnValue = 0;
  if (driverState->driverStateValid) {
    uint32_t length = filesystemIoCommandParameters->length;
    if (length > 0x7fffffff) {
      // Make sure we don't overflow the maximum value of a signed 32-bit int.
      length = 0x7fffffff;
    }
    returnValue = exFatRead(driverState,
      filesystemIoCommandParameters->buffer,
      length, 
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

/// @fn int exFatProcessWriteFileCommandHandler(
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
int exFatProcessWriteFileCommandHandler(
  ExFatDriverState *driverState, ProcessMessage *processMessage
) {
  FilesystemIoCommandParameters *filesystemIoCommandParameters
    = nanoOsMessageDataPointer(processMessage, FilesystemIoCommandParameters*);
  int32_t returnValue = 0;
  if (driverState->driverStateValid) {
    uint32_t length = filesystemIoCommandParameters->length;
    if (length > 0x7fffffff) {
      // Make sure we don't overflow the maximum value of a signed 32-bit int.
      length = 0x7fffffff;
    }
    returnValue = exFatWrite(driverState,
      filesystemIoCommandParameters->buffer,
      length,
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

/// @fn int exFatProcessRemoveFileCommandHandler(
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
int exFatProcessRemoveFileCommandHandler(
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

/// @fn int exFatProcessSeekFileCommandHandler(
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
int exFatProcessSeekFileCommandHandler(
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
  exFatProcessOpenFileCommandHandler,   // FILESYSTEM_OPEN_FILE
  exFatProcessCloseFileCommandHandler,  // FILESYSTEM_CLOSE_FILE
  exFatProcessReadFileCommandHandler,   // FILESYSTEM_READ_FILE
  exFatProcessWriteFileCommandHandler,  // FILESYSTEM_WRITE_FILE
  exFatProcessRemoveFileCommandHandler, // FILESYSTEM_REMOVE_FILE
  exFatProcessSeekFileCommandHandler,   // FILESYSTEM_SEEK_FILE
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
  fs->blockSize = fs->blockDevice->blockSize;
  
  fs->blockBuffer = (uint8_t*) malloc(fs->blockSize);
  getPartitionInfo(fs);
  exFatInitialize(driverState, fs);
  
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

/// @fn long exFatProcessFTell(FILE *stream)
///
/// @brief Get the current value of the position indicator of a
/// previously-opened file.
///
/// @param stream A pointer to a previously-opened file.
///
/// @return Returns the current position of the file on success, -1 on failure.
long exFatProcessFTell(FILE *stream) {
  if (stream == NULL) {
    return -1;
  }

  return (long) ((ExFatFileHandle*) stream->file)->currentPosition;
}

