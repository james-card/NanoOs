///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              12.02.2024
///
/// @file              Scheduler.h
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

/// @enum SchedulerCommand
///
/// @brief Commands understood by the scheduler inter-process message handler.
typedef enum SchedulerCommand {
  RUN_PROCESS,
  NUM_SCHEDULER_COMMANDS
} SchedulerCommand;


// Exported variables
extern RunningCommand *runningCommands;

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

// Exported functionality
void runScheduler(void);

#ifdef __cplusplus
} // extern "C"
#endif

// Console.h has to be included separately and last.
#include "Console.h"

#endif // SCHEDULER_H
