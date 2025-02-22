///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              02.20.2025
///
/// @file              string.h
///
/// @brief             Implementation of string.h for NanoOs C programs.
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
/// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///                                James Card
///                         http://www.jamescard.org
///
///////////////////////////////////////////////////////////////////////////////

#ifndef STRING_H
#define STRING_H

#ifdef __cplusplus
extern "C"
{
#endif

// Standard C includes
#include <stddef.h>

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

/// @fn int strncmp(const char *s1, const char *s2, size_t n)
///
/// @brief Compare two strings up to a provided number of characters.
///
/// @param s1 The first string to compare.
/// @param s2 The second string to compare.
/// @param n The maximum number of characters to compare.
///
/// @return Returns a value less than zero if s1 is logically less than s2
/// within the first n characters, zero if the two strings are equal for the
/// entirety of the n characters, and a value greater than zero if s1 is
/// logically greater thsn s2.
int strncmp(const char *s1, const char *s2, size_t n) {
  int returnValue = 0;

  while ((*s1) && (*s2) && (returnValue == 0)) {
    returnValue = ((int) *s1) - ((int) *s2);
  }

  return returnValue;
}

/// @def strcmp
///
/// @brief Compare the entirety of two strings until one of them ends.
#define strcmp(s1, s2) strncmp(s1, s2, (size_t) -1)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // STRING_H

