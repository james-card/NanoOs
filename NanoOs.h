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

#ifdef __cplusplus
extern "C"
{
#endif

/// @def NANO_OS_NUM_COROUTINES
///
/// @brief The total number of concurrent processes that can be run by the OS.
#define NANO_OS_NUM_COROUTINES             7

/// @def NANO_OS_STACK_SIZE
///
/// @brief The minimum size for an individual process's stack.  Actual size will
/// be slightly larger than this.  This value needs to be a multiple of 
/// COROUTINE_STACK_CHUNK_SIZE in Coroutines.h
#define NANO_OS_STACK_SIZE               512
#if ((NANO_OS_STACK_SIZE % COROUTINE_STACK_CHUNK_SIZE) != 0)
#error "NANO_OS_STACK_SIZE must be a multiple of COROUTINE_STACK_CHUNK_SIZE."
#endif

/// @def NANO_OS_NUM_MESSAGES
///
/// @brief The total number of inter-process messages that will be available
/// for use by processes.
#define NANO_OS_NUM_MESSAGES               8

/// @def NANO_OS_SYSTEM_PROCESS_ID
///
/// @brief The process ID (PID) of the process that is reserved for system
/// processes.
#define NANO_OS_SYSTEM_PROCESS_ID        0

/// @def NANO_OS_CONSOLE_PROCESS_ID
///
/// @brief The process ID (PID) of the process that will run the console.
#define NANO_OS_CONSOLE_PROCESS_ID         1

/// @def NANO_OS_FIRST_PROCESS_ID
///
/// @brief The process ID (PID) of the first user process.
#define NANO_OS_FIRST_PROCESS_ID           2

/// @def NANO_OS_VERSION
///
/// @brief The version string for NanoOs
#define NANO_OS_VERSION "0.0.1"

/// @enum MainCoroutineCommand
///
/// @brief Commands understood by the main coroutine inter-process message
/// handler.
typedef enum MainCoroutineCommand {
  CALL_FUNCTION,
  NUM_MAIN_COROUTINE_COMMANDS
} MainCoroutineCommand;

// Exported variables
extern RunningCommand *runningCommands;
extern Comessage *messages;

/// @def nanoOsExitProcess
///
/// @brief Macro to clean up the process's global state and then exit cleanly.
///
/// @param returnValue The value to return back to the scheduler.
#define nanoOsExitProcess(returnValue) \
  /* We need to clear the coroutine pointer. */ \
  runningCommands[coroutineId(NULL)].coroutine = NULL; \
  \
  return returnValue /* Deliberately omitting semicolon. */

// Arduino functions
void setup();
void loop();

// NanoOs inter-process message handler functions
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

// Console.h has to be included separately and last.
#include "Console.h"

#endif // NANO_OS_H
