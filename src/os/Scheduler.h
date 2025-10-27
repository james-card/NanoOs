///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              12.02.2024
///
/// @file              Scheduler.h
///
/// @brief             Scheduler functionality for NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
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
#include "NanoOsTypes.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// @enum SchedulerCommandResponse
///
/// @brief Commands and responses understood by the scheduler inter-process
/// message handler.
typedef enum SchedulerCommandResponse {
  // Commands:
  SCHEDULER_RUN_PROCESS,
  SCHEDULER_KILL_PROCESS,
  SCHEDULER_GET_NUM_RUNNING_PROCESSES,
  SCHEDULER_GET_PROCESS_INFO,
  SCHEDULER_GET_PROCESS_USER,
  SCHEDULER_SET_PROCESS_USER,
  SCHEDULER_CLOSE_ALL_FILE_DESCRIPTORS,
  SCHEDULER_GET_HOSTNAME,
  SCHEDULER_EXECVE,
  NUM_SCHEDULER_COMMANDS,
  // Responses:
  SCHEDULER_PROCESS_COMPLETE,
} SchedulerCommand;

// Exported functionality
void startScheduler(SchedulerState **coroutineStatePointer);
ProcessHandle schedulerGetProcessByPid(unsigned int pid);
int schedulerNotifyProcessComplete(ProcessId processId);
int schedulerWaitForProcessComplete(void);
ProcessId schedulerGetNumRunningProcesses(struct timespec *timeout);
ProcessInfo* schedulerGetProcessInfo(void);
int schedulerKillProcess(ProcessId processId);
int schedulerRunProcess(
  const CommandEntry *commandEntry, char *consoleInput, int consolePort);
UserId schedulerGetProcessUser(void);
int schedulerSetProcessUser(UserId userId);
FileDescriptor* schedulerGetFileDescriptor(FILE *stream);
int schedulerCloseAllFileDescriptors(void);
const char* schedulerGetHostname(void);
int schedulerExecve(const char *pathname,
  char *const argv[], char *const envp[]);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SCHEDULER_H
