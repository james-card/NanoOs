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

/// @fn int consolePrintMessage(
///   ConsoleState *consoleState, ProcessMessage *inputMessage, const char *message)
///
/// @brief Print a message to all console ports that are owned by a process.
///
/// @param consoleState The ConsoleState being maintained by the runConsole
///   function.
/// @param inputMessage The message received from the process printing the
///   message.
/// @param message The formatted string message to print.
///
/// @return Returns processSuccess on success, processError on failure.
int consolePrintMessage(
  ConsoleState *consoleState, ProcessMessage *inputMessage, const char *message
) {
  int returnValue = processSuccess;
  ProcessId owner = processId(processMessageFrom(inputMessage));
  ConsolePort *consolePorts = consoleState->consolePorts;

  bool portFound = false;
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (consolePorts[ii].owner == owner) {
      consolePorts[ii].printString(message);
      portFound = true;
    }
  }

  if (portFound == false) {
    printString("WARNING:  Request to print message \"");
    printString(message);
    printString("\" from non-owning process ");
    printInt(owner);
    printString("\n");
    returnValue = processError;
  }

  return returnValue;
}

/// @fn void consoleMessageCleanup(ProcessMessage *inputMessage)
///
/// @brief Release an input ProcessMessage if there are no waiters for the message.
///
/// @param inputMessage A pointer to the ProcessMessage to cleanup.
///
/// @return This function returns no value.
void consoleMessageCleanup(ProcessMessage *inputMessage) {
  if (processMessageWaiting(inputMessage) == false) {
    if (processMessageRelease(inputMessage) != processSuccess) {
      Serial.print("ERROR!!!  Could not release inputMessage from ");
      Serial.print(__func__);
      Serial.print("\n");
    }
  }
}

/// @fn void consoleWriteValueCommandHandler(
///   ConsoleState *consoleState, ProcessMessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_WRITE_VALUE command.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  Unused by this function.
/// @param inputMessage A pointer to the ProcessMessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
void consoleWriteValueCommandHandler(
  ConsoleState *consoleState, ProcessMessage *inputMessage
) {
  char staticBuffer[19]; // max length of a 64-bit value is 18 digits plus NULL.
  ConsoleValueType valueType
    = nanoOsMessageFuncValue(inputMessage, ConsoleValueType);
  const char *message = NULL;

  switch (valueType) {
    case CONSOLE_VALUE_CHAR:
      {
        char value = nanoOsMessageDataValue(inputMessage, char);
        sprintf(staticBuffer, "%c", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_UCHAR:
      {
        unsigned char value
          = nanoOsMessageDataValue(inputMessage, unsigned char);
        sprintf(staticBuffer, "%u", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_INT:
      {
        int value = nanoOsMessageDataValue(inputMessage, int);
        sprintf(staticBuffer, "%d", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_UINT:
      {
        unsigned int value
          = nanoOsMessageDataValue(inputMessage, unsigned int);
        sprintf(staticBuffer, "%u", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_LONG_INT:
      {
        long int value = nanoOsMessageDataValue(inputMessage, long int);
        sprintf(staticBuffer, "%ld", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_LONG_UINT:
      {
        long unsigned int value
          = nanoOsMessageDataValue(inputMessage, long unsigned int);
        sprintf(staticBuffer, "%lu", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_FLOAT:
      {
        float value = nanoOsMessageDataValue(inputMessage, float);
        sprintf(staticBuffer, "%f", (double) value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_DOUBLE:
      {
        double value = nanoOsMessageDataValue(inputMessage, double);
        sprintf(staticBuffer, "%lf", value);
        message = staticBuffer;
      }
      break;

    case CONSOLE_VALUE_STRING:
      {
        message = nanoOsMessageDataPointer(inputMessage, const char*);
      }
      break;

    default:
      // Do nothing.
      break;

  }

  // It's possible we were passed a bad type that didn't result in the value of
  // message being set, so only attempt to print it if it was set.
  if (message != NULL) {
    consolePrintMessage(consoleState, inputMessage, message);
  }

  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleGetBufferCommandHandler(
///   ConsoleState *consoleState, ProcessMessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_GET_BUFFER command.  Gets a free
/// buffer from the provided ConsoleState and replies to the message sender with
/// a pointer to the buffer structure.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  The buffers in this state will be searched for
///   something available to return to the message sender.
/// @param inputMessage A pointer to the ProcessMessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
/// Since this is a synchronous call, it also pushes a message onto the message
/// sender's queue with the free buffer on success.  On failure, the
/// inputMessage is marked as done but no response is sent.
void consoleGetBufferCommandHandler(
  ConsoleState *consoleState, ProcessMessage *inputMessage
) {
  ConsoleBuffer *consoleBuffers = consoleState->consoleBuffers;
  ConsoleBuffer *returnValue = NULL;
  // We're going to reuse the input message as the return message.
  ProcessMessage *returnMessage = inputMessage;
  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(returnMessage);
  nanoOsMessage->func = 0;
  nanoOsMessage->data = (intptr_t) NULL;

  for (int ii = 0; ii < CONSOLE_NUM_BUFFERS; ii++) {
    if (consoleBuffers[ii].inUse == false) {
      returnValue = &consoleBuffers[ii];
      returnValue->inUse = true;
      break;
    }
  }

  if (returnValue != NULL) {
    // Send the buffer back to the caller via the message we allocated earlier.
    nanoOsMessage->data = (intptr_t) returnValue;
    processMessageInit(returnMessage, CONSOLE_RETURNING_BUFFER,
      nanoOsMessage, sizeof(*nanoOsMessage), true);
    if (processMessageQueuePush(processMessageFrom(inputMessage), returnMessage)
      != processSuccess
    ) {
      returnValue->inUse = false;
    }
  }

  // Whether we were able to grab a buffer or not, we're now done with this
  // call, so mark the input message handled.  This is a synchronous call and
  // the caller is waiting on our response, so *DO NOT* release it.  The caller
  // is responsible for releasing it when they've received the response.
  processMessageSetDone(inputMessage);
  return;
}

/// @fn void consoleWriteBufferCommandHandler(
///   ConsoleState *consoleState, ProcessMessage *inputMessage)
///
/// @brief Command handler for the CONSOLE_WRITE_BUFFER command.  Writes the
/// contents of the sent buffer to the console and then clears its inUse flag.
///
/// @param consoleState A pointer to the ConsoleState structure held by the
///   runConsole process.  Unused by this function.
/// @param inputMessage A pointer to the ProcessMessage that was received from the
///   process that sent the command.
///
/// @return This function returns no value but does set the inputMessage to
/// done so that the calling process knows that we've handled the message.
void consoleWriteBufferCommandHandler(
  ConsoleState *consoleState, ProcessMessage *inputMessage
) {
  (void) consoleState;

  ConsoleBuffer *consoleBuffer
    = nanoOsMessageDataPointer(inputMessage, ConsoleBuffer*);
  if (consoleBuffer != NULL) {
    const char *message = consoleBuffer->buffer;
    if (message != NULL) {
      consolePrintMessage(consoleState, inputMessage, message);
    }
    consoleBuffer->inUse = false;
  }
  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consleSetPortShellCommandHandler(
///   ConsoleState *consoleState, ProcessMessage *inputMessage)
///
/// @brief Set the designated shell process ID for a port.
///
/// @param consoleState A pointer to the ConsoleState being maintained by the
///   runConsole function that's running.
/// @param inputMessage A pointer to the ProcessMessage with the received command.
///   This contains a NanoOsMessage that contains a ConsolePortPidAssociation
///   that will associate the port with the process if this function succeeds.
///
/// @return This function returns no value, but it marks the inputMessage as
/// being 'done' on success and does *NOT* mark it on failure.
void consleSetPortShellCommandHandler(
  ConsoleState *consoleState, ProcessMessage *inputMessage
) {
  ConsolePortPidUnion consolePortPidUnion;
  consolePortPidUnion.nanoOsMessageData
    = nanoOsMessageDataValue(inputMessage, NanoOsMessageData);
  ConsolePortPidAssociation *consolePortPidAssociation
    = &consolePortPidUnion.consolePortPidAssociation;

  uint8_t consolePort = consolePortPidAssociation->consolePort;
  ProcessId processId = consolePortPidAssociation->processId;

  if (consolePort < CONSOLE_NUM_PORTS) {
    consoleState->consolePorts[consolePort].shell = processId;
    processMessageSetDone(inputMessage);
    consoleMessageCleanup(inputMessage);
  } else {
    printString("ERROR:  Request to assign ownership of non-existent port ");
    printInt(consolePort);
    printString("\n");
    // *DON'T* call processMessageRelease or processMessageSetDone here.  The lack of the
    // message being done will indicate to the caller that there was a problem
    // servicing the command.
  }

  return;
}

/// @fn void consoleAssignPortCommandHandler(
///   ConsoleState *consoleState, ProcessMessage *inputMessage)
///
/// @brief Assign a console port to a running process.
///
/// @param consoleState A pointer to the ConsoleState being maintained by the
///   runConsole function that's running.
/// @param inputMessage A pointer to the ProcessMessage with the received command.
///   This contains a NanoOsMessage that contains a ConsolePortPidAssociation
///   that will associate the port with the process if this function succeeds.
///
/// @return This function returns no value, but it marks the inputMessage as
/// being 'done' on success and does *NOT* mark it on failure.
void consoleAssignPortCommandHandler(
  ConsoleState *consoleState, ProcessMessage *inputMessage
) {
  ConsolePortPidUnion consolePortPidUnion;
  consolePortPidUnion.nanoOsMessageData
    = nanoOsMessageDataValue(inputMessage, NanoOsMessageData);
  ConsolePortPidAssociation *consolePortPidAssociation
    = &consolePortPidUnion.consolePortPidAssociation;

  uint8_t consolePort = consolePortPidAssociation->consolePort;
  ProcessId processId = consolePortPidAssociation->processId;

  if (consolePort < CONSOLE_NUM_PORTS) {
    consoleState->consolePorts[consolePort].owner = processId;
    processMessageSetDone(inputMessage);
    consoleMessageCleanup(inputMessage);
  } else {
    printString("ERROR:  Request to assign ownership of non-existent port ");
    printInt(consolePort);
    printString("\n");
    // *DON'T* call processMessageRelease or processMessageSetDone here.  The lack of the
    // message being done will indicate to the caller that there was a problem
    // servicing the command.
  }

  return;
}

/// @fn void consoleReleasePortCommandHandler(
///   ConsoleState *consoleState, ProcessMessage *inputMessage)
///
/// @brief Release all the ports currently owned by a process.
///
/// @param consoleState A pointer to the ConsoleState being maintained by the
///   runConsole function that's running.
/// @param inputMessage A pointer to the ProcessMessage with the received command.
///
/// @return This function returns no value.
void consoleReleasePortCommandHandler(
  ConsoleState *consoleState, ProcessMessage *inputMessage
) {
  ProcessId owner = processId(processMessageFrom(inputMessage));
  ConsolePort *consolePorts = consoleState->consolePorts;

  bool portFound = false;
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (consolePorts[ii].owner == owner) {
      consolePorts[ii].owner = consolePorts[ii].shell;
      portFound = true;
    }
  }

  if (portFound == false) {
    printString("WARNING:  Request to release a port from non-owning process ");
    printInt(owner);
    printString("\n");
  }

  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleGetOwnedPortCommandHandler(
///   ConsoleState *consoleState, ProcessMessage *inputMessage)
///
/// @brief Get the first port currently owned by a process.
///
/// @note While it is technically possible for a single process to own multiple
/// ports, the expectation here is that this call is made by a process that is
/// only expecting to own one.  This is mostly for the purposes of transferring
/// ownership of the port from one process to another.
///
/// @param consoleState A pointer to the ConsoleState being maintained by the
///   runConsole function that's running.
/// @param inputMessage A pointer to the ProcessMessage with the received command.
///
/// @return This function returns no value.
void consoleGetOwnedPortCommandHandler(
  ConsoleState *consoleState, ProcessMessage *inputMessage
) {
  ProcessId owner = processId(processMessageFrom(inputMessage));
  ConsolePort *consolePorts = consoleState->consolePorts;
  ProcessMessage *returnMessage = inputMessage;

  int ownedPort = -1;
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (consolePorts[ii].owner == owner) {
      ownedPort = ii;
      break;
    }
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(returnMessage);
  nanoOsMessage->func = 0;
  nanoOsMessage->data = (intptr_t) ownedPort;
  processMessageInit(returnMessage, CONSOLE_RETURNING_PORT,
    nanoOsMessage, sizeof(*nanoOsMessage), true);
  sendProcessMessageToPid(owner, inputMessage);

  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleSetEchoCommandHandler(
///   ConsoleState *consoleState, ProcessMessage *inputMessage)
///
/// @brief Set whether or not input is echoed back to all console ports owned
/// by a process.
///
/// @param consoleState A pointer to the ConsoleState being maintained by the
///   runConsole function that's running.
/// @param inputMessage A pointer to the ProcessMessage with the received command.
///
/// @return This function returns no value.
void consoleSetEchoCommandHandler(
  ConsoleState *consoleState, ProcessMessage *inputMessage
) {
  ProcessId owner = processId(processMessageFrom(inputMessage));
  ConsolePort *consolePorts = consoleState->consolePorts;
  ProcessMessage *returnMessage = inputMessage;
  bool desiredEchoState = nanoOsMessageDataValue(inputMessage, bool);
  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(returnMessage);
  nanoOsMessage->func = 0;
  nanoOsMessage->data = 0;

  bool portFound = false;
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (consolePorts[ii].owner == owner) {
      consolePorts[ii].echo = desiredEchoState;
      portFound = true;
    }
  }

  if (portFound == false) {
    printString("WARNING:  Request to set echo from non-owning process ");
    printInt(owner);
    printString("\n");
    nanoOsMessage->data = (intptr_t) -1;
  }

  processMessageInit(returnMessage, CONSOLE_RETURNING_PORT,
    nanoOsMessage, sizeof(*nanoOsMessage), true);
  sendProcessMessageToPid(owner, inputMessage);
  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleWaitForInputCommandHandler(
///   ConsoleState *consoleState, ProcessMessage *inputMessage)
///
/// @brief Wait for input from any of the console ports owned by a process.
///
/// @param consoleState A pointer to the ConsoleState being maintained by the
///   runConsole function that's running.
/// @param inputMessage A pointer to the ProcessMessage with the received command.
///
/// @return This function returns no value.
void consoleWaitForInputCommandHandler(
  ConsoleState *consoleState, ProcessMessage *inputMessage
) {
  ProcessId owner = processId(processMessageFrom(inputMessage));
  ConsolePort *consolePorts = consoleState->consolePorts;

  bool portFound = false;
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (consolePorts[ii].owner == owner) {
      consolePorts[ii].waitingForInput = true;
      portFound = true;
    }
  }

  if (portFound == false) {
    printString("WARNING:  Request to wait for input from non-owning process ");
    printInt(owner);
    printString("\n");
  }

  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}

/// @fn void consoleReleasePidPortCommandHandler(
///   ConsoleState *consoleState, ProcessMessage *inputMessage)
///
/// @brief Release all the ports currently owned by a process.
///
/// @param consoleState A pointer to the ConsoleState being maintained by the
///   runConsole function that's running.
/// @param inputMessage A pointer to the ProcessMessage with the received command.
///
/// @return This function returns no value.
void consoleReleasePidPortCommandHandler(
  ConsoleState *consoleState, ProcessMessage *inputMessage
) {
  ProcessId sender = processId(processMessageFrom(inputMessage));
  if (sender != NANO_OS_SCHEDULER_PROCESS_ID) {
    // Sender is not the scheduler.  We will ignore this.
    processMessageSetDone(inputMessage);
    consoleMessageCleanup(inputMessage);
    return;
  }

  ProcessId owner
    = nanoOsMessageDataValue(inputMessage, ProcessId);
  ConsolePort *consolePorts = consoleState->consolePorts;
  ProcessMessage *processMessage
    = nanoOsMessageFuncPointer(inputMessage, ProcessMessage*);

  bool portFound = false;
  for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (consolePorts[ii].owner == owner) {
      consolePorts[ii].owner = consolePorts[ii].shell;
      // NOTE:  By calling sendProcessMessageToPid from within the for loop, we run
      // the risk of sending the same message to multiple shells.  That's
      // irrelevant in this case since nothing is waiting for the message and
      // all the shells will release the message.  In reality, one process
      // almost never owns multiple ports.  The only exception is during boot.
      if (owner != consolePorts[ii].shell) {
        sendProcessMessageToPid(consolePorts[ii].shell, processMessage);
      } else {
        // The scheduler is telling us to free the console's port.  That means
        // the shell is being killed and restarted.  It won't be able to
        // receive the message if we send it, so we need to go ahead and
        // release it.
        processMessageRelease(processMessage);
      }
      portFound = true;
    }
  }

  if (portFound == false) {
    // The process doesn't own any ports.  Release the message to prevent a
    // message leak.
    processMessageRelease(processMessage);
  }

  processMessageSetDone(inputMessage);
  consoleMessageCleanup(inputMessage);

  return;
}


/// @var consoleCommandHandlers
///
/// @brief Array of handlers for console command messages.
void (*consoleCommandHandlers[])(ConsoleState*, ProcessMessage*) = {
  consoleWriteValueCommandHandler,     // CONSOLE_WRITE_VALUE
  consoleGetBufferCommandHandler,      // CONSOLE_GET_BUFFER
  consoleWriteBufferCommandHandler,    // CONSOLE_WRITE_BUFFER
  consleSetPortShellCommandHandler,    // CONSOLE_SET_PORT_SHELL
  consoleAssignPortCommandHandler,     // CONSOLE_ASSIGN_PORT
  consoleReleasePortCommandHandler,    // CONSOLE_RELEASE_PORT
  consoleGetOwnedPortCommandHandler,   // CONSOLE_GET_OWNED_PORT
  consoleSetEchoCommandHandler,        // CONSOLE_SET_ECHO_PORT
  consoleWaitForInputCommandHandler,   // CONSOLE_WAIT_FOR_INPUT
  consoleReleasePidPortCommandHandler, // CONSOLE_RELEASE_PID_PORT
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
  ProcessMessage *message = processMessageQueuePop();
  while (message != NULL) {
    ConsoleCommand messageType = (ConsoleCommand) processMessageType(message);
    if (messageType >= NUM_CONSOLE_COMMANDS) {
      // Invalid.
      message = processMessageQueuePop();
      continue;
    }

    consoleCommandHandlers[messageType](consoleState, message);
    message = processMessageQueuePop();
  }

  return;
}

/// @fn int consoleSendInputToProcess(
///   ProcessId processId, char *consoleInput)
///
/// @brief Send input captured from a console port to a process that's waiting
/// for it.
///
/// @param processId The ID of the process that's waiting for input to be
///   returned.
/// @param consoleInput A copy of the input that was captured from the console.
///
/// @return Returns processSuccess on success, processError on failure.
int consoleSendInputToProcess(ProcessId processId, char *consoleInput) {
  int returnValue = processSuccess;

  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    processId, CONSOLE_RETURNING_INPUT,
    /* func= */ 0, (intptr_t) consoleInput, false);
  if (processMessage == NULL) {
    returnValue = processError;
  }

  return returnValue;
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

/// @fn int readSerialByte(ConsolePort *consolePort, UartClass &serialPort)
///
/// @brief Do a non-blocking read of a serial port.
///
/// @param ConsolePort A pointer to the ConsolePort data structure that contains
///   the buffer information to use.
/// @param serialPort A reference to the UartClass object (Serial or Serial1)
///   to read a byte from.
///
/// @return Returns the byte read, cast to an int, on success, -1 on failure.
int readSerialByte(ConsolePort *consolePort, UartClass &serialPort) {
  int serialData = -1;
  serialData = serialPort.read();
  if (serialData > -1) {
    ConsoleBuffer *consoleBuffer = consolePort->consoleBuffer;
    char *buffer = consoleBuffer->buffer;
    buffer[consolePort->consoleIndex] = (char) serialData;
    if (consolePort->echo == true) {
      if (((char) serialData != '\r')
        && ((char) serialData != '\n')
      ) {
        serialPort.print((char) serialData);
      } else {
        serialPort.print("\r\n");
      }
    }
    consolePort->consoleIndex++;
    consolePort->consoleIndex %= CONSOLE_BUFFER_SIZE;
  }

  return serialData;
}

/// @fn int readUsbSerialByte(ConsolePort *consolePort)
///
/// @brief Do a non-blocking read of the USB serial port.
///
/// @param ConsolePort A pointer to the ConsolePort data structure that contains
///   the buffer information to use.
///
/// @return Returns the byte read, cast to an int, on success, -1 on failure.
int readUsbSerialByte(ConsolePort *consolePort) {
  return readSerialByte(consolePort, Serial);
}

/// @fn int readGpioSerialByte(ConsolePort *consolePort)
///
/// @brief Do a non-blocking read of the GPIO serial port.
///
/// @param ConsolePort A pointer to the ConsolePort data structure that contains
///   the buffer information to use.
///
/// @return Returns the byte read, cast to an int, on success, -1 on failure.
int readGpioSerialByte(ConsolePort *consolePort) {
  return readSerialByte(consolePort, Serial1);
}

/// @fn int printSerialString(UartClass &serialPort, const char *string)
///
/// @brief Print a string to the default serial port.
///
/// @param serialPort A reference to the UartClass object (Serial or Serial1)
///   to read a byte from.
/// @param string A pointer to the string to print.
///
/// @return Returns the number of bytes written to the serial port.
int printSerialString(UartClass &serialPort, const char *string) {
  int returnValue = 0;
  size_t numBytes = 0;

  char *newlineAt = strchr(string, '\n');
  newlineAt = strchr(string, '\n');
  if (newlineAt == NULL) {
    numBytes = strlen(string);
  } else {
    numBytes = (size_t) (((uintptr_t) newlineAt) - ((uintptr_t) string));
  }
  while (newlineAt != NULL) {
    returnValue += (int) serialPort.write(string, numBytes);
    returnValue += (int) serialPort.write("\r\n");
    string = newlineAt + 1;
    newlineAt = strchr(string, '\n');
    if (newlineAt == NULL) {
      numBytes = strlen(string);
    } else {
      numBytes = (size_t) (((uintptr_t) newlineAt) - ((uintptr_t) string));
    }
  }
  returnValue += (int) serialPort.write(string, numBytes);

  return returnValue;
}

/// @fn int printUsbSerialString(const char *string)
///
/// @brief Print a string to the USB serial port.
///
/// @param string A pointer to the string to print.
///
/// @return Returns the number of bytes written to the serial port.
int printUsbSerialString(const char *string) {
  return printSerialString(Serial, string);
}

/// @fn int printGpioSerialString(const char *string)
///
/// @brief Print a string to the GPIO serial port.
///
/// @param string A pointer to the string to print.
///
/// @return Returns the number of bytes written to the serial port.
int printGpioSerialString(const char *string) {
  return printSerialString(Serial1, string);
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
  ProcessMessage *schedulerMessage = NULL;

  for (uint8_t ii = 0; ii < CONSOLE_NUM_BUFFERS; ii++) {
    consoleState.consoleBuffers[ii].inUse = false;
  }

  // For each console port, use the console buffer at the corresponding index.
  for (uint8_t ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    consoleState.consolePorts[ii].consoleBuffer
      = &consoleState.consoleBuffers[ii];
    consoleState.consolePorts[ii].consoleBuffer->inUse = true;
  }

  // Set the port-specific data.
  consoleState.consolePorts[USB_SERIAL_PORT].consoleIndex = 0;
  consoleState.consolePorts[USB_SERIAL_PORT].owner = COROUTINE_ID_NOT_SET;
  consoleState.consolePorts[USB_SERIAL_PORT].shell = COROUTINE_ID_NOT_SET;
  consoleState.consolePorts[USB_SERIAL_PORT].waitingForInput = false;
  consoleState.consolePorts[USB_SERIAL_PORT].readByte = readUsbSerialByte;
  consoleState.consolePorts[USB_SERIAL_PORT].echo = true;
  consoleState.consolePorts[USB_SERIAL_PORT].printString
    = printUsbSerialString;

  consoleState.consolePorts[GPIO_SERIAL_PORT].consoleIndex = 0;
  consoleState.consolePorts[GPIO_SERIAL_PORT].owner = COROUTINE_ID_NOT_SET;
  consoleState.consolePorts[GPIO_SERIAL_PORT].shell = COROUTINE_ID_NOT_SET;
  consoleState.consolePorts[GPIO_SERIAL_PORT].waitingForInput = false;
  consoleState.consolePorts[GPIO_SERIAL_PORT].readByte = readGpioSerialByte;
  consoleState.consolePorts[GPIO_SERIAL_PORT].echo = true;
  consoleState.consolePorts[GPIO_SERIAL_PORT].printString
    = printGpioSerialString;

  while (1) {
    ledToggle();

    for (uint8_t ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
      ConsolePort *consolePort = &consoleState.consolePorts[ii];
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
          consolePort->consoleIndex = 0;
          if (handleCommand(ii, bufferCopy) == processSuccess) {
            // If the command has already returned or wrote to the console
            // before its first yield, we may need to display its output.
            // Handle the next next message in our queue just in case.
            handleConsoleMessages(&consoleState);
          } else {
            consolePort->printString("Unknown command.\n");
            consolePort->printString("> ");
          }
        } else if (consolePort->waitingForInput == true) {
          consolePort->consoleBuffer->buffer[consolePort->consoleIndex] = '\0';
          char *bufferCopy = (char*) malloc(consolePort->consoleIndex + 1);
          memcpy(bufferCopy, consolePort->consoleBuffer->buffer,
            consolePort->consoleIndex);
          bufferCopy[consolePort->consoleIndex] = '\0';
          consolePort->consoleIndex = 0;
          consoleSendInputToProcess(consolePort->owner, bufferCopy);
          consolePort->waitingForInput = false;
        } else {
          // Console port is owned but owning process is not waiting for input.
          // Reset our buffer and do nothing.
          consolePort->consoleIndex = 0;
        }
      }
    }

    schedulerMessage = (ProcessMessage*) coroutineYield(NULL);

    if (schedulerMessage != NULL) {
      // We have a message from the scheduler that we need to process.  This
      // is not the expected case, but it's the priority case, so we need to
      // list it first.
      ConsoleCommand messageType
        = (ConsoleCommand) processMessageType(schedulerMessage);
      if (messageType < NUM_CONSOLE_COMMANDS) {
        consoleCommandHandlers[messageType](&consoleState, schedulerMessage);
      } else {
        printString("ERROR!!!  Received unknown console command ");
        printInt(messageType);
        printString(" from scheduler.\n");
      }
    } else {
      // No message from the scheduler.  Handle any user process messages in
      // our message queue.
      handleConsoleMessages(&consoleState);
    }
  }

  return NULL;
}

// Output support functions.

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
    ProcessMessage *processMessage = sendNanoOsMessageToPid(
      NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_GET_BUFFER, 0, 0, true);
    if (processMessage == NULL) {
      break; // will return returnValue, which is NULL
    }

    // We want to make sure the handler is done processing the message before
    // we wait for a reply.  Do a blocking wait.
    if (processMessageWaitForDone(processMessage, NULL) != processSuccess) {
      // Something is wrong.  Bail.
      break; // will return returnValue, which is NULL
    }

    // The handler only marks the message as done if it has successfully sent
    // us a reply or if there was an error and it could not send a reply.  So,
    // we don't want an infinite timeout to waitForDataMessage, we want zero
    // wait.  That's why we need the zeroed timespec above and we want to
    // manually wait for done above.
    processMessage = processMessageQueueWaitForType(CONSOLE_RETURNING_BUFFER, &ts);
    if (processMessage == NULL) {
      // The handler marked the sent message done but did not send a reply.
      // That means something is wrong internally to it.  Bail.
      processMessageRelease(processMessage);
      break; // will return returnValue, which is NULL
    }

    returnValue = nanoOsMessageDataPointer(processMessage, ConsoleBuffer*);
    processMessageRelease(processMessage);
    if (returnValue == NULL) {
      // Yield control to give the console a chance to get done processing the
      // buffers that are in use.
      processYield();
    }
  }

  return returnValue;
}

/// @fn int consoleFPuts(const char *s, FILE *stream)
///
/// @brief Print a raw string to the console.  Uses the CONSOLE_WRITE_STRING
/// command to print the literal string passed in.  Since this function has no
/// way of knowing whether or not the provided string is dynamically allocated,
/// it always waits for the console message handler to complete before
/// returning.
///
/// @param s A pointer to the string to print.
/// @param stream The file stream to print to.  Ignored by this function.
///
/// @return This function always returns 0.
int consoleFPuts(const char *s, FILE *stream) {
  (void) stream;
  int returnValue = 0;

  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_WRITE_VALUE,
    CONSOLE_VALUE_STRING, (intptr_t) s, true);
  processMessageWaitForDone(processMessage, NULL);
  processMessageRelease(processMessage);

  return returnValue;
}

/// @fn int consolePuts(const char *s)
///
/// @brief Print a string followed by a newline to stdout.  Calls consoleFPuts
/// twice:  Once to print the provided string and once to print the trailing
/// newline.
///
/// @param s A pointer to the string to print.
///
/// @return Returns the value of consoleFPuts when printing the newline.
int consolePuts(const char *s) {
  consoleFPuts(s, stdout);
  return consoleFPuts("\n", stdout);
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
    ProcessMessage *processMessage = sendNanoOsMessageToPid(
      NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_WRITE_BUFFER,
      0, (intptr_t) consoleBuffer, true);
    processMessageWaitForDone(processMessage, NULL);
    processMessageRelease(processMessage);
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
///   ConsoleValueType valueType, void *value, size_t length)
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
int printConsoleValue(ConsoleValueType valueType, void *value, size_t length) {
  NanoOsMessageData message = 0;
  length = (length <= sizeof(message)) ? length : sizeof(message);
  memcpy(&message, value, length);

  sendNanoOsMessageToPid(NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_WRITE_VALUE,
    valueType, message, false);

  return 0;
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
  return printConsoleValue(CONSOLE_VALUE_CHAR, &message, sizeof(message));
}
int printConsole(unsigned char message) {
  return printConsoleValue(CONSOLE_VALUE_UCHAR, &message, sizeof(message));
}
int printConsole(int message) {
  return printConsoleValue(CONSOLE_VALUE_INT, &message, sizeof(message));
}
int printConsole(unsigned int message) {
  return printConsoleValue(CONSOLE_VALUE_UINT, &message, sizeof(message));
}
int printConsole(long int message) {
  return printConsoleValue(CONSOLE_VALUE_LONG_INT, &message, sizeof(message));
}
int printConsole(long unsigned int message) {
  return printConsoleValue(CONSOLE_VALUE_LONG_UINT, &message, sizeof(message));
}
int printConsole(float message) {
  return printConsoleValue(CONSOLE_VALUE_FLOAT, &message, sizeof(message));
}
int printConsole(double message) {
  return printConsoleValue(CONSOLE_VALUE_DOUBLE, &message, sizeof(message));
}
int printConsole(const char *message) {
  return printConsoleValue(CONSOLE_VALUE_STRING, &message, sizeof(message));
}

// Input support functions.

/// @fn char* consoleWaitForInput(void)
///
/// @brief Wait for input from the console port owned by the current process.
///
/// @return Returns a pointer to the input retrieved on success, NULL on
/// failure.
char* consoleWaitForInput(void) {
  ProcessMessage *sent = sendNanoOsMessageToPid(
    NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_WAIT_FOR_INPUT,
    /* func= */ 0, /* data= */ 0, true);

  ProcessMessage *response = processMessageWaitForReplyWithType(sent, true,
    CONSOLE_RETURNING_INPUT, NULL);
  char *returnValue = nanoOsMessageDataPointer(response, char*);
  processMessageRelease(response);

  return returnValue;
}

/// @fn char *consoleFGets(char *buffer, int size, FILE *stream)
///
/// @brief Custom implementation of fgets for this library.
///
/// @param buffer The character buffer to write the captured input into.
/// @param size The maximum number of bytes to write into the buffer.
/// @param stream A pointer to the FILE stream to read from.  Currently, only
///   stdin is supported.
///
/// @return Returns the buffer pointer provided on success, NULL on failure.
char *consoleFGets(char *buffer, int size, FILE *stream) {
  char *returnValue = NULL;

  if (stream == stdin) {
    char *consoleInput = consoleWaitForInput();
    if (consoleInput == NULL) {
      return returnValue; // NULL
    }
    returnValue = buffer;
    int consoleInputLength = (int) strlen(consoleInput);

    int numBytesToCopy = MIN(size - 1, consoleInputLength);
    memcpy(buffer, consoleInput, numBytesToCopy);
    buffer[numBytesToCopy] = '\0';
    consoleInput = stringDestroy(consoleInput);
  }

  return returnValue;
}

/// @fn int consoleVFScanf(FILE *stream, const char *format, va_list args)
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
int consoleVFScanf(FILE *stream, const char *format, va_list args) {
  int returnValue = EOF;

  if (stream == stdin) {
    char *consoleInput = consoleWaitForInput();
    if (consoleInput == NULL) {
      return returnValue; // EOF
    }

    returnValue = vsscanf(consoleInput, format, args);
    consoleInput = stringDestroy(consoleInput);
  }

  return returnValue;
}

/// @fn int consoleFScanf(FILE *stream, const char *format, ...)
///
/// @brief Read formatted input from a file stream into provided arguments.
///
/// @param stream A pointer to the FILE stream to read from.  Currently, only
///   stdin is supported.
/// @param format The string specifying the expected format of the input data.
/// @param ... The arguments to store the parsed values into.
///
/// @return Returns the number of items parsed on success, EOF on failure.
int consoleFScanf(FILE *stream, const char *format, ...) {
  int returnValue = EOF;
  va_list args;

  va_start(args, format);
  returnValue = consoleVFScanf(stream, format, args);
  va_end(args);

  return returnValue;
}

/// @fn int consoleScanf(const char *format, ...)
///
/// @brief Read formatted input from the console into provided arguments.
///
/// @param format The string specifying the expected format of the input data.
/// @param ... The arguments to store the parsed values into.
///
/// @return Returns the number of items parsed on success, EOF on failure.
int consoleScanf(const char *format, ...) {
  int returnValue = EOF;
  va_list args;

  va_start(args, format);
  returnValue = consoleVFScanf(stdin, format, args);
  va_end(args);

  return returnValue;
}

// Console port support functions.

/// @fn void releaseConsole(void)
///
/// @brief Release the console and display the command prompt to the user again.
///
/// @return This function returns no value.
void releaseConsole(void) {
  // releaseConsole is sometimes called from within handleCommand, which runs
  // from within the console process.  That means we can't do blocking prints
  // from this function.  i.e. We can't use printf here.  Use printConsole
  // instead.
  sendNanoOsMessageToPid(NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_PORT,
    /* func= */ 0, /* data= */ 0, false);
  processYield();
}

/// @fn int getOwnedConsolePort(void)
///
/// @brief Get the first port owned by the running process.
///
/// @return Returns the numerical index of the console port the process owns on
/// success, -1 on failure.
int getOwnedConsolePort(void) {
  ProcessMessage *sent = sendNanoOsMessageToPid(
    NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_GET_OWNED_PORT,
    /* func= */ 0, /* data= */ 0, /* waiting= */ true);

  // The console will reuse the message we sent, so don't release the message
  // in processMessageWaitForReplyWithType.
  ProcessMessage *reply = processMessageWaitForReplyWithType(
    sent, /* releaseAfterDone= */ false,
    CONSOLE_RETURNING_PORT, NULL);

  int returnValue = nanoOsMessageDataValue(reply, int);
  processMessageRelease(reply);

  return returnValue;
}

/// @fn int setConsoleEcho(bool desiredEchoState)
///
/// @brief Get the echo state for all ports owned by the current process.
///
/// @return Returns 0 if the echo state was set for the current process's
/// ports, -1 on failure.
int setConsoleEcho(bool desiredEchoState) {
  ProcessMessage *sent = sendNanoOsMessageToPid(
    NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_SET_ECHO_PORT,
    /* func= */ 0, /* data= */ desiredEchoState, /* waiting= */ true);

  // The console will reuse the message we sent, so don't release the message
  // in processMessageWaitForReplyWithType.
  ProcessMessage *reply = processMessageWaitForReplyWithType(
    sent, /* releaseAfterDone= */ false,
    CONSOLE_RETURNING_PORT, NULL);

  int returnValue = nanoOsMessageDataValue(reply, int);
  processMessageRelease(reply);

  return returnValue;
}

