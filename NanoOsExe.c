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

  fullFile = fopen(fullFilePath, "w+");
  if (fullFile == NULL) {
    returnValue = -1;
    goto exit;
  }

  if (isNanoOsExe(fullFile)) {
    goto closeFullFile;
  }

  programFile = fopen(programPath, "r");
  if (programFile == NULL) {
    goto closeFullFile;
  }

closeProgramFile:
  fclose(programFile);

closeFullFile:
  fclose(fullFile);

exit:
  return returnValue;
}

