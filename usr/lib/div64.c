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

#include <stdint.h>

/// @fn uint64_t __udivdi3(uint64_t numerator, uint64_t denominator)
///
/// @brief Implementation of unsigned 64-bit division for GCC's libgcc.
///
/// @param numerator The number to divide.
/// @param denominator The number to divide by.
///
/// @return Returns the quotient of the division operation.
uint64_t __udivdi3(uint64_t numerator, uint64_t denominator) {
  uint64_t quotient = 0;
  uint64_t remainder = 0;
  
  // Handle division by zero
  if (denominator == 0) {
    return 0xFFFFFFFFFFFFFFFFULL;  // Return all ones on divide by zero
  }
  
  // Special case for division by 1
  if (denominator == 1) {
    return numerator;
  }
  
  // Handle case where numerator < denominator
  if (numerator < denominator) {
    return 0;
  }
  
  // Count leading zeros to normalize
  int shift = 0;
  uint64_t tempDenominator = denominator;
  while ((tempDenominator & 0x8000000000000000ULL) == 0) {
    tempDenominator <<= 1;
    shift++;
  }
  
  // Initialize the remainder and shift denominator
  remainder = numerator;
  denominator <<= shift;
  
  // Main division loop
  for (int ii = 0; ii <= shift; ii++) {
    quotient <<= 1;
    if (remainder >= denominator) {
      remainder -= denominator;
      quotient |= 1;
    }
    denominator >>= 1;
  }
  
  return quotient;
}

/// @fn uint64_t __umoddi3(uint64_t numerator, uint64_t denominator)
///
/// @brief Implementation of unsigned 64-bit modulo for GCC's libgcc.
///
/// @param numerator The number to divide.
/// @param denominator The number to divide by.
///
/// @return Returns the remainder of the division operation.
uint64_t __umoddi3(uint64_t numerator, uint64_t denominator) {
  uint64_t remainder = 0;
  
  // Handle division by zero
  if (denominator == 0) {
    return numerator;  // Return numerator on divide by zero
  }
  
  // Special case for division by 1
  if (denominator == 1) {
    return 0;
  }
  
  // Handle case where numerator < denominator
  if (numerator < denominator) {
    return numerator;
  }
  
  // Count leading zeros to normalize
  int shift = 0;
  uint64_t tempDenominator = denominator;
  while ((tempDenominator & 0x8000000000000000ULL) == 0) {
    tempDenominator <<= 1;
    shift++;
  }
  
  // Initialize the remainder and shift denominator
  remainder = numerator;
  denominator <<= shift;
  
  // Main division loop
  for (int ii = 0; ii <= shift; ii++) {
    if (remainder >= denominator) {
      remainder -= denominator;
    }
    denominator >>= 1;
  }
  
  return remainder;
}

