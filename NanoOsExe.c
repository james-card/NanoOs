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
#include <stdio.h>

NanoOsExeMetadata* nanoOsExeMetadataRead(const char *exePath) {
  NanoOsExeMetadata *returnValue = NULL;
  uint32_t fileVar = 0;

  FILE *exeFile = fopen(exePath, "r");
  if (exeFile == NULL) {
    return returnValue; // NULL
  }

  if (fseek(exeFile, NANO_OS_EXE_SIGNATURE_OFFSET, SEEK_END) != 0) {
    fclose(exeFile);
    return returnValue; // NULL
  }

  if (fread(&fileVar, 1, sizeof(fileVar), exeFile) != sizeof(fileVar)) {
    fclose(exeFile);
    return returnValue; // NULL
  }
}

NanoOsExeMetadata* nanoOsExeMetadataDestroy(
  NanoOsExeMetadata *nanoOsExeMetadata) {
}

int nanoOsExeMetadataV1Write(const char *exePath, const char *binPath) {
}


