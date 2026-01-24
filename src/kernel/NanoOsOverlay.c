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
#include "NanoOsOverlayFunctions.h"
#include "../user/NanoOsLibC.h"
#include "Tasks.h"

// Must come last
#include "../user/NanoOsStdio.h"

/// @fn int loadOverlay(const char *overlayDir, const char *overlay,
///   char **envp)
///
/// @brief Load and configure an overlay into the overlayMap in memory.
///
/// @param overlayDir The full path to the directory of the overlay on the
///   filesystem.
/// @param overlay The name of the overlay within the overlayDir to load (minus
///   the ".overlay" file extension).
/// @param envp The array of environment variables in "name=value" form.
///
/// @return Returns 0 on success, negative error code on failure.
int loadOverlay(const char *overlayDir, const char *overlay, char **envp) {
  if ((overlayDir == NULL) || (overlay == NULL)) {
    // There's no overlay to load.  This isn't really an error, but there's
    // nothing to do.  Just return 0.
    return 0;
  }

  NanoOsOverlayMap *overlayMap = HAL->overlayMap();
  uintptr_t overlaySize = HAL->overlaySize();
  if ((overlayMap == NULL) || (overlaySize == 0)) {
    fprintf(stderr, "No overlay memory available for use.\n");
    return -ENOMEM;
  }

  NanoOsOverlayHeader *overlayHeader = &overlayMap->header;
  if ((overlayHeader->overlayDir != NULL) && (overlayHeader->overlay != NULL)) {
    if ((strcmp(overlayHeader->overlayDir, overlayDir) == 0)
      && (strcmp(overlayHeader->overlay, overlay) == 0)
    ) {
      // Overlay is already loaded.  Do nothing.
      return 0;
    }
  }

  // We need two extra characters:  One for the '/' that separates the directory
  // and the file name and one for the terminating NULL byte.
  char *fullPath = (char*) malloc(
    strlen(overlayDir) + strlen(overlay) + OVERLAY_EXT_LEN + 2);
  if (fullPath == NULL) {
    return -ENOMEM;
  }
  strcpy(fullPath, overlayDir);
  strcat(fullPath, "/");
  strcat(fullPath, overlay);
  strcat(fullPath, OVERLAY_EXT);
  FILE *overlayFile = fopen(fullPath, "r");
  if (overlayFile == NULL) {
    fprintf(stderr, "Could not open file \"%s\" from the filesystem.\n",
      fullPath);
    free(fullPath); fullPath = NULL;
    return -ENOENT;
  }

  printDebugString(__func__);
  printDebugString(": Reading from overlayFile 0x");
  printDebugHex((uintptr_t) overlayFile);
  printDebugString("\n");
  if (fread(overlayMap, 1, overlaySize, overlayFile) == 0) {
    fprintf(stderr, "Could not read overlay from \"%s\" file.\n",
      fullPath);
    fclose(overlayFile); overlayFile = NULL;
    free(fullPath); fullPath = NULL;
    return -EIO;
  }
  printDebugString(__func__);
  printDebugString(": Closing overlayFile 0x");
  printDebugHex((uintptr_t) overlayFile);
  printDebugString("\n");
  fclose(overlayFile); overlayFile = NULL;

  printDebugString("Verifying overlay magic\n");
  if (overlayMap->header.magic != NANO_OS_OVERLAY_MAGIC) {
    fprintf(stderr, "Overlay magic for \"%s\" was not \"NanoOsOL\".\n",
      fullPath);
    free(fullPath); fullPath = NULL;
    return -ENOEXEC;
  }
  printDebugString("Verifying overlay version\n");
  if (overlayMap->header.version != NANO_OS_OVERLAY_VERSION) {
    fprintf(stderr, "Overlay version is 0x%08x for \"%s\"\n",
      overlayMap->header.version, fullPath);
    free(fullPath); fullPath = NULL;
    return -ENOEXEC;
  }
  free(fullPath); fullPath = NULL;

  // Set the pieces of the overlay header that the program needs to run.
  printDebugString("Configuring overlay environment\n");
  overlayHeader->osApi = &nanoOsApi;
  overlayHeader->env = envp;
  overlayHeader->overlayDir = overlayDir;
  overlayHeader->overlay = overlay;
  
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
  
  NanoOsOverlayMap *overlayMap = HAL->overlayMap();
  for (uint16_t ii = 0, jj = overlayMap->numExports - 1; ii <= jj;) {
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

/// @fn void* callOverlayFunction(const char *overlay, const char *function,
///   void *args)
///
/// @brief Kernel function to load an overlay into memory, find a designated
/// function, call it with the provided arguments, and return the return value.
///
/// @param overlay The name of the overlay minus the ".overlay" file extension
///   that is local to getRunningTask()->overlayDir.
/// @param function The name of the function exported by the overlay.
/// @param args Any arguments to be passed to the function in the overlay, cast
///   to a void*.  This parameter may be NULL.
///
/// @return Returns the value returned by the overlay function on success, NULL
/// on failure.
void* callOverlayFunction(const char *overlay, const char *function,
  void *args
) {
  void *returnValue = NULL;
  
  if ((overlay == NULL) || (function == NULL)) {
    fprintf(stderr,
      "ERROR: One or more NULL arguments provided to callOverlayFunction\n");
    return returnValue;
  }
  
  TaskDescriptor *runningTask = getRunningTask();
  if (runningTask == NULL) {
    // This should be impossible.
    return returnValue;
  }
  
  // Keep track of the overlay that's currently running.
  const char *previousOverlay = runningTask->overlay;
  
  // We have to copy the arguments we were provided into dynamic memory because
  // they may be pointers into the current overlay, which we're about to
  // replace.
  char *overlayCopy = (char*) malloc(strlen(overlay) + 1);
  if (overlayCopy == NULL) {
    goto exit;
  }
  strcpy(overlayCopy, overlay);
  
  char *functionCopy = (char*) malloc(strlen(function) + 1);
  if (functionCopy == NULL) {
    goto freeOverlay;
  }
  strcpy(functionCopy, function);
  
  // args does not get copied.  We have no way of knowing what the proper way
  // to copy the data would be.  The caller is responsible for allocating data
  // in dynamic memory before the pointer is passed to this function.
  
  // We want to load the new overlay in place of the one that's currently
  // there, but we want to give other processes some time to run, too.  Also,
  // loading the overlay is an operation that shouldn't be interrupted.   The
  // scheduler will automatically load the correct overlay before a process is
  // resumed, so just set the pointer of the task's overlay and then yield.
  //
  // A few things of note:
  //
  // 1.  Because the scheduler will automatically load the correct overlay
  //     before a task is resumed, it's technically permissible to set the
  //     task's overlay pointer and then load the overlay, however, that's
  //     redundant.  It would *NOT* be permissible to load the overlay and then
  //     set the pointer as that could create an overlay of mixed content if
  //     loading the overlay was preempted.
  //
  // 2.  Setting the pointer for the overlay has to be an atomic operation.
  //     We cannot assume that it will be so and use '=' because this system
  //     may run on an 8-bit processor where loading a pointer requires
  //     multiple fetches from memory.  The reason it has to be atomic is
  //     because the value of the pointer is used by the scheduler, which is
  //     outside the context of this process.  If we were to load, say, only
  //     one byte of a two-byte pointer and then do a context switch, the
  //     scheduler would attempt to load the overlay from a mangled pointer
  //     before the context of this process was restored to complete the load.
  //     That's why we have to use atomic_store here.
  //
  // JBC 2025-01-24
  atomic_store(&runningTask->overlay, overlayCopy);
  taskYield();
  
  // If we made it this far, then our new overlay has been successuflly loaded.
  OverlayFunction overlayFunction = findOverlayFunction(functionCopy);
  if (overlayFunction == NULL) {
    fprintf(stderr, "ERROR: Could not find overlay function \"%s\"\n",
      functionCopy);
    goto restorePreviousOverlay;
  }
  
  // The overlay function was found.  Get our return value.
  returnValue = overlayFunction(args);
  
restorePreviousOverlay:
  // See note above on use of atomic_store.
  atomic_store(&runningTask->overlay, previousOverlay);
  // Make the scheduler load the overlay back into memory.
  taskYield();
  free(functionCopy);
freeOverlay:
  free(overlayCopy);
exit:
  return returnValue;
}

/// @fn int runOverlayCommand(const char *commandPath,
///   int argc, char **argv, char **envp)
///
/// @brief Run a command that's in overlay format on the filesystem.
///
/// @param commandPath The full path to the command overlay file on the
///   filesystem.
/// @param argc The number of arguments from the command line.
/// @param argv The of arguments from the command line as an array of C strings.
/// @param envp The array of environment variable strings where each element is
///   in "name=value" form.
///
/// @return Returns 0 on success, a valid SUS value on failure.
int runOverlayCommand(const char *commandPath,
  int argc, char **argv, char **envp
) {
  int loadStatus = loadOverlay(commandPath, "main", envp);
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

