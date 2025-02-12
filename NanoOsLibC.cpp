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

#include "NanoOs.h"
#include "NanoOsLibC.h"
#include "Scheduler.h"
#include "NanoOsIo.h"
#include "Fat16Filesystem.h"

/// @var boolNames
///
/// @brief Mapping of bool values to their string names.
const char *boolNames[] = {
  "false",
  "true"
};

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
  spec->tv_nsec = (now % 1000UL) * 1000000UL;

  return base;
}

/// @fn int printString_(const char *string)
///
/// @brief C wrapper around Serial.print for a C string.
///
/// @return This function always returns 0;
int printString_(const char *string) {
  Serial.print(string);

  return 0;
}

/// @fn int printInt_(int integer)
///
/// @brief C wrapper around Serial.print for an integer.
///
/// @return This function always returns 0.
int printInt_(int integer) {
  Serial.print(integer);

  return 0;
}

/// @fn int printUInt_(unsigned int integer)
///
/// @brief C wrapper around Serial.print for an integer.
///
/// @return This function always returns 0.
int printUInt_(unsigned int integer) {
  Serial.print(integer);

  return 0;
}

/// @fn int printLong_(long int integer)
///
/// @brief C wrapper around Serial.print for an integer.
///
/// @return This function always returns 0.
int printLong_(long int integer) {
  Serial.print(integer);

  return 0;
}

/// @fn int printULong_(unsigned long int integer)
///
/// @brief C wrapper around Serial.print for an integer.
///
/// @return This function always returns 0.
int printULong_(unsigned long int integer) {
  Serial.print(integer);

  return 0;
}

/// @fn int printLongLong_(long long int integer)
///
/// @brief C wrapper around Serial.print for an integer.
///
/// @return This function always returns 0.
int printLongLong_(long long int integer) {
  Serial.print(integer);

  return 0;
}

/// @fn int printULongLong_(unsigned long long int integer)
///
/// @brief C wrapper around Serial.print for an integer.
///
/// @return This function always returns 0.
int printULongLong_(unsigned long long int integer) {
  Serial.print(integer);

  return 0;
}

/// @fn int printDouble(double floatingPointValue)
///
/// @brief C wrapper around Serial.print for a double.
///
/// @return This function always returns 0.
int printDouble(double floatingPointValue) {
  Serial.print(floatingPointValue);

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

/// @var errorStrings
///
/// @brief Array of error messages arranged by error code.
const char *errorStrings[] = {
  "Success",
  "Unknown error",
  "Device or resource busy",
  "Out of memory",
  "Permission denied",
  "Invalid argument",
  "I/O error",
};

/// @var NUM_ERRORS
///
/// @brief Constant value to hold the number of errors defined in errorStrings.
const int NUM_ERRORS = sizeof(errorStrings) / sizeof(errorStrings[0]);

/// @fn char* nanoOsStrError(int errnum)
///
/// @brief nanoOsStrError implementation for NanoOs.
///
/// @param errnum The error code either set as the variable errno or returned
///   from a function.
///
/// @return This function always succeeds and returns the string corrsponding
/// to an error.  If the provided error number is outside the range of the
/// defined errors, the string "Unknown error" will be returned.
char* nanoOsStrError(int errnum) {
  if ((errnum < 0) || (errnum >= NUM_ERRORS)) {
    errnum = EUNKNOWN;
  }

  return (char*) errorStrings[errnum];
}

/// @fn unsigned long getElapsedMilliseconds(unsigned long startTime)
///
/// @brief Get the number of milliseconds that have elapsed since a specified
/// time in the past.
///
/// @param startTime The time in the past to calcualte the elapsed time from.
///
/// @return Returns the number of milliseconds that have elapsed since the
/// provided start time.
unsigned long getElapsedMilliseconds(unsigned long startTime) {
  unsigned long now = millis();

  if (now < startTime) {
    return ULONG_MAX;
  }

  return now - startTime;
}

// Standard C I/O funtions

/// @fn FILE* nanoOsIoFOpen(const char *pathname, const char *mode)
///
/// @brief Implementation of the standard C fopen call.
///
/// @param pathname The full pathname to the file.  NOTE:  This implementation
///   can only open files in the root directory.  Subdirectories are NOT
///   supported.
/// @param mode The standard C file mode to open the file as.
///
/// @return Returns a pointer to an initialized FILE object on success, NULL on
/// failure.
FILE* nanoOsIoFOpen(const char *pathname, const char *mode) {
  if ((pathname == NULL) || (*pathname == '\0')
    || (mode == NULL) || (*mode == '\0')
  ) {
    return NULL;
  }

  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, NANO_OS_IO_OPEN_FILE,
    (intptr_t) mode, (intptr_t) pathname, true);
  processMessageWaitForDone(msg, NULL);
  FILE *file = nanoOsMessageDataPointer(msg, FILE*);
  processMessageRelease(msg);
  return file;
}

/// @fn int nanoOsIoFClose(FILE *stream)
///
/// @brief Implementation of the standard C fclose call.
///
/// @param stream A pointer to a previously-opened FILE object.
///
/// @return This function always succeeds and always returns 0.
int nanoOsIoFClose(FILE *stream) {
  if (stream != NULL) {
    ProcessMessage *msg = sendNanoOsMessageToPid(
      NANO_OS_FILESYSTEM_PROCESS_ID, NANO_OS_IO_CLOSE_FILE,
      0, (intptr_t) stream, true);
    processMessageWaitForDone(msg, NULL);
    processMessageRelease(msg);
  }
  return 0;
}

/// @fn int nanoOsIoRemove(const char *pathname)
///
/// @brief Implementation of the standard C remove call.
///
/// @param pathname The full pathname to the file.  NOTE:  This implementation
///   can only open files in the root directory.  Subdirectories are NOT
///   supported.
///
/// @return Returns 0 on success, -1 on failure.
int nanoOsIoRemove(const char *pathname) {
  int returnValue = 0;
  if ((pathname != NULL) && (*pathname != '\0')) {
    ProcessMessage *msg = sendNanoOsMessageToPid(
      NANO_OS_FILESYSTEM_PROCESS_ID, NANO_OS_IO_REMOVE_FILE,
      /* func= */ 0, (intptr_t) pathname, true);
    processMessageWaitForDone(msg, NULL);
    returnValue = nanoOsMessageDataValue(msg, int);
    processMessageRelease(msg);
  }
  return returnValue;
}

/// @fn int nanoOsIoFSeek(FILE *stream, long offset, int whence)
///
/// @brief Implementation of the standard C fseek call.
///
/// @param stream A pointer to a previously-opened FILE object.
/// @param offset A signed integer value that will be added to the specified
///   position.
/// @param whence The location within the file to apply the offset to.  Valid
///   values are SEEK_SET (the beginning of the file), SEEK_CUR (the current
///   file positon), and SEEK_END (the end of the file).
///
/// @return Returns 0 on success, -1 on failure.
int nanoOsIoFSeek(FILE *stream, long offset, int whence) {
  if (stream == NULL) {
    return -1;
  }

  NanoOsIoSeekParameters nanoOsIoSeekParameters = {
    .stream = stream,
    .offset = offset,
    .whence = whence,
  };
  ProcessMessage *msg = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID, NANO_OS_IO_SEEK_FILE,
    /* func= */ 0, (intptr_t) &nanoOsIoSeekParameters, true);
  processMessageWaitForDone(msg, NULL);
  int returnValue = nanoOsMessageDataValue(msg, int);
  processMessageRelease(msg);
  return returnValue;
}

/// @fn size_t nanoOsIoFRead(
///   void *ptr, size_t size, size_t nmemb, FILE *stream)
///
/// @brief Read data from a previously-opened file.
///
/// @param ptr A pointer to the memory to read data into.
/// @param size The size, in bytes, of each element that is to be read from the
///   file.
/// @param nmemb The number of elements that are to be read from the file.
/// @param stream A pointer to the previously-opened file.
///
/// @return Returns the total number of objects successfully read from the
/// file.
size_t nanoOsIoFRead(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  size_t returnValue = 0;
  if ((ptr == NULL) || (size == 0) || (nmemb == 0) || (stream == NULL)) {
    // Nothing to do.
    return returnValue; // 0
  }

  NanoOsIoCommandParameters nanoOsIoIoCommandParameters = {
    .file = stream,
    .buffer = ptr,
    .length = (uint32_t) (size * nmemb)
  };
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID,
    NANO_OS_IO_READ_FILE,
    /* func= */ 0,
    /* data= */ (intptr_t) &nanoOsIoIoCommandParameters,
    true);
  processMessageWaitForDone(processMessage, NULL);
  returnValue = (nanoOsIoIoCommandParameters.length / size);
  processMessageRelease(processMessage);

  return returnValue;
}

/// @fn size_t nanoOsIoFWrite(
///   const void *ptr, size_t size, size_t nmemb, FILE *stream)
///
/// @brief Write data to a previously-opened file.
///
/// @param ptr A pointer to the memory to write data from.
/// @param size The size, in bytes, of each element that is to be written to
///   the file.
/// @param nmemb The number of elements that are to be written to the file.
/// @param stream A pointer to the previously-opened file.
///
/// @return Returns the total number of objects successfully written to the
/// file.
size_t nanoOsIoFWrite(
  const void *ptr, size_t size, size_t nmemb, FILE *stream
) {
  size_t returnValue = 0;
  if ((ptr == NULL) || (size == 0) || (nmemb == 0) || (stream == NULL)) {
    // Nothing to do.
    return returnValue; // 0
  }

  NanoOsIoCommandParameters nanoOsIoIoCommandParameters = {
    .file = stream,
    .buffer = (void*) ptr,
    .length = (uint32_t) (size * nmemb)
  };
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID,
    NANO_OS_IO_WRITE_FILE,
    /* func= */ 0,
    /* data= */ (intptr_t) &nanoOsIoIoCommandParameters,
    true);
  processMessageWaitForDone(processMessage, NULL);
  returnValue = (nanoOsIoIoCommandParameters.length / size);
  processMessageRelease(processMessage);

  return returnValue;
}

/// @fn long nanoOsIoFTell(FILE *stream)
///
/// @brief Get the current value of the position indicator of a
/// previously-opened file.
///
/// @param stream A pointer to a previously-opened file.
///
/// @return Returns the current position of the file on success, -1 on failure.
long nanoOsIoFTell(FILE *stream) {
  if (stream == NULL) {
    return -1;
  }

  return (long) ((Fat16File*) stream->file)->currentPosition;
}

/// @fn size_t nanoOsIoFCopy(FILE *srcFile, off_t srcStart,
///   FILE *dstFile, off_t dstStart, size_t length)
///
/// @brief Copy a specified number of bytes from one position in a source file
///   to another position in a destination file.
///
/// @param srcFile A pointer to the FILE to copy from.  The file must be at
///   least srcStart + length bytes long.
/// @param srcStart The starting position, in bytes, to copy data from in the
///   source file.
/// @param dstFile A pointer to the FILE to copy to.  If the file is not
///   already dstStart + length bytes long, it will be padded with 0s until the
///   dstStart position is reached.
/// @param dstStart The starting position, in bytes, to copy data to in the
///   destination file.
/// @param length The number of bytes to copy from the source file to the
///   destination file.
///
/// @return Returns the number of bytes successfully copied.
size_t nanoOsIoFCopy(FILE *srcFile, off_t srcStart,
  FILE *dstFile, off_t dstStart, size_t length
) {
  if ((dstFile == NULL) || (length == 0)) {
    // Can't proceed or nothing to do.
    return 0;
  }

  FcopyArgs fcopyArgs = {
    .srcFile = srcFile,
    .srcStart = srcStart,
    .dstFile = dstFile,
    .dstStart  = dstStart,
    .length = length,
  };

  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_FILESYSTEM_PROCESS_ID,
    NANO_OS_IO_COPY_FILE,
    /* func= */ 0,
    /* data= */ (intptr_t) &fcopyArgs,
    true);
  processMessageWaitForDone(processMessage, NULL);
  size_t returnValue = nanoOsMessageDataValue(processMessage, size_t);
  processMessageRelease(processMessage);

  return returnValue;
}

// Input support functions.

/// @fn ConsoleBuffer* nanoOsIoWaitForInput(void)
///
/// @brief Wait for input from the nanoOs port owned by the current process.
///
/// @return Returns a pointer to the input retrieved on success, NULL on
/// failure.
ConsoleBuffer* nanoOsIoWaitForInput(void) {
  ConsoleBuffer *nanoOsIoBuffer = NULL;
  FileDescriptor *inputFd = schedulerGetFileDescriptor(stdin);
  if (inputFd == NULL) {
    printString("ERROR!!!  Could not get input file descriptor for process ");
    printInt(getRunningProcessId());
    printString(" and stream ");
    printInt((intptr_t) stdin);
    printString(".\n");

    // We can't proceed, so bail.
    return nanoOsIoBuffer; // NULL
  }
  IoPipe *inputPipe = &inputFd->inputPipe;

  if (inputPipe->processId == NANO_OS_CONSOLE_PROCESS_ID) {
    sendNanoOsMessageToPid(inputPipe->processId, inputPipe->messageType,
      /* func= */ 0, /* data= */ 0, false);
  }

  if (inputPipe->processId != PROCESS_ID_NOT_SET) {
    ProcessMessage *response
      = processMessageQueueWaitForType(CONSOLE_RETURNING_INPUT, NULL);
    nanoOsIoBuffer = nanoOsMessageDataPointer(response, ConsoleBuffer*);

    if (processMessageWaiting(response) == false) {
      // The usual case.
      processMessageRelease(response);
    } else {
      // Just tell the sender that we're done.
      processMessageSetDone(response);
    }
  }

  return nanoOsIoBuffer;
}

/// @fn char *nanoOsIoFGets(char *buffer, int size, FILE *stream)
///
/// @brief Custom implementation of fgets for this library.
///
/// @param buffer The character buffer to write the captured input into.
/// @param size The maximum number of bytes to write into the buffer.
/// @param stream A pointer to the FILE stream to read from.  Currently, only
///   stdin is supported.
///
/// @return Returns the buffer pointer provided on success, NULL on failure.
char *nanoOsIoFGets(char *buffer, int size, FILE *stream) {
  char *returnValue = NULL;
  ConsoleBuffer *nanoOsIoBuffer
    = (ConsoleBuffer*) getProcessStorage(FGETS_CONSOLE_BUFFER_KEY);
  int numBytesReceived = 0;
  char *newlineAt = NULL;
  int numBytesToCopy = 0;
  int nanoOsIoInputLength = 0;
  int bufferIndex = 0;

  if (stream == stdin) {
    // There are three stop conditions:
    // 1. nanoOsIoWaitForInput returns NULL, signalling the end of the input
    //    from the stream.
    // 2. We read a newline.
    // 3. We reach size - 1 bytes received from the stream.
    if (nanoOsIoBuffer == NULL) {
      nanoOsIoBuffer = nanoOsIoWaitForInput();
      setProcessStorage(FGETS_CONSOLE_BUFFER_KEY, nanoOsIoBuffer);
    } else {
      newlineAt = strchr(nanoOsIoBuffer->buffer, '\n');
      if (newlineAt == NULL) {
        newlineAt = strchr(nanoOsIoBuffer->buffer, '\r');
      }
      if (newlineAt != NULL) {
        bufferIndex = (((uintptr_t) newlineAt)
          - ((uintptr_t) nanoOsIoBuffer->buffer)) + 1;
      } else {
        // This should be impossible given the algorithm below, but assume
        // nothing.
      }
    }

    while (
      (nanoOsIoBuffer != NULL)
      && (newlineAt == NULL)
      && (numBytesReceived < (size - 1))
    ) {
      returnValue = buffer;
      newlineAt = strchr(&nanoOsIoBuffer->buffer[bufferIndex], '\n');
      if (newlineAt == NULL) {
        newlineAt = strchr(nanoOsIoBuffer->buffer, '\r');
      }

      if ((newlineAt == NULL) || (newlineAt[1] == '\0')) {
        // The usual case.
        nanoOsIoInputLength
          = (int) strlen(&nanoOsIoBuffer->buffer[bufferIndex]);
      } else {
        // We've received a buffer that contains a newline plus something after
        // it.  Copy everything up to and including the newline.  Return what
        // we copy and leave the pointer alone so that it's picked up on the
        // next call.
        nanoOsIoInputLength = (int) (((uintptr_t) newlineAt)
          - ((uintptr_t) &nanoOsIoBuffer->buffer[bufferIndex]));
      }

      numBytesToCopy
        = MIN((size - 1 - numBytesReceived), nanoOsIoInputLength);
      memcpy(&buffer[numBytesReceived], &nanoOsIoBuffer->buffer[bufferIndex],
        numBytesToCopy);
      numBytesReceived += numBytesToCopy;
      buffer[numBytesReceived] = '\0';
      // Release the buffer.
      sendNanoOsMessageToPid(
        NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
        /* func= */ 0, /* data= */ (uintptr_t) nanoOsIoBuffer, false);

      if (newlineAt != NULL) {
        // We've reached one of the stop cases, so we're not going to attempt
        // to receive any more data from the file descriptor.
        nanoOsIoBuffer = NULL;
      } else {
        // There was no newline in this message.  We need to get another one.
        nanoOsIoBuffer = nanoOsIoWaitForInput();
        bufferIndex = 0;
      }

      setProcessStorage(FGETS_CONSOLE_BUFFER_KEY, nanoOsIoBuffer);
    }
  } else {
    // stream is a regular FILE.
    NanoOsIoCommandParameters nanoOsIoCommandParameters = {
      .file = stream,
      .buffer = buffer,
      .length = (uint32_t) size - 1
    };
    ProcessMessage *processMessage = sendNanoOsMessageToPid(
      NANO_OS_FILESYSTEM_PROCESS_ID,
      NANO_OS_IO_READ_FILE,
      /* func= */ 0,
      /* data= */ (intptr_t) &nanoOsIoCommandParameters,
      true);
    processMessageWaitForDone(processMessage, NULL);
    if (nanoOsIoCommandParameters.length > 0) {
      buffer[nanoOsIoCommandParameters.length] = '\0';
      returnValue = buffer;
    }
    processMessageRelease(processMessage);
  }

  return returnValue;
}

/// @fn int nanoOsIoVFScanf(FILE *stream, const char *format, va_list args)
///
/// @brief Read formatted input from a file stream into arguments provided in
/// a va_list.
///
/// @param stream A pointer to the FILE stream to read from.  Currently, only
///   stdin is supported.
/// @param format The string specifying the expected format of the input data.
/// @param args The va_list containing the arguments to store the parsed values
///   into.
///
/// @return Returns the number of items parsed on success, EOF on failure.
int nanoOsIoVFScanf(FILE *stream, const char *format, va_list args) {
  int returnValue = EOF;

  if (stream == stdin) {
    ConsoleBuffer *nanoOsIoBuffer = nanoOsIoWaitForInput();
    if (nanoOsIoBuffer == NULL) {
      return returnValue; // EOF
    }

    returnValue = vsscanf(nanoOsIoBuffer->buffer, format, args);
    // Release the buffer.
    sendNanoOsMessageToPid(
      NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
      /* func= */ 0, /* data= */ (intptr_t) nanoOsIoBuffer, false);
  }

  return returnValue;
}

/// @fn int nanoOsIoFScanf(FILE *stream, const char *format, ...)
///
/// @brief Read formatted input from a file stream into provided arguments.
///
/// @param stream A pointer to the FILE stream to read from.  Currently, only
///   stdin is supported.
/// @param format The string specifying the expected format of the input data.
/// @param ... The arguments to store the parsed values into.
///
/// @return Returns the number of items parsed on success, EOF on failure.
int nanoOsIoFScanf(FILE *stream, const char *format, ...) {
  int returnValue = EOF;
  va_list args;

  va_start(args, format);
  returnValue = nanoOsIoVFScanf(stream, format, args);
  va_end(args);

  return returnValue;
}

/// @fn int nanoOsIoScanf(const char *format, ...)
///
/// @brief Read formatted input from the nanoOs into provided arguments.
///
/// @param format The string specifying the expected format of the input data.
/// @param ... The arguments to store the parsed values into.
///
/// @return Returns the number of items parsed on success, EOF on failure.
int nanoOsIoScanf(const char *format, ...) {
  int returnValue = EOF;
  va_list args;

  va_start(args, format);
  returnValue = nanoOsIoVFScanf(stdin, format, args);
  va_end(args);

  return returnValue;
}

// Output support functions.

/// @fn ConsoleBuffer* nanoOsIoGetBuffer(void)
///
/// @brief Get a buffer from the runConsole process by sending it a command
/// message and getting its response.
///
/// @return Returns a pointer to a ConsoleBuffer from the runConsole process on
/// success, NULL on failure.
ConsoleBuffer* nanoOsIoGetBuffer(void) {
  ConsoleBuffer *returnValue = NULL;
  struct timespec ts = {0, 0};

  // It's possible that all the nanoOs buffers are in use at the time this call
  // is made, so we may have to try multiple times.  Do a while loop until we
  // get a buffer back or until an error occurs.
  while (returnValue == NULL) {
    ProcessMessage *processMessage = sendNanoOsMessageToPid(
      NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_GET_BUFFER, 0, 0, true);
    if (processMessage == NULL) {
      break; // will return returnValue, which is NULL
    }

    // We want to make sure the handler is done processing the message before
    // we wait for a reply.  Do a blocking wait.
    if (processMessageWaitForDone(processMessage, NULL) != processSuccess) {
      // Something is wrong.  Bail.
      processMessageRelease(processMessage);
      break; // will return returnValue, which is NULL
    }
    processMessageRelease(processMessage);

    // The handler only marks the message as done if it has successfully sent
    // us a reply or if there was an error and it could not send a reply.  So,
    // we don't want an infinite timeout to waitForDataMessage, we want zero
    // wait.  That's why we need the zeroed timespec above and we want to
    // manually wait for done above.
    processMessage
      = processMessageQueueWaitForType(CONSOLE_RETURNING_BUFFER, &ts);
    if (processMessage == NULL) {
      // The handler marked the sent message done but did not send a reply.
      // That means something is wrong internally to it.  Bail.
      break; // will return returnValue, which is NULL
    }

    returnValue = nanoOsMessageDataPointer(processMessage, ConsoleBuffer*);
    processMessageRelease(processMessage);
    if (returnValue == NULL) {
      // Yield control to give the nanoOs a chance to get done processing the
      // buffers that are in use.
      processYield();
    }
  }

  return returnValue;
}

/// @fn int nanoOsIoWriteBuffer(FILE *stream, ConsoleBuffer *nanoOsIoBuffer)
///
/// @brief Send a CONSOLE_WRITE_BUFFER command to the nanoOs process.
///
/// @param stream A pointer to a FILE object designating which file to output
///   to (stdout or stderr).
/// @param nanoOsIoBuffer A pointer to a ConsoleBuffer previously returned from
///   a call to nanoOsIoGetBuffer.
///
/// @return Returns 0 on success, EOF on failure.
int nanoOsIoWriteBuffer(FILE *stream, ConsoleBuffer *nanoOsIoBuffer) {
  int returnValue = 0;
  if ((stream == stdout) || (stream == stderr)) {
    FileDescriptor *outputFd = schedulerGetFileDescriptor(stream);
    if (outputFd == NULL) {
      printString(
        "ERROR!!!  Could not get output file descriptor for process ");
      printInt(getRunningProcessId());
      printString(" and stream ");
      printInt((intptr_t) stream);
      printString(".\n");

      // Release the buffer to avoid creating a leak.
      sendNanoOsMessageToPid(
        NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
        /* func= */ 0, /* data= */ (intptr_t) nanoOsIoBuffer, false);

      // We can't proceed, so bail.
      returnValue = EOF;
      return returnValue;
    }
    IoPipe *outputPipe = &outputFd->outputPipe;

    if ((outputPipe != NULL) && (outputPipe->processId != PROCESS_ID_NOT_SET)) {
      if ((stream == stdout) || (stream == stderr)) {
        ProcessMessage *processMessage = sendNanoOsMessageToPid(
          outputPipe->processId, outputPipe->messageType,
          0, (intptr_t) nanoOsIoBuffer, true);
        if (processMessage != NULL) {
          processMessageWaitForDone(processMessage, NULL);
          processMessageRelease(processMessage);
        } else {
          returnValue = EOF;
        }
      } else {
        printString("ERROR!!!  Request to write to invalid stream ");
        printInt((intptr_t) stream);
        printString(" from process ");
        printInt(getRunningProcessId());
        printString(".\n");

        // Release the buffer to avoid creating a leak.
        sendNanoOsMessageToPid(
          NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
          /* func= */ 0, /* data= */ (intptr_t) nanoOsIoBuffer, false);

        returnValue = EOF;
      }
    } else {
      printString(
        "ERROR!!!  Request to write with no output pipe set from process ");
      printInt(getRunningProcessId());
      printString(".\n");

      // Release the buffer to avoid creating a leak.
      sendNanoOsMessageToPid(
        NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
        /* func= */ 0, /* data= */ (intptr_t) nanoOsIoBuffer, false);

      returnValue = EOF;
    }
  } else {
    // stream is a regular FILE.
    NanoOsIoCommandParameters nanoOsIoCommandParameters = {
      .file = stream,
      .buffer = nanoOsIoBuffer->buffer,
      .length = (uint32_t) strlen(nanoOsIoBuffer->buffer)
    };
    ProcessMessage *processMessage = sendNanoOsMessageToPid(
      NANO_OS_FILESYSTEM_PROCESS_ID,
      NANO_OS_IO_WRITE_FILE,
      /* func= */ 0,
      /* data= */ (intptr_t) &nanoOsIoCommandParameters,
      true);
    processMessageWaitForDone(processMessage, NULL);
    if (nanoOsIoCommandParameters.length == 0) {
      returnValue = EOF;
    }
    processMessageRelease(processMessage);
  }

  return returnValue;
}

/// @fn int nanoOsIoFPuts(const char *s, FILE *stream)
///
/// @brief Print a raw string to the nanoOs.  Uses the CONSOLE_WRITE_STRING
/// command to print the literal string passed in.  Since this function has no
/// way of knowing whether or not the provided string is dynamically allocated,
/// it always waits for the nanoOs message handler to complete before
/// returning.
///
/// @param s A pointer to the string to print.
/// @param stream The file stream to print to.  Ignored by this function.
///
/// @return This function always returns 0.
int nanoOsIoFPuts(const char *s, FILE *stream) {
  int returnValue = EOF;
  ConsoleBuffer *nanoOsIoBuffer = nanoOsIoGetBuffer();
  if (nanoOsIoBuffer == NULL) {
    // Nothing we can do.
    return returnValue;
  }

  strncpy(nanoOsIoBuffer->buffer, s, CONSOLE_BUFFER_SIZE);
  returnValue = nanoOsIoWriteBuffer(stream, nanoOsIoBuffer);

  return returnValue;
}


/// @fn int nanoOsIoPuts(const char *s)
///
/// @brief Print a string followed by a newline to stdout.  Calls nanoOsIoFPuts
/// twice:  Once to print the provided string and once to print the trailing
/// newline.
///
/// @param s A pointer to the string to print.
///
/// @return Returns the value of nanoOsIoFPuts when printing the newline.
int nanoOsIoPuts(const char *s) {
  nanoOsIoFPuts(s, stdout);
  return nanoOsIoFPuts("\n", stdout);
}

/// @fn int nanoOsIoVFPrintf(FILE *stream, const char *format, va_list args)
///
/// @brief Print a formatted string to the nanoOs.  Gets a string buffer from
/// the nanoOs, writes the formatted string to that buffer, then sends a
/// command to the nanoOs to print the buffer.  If the stream being printed to
/// is stderr, blocks until the buffer is printed to the nanoOs.
///
/// @param stream A pointer to the FILE stream to print to (stdout or stderr).
/// @param format The format string for the printf message.
/// @param args The va_list of arguments that were passed into one of the
///   higher-level printf functions.
///
/// @return Returns the number of bytes printed on success, -1 on error.
int nanoOsIoVFPrintf(FILE *stream, const char *format, va_list args) {
  int returnValue = -1;
  ConsoleBuffer *nanoOsIoBuffer = nanoOsIoGetBuffer();
  if (nanoOsIoBuffer == NULL) {
    // Nothing we can do.
    return returnValue;
  }

  returnValue
    = vsnprintf(nanoOsIoBuffer->buffer, CONSOLE_BUFFER_SIZE, format, args);
  if (nanoOsIoWriteBuffer(stream, nanoOsIoBuffer) == EOF) {
    returnValue = -1;
  }

  return returnValue;
}

/// @fn int nanoOsIoFPrintf(FILE *stream, const char *format, ...)
///
/// @brief Print a formatted string to the nanoOs.  Constructs a va_list from
/// the arguments provided and then calls nanoOsVFPrintf.
///
/// @param stream A pointer to the FILE stream to print to (stdout or stderr).
/// @param format The format string for the printf message.
/// @param ... Any additional arguments needed by the format string.
///
/// @return Returns the number of bytes printed on success, -1 on error.
int nanoOsIoFPrintf(FILE *stream, const char *format, ...) {
  int returnValue = 0;
  va_list args;

  va_start(args, format);
  returnValue = nanoOsIoVFPrintf(stream, format, args);
  va_end(args);

  return returnValue;
}

/// @fn int nanoOsIoFPrintf(FILE *stream, const char *format, ...)
///
/// @brief Print a formatted string to stdout.  Constructs a va_list from the
/// arguments provided and then calls nanoOsVFPrintf with stdout as the first
/// parameter.
///
/// @param format The format string for the printf message.
/// @param ... Any additional arguments needed by the format string.
///
/// @return Returns the number of bytes printed on success, -1 on error.
int nanoOsIoPrintf(const char *format, ...) {
  int returnValue = 0;
  va_list args;

  va_start(args, format);
  returnValue = nanoOsIoVFPrintf(stdout, format, args);
  va_end(args);

  return returnValue;
}

