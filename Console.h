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
  CONSOLE_WRITE_VALUE,
  CONSOLE_GET_BUFFER,
  CONSOLE_WRITE_BUFFER,
  CONSOLE_SET_PORT_SHELL,
  CONSOLE_ASSIGN_PORT,
  CONSOLE_ASSIGN_PORT_INPUT,
  CONSOLE_RELEASE_PORT,
  CONSOLE_GET_OWNED_PORT,
  CONSOLE_SET_ECHO_PORT,
  CONSOLE_WAIT_FOR_INPUT,
  CONSOLE_RELEASE_PID_PORT,
  CONSOLE_RELEASE_BUFFER,
  NUM_CONSOLE_COMMANDS,
  // Responses:
  CONSOLE_RETURNING_BUFFER,
  CONSOLE_RETURNING_PORT,
  CONSOLE_RETURNING_INPUT,
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
