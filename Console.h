// Standard C includes
#include <limits.h>
#include <string.h>

// Custom includes
#include "NanoOs.h"
#include "Coroutines.h"

#ifndef CONSOLE_H
#define CONSOLE_H

#ifdef __cplusplus
extern "C"
{
#endif

// Custom types
typedef enum ConsoleCommand {
  WRITE_CHAR,
  WRITE_UCHAR,
  WRITE_INT,
  WRITE_UINT,
  WRITE_LONG_INT,
  WRITE_LONG_UINT,
  WRITE_FLOAT,
  WRITE_DOUBLE,
  WRITE_STRING,
  NUM_CONSOLE_COMMANDS
} ConsoleCommand;

// Exported coroutines
void* runConsole(void *args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CONSOLE_H
