///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              12.02.2024
///
/// @file              Processes.h
///
/// @brief             Process functionality for NanoOs.
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

#ifndef PROCESSES_H
#define PROCESSES_H

// Custom includes
#include "NanoOs.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// @def getRunningProcess
///
/// @brief Function macro to get the pointer to the currently running Process
/// object.
#define getRunningProcess() getRunningCoroutine()

/// @def processId
///
/// @brief Function macro to get the numeric ProcessId given a pointer to a
/// Process object.
#define processId(process) coroutineId(process)

/// @def processYield
///
/// @brief Call to yield the processor to another process.
#define processYield() ((void) coroutineYield(NULL))

/// @def processMessageInit
///
/// @brief Function macro to initialize a process message.
#define processMessageInit(processMessage, type, data, size, waiting) \
  comessageInit(processMessage, type, data, size, waiting)

/// @def processMessageData
///
/// @brief Function macro to get the data pointer out of a process message.
#define processMessageData(processMessage) comessageData(processMessage)

/// @def processMessageSetDone
///
/// @brief Function macro to set a process message to the 'done' state.
#define processMessageSetDone(processMessage) comessageSetDone(processMessage)

/// @def processMessageRelease
///
/// @brief Function macro to release a process message.
#define processMessageRelease(processMessage) comessageRelease(processMessage)

/// @def processMessageWaitForDone
///
/// @brief Function macro to wait for a process message to enter the 'done'
/// state.
#define processMessageWaitForDone(processMessage, ts) \
  comessageWaitForDone(processMessage, ts)

/// @def processMessageQueueWaitForType
///
/// @brief Function macro to wait for a message of a specific type to be pushed
/// onto the running process's message queue.
#define processMessageQueueWaitForType(type, ts) \
  comessageQueueWaitForType(type, ts)

/// @def processMessageQueuePush
///
/// @brief Function macro to push a process message on to a process's message
/// queue.
#define processMessageQueuePush(process, message) \
  comessageQueuePush(process, message)

// Process message accessors
#define processMessageType(processMessagePointer) \
  comessageType(processMessagePointer)
#define processMessageData(processMessagePointer) \
  comessageData(processMessagePointer)
#define processMessageSize(processMessagePointer) \
  comessageSize(processMessagePointer)
#define processMessageWaiting(processMessagePointer) \
  comessageWaiting(processMessagePointer)
#define processMessageDone(processMessagePointer) \
  comessageDone(processMessagePointer)
#define processMessageInUse(processMessagePointer) \
  comessageInUse(processMessagePointer)
#define processMessageFrom(processMessagePointer) \
  comessageFrom(processMessagePointer)
#define processMessageTo(processMessagePointer) \
  comessageTo(processMessagePointer)
#define processMessageConfigured(processMessagePointer) \
  comessageConfigured(processMessagePointer)

/// @def nanoOsMessageFuncValue
///
/// @brief Given a pointer to a thrd_msg_t, extract the underlying function
/// value and cast it to the specified type.
#define nanoOsMessageFuncValue(msg, type) \
  ((type) ((NanoOsMessage*) (msg)->data)->func)

/// @def nanoOsMessageFuncPointer
///
/// @brief Given a pointer to a thrd_msg_t, extract the underlying function
/// value and cast it to the provided function pointer.
#define nanoOsMessageFuncPointer(msg, type) \
  ((type) nanoOsMessageFuncValue((msg), intptr_t))

/// @def nanoOsMessageDataValue
///
/// @brief Given a pointer to a thrd_msg_t, extract the underlying function
/// value and cast it to the specified type.
#define nanoOsMessageDataValue(msg, type) \
  ((type) ((NanoOsMessage*) (msg)->data)->data)

/// @def nanoOsMessageDataPointer
///
/// @brief Given a pointer to a thrd_msg_t, extract the underlying function
/// value and cast it to the provided function pointer.
#define nanoOsMessageDataPointer(msg, type) \
  ((type) nanoOsMessageDataValue((msg), intptr_t))

// Exported functionality
void* startCommand(void *args);
int sendProcessMessageToProcess(
  ProcessHandle processHandle, ProcessMessage *comessage);
int sendProcessMessageToPid(unsigned int pid, ProcessMessage *comessage);
ProcessMessage* getAvailableMessage(void);
ProcessMessage* sendNanoOsMessageToPid(int pid, int type,
  NanoOsMessageData func, NanoOsMessageData data, bool waiting);
void* waitForDataMessage(ProcessMessage *sent, int type, const struct timespec *ts);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PROCESSES_H
