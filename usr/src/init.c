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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

// Declaration of user program entry point
int main(int argc, char **argv);

/// @fn void _start(void)
///
/// @brief Main entry point of the a program.
///
/// @return This function returns no value and, in fact, never returns.
__attribute__((section(".text.startup")))
__attribute__((noinline))
void _start(void) {
  char *argv[] = {
    "main"
  };

  int returnValue = main(sizeof(argv) / sizeof(argv[0]), argv);

  exit(returnValue & 0xff);
}

/// @fn int main(int argc, char **argv)
///
/// @brief Entry point for the application.
///
/// @param argc Number of command line arguments provided on the command line.
/// @param argv Array of individual arguments from the command line.
///
/// @return Returns 0 on success.  Any other value is failure.
int main(int argc, char **argv) {
  (void) argc;
  (void) argv;

  fputs("Hello, world!\n", stdout);;

  return 0;
}

