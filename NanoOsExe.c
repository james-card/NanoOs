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
  uint32_t signature = 0;

  if (exePath == NULL) {
    goto exit;
  }

  FILE *exeFile = fopen(exePath, "r");
  if (exeFile == NULL) {
    goto exit;
  }

  // Get and validate the signature.
  if (fseek(exeFile, NANO_OS_EXE_SIGNATURE_OFFSET, SEEK_END) != 0) {
    goto closeFile;
  }
  if (fread(&signature, 1, sizeof(signature), exeFile) != sizeof(signature)) {
    goto closeFile;
  }
  if (signature != NANO_OS_EXE_SIGNATURE) {
    // Our signature is not at the end of this file.  It is not a NanoOs
    // executable file.  Bail.
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

NanoOsExeMetadata* nanoOsExeMetadataDestroy(
  NanoOsExeMetadata *nanoOsExeMetadata) {
  free(nanoOsExeMetadata);
  return NULL;
}

int nanoOsExeMetadataV1Write(
  const char *fullFilePath, const char *programPath
) {
  (void) fullFilePath;
  (void) programPath;
  return 0;
}


