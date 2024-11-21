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
      availableMessage->inUse = true;
      availableMessage->handled = false;
      break;
    }
  }

  return availableMessage;
}

static inline int releaseMessage(Comessage *comessage) {
  if (comessage != NULL) {
    comessage->type = 0;
    comessage->funcData.data = NULL;
    *((uint64_t*) comessage->storage) = 0;
    // Don't touch comessage->next.
    // Don't touch comessage->handled.
    comessage->inUse = false;
    comessage->from = NULL;
  }

  return 0;
}

static inline Comessage* sendDataMessageToCoroutine(
  Coroutine *coroutine, int type, void *data, void *storage
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

  comessage->type = type;
  if (storage != NULL) {
    *((uint64_t*) comessage->storage) = *((uint64_t*) storage);
    if (data == NULL) {
      comessage->funcData.data = comessage->storage;
    } else {
      comessage->funcData.data = data;
    }
  } else {
    comessage->funcData.data = data;
  }

  if (comessagePush(coroutine, comessage) != coroutineSuccess) {
    releaseMessage(comessage);
    comessage = NULL;
  }

  return comessage;
}

static inline Comessage* sendDataMessageToPid(
  int pid, int type, void *data, void *storage
) {
  Comessage *comessage = NULL;
  if (pid >= NANO_OS_NUM_COROUTINES) {
    // Not a valid PID.  Fail.
    return comessage; // NULL
  }

  Coroutine *coroutine = runningCommands[pid].coroutine;
  comessage = sendDataMessageToCoroutine(coroutine, type, data, storage);
  return comessage;
}

#ifdef __cplusplus
} // extern "C"
#endif

// Console.h has to be included separately (and last).
#include "Console.h"

#endif // NANO_OS_H
