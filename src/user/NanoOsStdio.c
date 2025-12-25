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

#include "NanoOsLibC.h"
#include "../kernel/Console.h"
#include "../kernel/Hal.h"
#include "../kernel/NanoOs.h"
#include "../kernel/Processes.h"
#include "../kernel/Scheduler.h"

// Must come last
#include "NanoOsStdio.h"

/// @var nanoOsStdin
///
/// @brief Implementation of nanoOsStdin which is the define value for stdin.
FILE *nanoOsStdin  = (FILE*) ((intptr_t) 0x1);

/// @var nanoOsStdout
///
/// @brief Implementation of nanoOsStdout which is the define value for stdout.
FILE *nanoOsStdout = (FILE*) ((intptr_t) 0x2);

/// @var nanoOsStderr
///
/// @brief Implementation of nanoOsStderr which is the define value for stderr.
FILE *nanoOsStderr = (FILE*) ((intptr_t) 0x3);

/// @fn int printString_(const char *string)
///
/// @brief C wrapper around HAL->writeSerialPort for a C string.
///
/// @return This function always returns 0;
int printString_(const char *string) {
  HAL->writeSerialPort(0, (uint8_t*) string, strlen(string));

  return 0;
}

/// @fn int ullToString(unsigned long long int number, char **nextChar)
///
/// @brief Convert an unsigned long long int to its base 10 string
/// representation.
///
/// @param number The non-negative number to convert.
/// @param nextChar A double pointer to the next character in the buffer to
///   populate.
int ullToString(unsigned long long int number, char **nextChar) {
  if (number == 0) {
    **nextChar = '0';
    // The caller expects nextChar to be positioned one character before the
    // most-recent one written, so we need to back up one character before
    // returning.
    (*nextChar)--;
    return 0;
  }

  while (number > 0) {
    **nextChar = '0' + (number % 10);
    (*nextChar)--;
    number /= 10;
  }

  return 0;
}

/// @fn int printInt_(long long int integer)
///
/// @brief C wrapper around HAL->writeSerialPort for an integer.
///
/// @return This function always returns 0.
int printInt_(long long int integer) {
  char number[20];
  number[19] = '\0';
  char *nextChar = &number[18];

  if (integer >= 0) {
    ullToString((unsigned long long int) integer, &nextChar);
    nextChar++;
  } else {
    ullToString((unsigned long long int) -integer, &nextChar);
    *nextChar = '-';
  }
  HAL->writeSerialPort(0, (uint8_t*) nextChar, strlen(nextChar));

  return 0;
}

/// @fn int printDouble(double floatingPointValue)
///
/// @brief C wrapper around HAL->writeSerialPort for a double.
///
/// @return This function always returns 0.
int printDouble(double floatingPointValue) {
  char number[20];
  sprintf(number, "%lf", floatingPointValue);
  HAL->writeSerialPort(0, (uint8_t*) number, strlen(number));

  return 0;
}

/// @fn int printHex_(unsigned long long int integer)
///
/// @brief C wrapper around HAL->writeSerialPort for a hexadecimal integer.
///
/// @return This function always returns 0.
int printHex_(unsigned long long int integer) {
  char number[20];
  number[19] = '\0';
  char *nextChar = &number[18];

  if (integer > 0) {
    const char *alphabet = "0123456789abcdef";
    while (integer > 0) {
      *nextChar = alphabet[integer & 0xf];
      nextChar--;
      integer >>= 4;
    }
    nextChar++;
  } else {
    *nextChar = '0';
  }

  HAL->writeSerialPort(0, (uint8_t*) nextChar, strlen(nextChar));

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
        {
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

// Input support functions.

/// @fn ConsoleBuffer* nanoOsWaitForInput(void)
///
/// @brief Wait for input from the nanoOs port owned by the current process.
///
/// @return Returns a pointer to the input retrieved on success, NULL on
/// failure.
ConsoleBuffer* nanoOsWaitForInput(void) {
  ConsoleBuffer *nanoOsBuffer = NULL;
  FileDescriptor *inputFd = schedulerGetFileDescriptor(stdin);
  if (inputFd == NULL) {
    printString("ERROR: Could not get input file descriptor for process ");
    printInt(getRunningProcessId());
    printString(" and stream ");
    printInt((intptr_t) stdin);
    printString(".\n");

    // We can't proceed, so bail.
    return nanoOsBuffer; // NULL
  }
  IoPipe *inputPipe = &inputFd->inputPipe;

  if (inputPipe->processId == NANO_OS_CONSOLE_PROCESS_ID) {
    sendNanoOsMessageToPid(inputPipe->processId, inputPipe->messageType,
      /* func= */ 0, /* data= */ 0, false);
  }

  if (inputPipe->processId != PROCESS_ID_NOT_SET) {
    ProcessMessage *response
      = processMessageQueueWaitForType(CONSOLE_RETURNING_INPUT, NULL);
    nanoOsBuffer = nanoOsMessageDataPointer(response, ConsoleBuffer*);

    if (processMessageWaiting(response) == false) {
      // The usual case.
      processMessageRelease(response);
    } else {
      // Just tell the sender that we're done.
      processMessageSetDone(response);
    }
  }

  return nanoOsBuffer;
}

/// @fn char *nanoOsFGets(char *buffer, int size, FILE *stream)
///
/// @brief Custom implementation of fgets for this library.
///
/// @param buffer The character buffer to write the captured input into.
/// @param size The maximum number of bytes to write into the buffer.
/// @param stream A pointer to the FILE stream to read from.  Currently, only
///   stdin is supported.
///
/// @return Returns the buffer pointer provided on success, NULL on failure.
char *nanoOsFGets(char *buffer, int size, FILE *stream) {
  char *returnValue = NULL;
  ConsoleBuffer *nanoOsBuffer
    = (ConsoleBuffer*) getProcessStorage(FGETS_CONSOLE_BUFFER_KEY);
  int numBytesReceived = 0;
  char *newlineAt = NULL;
  int numBytesToCopy = 0;
  int nanoOsInputLength = 0;
  int bufferIndex = 0;

  if (stream == stdin) {
    // There are four stop conditions:
    // 1. nanoOsWaitForInput returns NULL, signalling the end of the input
    //    from the stream.
    // 2. We read a newline.
    // 3. We read an escape sequence.
    // 4. We reach size - 1 bytes received from the stream.
    if (nanoOsBuffer == NULL) {
      nanoOsBuffer = nanoOsWaitForInput();
      setProcessStorage(FGETS_CONSOLE_BUFFER_KEY, nanoOsBuffer);
    } else {
      // We're continuing to read from a buffer that contained a newline plus
      // something else after it.
      newlineAt = strchr(nanoOsBuffer->buffer, ASCII_NEWLINE);
      if (newlineAt == NULL) {
        newlineAt = strchr(nanoOsBuffer->buffer, ASCII_RETURN);
      }
      if (newlineAt != NULL) {
        bufferIndex = (((uintptr_t) newlineAt)
          - ((uintptr_t) nanoOsBuffer->buffer)) + 1;
      } else {
        // This should be impossible given the algorithm below, but assume
        // nothing.
      }
    }

    while (
      (nanoOsBuffer != NULL)
      && (newlineAt == NULL)
      && (numBytesReceived < (size - 1))
    ) {
      returnValue = buffer;
      newlineAt = strchr(&nanoOsBuffer->buffer[bufferIndex], '\n');
      if (newlineAt == NULL) {
        newlineAt = strchr(nanoOsBuffer->buffer, '\r');
      }

      if ((newlineAt == NULL) || (newlineAt[1] == '\0')) {
        // The usual case.
        nanoOsInputLength = (int) strlen(&nanoOsBuffer->buffer[bufferIndex]);
      } else {
        // We've received a buffer that contains a newline plus something after
        // it.  Copy everything up to and including the newline.  Return what
        // we copy and leave the pointer alone so that it's picked up on the
        // next call.
        nanoOsInputLength = (int) (((uintptr_t) newlineAt)
          - ((uintptr_t) &nanoOsBuffer->buffer[bufferIndex]));
      }

      numBytesToCopy
        = MIN((size - 1 - numBytesReceived), nanoOsInputLength);
      memcpy(&buffer[numBytesReceived], &nanoOsBuffer->buffer[bufferIndex],
        numBytesToCopy);
      numBytesReceived += numBytesToCopy;
      buffer[numBytesReceived] = '\0';
      // Release the buffer.
      sendNanoOsMessageToPid(
        NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
        /* func= */ 0, /* data= */ (uintptr_t) nanoOsBuffer, false);

      if ((newlineAt != NULL) || (strchr(nanoOsBuffer->buffer, ASCII_ESCAPE))) {
        // We've reached one of the stop cases, so we're not going to attempt
        // to receive any more data from the file descriptor.
        nanoOsBuffer = NULL;
      } else {
        // There was no newline in this message.  We need to get another one.
        nanoOsBuffer = nanoOsWaitForInput();
        bufferIndex = 0;
      }

      setProcessStorage(FGETS_CONSOLE_BUFFER_KEY, nanoOsBuffer);
    }
  } else {
    // stream is a regular FILE.
    FilesystemIoCommandParameters filesystemIoCommandParameters = {
      .file = stream,
      .buffer = buffer,
      .length = (uint32_t) size - 1
    };
    ProcessMessage *processMessage = sendNanoOsMessageToPid(
      NANO_OS_FILESYSTEM_PROCESS_ID,
      FILESYSTEM_READ_FILE,
      /* func= */ 0,
      /* data= */ (intptr_t) &filesystemIoCommandParameters,
      true);
    processMessageWaitForDone(processMessage, NULL);
    if (filesystemIoCommandParameters.length > 0) {
      buffer[filesystemIoCommandParameters.length] = '\0';
      returnValue = buffer;
    }
    processMessageRelease(processMessage);
  }

  return returnValue;
}

/// @fn int nanoOsVFScanf(FILE *stream, const char *format, va_list args)
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
int nanoOsVFScanf(FILE *stream, const char *format, va_list args) {
  int returnValue = EOF;

  if (stream == stdin) {
    ConsoleBuffer *nanoOsBuffer = nanoOsWaitForInput();
    if (nanoOsBuffer == NULL) {
      return returnValue; // EOF
    }

    returnValue = vsscanf(nanoOsBuffer->buffer, format, args);
    // Release the buffer.
    sendNanoOsMessageToPid(
      NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
      /* func= */ 0, /* data= */ (intptr_t) nanoOsBuffer, false);
  }

  return returnValue;
}

/// @fn int nanoOsFScanf(FILE *stream, const char *format, ...)
///
/// @brief Read formatted input from a file stream into provided arguments.
///
/// @param stream A pointer to the FILE stream to read from.  Currently, only
///   stdin is supported.
/// @param format The string specifying the expected format of the input data.
/// @param ... The arguments to store the parsed values into.
///
/// @return Returns the number of items parsed on success, EOF on failure.
int nanoOsFScanf(FILE *stream, const char *format, ...) {
  int returnValue = EOF;
  va_list args;

  va_start(args, format);
  returnValue = nanoOsVFScanf(stream, format, args);
  va_end(args);

  return returnValue;
}

/// @fn int nanoOsScanf(const char *format, ...)
///
/// @brief Read formatted input from the nanoOs into provided arguments.
///
/// @param format The string specifying the expected format of the input data.
/// @param ... The arguments to store the parsed values into.
///
/// @return Returns the number of items parsed on success, EOF on failure.
int nanoOsScanf(const char *format, ...) {
  int returnValue = EOF;
  va_list args;

  va_start(args, format);
  returnValue = nanoOsVFScanf(stdin, format, args);
  va_end(args);

  return returnValue;
}

// Output support functions.

/// @fn ConsoleBuffer* nanoOsGetBuffer(void)
///
/// @brief Get a buffer from the runConsole process by sending it a command
/// message and getting its response.
///
/// @return Returns a pointer to a ConsoleBuffer from the runConsole process on
/// success, NULL on failure.
ConsoleBuffer* nanoOsGetBuffer(void) {
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

/// @fn int nanoOsWriteBuffer(FILE *stream, ConsoleBuffer *nanoOsBuffer)
///
/// @brief Send a CONSOLE_WRITE_BUFFER command to the nanoOs process.
///
/// @param stream A pointer to a FILE object designating which file to output
///   to (stdout or stderr).
/// @param nanoOsBuffer A pointer to a ConsoleBuffer previously returned from
///   a call to nanoOsGetBuffer.
///
/// @return Returns 0 on success, EOF on failure.
int nanoOsWriteBuffer(FILE *stream, ConsoleBuffer *nanoOsBuffer) {
  int returnValue = 0;
  if ((stream == stdout) || (stream == stderr)) {
    FileDescriptor *outputFd = schedulerGetFileDescriptor(stream);
    if (outputFd == NULL) {
      printString(
        "ERROR: Could not get output file descriptor for process ");
      printInt(getRunningProcessId());
      printString(" and stream ");
      printInt((intptr_t) stream);
      printString(".\n");

      // Release the buffer to avoid creating a leak.
      sendNanoOsMessageToPid(
        NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
        /* func= */ 0, /* data= */ (intptr_t) nanoOsBuffer, false);

      // We can't proceed, so bail.
      returnValue = EOF;
      return returnValue;
    }
    IoPipe *outputPipe = &outputFd->outputPipe;

    if ((outputPipe != NULL) && (outputPipe->processId != PROCESS_ID_NOT_SET)) {
      if ((stream == stdout) || (stream == stderr)) {
        ProcessMessage *processMessage = sendNanoOsMessageToPid(
          outputPipe->processId, outputPipe->messageType,
          0, (intptr_t) nanoOsBuffer, true);
        if (processMessage != NULL) {
          processMessageWaitForDone(processMessage, NULL);
          processMessageRelease(processMessage);
        } else {
          returnValue = EOF;
        }
      } else {
        printString("ERROR: Request to write to invalid stream ");
        printInt((intptr_t) stream);
        printString(" from process ");
        printInt(getRunningProcessId());
        printString(".\n");

        // Release the buffer to avoid creating a leak.
        sendNanoOsMessageToPid(
          NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
          /* func= */ 0, /* data= */ (intptr_t) nanoOsBuffer, false);

        returnValue = EOF;
      }
    } else {
      printString(
        "ERROR: Request to write with no output pipe set from process ");
      printInt(getRunningProcessId());
      printString(".\n");

      // Release the buffer to avoid creating a leak.
      sendNanoOsMessageToPid(
        NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_BUFFER,
        /* func= */ 0, /* data= */ (intptr_t) nanoOsBuffer, false);

      returnValue = EOF;
    }
  } else {
    // stream is a regular FILE.
    FilesystemIoCommandParameters filesystemIoCommandParameters = {
      .file = stream,
      .buffer = nanoOsBuffer->buffer,
      .length = (uint32_t) strlen(nanoOsBuffer->buffer)
    };
    ProcessMessage *processMessage = sendNanoOsMessageToPid(
      NANO_OS_FILESYSTEM_PROCESS_ID,
      FILESYSTEM_WRITE_FILE,
      /* func= */ 0,
      /* data= */ (intptr_t) &filesystemIoCommandParameters,
      true);
    processMessageWaitForDone(processMessage, NULL);
    if (filesystemIoCommandParameters.length == 0) {
      returnValue = EOF;
    }
    processMessageRelease(processMessage);
  }

  return returnValue;
}

/// @fn int nanoOsFPuts(const char *s, FILE *stream)
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
int nanoOsFPuts(const char *s, FILE *stream) {
  int returnValue = EOF;
  ConsoleBuffer *nanoOsBuffer = nanoOsGetBuffer();
  if (nanoOsBuffer == NULL) {
    // Nothing we can do.
    return returnValue;
  }

  strncpy(nanoOsBuffer->buffer, s, CONSOLE_BUFFER_SIZE);
  returnValue = nanoOsWriteBuffer(stream, nanoOsBuffer);

  return returnValue;
}


/// @fn int nanoOsPuts(const char *s)
///
/// @brief Print a string followed by a newline to stdout.  Calls nanoOsFPuts
/// twice:  Once to print the provided string and once to print the trailing
/// newline.
///
/// @param s A pointer to the string to print.
///
/// @return Returns the value of nanoOsFPuts when printing the newline.
int nanoOsPuts(const char *s) {
  nanoOsFPuts(s, stdout);
  return nanoOsFPuts("\n", stdout);
}

/// @fn int nanoOsVFPrintf(FILE *stream, const char *format, va_list args)
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
int nanoOsVFPrintf(FILE *stream, const char *format, va_list args) {
  int returnValue = -1;
  ConsoleBuffer *nanoOsBuffer = nanoOsGetBuffer();
  if (nanoOsBuffer == NULL) {
    // Nothing we can do.
    return returnValue;
  }

  returnValue
    = vsnprintf(nanoOsBuffer->buffer, CONSOLE_BUFFER_SIZE, format, args);
  if (nanoOsWriteBuffer(stream, nanoOsBuffer) == EOF) {
    returnValue = -1;
  }

  return returnValue;
}

/// @fn int nanoOsFPrintf(FILE *stream, const char *format, ...)
///
/// @brief Print a formatted string to the nanoOs.  Constructs a va_list from
/// the arguments provided and then calls nanoOsVFPrintf.
///
/// @param stream A pointer to the FILE stream to print to (stdout or stderr).
/// @param format The format string for the printf message.
/// @param ... Any additional arguments needed by the format string.
///
/// @return Returns the number of bytes printed on success, -1 on error.
int nanoOsFPrintf(FILE *stream, const char *format, ...) {
  int returnValue = 0;
  va_list args;

  va_start(args, format);
  returnValue = nanoOsVFPrintf(stream, format, args);
  va_end(args);

  return returnValue;
}

/// @fn int nanoOsFPrintf(FILE *stream, const char *format, ...)
///
/// @brief Print a formatted string to stdout.  Constructs a va_list from the
/// arguments provided and then calls nanoOsVFPrintf with stdout as the first
/// parameter.
///
/// @param format The format string for the printf message.
/// @param ... Any additional arguments needed by the format string.
///
/// @return Returns the number of bytes printed on success, -1 on error.
int nanoOsPrintf(const char *format, ...) {
  int returnValue = 0;
  va_list args;

  va_start(args, format);
  returnValue = nanoOsVFPrintf(stdout, format, args);
  va_end(args);

  return returnValue;
}

/// @fn int nanoOsFileno(FILE *stream)
///
/// @brief Get the numeric ID of the underlying file descriptor of a FILE
/// stream.
///
/// @param stream A pointer to the FILE stream to examine.
///
/// @return Returns the file descriptor value of the underlying file on success,
/// -1 on failure.  On failure, the value of errno is also set to indicate the
/// reason for the failure.
int nanoOsFileno(FILE *stream) {
  if (stream == NULL) {
    errno = EBADF;
    return -1;
  } else if (stream == nanoOsStdin) {
    return 0;
  } else if (stream == nanoOsStdout) {
    return 1;
  } else if (stream == nanoOsStderr) {
    return 2;
  }
  
  return stream->fd;
}

