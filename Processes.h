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

/// @struct RunningProcess
///
/// @brief Descriptor for a running process.
///
/// @param name The name of the command as stored in its CommandEntry or as
///   set by the scheduler at launch.
/// @param coroutine A pointer to the Coroutine instance that manages the
///   running command's execution state.
/// @param userId The numerical ID of the user that is running the process.
typedef struct RunningProcess {
  const char *name;
  Coroutine  *coroutine;
  UserId      userId;
} RunningProcess;

/// @struct ProcessInfoElement
///
/// @brief Information about a running process that is exportable to a user
/// process.
///
/// @param pid The numerical ID of the process.
/// @param name The name of the process.
typedef struct ProcessInfoElement {
  int pid;
  const char *name;
} ProcessInfoElement;

/// @struct ProcessInfo
///
/// @brief The object that's populated and returned by a getProcessInfo call.
///
/// @param numProcesses The number of elements in the processes array.
/// @param processes The array of ProcessInfoElements that describe the
///   processes.
typedef struct ProcessInfo {
  uint8_t numProcesses;
  ProcessInfoElement processes[1];
} ProcessInfo;

/// @struct User
///
/// @param userId The numeric ID for the user.
/// @param username The literal name of the user.
/// @param password The SHA1 hash of the user's password.
typedef struct User {
  UserId  userId;
  char   *username;
  char   *password;
} User;

/// @enum SchedulerCommand
///
/// @brief Commands understood by the scheduler inter-process message handler.
typedef enum SchedulerCommand {
  SCHEDULER_RUN_PROCESS,
  SCHEDULER_KILL_PROCESS,
  SCHEDULER_GET_NUM_RUNNING_PROCESSES,
  SCHEDULER_GET_PROCESS_INFO,
  SCHEDULER_GET_PROCESS_USER,
  SCHEDULER_SET_PROCESS_USER,
  NUM_SCHEDULER_COMMANDS
} SchedulerCommand;

/// @enum SchedulerResponse
///
/// @brief Responses the scheduler may send to a command.
typedef enum SchedulerResponse {
  SCHEDULER_PROCESS_COMPLETE,
  NUM_SCHEDULER_RESPONSES
} SchedulerResponse;

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

// externs
extern Coroutine *mainCoroutine;

// Exported functionality
void runScheduler(void);
int sendComessageToPid(unsigned int pid, Comessage *comessage);
Comessage* getAvailableMessage(void);
Comessage* sendNanoOsMessageToPid(int pid, int type,
  NanoOsMessageData func, NanoOsMessageData data, bool waiting);
void* waitForDataMessage(Comessage *sent, int type, const struct timespec *ts);
ProcessInfo* getProcessInfo(void);
int killProcess(COROUTINE_ID_TYPE processId);
int runProcess(CommandEntry *commandEntry, char *consoleInput, int consolePort);
UserId getProcessUser(void);
int setProcessUser(UserId userId);

#ifdef __cplusplus
} // extern "C"
#endif

// Console.h has to be included separately and last.
#include "Console.h"

#endif // SCHEDULER_H
