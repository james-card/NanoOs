///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              12.26.2024
///
/// @file              NanoOsTypes.h
///
/// @brief             Types used across NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
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

#ifndef NANO_OS_TYPES_H
#define NANO_OS_TYPES_H

// Custom includes
#include "Coroutines.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Constants

/// @def NANO_OS_NUM_PROCESSES
///
/// @brief The total number of concurrent processes that can be run by the OS,
/// including the scheduler.
///
/// @note If this value is increased beyond 15, the number of bits used to store
/// the owner in a MemNode in MemoryManager.cpp must be extended and the value
/// of COROUTINE_ID_NOT_SET must be changed in NanoOsLibC.h.  If this value is
/// increased beyond 255, then the type defined by CoroutineId in
/// NanoOsLibC.h must also be extended.
#define NANO_OS_NUM_PROCESSES                             7

/// @def SCHEDULER_NUM_PROCESSES
///
/// @brief The number of processes managed by the scheduler.  This is one fewer
/// than the total number of processes managed by NanoOs since the scheduler is
/// a process.
#define SCHEDULER_NUM_PROCESSES (NANO_OS_NUM_PROCESSES - 1)

/// @def CONSOLE_BUFFER_SIZE
///
/// @brief The size, in bytes, of a single console buffer.  This is the number
/// of bytes that printf calls will have to work with.
#define CONSOLE_BUFFER_SIZE 48

/// @def NUM_CONSOLE_PORTS
///
/// @brief The number of console supports supported.
#define NUM_CONSOLE_PORTS 2

/// @def CONSOLE_NUM_BUFFERS
///
/// @brief The number of console buffers that will be allocated within the main
/// console process's stack.
#define CONSOLE_NUM_BUFFERS NUM_CONSOLE_PORTS

// Process status values
#define processSuccess  coroutineSuccess
#define processBusy     coroutineBusy
#define processError    coroutineError
#define processNomem    coroutineNomem
#define processTimedout coroutineTimedout

// Primitive types

/// @typedef Process
///
/// @brief Definition of the Process object used by the OS.
typedef Coroutine* ProcessHandle;

/// @typedef ProcessId
///
/// @brief Definition of the type to use for a process ID.
typedef CoroutineId ProcessId;

/// @typedef ProcessMessage
///
/// @brief Definition of the ProcessMessage object that processes will use for
/// inter-process communication.
typedef Comessage ProcessMessage;

/// @typedef CommandFunction
///
/// @brief Type definition for the function signature that NanoOs commands must
/// have.
typedef int (*CommandFunction)(int argc, char **argv);

/// @typedef UserId
///
/// @brief The type to use to represent a numeric user ID.
typedef int16_t UserId;

/// @typedef NanoOsMessageData
///
/// @brief Data type used in a NanoOsMessage.
typedef unsigned long long int NanoOsMessageData;

// Composite types

/// @struct NanoOsFile
///
/// @brief Definition of the FILE structure used internally to NanoOs.
///
/// @param file TODO Placeholder for file data.
typedef struct NanoOsFile {
  void *file;
} NanoOsFile;

/// @struct IoPipe
///
/// @brief Information that can be used to direct the output of one process
/// into the input of another one.
///
/// @param processId The process ID (PID) of the destination process.
/// @param messageType The type of message to send to the process.
typedef struct IoPipe {
  ProcessId processId;
  uint8_t messageType;
} Pipe;

/// @struct FileDescriptor
///
/// @brief Definition of a file descriptor that a process can use for input
/// and/or output.
///
/// @param inputPipe An IoPipe object that describes where the file descriptor
///   gets its input, if any.
/// @param outputPipe An IoPipe object that describes where the file descriptor
///   sends its output, if any.
typedef struct FileDescriptor {
  IoPipe inputPipe;
  IoPipe outputPipe;
} FileDescriptor;

/// @struct ProcessDescriptor
///
/// @brief Descriptor for a running process.
///
/// @param name The name of the command as stored in its CommandEntry or as
///   set by the scheduler at launch.
/// @param process A ProcessHandle that manages the running command's execution
///   state.
/// @param 
/// @param userId The numerical ID of the user that is running the process.
/// @param numFileDescriptors The number of FileDescriptor objects contained by
///   the fileDescriptors array.
/// @param fileDescriptors Pointer to an array of FileDescriptors that are
///   currently in use by the process.
typedef struct ProcessDescriptor {
  const char     *name;
  ProcessHandle   processHandle;
  ProcessId       processId;
  UserId          userId;
  uint8_t         numFileDescriptors;
  FileDescriptor *fileDescriptors;
} ProcessDescriptor;

/// @struct ProcessInfoElement
///
/// @brief Information about a running process that is exportable to a user
/// process.
///
/// @param pid The numerical ID of the process.
/// @param name The name of the process.
/// @param userId The UserId of the user that owns the process.
typedef struct ProcessInfoElement {
  int pid;
  const char *name;
  UserId userId;
} ProcessInfoElement;

/// @struct ProcessInfo
///
/// @brief The object that's populated and returned by a getProcessInfo call.
///
/// @param numProcesses The number of elements in the processes array.
/// @param processes The array of ProcessInfoElements that describe the
///   processes.
typedef struct ProcessInfo {
  uint8_t numProcesses;
  ProcessInfoElement processes[1];
} ProcessInfo;

/// @struct ProcessQueue
///
/// @brief Structure to manage an individual process queue
///
/// @param name The string name of the queue for use in error messages.
/// @param processes The array of pointers to ProcessDescriptors from the
///   allProcesses array.
/// @param head The index of the head of the queue.
/// @param tail The index of the tail of the queue.
/// @param numElements The number of elements currently in the queue.
typedef struct ProcessQueue {
  const char *name;
  ProcessDescriptor *processes[SCHEDULER_NUM_PROCESSES];
  uint8_t head:4;
  uint8_t tail:4;
  uint8_t numElements:4;
} ProcessQueue;

/// @struct SchedulerState
///
/// @brief State data used by the scheduler.
///
/// @param allProcesses Array that will hold the metadata for every process,
///   including the scheduler.
/// @param ready Queue of processes that are allocated and not waiting on
///   anything but not currently running.  This queue never includes the
///   scheduler process.
/// @param waiting Queue of processes that are waiting on a mutex or condition
///   with an infinite timeout.  This queue never includes the scheduler
///   process.
/// @param timedWaiting Queue of processes that are waiting on a mutex or
///   condition with a defined timeout.  This queue never includes the scheduler
///   process.
/// @param free Queue of processes that are free within the allProcesses
///   array.
/// @param hostname The contents of the /etc/hostname file read at startup.
/// @param bootComplete Whether or not all the setup and configuration of the
///   startScheduler function has completed.
typedef struct SchedulerState {
  ProcessDescriptor allProcesses[NANO_OS_NUM_PROCESSES];
  ProcessQueue ready;
  ProcessQueue waiting;
  ProcessQueue timedWaiting;
  ProcessQueue free;
  char *hostname;
  bool bootComplete;
} SchedulerState;

/// @struct CommandDescriptor
///
/// @brief Container of information for launching a process.
///
/// @param consolePort The index of the ConsolePort the input came from.
/// @param consoleInput The input as provided by the console.
/// @param callingProcess The process ID of the process that is launching the
///   command.
/// @param schedulerState A pointer to the SchedulerState structure maintained
///   by the scheduler.
typedef struct CommandDescriptor {
  int                consolePort;
  char              *consoleInput;
  ProcessId         callingProcess;
  SchedulerState    *schedulerState;
} CommandDescriptor;

/// @struct CommandEntry
///
/// @brief Descriptor for a command that can be looked up and run by the
/// handleCommand function.
///
/// @param name The textual name of the command.
/// @param func A function pointer to the process that will be spawned to
///   execute the command.
/// @param help A one-line summary of what this command does.
typedef struct CommandEntry {
  const char      *name;
  CommandFunction  func;
  const char      *help;
} CommandEntry;

/// @struct ConsoleBuffer
///
/// @brief Definition of a single console buffer that may be returned to a
/// sender of a CONSOLE_GET_BUFFER command via a CONSOLE_RETURNING_BUFFER
/// response.
///
/// @param inUse Whether or not this buffer is in use by a process.  Set by the
///   consoleGetBuffer function when getting a buffer for a caller and cleared
///   by the caller when no longer being used.
/// @param numBytes The number of valid bytes that are in the buffer.
/// @param buffer The array of CONSOLE_BUFFER_SIZE characters that the calling
///   process can use.
typedef struct ConsoleBuffer {
  bool inUse;
  uint8_t numBytes;
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
/// @param outputOwner The ID of the process that currently has the ability to
///   write output to the port.
/// @param inputOwner The ID of the process that currently has the ability to
///   read input from the port.
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
  uint8_t             consoleIndex;
  ProcessId           outputOwner;
  ProcessId           inputOwner;
  ProcessId           shell;
  bool                waitingForInput;
  int               (*readByte)(ConsolePort *consolePort);
  bool                echo;
  int               (*printString)(const char *string, size_t numBytes);
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
  ConsolePort consolePorts[NUM_CONSOLE_PORTS];
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
  ProcessId         processId;
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

/// @struct ReallocMessage
///
/// @brief Structure that holds the data needed to make a request to reallocate
/// an existing pointer.
///
/// @param ptr The pointer to be reallocated.  If this value is NULL, new
///   memory will be allocated.
/// @param size The number of bytes to allocate.  If this value is 0, the memory
///   at ptr will be freed.
/// @param responseType The response type the caller is waiting for.
typedef struct ReallocMessage {
  void *ptr;
  size_t size;
  int responseType;
} ReallocMessage;

/// @struct MemoryManagerState
///
/// @brief State metadata the memory manager process uses for allocations and
/// deallocations.
///
/// @parm mallocBuffer A pointer to the first character of the buffer to
///   allocate memory from.
/// @param mallocNext A pointer to the next free piece of memory.
/// @param mallocStart The numeric value of the first address available to
///   allocate memory from.
/// @param mallocEnd The numeric value of the last address available to allocate
///   memory from.
typedef struct MemoryManagerState {
  char *mallocBuffer;
  char *mallocNext;
  uintptr_t mallocStart;
  uintptr_t mallocEnd;
} MemoryManagerState;

/// @struct User
///
/// @param userId The numeric ID for the user.
/// @param username The literal name of the user.
/// @param checksum The checksum of the username and password.
typedef struct User {
  UserId        userId;
  const char   *username;
  unsigned int  checksum;
} User;

/// @struct NanoOsMessage
///
/// @brief A generic message that can be exchanged between processes.
///
/// @param func Information about the function to run, cast to an unsigned long
///   long int.
/// @param data Information about the data to use, cast to an unsigned long
///   long int.
typedef struct NanoOsMessage {
  NanoOsMessageData  func;
  NanoOsMessageData  data;
} NanoOsMessage;

/// @struct BlockStorageDevice
///
/// @brief The collection of data and functions needed to interact with a block
/// storage device.
///
/// @param context The device-specific context to pass to the functions.
/// @param readBlocks Function pointer for the function to read a given number
///   of blocks from the storage device.
/// @param writeBlocks Function pointer for the function to write a given number
///   of blocks to the storage device.
/// @param partitionNumber The one-based partition index that is to be used by
///   a filesystem.
typedef struct BlockStorageDevice {
  void *context;
  int (*readBlocks)(void *context, uint32_t startBlock,
    uint32_t numBlocks, uint16_t blockSize, uint8_t *buffer);
  int (*writeBlocks)(void *context, uint32_t startBlock,
    uint32_t numBlocks, uint16_t blockSize, const uint8_t *buffer);
  uint8_t partitionNumber;
} BlockStorageDevice;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NANO_OS_TYPES_H
