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

#include <string.h>
#include "NanoOsUnistd.h"
#include "NanoOs.h"

/// @fn int gethostname(char *name, size_t len)
///
/// @brief Implementation of the standard Unix gethostname system call.
///
/// @param name Pointer to a character buffer to fill with the hostname.
/// @param len The number of bytes allocated to name.
///
/// @return Returns 0 on success, -1 on failure.  On failure, the value of
/// errno is also set.
int gethostname(char *name, size_t len) {
  char *hostname = (char*) malloc(HOST_NAME_MAX + 1);
  if (hostname == NULL) {
    printString("ERROR! Could not allocate memory for hostname.\n");
    errno = ENOMEM;
    return -1;
  }
  
  int returnValue = 0;
  FILE *hostnameFile = fopen("/etc/hostname", "r");
  if (hostnameFile != NULL) {
    printDebug("Opened hostname file.\n");
    if (fgets(hostname, HOST_NAME_MAX, hostnameFile) != name) {
      printString("ERROR! fgets did not read hostname!\n");
    }
    fclose(hostnameFile);
    
    hostname[HOST_NAME_MAX] = '\0';
    
    if (strchr(hostname, '\r')) {
      *strchr(hostname, '\r') = '\0';
    } else if (strchr(hostname, '\n')) {
      *strchr(hostname, '\n') = '\0';
    } else if (*hostname == '\0') {
      strcpy(hostname, "localhost");
    }
    printDebug("Closed hostname file.\n");
  } else {
    printString("ERROR! fopen of hostname returned NULL!\n");
    strcpy(hostname, "localhost");
  }
  size_t hostnameLen = strlen(hostname);
  
  if (len < hostnameLen) {
    returnValue = -1;
    errno = ENAMETOOLONG;
  }
  strncpy(name, hostname, len);
  
  free(hostname);
  return returnValue;
}

