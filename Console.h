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
#define LED_CYCLE_TIME_MS 2000

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

/// @def CONSOLE_NUM_PORTS
///
/// @brief The number of console supports supported.
#define CONSOLE_NUM_PORTS 2

/// @def USB_SERIAL_PORT
///
/// Index into ConsoleState.conslePorts for the USB serial port.
#define USB_SERIAL_PORT 0

/// @def GPIO_SERIAL_PORT
///
/// Index into ConsoleState.conslePorts for the GPIO serial port.
#define GPIO_SERIAL_PORT 1

/// @enum ConsoleCommand
///
/// @brief The commands that the console understands via inter-process messages.
typedef enum ConsoleCommand {
  CONSOLE_WRITE_VALUE,
  CONSOLE_GET_BUFFER,
  CONSOLE_WRITE_BUFFER,
  CONSOLE_SET_PORT_SHELL,
  CONSOLE_ASSIGN_PORT,
  CONSOLE_RELEASE_PORT,
  CONSOLE_GET_OWNED_PORT,
  CONSOLE_SET_ECHO_PORT,
  CONSOLE_WAIT_FOR_INPUT,
  CONSOLE_GET_PROCESS_PORT,
  CONSOLE_GET_PORT_SHELL,
  CONSOLE_RELEASE_PID_PORT,
  NUM_CONSOLE_COMMANDS
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

/// @enum ConsoleResponse
///
/// @brief The responses that the console may send as a response to a command
/// from an inter-process message.  One of these will be sent back to the sender
/// of the command for synchronous commands.  Note that not all commands are
/// synchronous, so only responses for synchronous commands are defined.
typedef enum ConsoleResponse {
  CONSOLE_RETURNING_BUFFER,
  CONSOLE_RETURNING_PORT,
  CONSOLE_RETURNING_INPUT,
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

/// @struct ConsolePort
///
/// @brief Descriptor for a single console port that can be used for input from
/// a user.
///
/// @param consoleBuffer A pointer to the ConsoleBuffer used to buffer input
///   from the user.
/// @param consoleIndex An index into the buffer provided by consoleBuffer of
///   the next position to read a byte into.
/// @param owner The ID of the process that currently owns the console port.
/// @param shell The ID of the process that serves as the console port's shell.
/// @param waitingForInput Whether or not the owning process is currently
///   waiting for input from the user.
/// @param readByte A pointer to the non-blocking function that will attempt to
///   read a byte of input from the user.
/// @param echo Whether or not the data read from the port should be echoed back
///   to the port.
/// @param printString A pointer to the function that will print a string of
///   output to the console port.
typedef struct ConsolePort {
  ConsoleBuffer      *consoleBuffer;
  unsigned char       consoleIndex;
  COROUTINE_ID_TYPE   owner;
  COROUTINE_ID_TYPE   shell;
  bool                waitingForInput;
  int               (*readByte)(ConsolePort *consolePort);
  bool                echo;
  int               (*printString)(const char *string);
} ConsolePort;

/// @struct ConsoleState
///
/// @brief State maintained by the main console process and passed to the inter-
/// process command handlers.
///
/// @param consolePorts The array of ConsolePorts that will be polled for input
///   from the user.
/// @param consoleBuffers The array of ConsoleBuffers that can be used by
///   the console ports for input and by processes for output.
typedef struct ConsoleState {
  ConsolePort consolePorts[CONSOLE_NUM_PORTS];
  // consoleBuffers needs to come at the end.
  ConsoleBuffer consoleBuffers[CONSOLE_NUM_BUFFERS];
} ConsoleState;

/// @struct ConsolePortPidAssociation
///
/// @brief Structure to associate a console port with a process ID.  This
/// information is used in a CONSOLE_ASSIGN_PORT command.
///
/// @param consolePort The index into the consolePorts array of a ConsoleState
///   object.
/// @param processId The process ID associated with the port.
typedef struct ConsolePortPidAssociation {
  uint8_t           consolePort;
  COROUTINE_ID_TYPE processId;
} ConsolePortPidAssociation;

/// @union ConsolePortPidUnion
///
/// @brief Union of a ConsolePortPidAssociation and a NanoOsMessageData to
/// allow for easy conversion between the two.
///
/// @param consolePortPidAssociation The ConsolePortPidAssociation part.
/// @param nanoOsMessageData The NanoOsMessageData part.
typedef union ConsolePortPidUnion {
  ConsolePortPidAssociation consolePortPidAssociation;
  NanoOsMessageData         nanoOsMessageData;
} ConsolePortPidUnion;

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
