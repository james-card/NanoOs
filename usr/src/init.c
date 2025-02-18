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

#include "NanoOsSystemCalls.h"

void _start(void) {
  const char *message = "Hello, world!\n";
  
  // Calculate string length
  int length = 0;
  const char *ptr = message;
  while (*ptr++) {
    length++;
  }
  
  // Write to stdout using syscall
  register int a0 asm("a0") = NANO_OS_STDOUT_FILENO; // stdout file descriptor
  register const char *a1 asm("a1") = message;       // buffer address
  register int a2 asm("a2") = length;                // length
  register int a7 asm("a7") = NANO_OS_SYSCALL_WRITE; // write syscall

  asm volatile(
    "ecall"
    : "+r"(a0)
    : "r"(a1), "r"(a2), "r"(a7)
  );
  
  // Exit syscall
  a0 = 0;                    // exit code 0
  a7 = NANO_OS_SYSCALL_EXIT; // exit syscall
  
  asm volatile(
    "ecall"
    : : "r"(a0), "r"(a7)
  );
}
