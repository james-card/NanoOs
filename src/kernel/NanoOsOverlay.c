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
#include "Hal.h"
#include "NanoOs.h"
#include "NanoOsOverlay.h"

// Must come last
#include "../user/NanoOsStdio.h"

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

  printDebugString(__func__);
  printDebugString(": Reading from overlayFile 0x");
  printDebugHex((uintptr_t) overlayFile);
  printDebugString("\n");
  if (fread(HAL->overlayMap, 1, HAL->overlaySize, overlayFile) == 0) {
    fprintf(stderr, "Could not read overlay from \"%s\" file.\n",
      overlayPath);
    fclose(overlayFile); overlayFile = NULL;
    return -EIO;
  }
  printDebugString(__func__);
  printDebugString(": Closing overlayFile 0x");
  printDebugHex((uintptr_t) overlayFile);
  printDebugString("\n");
  fclose(overlayFile); overlayFile = NULL;

  printDebugString("Verifying overlay magic\n");
  if (HAL->overlayMap->header.magic != NANO_OS_OVERLAY_MAGIC) {
    fprintf(stderr, "Overlay magic for \"%s\" was not \"NanoOsOL\".\n",
      overlayPath);
    return -ENOEXEC;
  }
  printDebugString("Verifying overlay version\n");
  if (HAL->overlayMap->header.version
    != ((0 << 24) | (0 << 16) | (1 << 8) | (0 << 0))
  ) {
    fprintf(stderr, "Overlay version is 0x%08x for \"%s\"\n",
      HAL->overlayMap->header.version, overlayPath);
    return -ENOEXEC;
  }

  // Set the pieces of the overlay header that the program needs to run.
  printDebugString("Configuring overlay environment\n");
  HAL->overlayMap->header.osApi = &nanoOsApi;
  HAL->overlayMap->header.env = env;
  
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
  
  for (uint16_t ii = 0, jj = HAL->overlayMap->numExports - 1; ii <= jj;) {
    cur = (ii + jj) >> 1;
    comp = strcmp(HAL->overlayMap->exports[cur].name, overlayFunctionName);
    if (comp == 0) {
      overlayFunction = HAL->overlayMap->exports[cur].fn;
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
  size_t entrypointLength = strlen(BIN_ENTRYPOINT);
  char *fullPath = (char*) malloc(strlen(commandPath) + entrypointLength + 1);
  if (fullPath == NULL) {
    return COMMAND_CANNOT_EXECUTE;
  }
  strcpy(fullPath, commandPath);
  strcat(fullPath, BIN_ENTRYPOINT);
  int loadStatus = loadOverlay(fullPath, env);
  free(fullPath); fullPath = NULL;
  if (loadStatus == -ENOENT) {
    // Error message already printed.
    return COMMAND_NOT_FOUND;
  } else if (loadStatus < 0) {
    // Error message already printed.
    return COMMAND_CANNOT_EXECUTE;
  }
  printDebugString("Overlay loaded successfully\n");

  OverlayFunction _start = findOverlayFunction("_start");
  if (_start == NULL) {
    fprintf(stderr,
      "Could not find exported _start function in \"%s\" overlay.\n",
      commandPath);
    return 1;
  }
  printDebugString("Found _start function\n");

  MainArgs mainArgs = {
    .argc = argc,
    .argv = argv,
  };
  printDebugString("Calling _start function at address 0x");
  printDebugHex((uintptr_t) _start);
  printDebugString("\n");
  int returnValue = (int) ((intptr_t) _start(&mainArgs));
  printDebugString("Got return value ");
  printDebugInt(returnValue);
  printDebugString(" from _start function\n");
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

