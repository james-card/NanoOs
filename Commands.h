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

#define LED_CYCLE_TIME_MS 1000

// Custom types
typedef void* (*CommandFunction)(void* args);

typedef struct CommandEntry {
  const char      *name;
  CommandFunction  function;
  bool             spawnNewProcess;
} CommandEntry;

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
