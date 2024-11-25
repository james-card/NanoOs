///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              11.21.2024
///
/// @file              Console.h
///
/// @brief             Console library for NanoOs.
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
  CONSOLE_WRITE_BUFFER,
  NUM_CONSOLE_COMMANDS
} ConsoleCommand;

typedef enum ConsoleResponse {
  CONSOLE_RETURNING_BUFFER,
  NUM_CONSOLE_RESPONSES
} ConsleResponse;

// Exported coroutines
void* runConsole(void *args);

// Exported IO functions
int consoleFPrintf(FILE *stream, const char *format, ...);
#ifdef fprintf
#undef fprintf
#endif
#define fprintf consoleFPrintf

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

  comessageInit(comessage, CONSOLE_WRITE_CHAR, NULL, message);
  comessageQueuePush(
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

  comessageInit(comessage, CONSOLE_WRITE_UCHAR, NULL, message);
  comessageQueuePush(
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

  comessageInit(comessage, CONSOLE_WRITE_INT, NULL, message);
  comessageQueuePush(
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

  comessageInit(comessage, CONSOLE_WRITE_UINT, NULL, message);
  comessageQueuePush(
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

  comessageInit(comessage, CONSOLE_WRITE_LONG_INT, NULL, message);
  comessageQueuePush(
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

  comessageInit(comessage, CONSOLE_WRITE_LONG_UINT, NULL, message);
  comessageQueuePush(
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

  ComessageData data = 0;
  memcpy(&data, &message, sizeof(message));
  comessageInit(comessage, CONSOLE_WRITE_FLOAT, NULL, data);
  comessageQueuePush(
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

  ComessageData data = 0;
  memcpy(&data, &message, sizeof(message));
  comessageInit(comessage, CONSOLE_WRITE_DOUBLE, NULL, data);
  comessageQueuePush(
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

  comessageInit(comessage, CONSOLE_WRITE_STRING, NULL,
    (intptr_t) message);
  comessageQueuePush(
    runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine,
    comessage);

  return 0;
}

static inline void releaseConsole() {
  // releaseConsole is sometimes called from within handleCommand, which runs
  // from within the console coroutine.  That means we can't do blocking prints
  // from this function.  i.e. We can't use printf here.  Use printConsole
  // instead.
  printConsole("> ");
}

#endif // CONSOLE_H
