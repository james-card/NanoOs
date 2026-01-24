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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "NanoOsUser.h"

int main(int argc, char **argv) {
  (void) argc;
  
  char *buffer = (char*) malloc(96);
  if (buffer == NULL) {
    fprintf(stderr, "ERROR! Could not allocate space for buffer in %s.\n",
      argv[0]);
    return 1;
  }
  *buffer = '\0';
  
  intptr_t returnValue = 0;
  do {
    fputs("$ ", stdout);
    char *input = fgets(buffer, 96, stdin);
    if ((input != NULL) && (strlen(input) > 0)
      && (input[strlen(input) - 1] == '\n')
    ) {
      input[strlen(input) - 1] = '\0';
    }
    
    // The variable 'input' is the same as the variable 'buffer', which is a
    // pointer to dynamic memory.  So, it's safe to pass as a parameter to
    // callOverlayFunction.
    returnValue = (intptr_t) callOverlayFunction(
      "Builtins", "processBuiltin", input);
    if (returnValue < -1)  {
      fputs(input, stdout);
      fputs(": command not found\n", stdout);
    }
  } while (returnValue != -1);
  
  free(buffer);
  
  printf("Gracefully exiting %s\n", argv[0]);
  return 0;
}

