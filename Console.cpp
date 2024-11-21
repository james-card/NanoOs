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

#include "Console.h"
#include "Commands.h"

#define CONSOLE_BUFFER_SIZE 96
#define CONSOLE_NUM_BUFFERS  4

typedef struct ConsoleBuffer {
  char buffer[CONSOLE_BUFFER_SIZE];
  bool inUse;
} ConsoleBuffer;

typedef struct ConsoleState {
  ConsoleBuffer consoleBuffers[CONSOLE_NUM_BUFFERS];
} ConsoleState;

void consoleWriteChar(ConsoleState *consoleState, Comessage *inputMessage) {
  const char *message
    = (const char *) inputMessage->funcData.data;
  Serial.print(*message);
  inputMessage->handled = true;
  releaseMessage(inputMessage);

  return;
}

void consoleWriteUChar(ConsoleState *consoleState, Comessage *inputMessage) {
  const unsigned char *message
    = (const unsigned char *) inputMessage->funcData.data;
  Serial.print(*message);
  inputMessage->handled = true;
  releaseMessage(inputMessage);

  return;
}

void consoleWriteInt(ConsoleState *consoleState, Comessage *inputMessage) {
  int *message
    = (int *) inputMessage->funcData.data;
  Serial.print(*message);
  inputMessage->handled = true;
  releaseMessage(inputMessage);

  return;
}

void consoleWriteUInt(ConsoleState *consoleState, Comessage *inputMessage) {
  unsigned int *message
    = (unsigned int *) inputMessage->funcData.data;
  Serial.print(*message);
  inputMessage->handled = true;
  releaseMessage(inputMessage);

  return;
}

void consoleWriteLongInt(ConsoleState *consoleState, Comessage *inputMessage) {
  long int *message
    = (long int *) inputMessage->funcData.data;
  Serial.print(*message);
  inputMessage->handled = true;
  releaseMessage(inputMessage);

  return;
}

void consoleWriteLongUInt(ConsoleState *consoleState, Comessage *inputMessage) {
  long int *message
    = (long int *) inputMessage->funcData.data;
  Serial.print(*message);
  inputMessage->handled = true;
  releaseMessage(inputMessage);

  return;
}

void consoleWriteFloat(ConsoleState *consoleState, Comessage *inputMessage) {
  float *message
    = (float *) inputMessage->funcData.data;
  Serial.print(*message);
  inputMessage->handled = true;
  releaseMessage(inputMessage);

  return;
}

void consoleWriteDouble(ConsoleState *consoleState, Comessage *inputMessage) {
  double *message
    = (double *) inputMessage->funcData.data;
  Serial.print(*message);
  inputMessage->handled = true;
  releaseMessage(inputMessage);

  return;
}

void consoleWriteString(ConsoleState *consoleState, Comessage *inputMessage) {
  const char *message
    = (const char *) inputMessage->funcData.data;
  Serial.print(message);
  inputMessage->handled = true;
  releaseMessage(inputMessage);

  return;
}

void consoleGetBuffer(ConsoleState *consoleState, Comessage *inputMessage) {
  ConsoleBuffer *consoleBuffers = consoleState->consoleBuffers;
  ConsoleBuffer *returnValue = NULL;
  Comessage *returnMessage = getAvailableMessage();
  if (returnMessage == NULL)  {
    // No return message available.  Nothing we can do.  Bail.
    inputMessage->handled = true;
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
    returnMessage->type = (int) CONSOLE_RETURNING_BUFFER;
    returnMessage->funcData.data = returnValue;
    if (comessagePush(inputMessage->from, returnMessage) != coroutineSuccess) {
      releaseMessage(returnMessage);
      returnValue->inUse = false;
    }
  } else {
    // No free buffer.  Nothing we can do.
    releaseMessage(returnMessage);
  }

  // Whether we were able to grab a buffer or not, we're now done with this
  // call, so mark the input message handled.  This is a synchronous call and
  // the caller is waiting on our response, so *DO NOT* release it.  The caller
  // is responsible for releasing it when they've received the response.
  inputMessage->handled = true;
  return;
}

void (*consoleCommandHandlers[])(ConsoleState*, Comessage*) {
  consoleWriteChar,
  consoleWriteUChar,
  consoleWriteInt,
  consoleWriteUInt,
  consoleWriteLongInt,
  consoleWriteLongUInt,
  consoleWriteFloat,
  consoleWriteDouble,
  consoleWriteString,
  consoleGetBuffer,
};

static inline void handleConsoleMessages(ConsoleState *consoleState) {
  Comessage *message = comessagePop(NULL);
  while (message != NULL) {
    ConsoleCommand messageType = (ConsoleCommand) message->type;
    if (messageType >= NUM_CONSOLE_COMMANDS) {
      // Invalid.
      message = comessagePop(NULL);
      continue;
    }

    consoleCommandHandlers[messageType](consoleState, message);
    message->handled = true;
    message = comessagePop(NULL);
  }

  return;
}

unsigned long startTime = 0;
int ledStates[2] = {LOW, HIGH};
int ledStateIndex = 0;
void blink() {
  if (getElapsedMilliseconds(startTime) >= (LED_CYCLE_TIME_MS >> 1)) {
    // toggle the LED state
    digitalWrite(LED_BUILTIN, ledStates[ledStateIndex]);
    ledStateIndex ^= 1;
    startTime = getElapsedMilliseconds(0);
  }

  return;
}

char consoleBuffer[256];
unsigned char consoleIndex = 0;
void* runConsole(void *args) {
  (void) args;
  int serialData = -1;
  ConsoleState consoleState;

  while (1) {
    blink();

    serialData = Serial.read();
    if (serialData > -1) {
      consoleBuffer[consoleIndex] = (char) serialData;
      Serial.print(consoleBuffer[consoleIndex]);
      consoleIndex++;
    }

    if ((serialData == ((int) '\n')) || (serialData == ((int) '\r'))) {
      // NULL-terminate the buffer.
      consoleBuffer[consoleIndex] = '\0';
      if (serialData == ((int) '\r')) {
        Serial.println("");
      }
      handleCommand(consoleBuffer);
      // If the command has already returned or wrote to the console before its
      // first yield, we may need to display its output.  Handle the next
      // next message in our queue just in case.
      handleConsoleMessages(&consoleState);
      consoleIndex = 0;
    }
    
    coroutineYield(NULL);
    handleConsoleMessages(&consoleState);
  }

  return NULL;
}

ConsoleBuffer* consoleGetBuffer(void) {
  ConsoleBuffer *returnValue = NULL;

  while (returnValue == NULL) {
    Comessage *comessage = sendDataMessageToPid(
      NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_GET_BUFFER, NULL, NULL);
    if (comessage == NULL) {
      break; // will return returnValue, which is NULL
    }

    returnValue = (ConsoleBuffer*) waitForDataMessage(
      comessage, CONSOLE_RETURNING_BUFFER);
  }

  return returnValue;
}

int printf(const char *format, ...) {
  int returnValue = 0;
  va_list args;
  ConsoleBuffer *consoleBuffer = consoleGetBuffer();
  if (consoleBuffer == NULL) {
    // Nothing we can do.
    returnValue = -1;
    return returnValue;
  }

  va_start(args, format);
  returnValue
    = vsnprintf(consoleBuffer->buffer, CONSOLE_BUFFER_SIZE, format, args);
  va_end(args);

  Comessage *comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessage->type = (int) CONSOLE_WRITE_STRING;
  comessage->funcData.data = consoleBuffer->buffer;
  comessagePush(
    runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine,
    comessage);
  while (comessage->handled == false) {
    coroutineYield(NULL);
  }
  // The command we just called is asynchronous and the message was released by
  // the caller.  Just release the buffer.
  consoleBuffer->inUse = false;

  return returnValue;
}

