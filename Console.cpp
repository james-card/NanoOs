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

#include "Console.h"
#include "Commands.h"

void consoleMessageCleanup(Comessage *inputMessage) {
  if (comessageWaiting(inputMessage) == false) {
    if (comessageRelease(inputMessage) != coroutineSuccess) {
      Serial.print("ERROR!!!  Could not release inputMessage from ");
      Serial.print(__func__);
      Serial.print("\n");
    }
  }
}

/// @fn void consoleWriteCharHandler(
///   ConsoleState *consoleState, Comessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_WRITE_CHAR command.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  Unused by this function.
/// @param inputMessage A pointer to the Comessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
void consoleWriteCharHandler(
  ConsoleState *consoleState, Comessage *inputMessage
) {
  (void) consoleState;

  char message = nanoOsMessageDataValue(inputMessage, char);
  Serial.print(message);
  comessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleWriteUCharHandler(
///   ConsoleState *consoleState, Comessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_WRITE_UCHAR command.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  Unused by this function.
/// @param inputMessage A pointer to the Comessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
void consoleWriteUCharHandler(
  ConsoleState *consoleState, Comessage *inputMessage
) {
  (void) consoleState;

  unsigned char message = nanoOsMessageDataValue(inputMessage, unsigned char);
  Serial.print(message);
  comessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleWriteIntHandler(
///   ConsoleState *consoleState, Comessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_WRITE_INT command.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  Unused by this function.
/// @param inputMessage A pointer to the Comessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
void consoleWriteIntHandler(
  ConsoleState *consoleState, Comessage *inputMessage
) {
  (void) consoleState;

  int message = nanoOsMessageDataValue(inputMessage, int);
  Serial.print(message);
  comessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleWriteUIntHandler(
///   ConsoleState *consoleState, Comessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_WRITE_UINT command.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  Unused by this function.
/// @param inputMessage A pointer to the Comessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
void consoleWriteUIntHandler(
  ConsoleState *consoleState, Comessage *inputMessage
) {
  (void) consoleState;

  unsigned int message = nanoOsMessageDataValue(inputMessage, unsigned int);
  Serial.print(message);
  comessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleWriteLongIntHandler(
///   ConsoleState *consoleState, Comessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_WRITE_LONG_INT command.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  Unused by this function.
/// @param inputMessage A pointer to the Comessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
void consoleWriteLongIntHandler(
  ConsoleState *consoleState, Comessage *inputMessage
) {
  (void) consoleState;

  long int message = nanoOsMessageDataValue(inputMessage, long int);
  Serial.print(message);
  comessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleWriteLongUIntHandler(
///   ConsoleState *consoleState, Comessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_WRITE_LONG_UINT command.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  Unused by this function.
/// @param inputMessage A pointer to the Comessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
void consoleWriteLongUIntHandler(
  ConsoleState *consoleState, Comessage *inputMessage
) {
  (void) consoleState;

  long unsigned int message
    = nanoOsMessageDataValue(inputMessage, long unsigned int);
  Serial.print(message);
  comessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleWriteFloatHandler(
///   ConsoleState *consoleState, Comessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_WRITE_FLOAT command.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  Unused by this function.
/// @param inputMessage A pointer to the Comessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
void consoleWriteFloatHandler(
  ConsoleState *consoleState, Comessage *inputMessage
) {
  (void) consoleState;

  NanoOsMessageData data = nanoOsMessageDataValue(inputMessage, NanoOsMessageData);
  float message = 0.0;
  memcpy(&message, &data, sizeof(message));
  Serial.print(message);
  comessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleWriteDoubleHandler(
///   ConsoleState *consoleState, Comessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_WRITE_DOUBLE command.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  Unused by this function.
/// @param inputMessage A pointer to the Comessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
void consoleWriteDoubleHandler(
  ConsoleState *consoleState, Comessage *inputMessage
) {
  (void) consoleState;

  NanoOsMessageData data = nanoOsMessageDataValue(inputMessage, NanoOsMessageData);
  double message = 0.0;
  memcpy(&message, &data, sizeof(message));
  Serial.print(message);
  comessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleWriteStringHandler(
///   ConsoleState *consoleState, Comessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_WRITE_STRING command.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  Unused by this function.
/// @param inputMessage A pointer to the Comessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
void consoleWriteStringHandler(
  ConsoleState *consoleState, Comessage *inputMessage
) {
  (void) consoleState;

  const char *message = nanoOsMessageDataPointer(inputMessage, const char*);
  if (message != NULL) {
    Serial.print(message);
  }
  comessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleGetBufferHandler(
///   ConsoleState *consoleState, Comessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_GET_BUFFER command.  Gets a free
/// buffer from the provided ConsoleState and replies to the message sender with
/// a pointer to the buffer structure.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  The buffers in this state will be searched for
///   something available to return to the message sender.
/// @param inputMessage A pointer to the Comessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
/// Since this is a synchronous call, it also pushes a message onto the message
/// sender's queue with the free buffer on success.  On failure, the
/// inputMessage is marked as done but no response is sent.
void consoleGetBufferHandler(
  ConsoleState *consoleState, Comessage *inputMessage
) {
  ConsoleBuffer *consoleBuffers = consoleState->consoleBuffers;
  ConsoleBuffer *returnValue = NULL;
  Comessage *returnMessage = getAvailableMessage();
  if (returnMessage == NULL)  {
    // No return message available.  Nothing we can do.  Bail.
    comessageSetDone(inputMessage);
    return;
  }

  for (int ii = 0; ii < CONSOLE_NUM_BUFFERS; ii++) {
    if (consoleBuffers[ii].inUse == false) {
      returnValue = &consoleBuffers[ii];
      returnValue->inUse = true;
      break;
    }
  }

  if (returnValue != NULL) {
    // Send the buffer back to the caller via the message we allocated earlier.
    NanoOsMessage *nanoOsMessage
      = (NanoOsMessage*) comessageData(returnMessage);
    nanoOsMessage->func = 0;
    nanoOsMessage->data = (intptr_t) returnValue;
    comessageInit(returnMessage, CONSOLE_RETURNING_BUFFER,
      nanoOsMessage, sizeof(*nanoOsMessage), false);
    if (comessageQueuePush(comessageFrom(inputMessage), returnMessage)
      != coroutineSuccess
    ) {
      comessageRelease(returnMessage);
      if (comessageRelease(inputMessage) != coroutineSuccess) {
        Serial.print("ERROR!!!  Could not release returnMessage from ");
        Serial.print(__func__);
        Serial.print(" after comessageQueuePush.\n");
      }
      returnValue->inUse = false;
    }
  } else {
    // No free buffer.  Nothing we can do.
    printString(
      "ERROR!!!  No free console buffer in consoleGetBufferHandler.\n");
    comessageRelease(returnMessage);
    if (comessageRelease(inputMessage) != coroutineSuccess) {
      Serial.print("ERROR!!!  Could not release returnMessage from ");
      Serial.print(__func__);
      Serial.print(" when returnValue was NULL.\n");
    }
  }

  // Whether we were able to grab a buffer or not, we're now done with this
  // call, so mark the input message handled.  This is a synchronous call and
  // the caller is waiting on our response, so *DO NOT* release it.  The caller
  // is responsible for releasing it when they've received the response.
  comessageSetDone(inputMessage);
  return;
}

/// @fn void consoleWriteBufferHandler(
///   ConsoleState *consoleState, Comessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_WRITE_BUFFER command.  Writes the
/// contents of the sent buffer to the console and then clears its inUse flag.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  Unused by this function.
/// @param inputMessage A pointer to the Comessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
void consoleWriteBufferHandler(
  ConsoleState *consoleState, Comessage *inputMessage
) {
  (void) consoleState;

  ConsoleBuffer *consoleBuffer
    = nanoOsMessageDataPointer(inputMessage, ConsoleBuffer*);
  if (consoleBuffer != NULL) {
    const char *message = consoleBuffer->buffer;
    if (message != NULL) {
      Serial.print(message);
    }
    consoleBuffer->inUse = false;
  }
  comessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @var consoleCommandHandlers
///
/// @brief Array of handlers for console command messages.
void (*consoleCommandHandlers[])(ConsoleState*, Comessage*) = {
  consoleWriteCharHandler,
  consoleWriteUCharHandler,
  consoleWriteIntHandler,
  consoleWriteUIntHandler,
  consoleWriteLongIntHandler,
  consoleWriteLongUIntHandler,
  consoleWriteFloatHandler,
  consoleWriteDoubleHandler,
  consoleWriteStringHandler,
  consoleGetBufferHandler,
  consoleWriteBufferHandler,
};

/// @fn void handleConsoleMessages(ConsoleState *consoleState)
///
/// @brief Handle all messages currently in the console process's message queue.
///
/// @param consoleState A pointer to the ConsoleState structure maintained by
///   the runConsole process.
///
/// @return This function returns no value.
void handleConsoleMessages(ConsoleState *consoleState) {
  Comessage *message = comessageQueuePop();
  while (message != NULL) {
    ConsoleCommand messageType = (ConsoleCommand) comessageType(message);
    if (messageType >= NUM_CONSOLE_COMMANDS) {
      // Invalid.
      message = comessageQueuePop();
      continue;
    }

    consoleCommandHandlers[messageType](consoleState, message);
    comessageSetDone(message);
    message = comessageQueuePop();
  }

  return;
}

/// @var lastToggleTime
///
/// @brief Time at which the last LED state toggle occurred.
unsigned long lastToggleTime = 0;

/// @var ledStates
///
/// @brief Array of states to put the LED pin into.  Values are LOW and HIGH
/// which will turn the LED off and on, respectively.
int ledStates[2] = {LOW, HIGH};

/// @var ledStateIndex
///
/// @brief Index of the value in ledStates that is currently set for the LED.
int ledStateIndex = 0;

/// @fn void ledToggle()
///
/// @brief Check the current time and, if it's half the duration of
/// LED_CYCLE_TIME_MS since the last state change, toggle the state.
///
/// @return This function returns no value.
void ledToggle() {
  if (getElapsedMilliseconds(lastToggleTime) >= (LED_CYCLE_TIME_MS >> 1)) {
    // toggle the LED state
    digitalWrite(LED_BUILTIN, ledStates[ledStateIndex]);
    ledStateIndex ^= 1;
    lastToggleTime = getElapsedMilliseconds(0);
  }

  return;
}

/// @fn int readSerialByte(ConsolePort *consolePort)
///
/// @brief Do a non-blocking read of the serial port.
///
/// @param ConsolePort A pointer to the ConsolePort data structure that contains
///   the buffer information to use.
///
/// @return Returns the byte read, cast to an int, on success, -1 on failure.
int readSerialByte(ConsolePort *consolePort) {
  int serialData = -1;
  serialData = Serial.read();
  if (serialData > -1) {
    ConsoleBuffer *consoleBuffer = consolePort->consoleBuffer;
    char *buffer = consoleBuffer->buffer;
    buffer[consolePort->consoleIndex] = (char) serialData;
    if (consolePort->echo == true) {
      Serial.print(buffer[consolePort->consoleIndex]);
    }
    consolePort->consoleIndex++;
    consolePort->consoleIndex %= CONSOLE_BUFFER_SIZE;
  }

  return serialData;
}

/// @fn int printSerialString(const char *string)
///
/// @brief Print a string to the default serial port.
///
/// @param string A pointer to the string to print.
///
/// @return Returns the number of bytes written to the serial port.
int printSerialString(const char *string) {
  return Serial.print(string);
}

/// @fn void* runConsole(void *args)
///
/// @brief Main process for managing console input and output.  Runs in an
/// infinite loop and never exits.  Every iteration, it calls the ledToggle
/// function, cheks the serial connection for a byte and adds it to the buffer
/// if there is anything, handles the user command if the incoming byte is a
/// newline, and handles any messages that were sent to this process.
///
/// @param args Any arguments provided by the scheduler.  Ignored by this
///   process.
///
/// @return This function never returns, but would return NULL if it did.
void* runConsole(void *args) {
  (void) args;
  int byteRead = -1;
  ConsoleState consoleState;
  memset(&consoleState, 0, sizeof(ConsoleState));

  for (int ii = 0; ii < CONSOLE_NUM_BUFFERS; ii++) {
    consoleState.consoleBuffers[ii].inUse = false;
  }

  // For each console port, use the console buffer at the corresponding index.
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    consoleState.consolePorts[ii].consoleBuffer
      = &consoleState.consoleBuffers[ii];
    consoleState.consolePorts[ii].consoleBuffer->inUse = true;
  }

  // Set the port-specific data.
  consoleState.consolePorts[CONSOLE_SERIAL_PORT].consoleIndex = 0;
  consoleState.consolePorts[CONSOLE_SERIAL_PORT].owner = COROUTINE_ID_NOT_SET;
  consoleState.consolePorts[CONSOLE_SERIAL_PORT].waitingForInput = false;
  consoleState.consolePorts[CONSOLE_SERIAL_PORT].readByte = readSerialByte;
  consoleState.consolePorts[CONSOLE_SERIAL_PORT].echo = true;
  consoleState.consolePorts[CONSOLE_SERIAL_PORT].printString
    = printSerialString;

  // Use the first console buffer as the buffer for console input.
  consoleState.consoleBuffers[0].inUse = true;

  while (1) {
    ledToggle();

    for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
      ConsolePort *consolePort
        = &consoleState.consolePorts[CONSOLE_SERIAL_PORT];
      byteRead = consolePort->readByte(consolePort);
      if ((byteRead == ((int) '\n')) || (byteRead == ((int) '\r'))) {
        if (consolePort->owner == COROUTINE_ID_NOT_SET) {
          // NULL-terminate the buffer.
          consolePort->consoleIndex--;
          consolePort->consoleBuffer->buffer[consolePort->consoleIndex] = '\0';
          if (byteRead == ((int) '\r')) {
            consolePort->printString("\n");
          }

          // Use consoleIndex as the size to create a buffer and make a copy.
          char *bufferCopy = (char*) malloc(consolePort->consoleIndex + 1);
          memcpy(bufferCopy, consolePort->consoleBuffer->buffer,
            consolePort->consoleIndex);
          bufferCopy[consolePort->consoleIndex] = '\0';
          handleCommand(ii, bufferCopy);
          // If the command has already returned or wrote to the console before
          // its first yield, we may need to display its output.  Handle the
          // next next message in our queue just in case.
          handleConsoleMessages(&consoleState);
          consolePort->consoleIndex = 0;
        }
      }
      
      coroutineYield(NULL);
      handleConsoleMessages(&consoleState);
    }
  }

  return NULL;
}

/// @fn ConsoleBuffer* consoleGetBuffer(void)
///
/// @brief Get a buffer from the runConsole process by sending it a command
/// message and getting its response.
///
/// @return Returns a pointer to a ConsoleBuffer from the runConsole process on
/// success, NULL on failure.
ConsoleBuffer* consoleGetBuffer(void) {
  ConsoleBuffer *returnValue = NULL;
  struct timespec ts = {0, 0};

  // It's possible that all the console buffers are in use at the time this call
  // is made, so we may have to try multiple times.  Do a while loop until we
  // get a buffer back or until an error occurs.
  while (returnValue == NULL) {
    Comessage *comessage = sendNanoOsMessageToPid(
      NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_GET_BUFFER, 0, 0, true);
    if (comessage == NULL) {
      break; // will return returnValue, which is NULL
    }

    // We want to make sure the handler is done processing the message before
    // we wait for a reply.  Do a blocking wait.
    if (comessageWaitForDone(comessage, NULL) != coroutineSuccess) {
      // Something is wrong.  Bail.
      break; // will return returnValue, which is NULL
    }

    // The handler only marks the message as done if it has successfully sent
    // us a reply or if there was an error and it could not send a reply.  So,
    // we don't want an infinite timeout to waitForDataMessage, we want zero
    // wait.  That's why we need the zeroed timespec above and we want to
    // manually wait for done above.
    returnValue = (ConsoleBuffer*) waitForDataMessage(
      comessage, CONSOLE_RETURNING_BUFFER, &ts);
    if (returnValue == NULL) {
      // Yield control to give the console a chance to get done processing the
      // buffers that are in use.
      coroutineYield(NULL);
    }
  }

  return returnValue;
}

/// @fn int consoleVFPrintf(FILE *stream, const char *format, va_list args)
///
/// @brief Print a formatted string to the console.  Gets a string buffer from
/// the console, writes the formatted string to that buffer, then sends a
/// command to the console to print the buffer.  If the stream being printed to
/// is stderr, blocks until the buffer is printed to the console.
///
/// @param stream A pointer to the FILE stream to print to (stdout or stderr).
/// @param format The format string for the printf message.
/// @param args The va_list of arguments that were passed into one of the
///   higher-level printf functions.
///
/// @return Returns the number of bytes printed on success, -1 on error.
int consoleVFPrintf(FILE *stream, const char *format, va_list args) {
  int returnValue = -1;
  ConsoleBuffer *consoleBuffer = consoleGetBuffer();
  if (consoleBuffer == NULL) {
    // Nothing we can do.
    return returnValue;
  }

  returnValue
    = vsnprintf(consoleBuffer->buffer, CONSOLE_BUFFER_SIZE, format, args);

  if (stream == stdout) {
    sendNanoOsMessageToPid(
      NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_WRITE_BUFFER,
      0, (intptr_t) consoleBuffer, false);
  } else if (stream == stderr) {
    Comessage *comessage = sendNanoOsMessageToPid(
      NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_WRITE_BUFFER,
      0, (intptr_t) consoleBuffer, true);
    comessageWaitForDone(comessage, NULL);
    comessageRelease(comessage);
  }

  return returnValue;
}

/// @fn int consoleFPrintf(FILE *stream, const char *format, ...)
///
/// @brief Print a formatted string to the console.  Constructs a va_list from
/// the arguments provided and then calls consoleVFPrintf.
///
/// @param stream A pointer to the FILE stream to print to (stdout or stderr).
/// @param format The format string for the printf message.
/// @param ... Any additional arguments needed by the format string.
///
/// @return Returns the number of bytes printed on success, -1 on error.
int consoleFPrintf(FILE *stream, const char *format, ...) {
  int returnValue = 0;
  va_list args;

  va_start(args, format);
  returnValue = consoleVFPrintf(stream, format, args);
  va_end(args);

  return returnValue;
}

/// @fn int consoleFPrintf(FILE *stream, const char *format, ...)
///
/// @brief Print a formatted string to stdout.  Constructs a va_list from the
/// arguments provided and then calls consoleVFPrintf with stdout as the first
/// parameter.
///
/// @param format The format string for the printf message.
/// @param ... Any additional arguments needed by the format string.
///
/// @return Returns the number of bytes printed on success, -1 on error.
int consolePrintf(const char *format, ...) {
  int returnValue = 0;
  va_list args;

  va_start(args, format);
  returnValue = consoleVFPrintf(stdout, format, args);
  va_end(args);

  return returnValue;
}

/// @fn int printConsoleValue(
///   ConsoleCommand command, void *value, size_t length)
///
/// @brief Send a command to print a value to the console.
///
/// @param command The ConsoleCommand of the message to send, which will
///   determine the handler to use and therefore the type of the value that is
///   displayed.
/// @param value A pointer to the value to send as data.
/// @param length The length of the data at the address provided by the value
///   pointer.  Will be truncated to the size of NanoOsMessageData if needed.
///
/// @return This function is non-blocking, always succeeds, and always returns
/// 0.
int printConsoleValue(ConsoleCommand command, void *value, size_t length) {
  NanoOsMessageData message = 0;
  length = (length <= sizeof(message)) ? length : sizeof(message);
  memcpy(&message, value, length);

  sendNanoOsMessageToPid(NANO_OS_CONSOLE_PROCESS_ID, command,
    0, message, false);

  return 0;
}

/// @fn void releaseConsole()
///
/// @brief Release the console and display the command prompt to the user again.
///
/// @return This function returns no value.
void releaseConsole() {
  // releaseConsole is sometimes called from within handleCommand, which runs
  // from within the console coroutine.  That means we can't do blocking prints
  // from this function.  i.e. We can't use printf here.  Use printConsole
  // instead.
  printConsole("> ");
}

/// @fn printConsole(<type> message)
///
/// @brief Print a message of an arbitrary type to the console.
///
/// @details
/// This is basically just a switch statement where the type is the switch
/// value.  They all call printConsole with the appropriate console command, a
/// pointer to the provided message, and the size of the message.
///
/// @param message The message to send to the console.
///
/// @return Returns the value returned by printConsoleValue.
int printConsole(char message) {
  return printConsoleValue(CONSOLE_WRITE_CHAR, &message, sizeof(message));
}
int printConsole(unsigned char message) {
  return printConsoleValue(CONSOLE_WRITE_UCHAR, &message, sizeof(message));
}
int printConsole(int message) {
  return printConsoleValue(CONSOLE_WRITE_INT, &message, sizeof(message));
}
int printConsole(unsigned int message) {
  return printConsoleValue(CONSOLE_WRITE_UINT, &message, sizeof(message));
}
int printConsole(long int message) {
  return printConsoleValue(CONSOLE_WRITE_LONG_INT, &message, sizeof(message));
}
int printConsole(long unsigned int message) {
  return printConsoleValue(CONSOLE_WRITE_LONG_UINT, &message, sizeof(message));
}
int printConsole(float message) {
  return printConsoleValue(CONSOLE_WRITE_FLOAT, &message, sizeof(message));
}
int printConsole(double message) {
  return printConsoleValue(CONSOLE_WRITE_DOUBLE, &message, sizeof(message));
}
int printConsole(const char *message) {
  return printConsoleValue(CONSOLE_WRITE_STRING, &message, sizeof(message));
}

