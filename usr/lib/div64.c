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

/// @fn int64_t __divdi3(int64_t numerator, int64_t denominator)
///
/// @brief Implementation of signed 64-bit division for GCC's libgcc.
///
/// @param numerator The number to divide.
/// @param denominator The number to divide by.
///
/// @return Returns the quotient of the division operation.
int64_t __divdi3(int64_t numerator, int64_t denominator) {
  // Handle special cases for INT64_MIN
  const int64_t INT64_MIN = (int64_t) 0x8000000000000000ULL;
  if (numerator == INT64_MIN) {
    if (denominator == -1) {
      return INT64_MIN;  // Would overflow, return INT64_MIN
    } else if (denominator == 1) {
      return INT64_MIN;
    }
  }
  
  // Get the signs
  int numNegative = (numerator < 0);
  int denNegative = (denominator < 0);
  
  // Convert to positive values
  uint64_t positiveNum = (numerator < 0) ? -numerator : numerator;
  uint64_t positiveDen = (denominator < 0) ? -denominator : denominator;
  
  // Perform unsigned division
  uint64_t quotient = __udivdi3(positiveNum, positiveDen);
  
  // Apply the correct sign to the result
  if (numNegative != denNegative) {
    // If only one of the operands was negative, result should be negative
    if (quotient > (uint64_t) INT64_MIN) {
      return -(int64_t) quotient;
    } else {
      return INT64_MIN;
    }
  }
  
  return (int64_t) quotient;
}

/// @fn int64_t __moddi3(int64_t numerator, int64_t denominator)
///
/// @brief Implementation of signed 64-bit modulo for GCC's libgcc.
///
/// @param numerator The number to divide.
/// @param denominator The number to divide by.
///
/// @return Returns the remainder of the division operation.
int64_t __moddi3(int64_t numerator, int64_t denominator) {
  // Handle special cases for INT64_MIN
  const int64_t INT64_MIN = (int64_t) 0x8000000000000000ULL;
  if (numerator == INT64_MIN) {
    if (denominator == -1 || denominator == 1) {
      return 0;
    }
  }
  
  // Get the sign of the numerator (this will be the sign of the result)
  int numNegative = (numerator < 0);
  
  // Convert to positive values
  uint64_t positiveNum = (numerator < 0) ? -numerator : numerator;
  uint64_t positiveDen = (denominator < 0) ? -denominator : denominator;
  
  // Perform unsigned modulo
  uint64_t remainder = __umoddi3(positiveNum, positiveDen);
  
  // Apply the correct sign to the result
  // The sign of the remainder matches the sign of the numerator
  if (numNegative && remainder != 0) {
    return -(int64_t) remainder;
  }
  
  return (int64_t) remainder;
}

