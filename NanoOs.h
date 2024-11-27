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

#define NANO_OS_NUM_COROUTINES             7
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
extern RunningCommand *runningCommands;
extern Comessage *messages;

// Function defines
#define nanoOsExitProcess(returnValue) \
  /* We need to clear the coroutine pointer. */ \
  runningCommands[coroutineId(NULL)].coroutine = NULL; \
  \
  return returnValue /* Deliberately omitting semicolon. */

#ifdef __cplusplus
extern "C"
{
#endif

// Arduino functions
void setup();
void loop();

// NanoOs command functions
void callFunction(Comessage *comessage);
void handleMainCoroutineMessage(void);

// Dummy coroutine
void* dummy(void *args);

// Support functions
int getFreeRamBytes(void);
long getElapsedMilliseconds(unsigned long startTime);
Comessage* getAvailableMessage(void);
Comessage* sendDataMessageToCoroutine(
  Coroutine *coroutine, int type, void *data, bool waiting);
Comessage* sendDataMessageToPid(int pid, int type, void *data, bool waiting);
void* waitForDataMessage(Comessage *sent, int type);

#ifdef __cplusplus
} // extern "C"
#endif

// Console.h has to be included separately (and last).
#include "Console.h"

#endif // NANO_OS_H
