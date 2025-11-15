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

#include "Commands.h"
#include "NanoOsOverlay.h"

/// @fn int loadOverlay(const char *overlayPath, char **env)
///
/// @brief Load and configure an overlay into the overlayMap in memory.
///
/// @param overlayPath The full path to the overlay file on the filesystem.
/// @param env The array of environment variables in "name=value" form.
///
/// @return Returns 0 on success, negative error code on failure.
int loadOverlay(const char *overlayPath, char **env) {
  FILE *overlayFile = fopen(overlayPath, "r");
  if (overlayFile == NULL) {
    fprintf(stderr, "Could not open file \"%s\" from the filesystem.\n",
      overlayPath);
    return -ENOENT;
  }

  if (fread(overlayMap, 1, OVERLAY_SIZE, overlayFile) == 0) {
    fprintf(stderr, "Could not read overlay from \"%s\" file.\n",
      overlayPath);
    fclose(overlayFile); overlayFile = NULL;
    return -EIO;
  }
  fclose(overlayFile); overlayFile = NULL;

  if (overlayMap->header.magic != NANO_OS_OVERLAY_MAGIC) {
    fprintf(stderr, "Overlay magic for \"%s\" was not \"NanoOsOL\".\n",
      overlayPath);
    return -(EEND + 1);
  }
  if (overlayMap->header.version
    != ((0 << 24) | (0 << 16) | (1 << 8) | (0 << 0))
  ) {
    fprintf(stderr, "Overlay version is 0x%08x for \"%s\"\n",
      overlayMap->header.version, overlayPath);
    return -(EEND + 2);
  }

  // Set the pieces of the overlay header that the program needs to run.
  overlayMap->header.unixApi = &nanoOsUnixApi;
  overlayMap->header.env = env;
  
  return 0;
}

/// @fn OverlayFunction findOverlayFunction(const char *overlayFunctionName)
///
/// @brief Find a function in an overlay that's been previously loaded into RAM.
///
/// @param overlayFunctionName The name of the function in the overlay to find.
///
/// @return Returns a pointer to the found function on success, NULL on failure.
OverlayFunction findOverlayFunction(const char *overlayFunctionName) {
  uint16_t cur = 0;
  int comp = 0;
  OverlayFunction overlayFunction = NULL;
  
  for (uint16_t ii = 0, jj = overlayMap->header.numExports - 1; ii <= jj;) {
    cur = (ii + jj) >> 1;
    comp = strcmp(overlayMap->exports[cur].name, overlayFunctionName);
    if (comp == 0) {
      overlayFunction = overlayMap->exports[cur].fn;
      break;
    } else if (comp < 0) { // cur < overlayFunctionName
      // Move the left bound to one greater than cur.
      ii = cur + 1;
    } else { // comp > 0, overlayFunctionName < cur
      // Move the right bound to one less than cur.
      jj = cur - 1;
    }
  }
  
  return overlayFunction;
}

/// @fn int runOverlayCommand(const char *commandPath,
///   int argc, char **argv, char **env)
///
/// @brief Run a command that's in overlay format on the filesystem.
///
/// @param commandPath The full path to the command overlay file on the
///   filesystem.
/// @param argc The number of arguments from the command line.
/// @param argv The of arguments from the command line as an array of C strings.
/// @param env The array of environment variable strings where each element is
///   in "name=value" form.
///
/// @return Returns 0 on success, a valid SUS value on failure.
int runOverlayCommand(const char *commandPath,
  int argc, char **argv, char **env
) {
  int loadStatus = loadOverlay(commandPath, env);
  if (loadStatus == -ENOENT) {
    // Error message already printed.
    return COMMAND_NOT_FOUND;
  } else if (loadStatus == -EIO) {
    // Error message already printed.
    return COMMAND_CANNOT_EXECUTE;
  }

  OverlayFunction _start = findOverlayFunction("_start");
  if (_start == NULL) {
    fprintf(stderr,
      "Could not find exported _start function in \"%s\" overlay.\n",
      commandPath);
    return 1;
  }

  MainArgs mainArgs = {
    .argc = argc,
    .argv = argv,
  };
  int returnValue = (int) ((intptr_t) _start(&mainArgs));
  if (returnValue != ENOERR) {
    fprintf(stderr,
      "Got unexpected return value %d from _start in \"%s\"\n",
      returnValue, commandPath);
  }
  if ((returnValue < 0) || (returnValue > 255)) {
    // Invalid return value.
    returnValue = COMMAND_EXIT_INVALID;
  }

  return returnValue;
}

