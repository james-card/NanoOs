///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              12.02.2024
///
/// @file              Processes.h
///
/// @brief             Process scheduler functionality for NanoOs.
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

#ifndef SCHEDULER_H
#define SCHEDULER_H

// Custom includes
#include "NanoOs.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// @struct RunningCommand
///
/// @brief Descriptor for a running instance of a command.
///
/// @param name The name of the command as stored in its CommandEntry.
/// @param coroutine A pointer to the Coroutine instance that manages the
///   running command's execution state.
typedef struct RunningCommand {
  const char *name;
  Coroutine  *coroutine;
} RunningCommand;

/// @enum SchedulerCommand
///
/// @brief Commands understood by the scheduler inter-process message handler.
typedef enum SchedulerCommand {
  SCHEDULER_RUN_PROCESS,
  SCHEDULER_KILL_PROCESS,
  SCHEDULER_GET_NUM_RUNNING_PROCESSES,
  NUM_SCHEDULER_COMMANDS
} SchedulerCommand;

/// @typedef NanoOsMessageData
///
/// @brief Data type used in a NanoOsMessage.
typedef unsigned long long int NanoOsMessageData;

/// @struct NanoOsMessage
///
/// @brief A generic message that can be exchanged between processes.
///
/// @param func Information about the function to run, cast to an unsigned long
///   long int.
/// @param data Information about the data to use, cast to an unsigned long
///   long int.
/// @param comessage A pointer to the comessage that points to this
///   NanoOsMessage.
typedef struct NanoOsMessage {
  NanoOsMessageData  func;
  NanoOsMessageData  data;
  Comessage         *comessage;
} NanoOsMessage;

/// @def NUM_PROCESS_INFO_COLUMNS
///
/// @brief The number of columns of information that will be returned when the
/// running processes are intererogated.
#define NUM_PROCESS_INFO_COLUMNS 2

/// @def nanoOsMessageFuncValue
///
/// @brief Given a pointer to a thrd_msg_t, extract the underlying function
/// value and cast it to the specified type.
#define nanoOsMessageFuncValue(msg, type) \
  ((type) ((NanoOsMessage*) msg->data)->func)

/// @def nanoOsMessageFuncPointer
///
/// @brief Given a pointer to a thrd_msg_t, extract the underlying function
/// value and cast it to the provided function pointer.
#define nanoOsMessageFuncPointer(msg, type) \
  ((type) nanoOsMessageFuncValue(msg, intptr_t))

/// @def nanoOsMessageDataValue
///
/// @brief Given a pointer to a thrd_msg_t, extract the underlying function
/// value and cast it to the specified type.
#define nanoOsMessageDataValue(msg, type) \
  ((type) ((NanoOsMessage*) msg->data)->data)

/// @def nanoOsMessageDataPointer
///
/// @brief Given a pointer to a thrd_msg_t, extract the underlying function
/// value and cast it to the provided function pointer.
#define nanoOsMessageDataPointer(msg, type) \
  ((type) nanoOsMessageDataValue(msg, intptr_t))

/// @def stringDestroy
///
/// @brief Convenience macro for the common operation of destroying a string.
#define stringDestroy(string) ((char*) (free((void*) string), NULL))

/// @def nanoOsExitProcess
///
/// @brief Macro to clean up the process's global state and then exit cleanly.
///
/// @param returnValue The value to return back to the scheduler.
#define nanoOsExitProcess(returnValue) \
  return returnValue /* Deliberately omitting semicolon. */

// Exported functionality
void runScheduler(void);
int sendComessageToPid(unsigned int pid, Comessage *comessage);
Comessage* getAvailableMessage(void);
Comessage* sendNanoOsMessageToPid(int pid, int type,
  NanoOsMessageData func, NanoOsMessageData data, bool waiting);
void* waitForDataMessage(Comessage *sent, int type, const struct timespec *ts);

#ifdef __cplusplus
} // extern "C"
#endif

// Console.h has to be included separately and last.
#include "Console.h"

#endif // SCHEDULER_H
