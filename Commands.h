///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              11.21.2024
///
/// @file              Commands.h
///
/// @brief             Commands library for NanoOs.
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

// Custom includes
#include "NanoOs.h"
#include "Coroutines.h"

// #ifndef has to come after includes for this file.
#ifndef COMMANDS_H
#define COMMANDS_H

#ifdef __cplusplus
extern "C"
{
#endif

/// @typedef CommandFunction
///
/// @brief Type definition for the function signature that NanoOs commands must
/// have.
typedef int (*CommandFunction)(int argc, char **argv);

/// @struct CommandEntry
///
/// @brief Descriptor for a command that can be looked up and run by the
/// handleCommand function.
///
/// @param name The textual name of the command.
/// @param func A function pointer to the process that will be spawned to
///   execute the command.
/// @param shellProcess Whether this is a process that should replace the shell
///   process slot (true) or run in a separate slot (false).
/// @param help A one-line summary of what this command does.
typedef struct CommandEntry {
  const char      *name;
  CommandFunction  func;
  bool             shellProcess;
  const char      *help;
} CommandEntry;

// Exported support functions
int handleCommand(int consolePort, char *consoleInput);

// Exported processes
void* runShell(void *args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // COMMANDS_H
