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

/// @def SCHEDULER_NUM_PROCESSES
///
/// @brief The number of processes managed by the scheduler.  This may be
/// different than the total number of processes managed by NanoOs.
#define SCHEDULER_NUM_PROCESSES (NANO_OS_NUM_PROCESSES - 1)

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
/// @param userId The UserId of the user that owns the process.
typedef struct ProcessInfoElement {
  int pid;
  const char *name;
  UserId userId;
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

/// @struct ProcessQueue
///
/// @brief Structure to manage an individual process queue
///
/// @param processes The array of pointers to Coroutine pointers from the
///   runningProcesses array.
/// @param head The index of the head of the queue.
/// @param tail The index of the tail of the queue.
/// @param numElements The number of elements currently in the queue.
typedef struct ProcessQueue {
  Coroutine **processes[SCHEDULER_NUM_PROCESSES];
  uint8_t head:4;
  uint8_t tail:4;
  uint8_t numElements:4;
} ProcessQueue;

/// @struct SchedulerState
///
/// @brief State data used by the scheduler.
///
/// @param runningProcesses Array that will hold the metadata for every running
///   process, including the scheduler.
/// @param ready Queue of processes that are allocated and not waiting on
///   anything but not currently running.  This queue never includes the
///   scheduler process.
/// @param waiting Queue of processes that are waiting on a mutex or condition
///   with an infinite timeout.  This queue never includes the scheduler
///   process.
/// @param timedWaiting Queue of processes that are waiting on a mutex or
///   condition with a defined timeout.  This queue never includes the scheduler
///   process.
/// @param free Queue of processes that are free within the runningProcesses
///   array.
typedef struct SchedulerState {
  RunningProcess runningProcesses[NANO_OS_NUM_PROCESSES];
  ProcessQueue ready;
  ProcessQueue waiting;
  ProcessQueue timedWaiting;
  ProcessQueue free;
} SchedulerState;

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
void startScheduler(void);
int sendComessageToPid(unsigned int pid, Comessage *comessage);
Comessage* getAvailableMessage(void);
Comessage* sendNanoOsMessageToPid(int pid, int type,
  NanoOsMessageData func, NanoOsMessageData data, bool waiting);
void* waitForDataMessage(Comessage *sent, int type, const struct timespec *ts);
ProcessInfo* getProcessInfo(void);
int killProcess(CoroutineId processId);
int runProcess(CommandEntry *commandEntry, char *consoleInput, int consolePort);
UserId getProcessUser(void);
int setProcessUser(UserId userId);

#ifdef __cplusplus
} // extern "C"
#endif

// Console.h has to be included separately and last.
#include "Console.h"

#endif // SCHEDULER_H
