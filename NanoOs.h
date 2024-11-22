///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              11.21.2024
///
/// @file              NanoOs.h
///
/// @brief             Core nanokernel functionality for NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2024 James Card
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

#ifndef NANO_OS_H
#define NANO_OS_H

// Arduino includes
#include <Arduino.h>
#include <HardwareSerial.h>

// Standard C includes
#include <limits.h>
#include <string.h>

// Custom includes
#include "NanoOsLibC.h"
#include "Coroutines.h"
#include "Commands.h"

#define NANO_OS_NUM_COROUTINES             8
#define NANO_OS_STACK_SIZE               512
#define NANO_OS_NUM_MESSAGES               8
#define NANO_OS_RESERVED_PROCESS_ID        0
#define NANO_OS_CONSOLE_PROCESS_ID         1
#define NANO_OS_FIRST_PROCESS_ID           2

// Custom types
typedef enum MainCoroutineCommand {
  CALL_FUNCTION,
  NUM_MAIN_COROUTINE_COMMANDS
} MainCoroutineCommand;

// Exported variables
extern Coroutine mainCoroutine;
extern RunningCommand runningCommands[NANO_OS_NUM_COROUTINES];
extern Comessage messages[NANO_OS_NUM_MESSAGES];

#ifdef __cplusplus
extern "C"
{
#endif

// Support functions
static inline int freeRamBytes(void) {
  extern int __heap_start,*__brkval;
  int v;
  return (int)&v - (__brkval == 0  
    ? (int)&__heap_start : (int) __brkval);  
}

static unsigned long getElapsedMilliseconds(unsigned long startTime) {
  unsigned long now = millis();

  if (now < startTime) {
    return ULONG_MAX;
  }

  return now - startTime;
}

static inline Comessage* getAvailableMessage(void) {
  Comessage *availableMessage = NULL;

  for (int ii = 0; ii < NANO_OS_NUM_MESSAGES; ii++) {
    if (messages[ii].inUse == false) {
      availableMessage = &messages[ii];
      comessageInitData(availableMessage, 0, NULL, NULL, 0);
      break;
    }
  }

  return availableMessage;
}

static inline int releaseMessage(Comessage *comessage) {
  return comessageDestroy(comessage);
}

static inline Comessage* sendDataMessageToCoroutine(Coroutine *coroutine,
  int type, void *data, void *storage, size_t storageLength
) {
  Comessage *comessage = NULL;
  if (!coroutineRunning(coroutine)) {
    // Can't send to a non-resumable coroutine.
    return comessage; // NULL
  }

  comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessageInitData(comessage, type, data, storage, storageLength);

  if (comessagePush(coroutine, comessage) != coroutineSuccess) {
    releaseMessage(comessage);
    comessage = NULL;
  }

  return comessage;
}

static inline Comessage* sendDataMessageToPid(
  int pid, int type, void *data, void *storage, size_t storageLength
) {
  Comessage *comessage = NULL;
  if (pid >= NANO_OS_NUM_COROUTINES) {
    // Not a valid PID.  Fail.
    return comessage; // NULL
  }

  Coroutine *coroutine = runningCommands[pid].coroutine;
  comessage
    = sendDataMessageToCoroutine(coroutine, type, data, storage, storageLength);
  return comessage;
}

static inline void* waitForDataMessage(Comessage *sent, int type) {
  void *returnValue = NULL;

  while (sent->done == false) {
    coroutineYield(NULL);
  }
  releaseMessage(sent);

  Comessage *incoming = comessagePopType(NULL, type);
  if (incoming != NULL)  {
    returnValue = incoming->funcData.data;
    releaseMessage(incoming);
  }

  return returnValue;
}

#ifdef __cplusplus
} // extern "C"
#endif

// Console.h has to be included separately (and last).
#include "Console.h"

#endif // NANO_OS_H
