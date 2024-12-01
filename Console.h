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

/// @def LED_CYCLE_TIME_MS
///
/// @brief The amount of time, in milliseconds of a full cycle of the LED.  The
/// console process is also responsible for blinking the LED so that the user
/// has external confirmation that the system is still running.
#define LED_CYCLE_TIME_MS 1000

/// @def CONSOLE_BUFFER_SIZE
///
/// @brief The size, in bytes, of a single console buffer.  This is the number
/// of bytes that printf calls will have to work with.
#define CONSOLE_BUFFER_SIZE 96

/// @def CONSOLE_NUM_BUFFERS
///
/// @brief The number of console buffers that will be allocated within the main
/// console process's stack.
#define CONSOLE_NUM_BUFFERS  4

/// @enum ConsoleCommand
///
/// @brief The commands that the console understands via inter-process messages.
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

/// @enum ConsoleResponse
///
/// @brief The responses that the console may send as a response to a command
/// from an inter-process message.  One of these will be sent back to the sender
/// of the command for synchronous commands.  Note that not all commands are
/// synchronous, so only responses for synchronous commands are defined.
typedef enum ConsoleResponse {
  CONSOLE_RETURNING_BUFFER,
  NUM_CONSOLE_RESPONSES
} ConsleResponse;

/// @struct ConsoleBuffer
///
/// @brief Definition of a single console buffer that may be returned to a
/// sender of a CONSOLE_GET_BUFFER command via a CONSOLE_RETURNING_BUFFER
/// response.
///
/// @param inUse Whether or not this buffer is in use by a process.  Set by the
///   consoleGetBuffer function when getting a buffer for a caller and cleared
///   by the caller when no longer being used.
/// @param buffer The array of CONSOLE_BUFFER_SIZE characters that the calling
///   process can use.
typedef struct ConsoleBuffer {
  bool inUse;
  char buffer[CONSOLE_BUFFER_SIZE];
} ConsoleBuffer;

/// @struct ConsoleState
///
/// @brief State maintained by the main console process and passed to the inter-
/// process command handlers.
typedef struct ConsoleState {
  ConsoleBuffer consoleBuffers[CONSOLE_NUM_BUFFERS];
} ConsoleState;

// Inter-process command handlers
void consoleWriteCharHandler(
  ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteUCharHandler(
  ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteIntHandler(
  ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteUIntHandler(
  ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteLongIntHandler(
  ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteLongUIntHandler(
  ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteFloatHandler(
  ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteDoubleHandler(
  ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteStringHandler(
  ConsoleState *consoleState, Comessage *inputMessage);
void consoleGetBufferHandler(
  ConsoleState *consoleState, Comessage *inputMessage);
void consoleWriteBufferHandler(
  ConsoleState *consoleState, Comessage *inputMessage);

// Support functions
void consoleMessageCleanup(Comessage *inputMessage);
void handleConsoleMessages(ConsoleState *consoleState);
void blink();
void releaseConsole();
int printConsoleValue(ConsoleCommand command, void *value, size_t length);

// Exported processes
void* runConsole(void *args);

// Exported IO functions
ConsoleBuffer* consoleGetBuffer(void);

int consoleVFPrintf(FILE *stream, const char *format, va_list args);
#ifdef vfprintf
#undef vfprintf
#endif
#define vfprintf consoleVFPrintf

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

#endif // CONSOLE_H
