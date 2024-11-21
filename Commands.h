// Standard C includes
#include <limits.h>
#include <string.h>

// Custom includes
#include "NanoOs.h"
#include "Coroutines.h"

#ifndef COMMANDS_H
#define COMMANDS_H

#ifdef __cplusplus
extern "C"
{
#endif

// Custom types

/// @struct CommandEntry
///
/// @brief Descriptor for a command that can be looked up and run by the
/// handleCommand function.
///
/// @param name The textual name of the command.
/// @param function A function pointer to the coroutine that will be spawned to
///   execute the command.
/// @param userProcess Whether this is a user process that should be run in one
///   of the general-purpose process slots (true) or it should be run in the
///   slot reserved for system processes (false).
typedef struct CommandEntry {
  const char        *name;
  CoroutineFunction  function;
  bool               userProcess;
} CommandEntry;

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

// Exported functions
void handleCommand(const char *consoleInput);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // COMMANDS_H
