///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              11.21.2024
///
/// @file              Console.h
///
/// @brief             Console library for NanoOs.
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
#include <stdarg.h>

// Custom includes
#include "NanoOs.h"
#include "Coroutines.h"

#ifndef CONSOLE_H
#define CONSOLE_H

#ifdef __cplusplus
extern "C"
{
#endif

// Defines
#define LED_CYCLE_TIME_MS 1000
#define CONSOLE_BUFFER_SIZE 96
#define CONSOLE_NUM_BUFFERS  4

// Custom types
typedef enum ConsoleCommand {
  CONSOLE_WRITE_CHAR,
  CONSOLE_WRITE_UCHAR,
  CONSOLE_WRITE_INT,
  CONSOLE_WRITE_UINT,
  CONSOLE_WRITE_LONG_INT,
  CONSOLE_WRITE_LONG_UINT,
  CONSOLE_WRITE_FLOAT,
  CONSOLE_WRITE_DOUBLE,
  CONSOLE_WRITE_STRING,
  CONSOLE_GET_BUFFER,
  CONSOLE_WRITE_BUFFER,
  NUM_CONSOLE_COMMANDS
} ConsoleCommand;

typedef enum ConsoleResponse {
  CONSOLE_RETURNING_BUFFER,
  NUM_CONSOLE_RESPONSES
} ConsleResponse;

typedef struct ConsoleBuffer {
  char buffer[CONSOLE_BUFFER_SIZE];
  bool inUse;
} ConsoleBuffer;

typedef struct ConsoleState {
  ConsoleBuffer consoleBuffers[CONSOLE_NUM_BUFFERS];
} ConsoleState;

// Exported support functions.
void consoleWriteChar(ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteUChar(ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteInt(ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteUInt(ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteLongInt(ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteLongUInt(ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteFloat(ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteDouble(ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteString(ConsoleState *consoleState, Comessage *inputMessage);
void consoleGetBuffer(ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteBuffer(ConsoleState *consoleState, Comessage *inputMessage);
void handleConsoleMessages(ConsoleState *consoleState);
void blink();

// Exported coroutines
void* runConsole(void *args);

// Exported IO functions
ConsoleBuffer* consoleGetBufferFromMessage(void);
int consoleVFPrintf(FILE *stream, const char *format, va_list args);
int consoleFPrintf(FILE *stream, const char *format, ...);
#ifdef fprintf
#undef fprintf
#endif
#define fprintf consoleFPrintf

int consolePrintf(const char *format, ...);
#ifdef printf
#undef printf
#endif
#define printf consolePrintf

#ifdef __cplusplus
} // extern "C"
#endif

int printConsole(char message);
int printConsole(unsigned char message);
int printConsole(int message);
int printConsole(unsigned int message);
int printConsole(long int message);
int printConsole(long unsigned int message);
int printConsole(float message);
int printConsole(double message);
int printConsole(const char *message);
void releaseConsole();

#endif // CONSOLE_H
