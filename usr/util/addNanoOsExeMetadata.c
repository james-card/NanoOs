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

// Standard C includes
#include <stdio.h>
#include <string.h>

// NanoOs includes
#include "NanoOsExe.h"

/// @fn void usage(const char *argv0)
///
/// @brief Print the usage statement for this command.
///
/// @param argv0 The pointer provided as argv[0] to the main function.
///
/// @return This function returns no value.
void usage(const char *argv0) {
  const char *programName = strrchr(argv0, '/');
  if (programName == NULL) {
    programName = strrchr(argv0, '\\');
  }

  if (programName != NULL) {
    programName++;
  } else {
    programName = argv0;
  }

  printf("Usage:  %s <full binary> <program binary>\n", programName);
}

/// @fn int main(int argc, char **argv)
///
/// @brief Main entry point for the command.
///
/// @param argc The number of arguments parsed from the command line.
/// @param argv The array of individual C string command line arguments parsed
///   from the command line.
///
/// @return Returns zero on success, non-zero on error.
int main(int argc, char **argv) {
  if (argc != 3) {
    usage(argv[0]);
    return 1;
  }

  const char *fullFilePath = argv[1];
  const char *programPath  = argv[2];

  int returnValue = nanoOsExeMetadataV1Write(fullFilePath, programPath);
  if (returnValue != 0) {
    fprintf(stderr, "ERROR: Could not write metadata to \"%s\".\n",
      fullFilePath);
    fprintf(stderr, "nanoOsExeMetadataV1Write returned status %d.\n",
      returnValue);
    return 1;
  }

  return 0;
}

