////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2024 James Card                     //
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

#include "NanoOs.h"
#include "NanoOsLibC.h"

/// @fn int timespec_get(struct timespec* spec, int base)
///
/// @brief Get the current time in the form of a struct timespec.
///
/// @param spec A pointer to the sturct timespec to populate.
/// @param base The base for the time (TIME_UTC).
///
/// @return Returns teh value of base on success, 0 on failure.
int timespec_get(struct timespec* spec, int base) {
  if (spec == NULL) {
    return 0;
  }
  
  unsigned long now = getElapsedMilliseconds(0);
  spec->tv_sec = (time_t) (now / 1000UL);
  spec->tv_nsec = now * 1000000UL;

  return base;
}

/// @fn int printString(const char *string)
///
/// @brief C wrapper around Serial.print for a C string.
///
/// @return This function always returns 0;
int printString(const char *string) {
  Serial.print(string);

  return 0;
}

/// @fn int printInt(int integer)
///
/// @brief C wrapper around Serial.print for an integer.
///
/// @return This function always returns 0.
int printInt(int integer) {
  Serial.print(integer);

  return 0;
}

/// @fn int printList_(const char *firstString, ...)
///
/// @brief Print a list of values.  Values are in (type, value) pairs until the
/// STOP type is reached.
///
/// @param firstString The first string value to print.
/// @param ... All following parameters are in (type, value) format.
///
/// @return Returns 0 on success, -1 on failure.
int printList_(const char *firstString, ...) {
  int returnValue = 0;
  TypeDescriptor *type = NULL;
  va_list args;

  if (firstString == NULL) {
    // Invalid.
    returnValue = -1;
    return returnValue;
  }
  printString(firstString);

  va_start(args, firstString);

  type = va_arg(args, TypeDescriptor*);
  while (type != STOP) {
    if (type == typeInt) {
      int value = va_arg(args, int);
      printInt(value);
    } else if (type == typeString) {
      char *value = va_arg(args, char*);
      printString(value);
    } else {
      printString("Invalid type ");
      printInt((intptr_t) type);
      printString(".  Exiting parsing.\n");
      returnValue = -1;
      break;
    }

    type = va_arg(args, TypeDescriptor*);
  }

  va_end(args);

  return returnValue;
}

/// @enum TypeModifier
///
/// @brief The type modifier parsed from a format string in a sscanf call.
typedef enum TypeModifier {
  TYPE_MODIFIER_NONE,
  TYPE_MODIFIER_HALF,
  TYPE_MODIFIER_HALF_HALF,
  TYPE_MODIFIER_INTMAX_T,
  TYPE_MODIFIER_LONG,
  TYPE_MODIFIER_LONG_LONG,
  TYPE_MODIFIER_LONG_DOUBLE,
  TYPE_MODIFIER_PTRDIFF_T,
  TYPE_MODIFIER_SIZE_T,
  NUM_TYPE_MODIFIERS
} TypeModifier;

/// @fn int scanfParseSignedInt(
///   const char **buffer, TypeModifier typeModifier, void *valuePointer)
///
/// @brief Parse a signed integer value and store it in a variable at a provided
/// pointer.
///
/// @param buffer A pointer to the character buffer that is in the process of
///   being parsed.  This value will be updated on success.
/// @param typeModifier The TypeModifier value that specifies the size of the
///   variable being stored.
/// @param valuePointer The pointer to the variable to update.
///
/// @param Returns the number of values parsed on success, -1 on failure.
int scanfParseSignedInt(
  const char **buffer, TypeModifier typeModifier, void *valuePointer
) {
  int numParsedValues = 0;
  char *nextBufferChar = NULL;

  long value = strtol(*buffer, &nextBufferChar, 0);
  if (nextBufferChar == NULL) {
    // Nothing was parsed.  Bail.
    return numParsedValues; // 0
  } else if (valuePointer == NULL) {
    numParsedValues = 1;
    *buffer = nextBufferChar;
    return numParsedValues;
  }
  

  switch (typeModifier) {
    case TYPE_MODIFIER_NONE:
      {
        int *outputPointer = (int*) valuePointer;
        *outputPointer = (int) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_HALF:
      {
        short int *outputPointer = (short int*) valuePointer;
        *outputPointer = (short int) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_HALF_HALF:
      {
        char *outputPointer = (char*) valuePointer;
        *outputPointer = (char) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_INTMAX_T:
      {
        intmax_t *outputPointer = (intmax_t*) valuePointer;
        *outputPointer = (intmax_t) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_LONG:
      {
        long *outputPointer = (long*) valuePointer;
        *outputPointer = (long) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_LONG_LONG:
      {
        long long *outputPointer = (long long*) valuePointer;
        *outputPointer = (long long) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_PTRDIFF_T:
      {
        ptrdiff_t *outputPointer = (ptrdiff_t*) valuePointer;
        *outputPointer = (ptrdiff_t) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_SIZE_T:
      {
        size_t *outputPointer = (size_t*) valuePointer;
        *outputPointer = (size_t) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    default:
      // Unrecognized TypeModifier value for parsing integer.  Error.
      numParsedValues = -1;
      break;
  }

  return numParsedValues;
}

/// @fn int scanfParseUnsignedInt(
///   const char **buffer, TypeModifier typeModifier, void *valuePointer)
///
/// @brief Parse an unsigned integer value and store it in a variable at a
/// provided pointer.
///
/// @param buffer A pointer to the character buffer that is in the process of
///   being parsed.  This value will be updated on success.
/// @param typeModifier The TypeModifier value that specifies the size of the
///   variable being stored.
/// @param valuePointer The pointer to the variable to update.
///
/// @param Returns the number of values parsed on success, -1 on failure.
int scanfParseUnsignedInt(
  const char **buffer, TypeModifier typeModifier, void *valuePointer
) {
  int numParsedValues = 0;
  char *nextBufferChar = NULL;

  unsigned long value = strtoul(*buffer, &nextBufferChar, 0);
  if (nextBufferChar == NULL) {
    // Nothing was parsed.  Bail.
    return numParsedValues; // 0
  } else if (valuePointer == NULL) {
    numParsedValues = 1;
    *buffer = nextBufferChar;
    return numParsedValues;
  }

  switch (typeModifier) {
    case TYPE_MODIFIER_NONE:
      {
        unsigned int *outputPointer = (unsigned int*) valuePointer;
        *outputPointer = (unsigned int) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_HALF:
      {
        short unsigned int *outputPointer = (short unsigned int*) valuePointer;
        *outputPointer = (short unsigned int) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_HALF_HALF:
      {
        unsigned char *outputPointer = (unsigned char*) valuePointer;
        *outputPointer = (unsigned char) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_INTMAX_T:
      {
        uintmax_t *outputPointer = (uintmax_t*) valuePointer;
        *outputPointer = (uintmax_t) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_LONG:
      {
        unsigned long *outputPointer = (unsigned long*) valuePointer;
        *outputPointer = (unsigned long) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_LONG_LONG:
      {
        unsigned long long *outputPointer = (unsigned long long*) valuePointer;
        *outputPointer = (unsigned long long) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_SIZE_T:
      {
        size_t *outputPointer = (size_t*) valuePointer;
        *outputPointer = (size_t) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    default:
      // Unrecognized TypeModifier value for parsing integer.  Error.
      numParsedValues = -1;
      break;
  }

  return numParsedValues;
}

/// @fn int scanfParseFloat(
///   const char **buffer, TypeModifier typeModifier, void *valuePointer)
///
/// @brief Parse an floating-point value and store it in a variable at a
/// provided pointer.
///
/// @param buffer A pointer to the character buffer that is in the process of
///   being parsed.  This value will be updated on success.
/// @param typeModifier The TypeModifier value that specifies the size of the
///   variable being stored.
/// @param valuePointer The pointer to the variable to update.
///
/// @param Returns the number of values parsed on success, -1 on failure.
int scanfParseFloat(
  const char **buffer, TypeModifier typeModifier, void *valuePointer
) {
  int numParsedValues = 0;
  char *nextBufferChar = NULL;

  double value = strtod(*buffer, &nextBufferChar);
  if (nextBufferChar == NULL) {
    // Nothing was parsed.  Bail.
    return numParsedValues; // 0
  } else if (valuePointer == NULL) {
    numParsedValues = 1;
    *buffer = nextBufferChar;
    return numParsedValues;
  }

  switch (typeModifier) {
    case TYPE_MODIFIER_NONE:
      {
        float *outputPointer = (float*) valuePointer;
        *outputPointer = (float) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_LONG:
      {
        double *outputPointer = (double*) valuePointer;
        *outputPointer = (double) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;

    case TYPE_MODIFIER_LONG_DOUBLE:
      {
        long double *outputPointer = (long double*) valuePointer;
        *outputPointer = (long double) value;
        numParsedValues = 1;
        *buffer = nextBufferChar;
      }
      break;


    default:
      // Unrecognized TypeModifier value for parsing integer.  Error.
      numParsedValues = -1;
      break;
  }

  return numParsedValues;
}

/// @fn int scanfParseString(const char **buffer, size_t numBytes,
///   bool addNullByte, void *valuePointer)
///
/// @brief Parse a string value and store it in a variable at a provided
/// pointer.
///
/// @param buffer A pointer to the character buffer that is in the process of
///   being parsed.  This value will be updated on success.
/// @param numBytes The number of bytes to read from the buffer.
/// @param addNullByte Whether or not to add a terminating NULL byte to the end
///   of the string.
/// @param valuePointer The pointer to the variable to update.
///
/// @param Returns the number of values parsed on success, -1 on failure.
int scanfParseString(
  const char **buffer, size_t numBytes, bool addNullByte, void *valuePointer
) {
  int numParsedValues = 0;
  char *outputPointer = (char*) valuePointer;

  if (numBytes == 0) {
    // Calculate the number of bytes until the first whitespace character.
    numBytes = strcspn(*buffer, " \t\r\n");
  }

  if ((numBytes == 0) || (**buffer == '\0')) {
    // Nothing to parse.
    return numParsedValues; // 0
  } else if (valuePointer == NULL) {
    numParsedValues = 1;
    *buffer += numBytes;
    return numParsedValues;
  }

  memcpy(outputPointer, *buffer, numBytes);
  if (addNullByte == true) {
    outputPointer[numBytes] = '\0';
  }

  // Update the output variables.
  *buffer += numBytes;
  numParsedValues = 1;

  return numParsedValues;
}

/// @fn int vsscanf(const char *buffer, const char *format, va_list args)
///
/// @brief Read formatted input from a string into arguments provided in a
/// va_list.
///
/// @param buffer A string containing the formatted input to parse.
/// @param format The string specifying the format of the input to use.
/// @param args The va_list containing the arguments to store the parsed values
///   into.
///
/// @return Returns the number of items parsed on success, EOF on failure.
int vsscanf(const char *buffer, const char *format, va_list args) {
  const char *startOfBuffer = buffer;
  int returnValue = EOF;
  if ((buffer == NULL) || (format == NULL)) {
    return returnValue; // EOF
  }

  void *outputArg = NULL;
  while ((*buffer) && (*format)) {
    while ((*format != '%') && (*format == *buffer)) {
      format++;
      buffer++;
    }
    if ((*format != '%') && (*format != *buffer)) {
      // No more matches.  Bail.
      break;
    } else if (*format == '\0') {
      // End of match string.  Bail.
      break;
    }

    // (*format == '%')
    if (format[1] == '%') {
      if (*buffer == '%') {
        // Escaped percent matched.
        buffer++;
        format += 2;
        continue;
      } else {
        // Escaped percent *NOT* matched.
        break;
      }
    }

    // We're being asked to parse a value.  Get the pointer to store it in.
    outputArg = va_arg(args, void*);

    // Get any modifier for it.
    TypeModifier typeModifier = TYPE_MODIFIER_NONE;
    format++;
    size_t typeSize = 0;
    switch (*format) {
      case 'h':
        {
          typeModifier = TYPE_MODIFIER_HALF;
          if (format[1] == 'h') {
            typeModifier = TYPE_MODIFIER_HALF_HALF;
            format++;
          }
          format++;
        }
        break;

      case 'j':
        {
          typeModifier = TYPE_MODIFIER_INTMAX_T;
          format++;
        }
        break;

      case 'l':
        {
          typeModifier = TYPE_MODIFIER_LONG;
          if (format[1] == 'l') {
            typeModifier = TYPE_MODIFIER_LONG_LONG;
            format++;
          }
          format++;
        }
        break;

      case 'L':
      case 'q':
        {
          typeModifier = TYPE_MODIFIER_LONG_DOUBLE;
          format++;
        }
        break;

      case 't':
        {
          typeModifier = TYPE_MODIFIER_PTRDIFF_T;
          format++;
        }
        break;

      case 'z':
        {
          typeModifier = TYPE_MODIFIER_SIZE_T;
          format++;
        }
        break;

      default:
        // No modifier present.  typeModifier will remain TYPE_MODIFIER_NONE.
        char formatChar = *format;
        if ((formatChar >= '0') && (formatChar <= '9')) {
          // By definition, strtoul will have to succeed, so we can just pass
          // in the pointer to the format string to have it set to the first
          // character past the size specifier.
          char *nextChar = NULL;
          typeSize = (size_t) strtoul(format, &nextChar, 10);
          if (nextChar != NULL) {
            format = (const char*) nextChar;
          }
        }
        break;
    } // End of switch *format for the type modifier.

    // Now parse the value based on the conversion specifier.
    int numParsedItems = 0;
    bool addNullByte = true;
    if (*format == '*') {
      // We're being requested to parse but *NOT* store a value.  Set outputArg
      // to NULL so the parsers don't try to store the value.
    }
    switch (*format) {
      case 'd':
      case 'i':
        {
          numParsedItems
            = scanfParseSignedInt(&buffer, typeModifier, outputArg);
        }
        break;

      case 'o':
      case 'u':
      case 'x':
      case 'X':
      case 'p':
        {
          numParsedItems
            = scanfParseUnsignedInt(&buffer, typeModifier, outputArg);
        }
        break;

      case 'f':
      case 'e':
      case 'g':
      case 'E':
      case 'a':
        {
          numParsedItems
            = scanfParseFloat(&buffer, typeModifier, outputArg);
        }
        break;

      case 'c':
        {
          if (typeSize == 0) {
            // We're reading a single character.  Set typeSize to 1.
            typeSize = 1;
          }
          addNullByte = false;
        }
        // fall through
      
      case 's':
        {
          numParsedItems
            = scanfParseString(&buffer, typeSize, addNullByte, outputArg);
        }
        break;

      case 'n':
        {
          if (outputArg != NULL) {
            unsigned int bytesConsumed = (unsigned int) (
              ((uintptr_t) buffer) - ((uintptr_t) startOfBuffer));
            *((unsigned int*) outputArg) = bytesConsumed;
          }
        }
        break;

      default:
        // Unknown conversion specifier.  Do nothing.  Next pass of the while
        // loop will fail the conditional expression and we will exit parsing.
        break;
    } // End of switch *format for the conversion specifier.

    if (numParsedItems > 0) {
      if (returnValue != EOF) {
        // The usual case.
        returnValue += numParsedItems;
      } else {
        // Initialize returnValue to a valid value.
        returnValue = numParsedItems;
      }
    }

    // Increment the format to the next character to parse.
    format++;
  }

  return returnValue;
}

/// @fn int sscanf(const char *buffer, const char *format, va_list args)
///
/// @brief Read formatted input from a string into provided arguments.
///
/// @param buffer A string containing the formatted input to parse.
/// @param format The string specifying the format of the input to use.
/// @param ... The arguments to store the parsed values into.
///
/// @return Returns the number of items parsed on success, EOF on failure.
int sscanf(const char *buffer, const char *format, ...) {
  int returnValue = 0;
  va_list args;

  va_start(args, format);
  returnValue = vsscanf(buffer, format, args);
  va_end(args);

  return returnValue;
}

