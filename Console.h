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
#include "Coroutines.h"
#include "NanoOs.h"

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
#define LED_CYCLE_TIME_MS 2000

/// @def USB_SERIAL_PORT
///
/// Index into ConsoleState.conslePorts for the USB serial port.
#define USB_SERIAL_PORT 0

/// @def GPIO_SERIAL_PORT
///
/// Index into ConsoleState.conslePorts for the GPIO serial port.
#define GPIO_SERIAL_PORT 1

/// @enum ConsoleCommandResponse
///
/// @brief The commands and responses that the console understands via
/// inter-process messages.
typedef enum ConsoleCommandResponse {
  // Commands:
  CONSOLE_WRITE_VALUE           =  0,
  CONSOLE_GET_BUFFER            =  1,
  CONSOLE_WRITE_BUFFER          =  2,
  CONSOLE_SET_PORT_SHELL        =  3,
  CONSOLE_ASSIGN_PORT           =  4,
  CONSOLE_RELEASE_PORT          =  5,
  CONSOLE_GET_OWNED_PORT        =  6,
  CONSOLE_SET_ECHO_PORT         =  7,
  CONSOLE_WAIT_FOR_INPUT        =  8,
  CONSOLE_RELEASE_PID_PORT      =  9,
  CONSOLE_RELEASE_BUFFER        = 10,
  // Responses:
  CONSOLE_RETURNING_BUFFER      = 11,
  CONSOLE_RETURNING_PORT        = 12,
  CONSOLE_RETURNING_INPUT       = 13,
  NUM_CONSOLE_COMMAND_RESPONSES
} ConsoleCommand;

/// @enum ConsoleValueType
///
/// @brief Types to be used with the CONSOLE_WRITE_VALUE command.
typedef enum ConsoleValueType {
  CONSOLE_VALUE_CHAR,
  CONSOLE_VALUE_UCHAR,
  CONSOLE_VALUE_INT,
  CONSOLE_VALUE_UINT,
  CONSOLE_VALUE_LONG_INT,
  CONSOLE_VALUE_LONG_UINT,
  CONSOLE_VALUE_FLOAT,
  CONSOLE_VALUE_DOUBLE,
  CONSOLE_VALUE_STRING,
  NUM_CONSOLE_VALUES
} ConsoleValueType;

// Support functions
void releaseConsole(void);
int getOwnedConsolePort(void);
int setConsoleEcho(bool desiredEchoState);

// Exported processes
void* runConsole(void *args);

// Exported IO functions
ConsoleBuffer* consoleGetBuffer(void);

int consoleFPuts(const char *s, FILE *stream);
#ifdef fputs
#undef fputs
#endif
#define fputs consoleFPuts

int consolePuts(const char *s);
#ifdef puts
#undef puts
#endif
#define puts consolePuts

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

char *consoleFGets(char *buffer, int size, FILE *stream);
#ifdef fgets
#undef fgets
#endif
#define fgets consoleFGets

int consoleVFScanf(FILE *stream, const char *format, va_list ap);
#ifdef vfscanf
#undef vfscanf
#endif
#define vfscanf consoleVFScanf

int consoleFScanf(FILE *stream, const char *format, ...);
#ifdef fscanf
#undef fscanf
#endif
#define fscanf consoleFScanf

int consoleScanf(const char *format, ...);
#ifdef scanf
#undef scanf
#endif
#define scanf consoleScanf

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
