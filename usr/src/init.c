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
#include <stdint.h>
#include <stddef.h>

// Declaration of user program entry point
int main(int argc, char **argv);

// string.h functions

/// @fn size_t strlen(const char *str)
///
/// @brief Implementation of the standard C strlen function.  Calculates the
/// number of characters in a given C string, not including the terminating
/// NUL byte.
///
/// @param str A pointer to the C string to examine in memory.
///
/// @return Returns the total number of non-NUL characters in the provided C
/// string.
static inline size_t strlen(const char *str) {
  size_t length = 0;
  for (; *str; str++, length++);
  return length;

  /*
   * In theory, the below algorithm would be fast than what we're doing above.
   * HOWEVER, this implemenation is optimized for the host environment (the
   * Arduino Nano Every) as opposed to the virtual environment (the RV32I VM).
   * This is important.  Using the algorithm below is over *EIGHT TIMES* slower
   * per instruction than using the algorihm above.  Despite the fact that the
   * algorithm below uses fewer than 1/4 of the VM instructions, the algorithm
   * above actually executes in less absolute time for the same string.  The
   * reason for this is the fact that the Nano Every uses the 8-bit ATMEGA4809
   * processor.  It takes significantly more host instructions and bus time
   * to do 32-bit calculations than it does to do the simple 8-bit comparison
   * above.  I'm leaving this implementation optimized for the host environment
   * so that the total execution time of programs written against it will
   * execute as fas as possible on the target host environment.
   *
   * JBC 2025-02-19
   *
   * const char *s = str;
   * const uint32_t *wordPtr;
   * 
   * 
   * // Align to word boundary
   * while (((uintptr_t) s) & (sizeof(void*) - 1)) {
   *   if (*s == '\0') {
   *     return (size_t) (s - str);
   *   }
   *   s++;
   * }
   * 
   * 
   * // Now check a word at a time
   * wordPtr = (const uint32_t*) s;
   * while (1) {
   *   uint32_t word = *wordPtr;
   *   
   *   // Check if any byte in this word is zero
   *   // Using magic number to detect zero byte
   *   if ((word - 0x01010101) & ~word & 0x80808080) {
   *     // Found null terminator, now find exact position
   *     s = (const char*) wordPtr;
   *     for (;  *s; s++);
   *     return (size_t) (s - str);
   *   }
   *   wordPtr++;
   * }
   */
}

/// @fn int fputs(const char *s, FILE *stream)
///
/// @brief Implementation of the standard C fputs function.  Prints the provided
/// string to the provided FILE stream.
///
/// @param s A pointer to the C string to print.
/// @param stream A pointer to the FILE stream to direct the string to.
///
/// @return Returns the number of characters written on success, EOF on failure.
static inline int fputs(const char *s, FILE *stream) {
  // Calculate string length
  size_t length = strlen(s);
  size_t numBytesWritten = fwrite(s, 1, length, stream);
  
  return (int) numBytesWritten;
}

// stdlib.h functions

/// @fn void exit(int status)
///
/// @brief Implementation of the standard C exit function.  Returns the provided
/// status value back to the host operating system.
///
/// @param status The integer value to return back to the OS.
///
/// @return This function returns no value and, in fact, never returns.
static inline void exit(int status) {
  register int a0 asm("a0") = status;               // exit code 0
  register int a7 asm("a7") = NANO_OS_SYSCALL_EXIT; // exit syscall
  
  asm volatile(
    "ecall"
    : : "r"(a0), "r"(a7)
  );
}

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

