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

// Doxygen marker
/// @file

#include "NanoOsExe.h"

#ifdef __x86_64__
#include <stdio.h>
#include <stdlib.h>
#else
#include "NanoOsLibC.h"
#endif // __x86_64__

/// @fn uint32_t byteSwapIfNotLittleEndian(uint32_t u32Int)
///
/// @brief Byte swap a 32-bit integer value if the host is not a little endian
/// system.
///
/// @param u32Int The value to byte swap if the host is not little endian.
///
/// @return Returns the exact input value if the host is little endian, the
/// byte swapped verion of the input value if the host is not little endian.
uint32_t byteSwapIfNotLittleEndian(uint32_t u32Int) {
  union {
    int integer;
    char character;
  } littleEndianUnion = { .integer = 1 };

  if (!littleEndianUnion.character) {
    u32Int
      = ((u32Int & 0xff000000) >> 24)
      | ((u32Int & 0x00ff0000) >>  8)
      | ((u32Int & 0x0000ff00) <<  8)
      | ((u32Int & 0x000000ff) << 24);
  }

  return u32Int;
}

/// @fn bool isNanoOsExe(FILE *exeFile)
///
/// @brief Determine whether or not an opened file is a valid NanoOs executable
/// file (i.e. has the right signature in the right place).
///
/// @param exeFile A pointer to a previously-opened FILE object that is read-
///   accessible.
///
/// @return Returns true if the file is confirmed to be a valid NanoOs
/// executable, false otherwise.
bool isNanoOsExe(FILE *exeFile) {
  uint32_t signature = 0;

  // Get and validate the signature.
  if (fseek(exeFile, NANO_OS_EXE_SIGNATURE_OFFSET, SEEK_END) != 0) {
    return false;
  }
  if (fread(&signature, 1, sizeof(signature), exeFile) != sizeof(signature)) {
    return false;
  }
  if (signature != NANO_OS_EXE_SIGNATURE) {
    // Our signature is not at the end of this file.  It is not a NanoOs
    // executable file.  Bail.
    return false;
  }

  return true;
}

/// @fn NanoOsExeMetadata* nanoOsExeMetadataRead(const char *exePath)
///
/// @brief Read the metadata out of a NanoOs executable.
///
/// @param exePath The full path to the NanoOs executable binary.
///
/// @return Returns a newly-allocated, fully-populated NanoOsExeMetadata object
/// on success, NULL on failure.
NanoOsExeMetadata* nanoOsExeMetadataRead(const char *exePath) {
  NanoOsExeMetadata *returnValue = NULL;

  if (exePath == NULL) {
    goto exit;
  }

  FILE *exeFile = fopen(exePath, "r");
  if (exeFile == NULL) {
    goto exit;
  }

  if (!isNanoOsExe(exeFile)) {
    goto closeFile;
  }

  returnValue = (NanoOsExeMetadata*) malloc(sizeof(NanoOsExeMetadata));
  if (returnValue == NULL) {
    goto closeFile;
  }

  // Get the metadata version.
  if (fseek(exeFile, NANO_OS_EXE_VERSION_OFFSET, SEEK_END) != 0) {
    goto freeReturnValue;
  }
  if (fread(&returnValue->version, 1, sizeof(returnValue->version), exeFile)
    != sizeof(returnValue->version)
  ) {
    goto freeReturnValue;
  }
  if (returnValue->version > NANO_OS_EXE_METADATA_CURRENT_VERSION) {
    // We've already validated that this is one of our files by validating the
    // signature.  This is a version beyond what we know about, so just parse
    // the parts we know.
    returnValue->version = NANO_OS_EXE_METADATA_CURRENT_VERSION;
  }

  switch (returnValue->version) {
    // Cases above this (which will be more-recent versions) should fall
    // through to this.
    case 1:
      {
        if (fseek(exeFile, NANO_OS_EXE_PROGRAM_LENGTH_OFFSET, SEEK_END) != 0) {
          goto freeReturnValue;
        }
        if (fread(&returnValue->programLength, 1,
            sizeof(returnValue->programLength), exeFile)
          != sizeof(returnValue->programLength)
        ) {
          goto freeReturnValue;
        }

        if (fseek(exeFile, NANO_OS_EXE_DATA_LENGTH_OFFSET, SEEK_END) != 0) {
          goto freeReturnValue;
        }
        if (fread(&returnValue->dataLength, 1,
            sizeof(returnValue->dataLength), exeFile)
          != sizeof(returnValue->dataLength)
        ) {
          goto freeReturnValue;
        }

        break;
      }

    default:
      goto freeReturnValue;
  }

  goto closeFile;

freeReturnValue:
  free(returnValue); returnValue = NULL;

closeFile:
  fclose(exeFile);

exit:
  return returnValue;
}

/// @fn NanoOsExeMetadata* nanoOsExeMetadataDestroy(
///   NanoOsExeMetadata *nanoOsExeMetadata)
///
/// @brief Destroy a previously-allocated NanoOsExeMetadata object.
///
/// @param nanoOsExeMetadata A pointer to the allocated NanoOsExeMetadata to
///   destroy.
///
/// @return This function always succeeds and always returns NULL.
NanoOsExeMetadata* nanoOsExeMetadataDestroy(
  NanoOsExeMetadata *nanoOsExeMetadata
) {
  free(nanoOsExeMetadata);
  return NULL;
}

/// @fn int nanoOsExeMetadataV1Write(
///   const char *fullFilePath, const char *programPath)
///
/// @brief Write the metadata for a NanoOs executable to the executable.
///
/// @param fullFilePath The path to the file with the full executable content.
/// @param programPath The path to the file with the program segment content.
///
/// @return Returns 0 on success, negative value on error.
int nanoOsExeMetadataV1Write(
  const char *fullFilePath, const char *programPath
) {
  int returnValue = 0;
  FILE *fullFile = NULL, *programFile = NULL;
  uint32_t fileVar = 0;

  fullFile = fopen(fullFilePath, "w+");
  if (fullFile == NULL) {
    returnValue = -1;
    goto exit;
  }

  if (isNanoOsExe(fullFile)) {
    // Metadata is already written.  This is not an error, so, just close the
    // file and return good status.
    goto closeFullFile;
  }
  // isNanoOsExe will return false in the event of a file error, so don't trust
  // that the file is in the right state.  Do an fseek to the end ourselves and
  // verify that we're able to do it successfully.
  if (fseek(fullFile, 0, SEEK_END) != 0) {
    returnValue = -2;
    goto closeFullFile;
  }

  programFile = fopen(programPath, "r");
  if (programFile == NULL) {
    returnValue = -3;
    goto closeFullFile;
  }
  if (fseek(programFile, 0, SEEK_END) != 0) {
    returnValue = -4;
    goto closeProgramFile;
  }

  // Version 1 metadata format:
  // Data Length
  // Program Length
  // Version Number (1 for this function)
  // NanoOs Executable Signature

  // Data Length = (fullFile length) - (programFile length)
  fileVar = ((uint32_t) ftell(fullFile)) - ((uint32_t) ftell(programFile));
  if (fwrite(&fileVar, 1, sizeof(fileVar), programFile) != sizeof(fileVar)) {
    returnValue = -5;
    goto closeProgramFile;
  }

  // Write the program length next
  fileVar = (uint32_t) ftell(programFile);
  if (fwrite(&fileVar, 1, sizeof(fileVar), programFile) != sizeof(fileVar)) {
    returnValue = -6;
    goto closeProgramFile;
  }

  // Version is next
  fileVar = 1;
  if (fwrite(&fileVar, 1, sizeof(fileVar), programFile) != sizeof(fileVar)) {
    returnValue = -7;
    goto closeProgramFile;
  }

  // Signature is last
  fileVar = NANO_OS_EXE_SIGNATURE;
  if (fwrite(&fileVar, 1, sizeof(fileVar), programFile) != sizeof(fileVar)) {
    returnValue = -8;
    goto closeProgramFile;
  }

closeProgramFile:
  fclose(programFile);

closeFullFile:
  fclose(fullFile);

exit:
  return returnValue;
}

