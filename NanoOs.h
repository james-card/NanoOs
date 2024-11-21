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
#include "Console.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define NANO_OS_NUM_COROUTINES             9
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

// Support functions
static inline int freeRamBytes(void) {
  extern int __heap_start,*__brkval;
  int v;
  return (int)&v - (__brkval == 0  
    ? (int)&__heap_start : (int) __brkval);  
}

static inline void printFreeRam(void) {
  Serial.print(F("- SRAM left: "));
  Serial.println(freeRamBytes());
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

#ifdef __cplusplus
} // extern "C"
#endif

// C++ functions
static inline int printConsole(char message) {
  Comessage *comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessage->type = (int) WRITE_CHAR;
  comessage->funcData.data = comessage->storage;
  *((char*) comessage->funcData.data) = message;
  comessage->handled = false;
  comessagePush(
    runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine,
    comessage);

  return 0;
}

static inline int printConsole(unsigned char message) {
  Comessage *comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessage->type = (int) WRITE_UCHAR;
  comessage->funcData.data = comessage->storage;
  *((unsigned char*) comessage->funcData.data) = message;
  comessage->handled = false;
  comessagePush(
    runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine,
    comessage);

  return 0;
}

static inline int printConsole(int message) {
  Comessage *comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessage->type = (int) WRITE_INT;
  comessage->funcData.data = comessage->storage;
  *((int*) comessage->funcData.data) = message;
  comessage->handled = false;
  comessagePush(
    runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine,
    comessage);

  return 0;
}

static inline int printConsole(unsigned int message) {
  Comessage *comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessage->type = (int) WRITE_UINT;
  comessage->funcData.data = comessage->storage;
  *((unsigned int*) comessage->funcData.data) = message;
  comessage->handled = false;
  comessagePush(
    runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine,
    comessage);

  return 0;
}

static inline int printConsole(long int message) {
  Comessage *comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessage->type = (int) WRITE_LONG_INT;
  comessage->funcData.data = comessage->storage;
  *((long int*) comessage->funcData.data) = message;
  comessage->handled = false;
  comessagePush(
    runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine,
    comessage);

  return 0;
}

static inline int printConsole(long unsigned int message) {
  Comessage *comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessage->type = (int) WRITE_LONG_UINT;
  comessage->funcData.data = comessage->storage;
  *((long unsigned int*) comessage->funcData.data) = message;
  comessage->handled = false;
  comessagePush(
    runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine,
    comessage);

  return 0;
}

static inline int printConsole(float message) {
  Comessage *comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessage->type = (int) WRITE_FLOAT;
  comessage->funcData.data = comessage->storage;
  *((float*) comessage->funcData.data) = message;
  comessage->handled = false;
  comessagePush(
    runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine,
    comessage);

  return 0;
}

static inline int printConsole(double message) {
  Comessage *comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessage->type = (int) WRITE_DOUBLE;
  comessage->funcData.data = comessage->storage;
  *((double*) comessage->funcData.data) = message;
  comessage->handled = false;
  comessagePush(
    runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine,
    comessage);

  return 0;
}

static inline int printConsole(const char *message) {
  Comessage *comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessage->type = (int) WRITE_STRING;
  comessage->funcData.data = message;
  comessage->handled = false;
  comessagePush(
    runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine,
    comessage);

  return 0;
}

static inline void releaseConsole() {
  printConsole("> ");
}

#endif // NANO_OS_H
