// Standard C includes
#include <limits.h>
#include <string.h>
#include <stdarg.h>

// Custom includes
#include "NanoOs.h"
#include "Coroutines.h"

#ifndef CONSOLE_H
#define CONSOLE_H

#ifdef __cplusplus
extern "C"
{
#endif

// Defines
#define LED_CYCLE_TIME_MS 1000

// Custom types
typedef enum ConsoleCommand {
  CONSOLE_WRITE_CHAR,
  CONSOLE_WRITE_UCHAR,
  CONSOLE_WRITE_INT,
  CONSOLE_WRITE_UINT,
  CONSOLE_WRITE_LONG_INT,
  CONSOLE_WRITE_LONG_UINT,
  CONSOLE_WRITE_FLOAT,
  CONSOLE_WRITE_DOUBLE,
  CONSOLE_WRITE_STRING,
  CONSOLE_GET_BUFFER,
  NUM_CONSOLE_COMMANDS
} ConsoleCommand;

typedef enum ConsoleResponse {
  CONSOLE_RETURNING_BUFFER,
  NUM_CONSOLE_RESPONSES
} ConsleResponse;

// Exported coroutines
void* runConsole(void *args);

// Exported IO functions
int consolePrintf(const char *format, ...);

#ifdef printf
#undef printf
#endif
#define printf consolePrintf

#ifdef __cplusplus
} // extern "C"
#endif

static inline int printConsole(char message) {
  Comessage *comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessage->type = (int) CONSOLE_WRITE_CHAR;
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

  comessage->type = (int) CONSOLE_WRITE_UCHAR;
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

  comessage->type = (int) CONSOLE_WRITE_INT;
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

  comessage->type = (int) CONSOLE_WRITE_UINT;
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

  comessage->type = (int) CONSOLE_WRITE_LONG_INT;
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

  comessage->type = (int) CONSOLE_WRITE_LONG_UINT;
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

  comessage->type = (int) CONSOLE_WRITE_FLOAT;
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

  comessage->type = (int) CONSOLE_WRITE_DOUBLE;
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

  comessage->type = (int) CONSOLE_WRITE_STRING;
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

#endif // CONSOLE_H
