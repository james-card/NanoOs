///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              01.12.2022
///
/// @file              Coroutines.h
///
/// @brief             Minimal coroutines for C.
///
/// @details           Originally written by Tony Finch <dot@dotat.at> and
///                    released to the public domain (see
///                    http://creativecommons.org/publicdomain/zero/1.0/).
///                    The API was modeled after Lua's coroutines (see
///                    http://www.lua.org/manual/5.1/manual.html#2.11).
///                    Source code cloned from
///                    git://git.chiark.greenend.org.uk/~fanf/picoro.git
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

#ifndef COROUTINES_H
#define COROUTINES_H

#include <setjmp.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <stdint.h>
#include "NanoOsLibC.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if !defined(SINGLE_CORE_COROUTINES) && !defined(THREAD_SAFE_COROUTINES)
#define THREAD_SAFE_COROUTINES
#endif
#if defined(SINGLE_CORE_COROUTINES) && defined(THREAD_SAFE_COROUTINES)
#error "Only one of SINGLE_CORE_COROUTINES or THREAD_SAFE_COROUTINES may be defined."
#endif

// Base coroutine support.

// Coroutine status values.
#define coroutineSuccess  0
#define coroutineBusy     1
#define coroutineError    2
#define coroutineNomem    3
#define coroutineTimedout 4

/// @def COROUTINE_NOT_RESUMABLE
///
/// @brief Special value to indicate to a caller of coroutineResume() that the
/// provided coroutine cannot be resumed, either because it is blocked or
/// because it has completed.
#define COROUTINE_NOT_RESUMABLE ((void*) ((intptr_t) -1))

/// @def COROUTINE_BLOCKED
///
/// @brief Special value to indicate to a caller of coroutineResume() that the
/// provided coroutine is blocked within a blocking coroutine operation.
#define COROUTINE_BLOCKED ((void*) ((intptr_t) -2))

/// @def COROUTINE_STACK_CHUNK_SIZE
///
/// @brief The size of a single chunk of the stack allocated by
/// coroutineAllocateStack.
#define COROUTINE_STACK_CHUNK_SIZE 512

/// @def COROUTINE_DEFAULT_STACK_SIZE
///
/// @brief The default stack size to allocate, in bytes.
#ifndef COROUTINE_DEFAULT_STACK_SIZE
#define COROUTINE_DEFAULT_STACK_SIZE 16384
#endif

/// @def COROUTINE_ID_TYPE
///
/// @brief The integer type to use for coroutine IDs.  Defaults to 64-bit IDs
/// if none is provided.
#ifndef COROUTINE_ID_TYPE
#define COROUTINE_ID_TYPE int64_t
#endif

/// @def COROUTINE_ID_TYPE_int64_t_int64_t
///
/// @brief Define that gets matched when COROUTINE_ID_TYPE is an int64_t.
#define COROUTINE_ID_TYPE_int64_t_int64_t 1

/// @def COROUTINE_ID_TYPE_int32_t_int32_t
///
/// @brief Define that gets matched when COROUTINE_ID_TYPE is an int32_t.
#define COROUTINE_ID_TYPE_int32_t_int32_t 1

/// @def COROUTINE_ID_TYPE_int16_t_int16_t
///
/// @brief Define that gets matched when COROUTINE_ID_TYPE is an int16_t.
#define COROUTINE_ID_TYPE_int16_t_int16_t 1

/// @def COROUTINE_ID_TYPE_int8_t_int8_t
///
/// @brief Define that gets matched when COROUTINE_ID_TYPE is an int8_t.
#define COROUTINE_ID_TYPE_int8_t_int8_t   1

/// @def EXPAND_COROUTINE_ID_TYPE
///
/// @brief Second-level macro to fully expand the types passed into
/// TEST_COROUTINE_ID_TYPE.
#define EXPAND_COROUTINE_ID_TYPE(type1, type2) \
  COROUTINE_ID_TYPE_##type1##_##type2

/// @def TEST_COROUTINE_ID_TYPE
///
/// @brief Determine if the type specified by type1 matches the type specified
/// by type2.
#define TEST_COROUTINE_ID_TYPE(type1, type2) \
  EXPAND_COROUTINE_ID_TYPE(type1, type2)

/// @def COROUTINE_ID_NOT_SET
///
/// @brief Special value to indicate that a coroutine's ID value is not set.
/// This is the initial value just after the coroutine constructor completes.
#ifndef COROUTINE_ID_NOT_SET
#if TEST_COROUTINE_ID_TYPE(COROUTINE_ID_TYPE, int64_t)
#define COROUTINE_ID_NOT_SET ((int64_t) 0x8000000000000000)
#elif TEST_COROUTINE_ID_TYPE(COROUTINE_ID_TYPE, int32_t)
#define COROUTINE_ID_NOT_SET ((int32_t) 0x80000000)
#elif TEST_COROUTINE_ID_TYPE(COROUTINE_ID_TYPE, int16_t)
#define COROUTINE_ID_NOT_SET ((int16_t) 0x8000)
#elif TEST_COROUTINE_ID_TYPE(COROUTINE_ID_TYPE, int8_t)
#define COROUTINE_ID_NOT_SET ((int8_t) 0x80)
#else
#error "Invalid COROUTINE_ID_TYPE."
#endif // COROUTINE_ID_TYPE
#endif // COROUTINE_ID_NOT_SET

/// @enum CoroutineState
///
/// @brief States that a Coroutine can be in.
typedef enum CoroutineState {
  COROUTINE_STATE_NOT_RUNNING,
  COROUTINE_STATE_RUNNING,
  COROUTINE_STATE_BLOCKED,
  NUM_COROUTINE_STATES
} CoroutineState;

/// @typedef CoroutineFunction
///
/// Definition for a function signature that can be used as a coroutine.
typedef void* (*CoroutineFunction)(void *arg);

/// @union CoroutineFuncData
///
/// @brief Translation between a function pointer and a data pointer.
///
/// @details Due to the way this library works, we sometimes need to pass and
/// return function pointers to our yield and resume functions, which take and
/// return data pointers.  ISO C doesn't permit casting between these two, so
/// we use a union to do the conversion.
///
/// @param func The function pointer portion of the pointer value.
/// @param data The data pointer portion of the pointer value.
typedef union CoroutineFuncData {
  CoroutineFunction func;
  void *data;
} CoroutineFuncData;

// Forward declarations.  Doxygen below.
typedef struct Comutex Comutex;
typedef struct Cocondition Cocondition;
typedef struct Coroutine Coroutine;
typedef struct Comessage Comessage;

// Coroutine mutex support.

// Comutex types
#define comutexPlain     0
#define comutexRecursive 1
#define comutexTimed     2

/// @struct Comutex
///
/// @brief Definition for a coroutine mutex to provide mutual exclusion between
/// coroutines.
///
/// @param lastYieldValue The last value returned by a yield call if a comutex
///   lock function blocked while acquiring the lock, or NULL if the lock was
///   acquired on the first attempt.
/// @param type The type of the mutex (see above type definitions)
/// @param coroutine The coroutine that has the lock.
/// @param recursionLevel The number of times this mutex has been successfully
///   locked in this coroutine.
typedef struct Comutex {
  void *lastYieldValue;
  int type;
  Coroutine *coroutine;
  int recursionLevel;
} Comutex;

// Coroutine condition support.

/// @struct Cocondition
///
/// @brief Definition for a coroutine condition for signaling between
/// coroutines.
///
/// @param lastYieldValue The last value returned by a yield call while a
///   cocondition wait function was blocked.
/// @param numWaiters The number of coroutines blocked waiting on this
///   condition.
/// @param numSignal The number of signals emitted for unblocking waiting
///   coroutines.
/// @param head The head of the coroutine queue (the next coroutine to signal).
/// @param tail The tail of the coroutine queue (where the next waiting
///   coroutine will be added).
typedef struct Cocondition {
  void *lastYieldValue;
  int numWaiters;
  int numSignals;
  Coroutine *head;
  Coroutine *tail;
} Cocondition;

// Coroutine base support.

/// @struct Coroutine
///
/// @brief Data structure to manage an individual coroutine.
///
/// @param next Pointer to the next Coroutine on the list.
/// @param context The jmp_buf to hold the context of the coroutine.
/// @param id The ID of the coroutine.
/// @param state The state of the coroutine.  (See enum above.)
/// @param nextToSignal The next coroutine to signal when waiting on a signal.
/// @param prevToSignal The previous coroutine to signal when waiting on a
///   signal.
/// @param passed The CoroutineFuncData that's passed between contexts by the
///   coroutinePass function (on a yield or resume call).
/// @param nextMessage A pointer to the next message that is waiting for the
///   coroutine to process.
/// @param lastMessage A pointer to the last message that is waiting for the
///   coroutine to process.
/// @param messageCondition A condition (Cocondition) that will allow for
///   signalling between coroutines when adding a message to the queue.
/// @param messageLock A mutex (Comutex) to guard the message condition.
typedef struct Coroutine {
  struct Coroutine *next;
  jmp_buf context;
  COROUTINE_ID_TYPE id;
  CoroutineState state;
  struct Coroutine *nextToSignal;
  struct Coroutine *prevToSignal;
  jmp_buf resetContext;
  CoroutineFuncData passed;
  Comessage *nextMessage;
  Comessage *lastMessage;
  Cocondition messageCondition;
  Comutex messageLock;
} Coroutine;

// Coroutine message support.

/// @struct Comessage
///
/// @brief Definition for a coroutine message that can be pushed onto a
/// Coroutine's message queue.
///
/// @param type Integer value designating the type of message for the receiving
///   coroutine.
/// @param data A pointer to the data of the message, if any.
/// @param size The number of bytes pointed to by the data pointer.
/// @param next A pointer to the next Comessage in a coroutine's message queue.
/// @param waiting A Boolean flag to indicate whether or not the sender is
///   waiting on a response message from the recipient of the message.
/// @param done A Boolean flag to indicate whether or not the receiving
///   coroutine has handled the message yet.
/// @param inUse A Boolean flag to indicate whether or not this Comessage is in
///   in use.
/// @param from A pointer to the Coroutine instance for the sending coroutine.
/// @param to A pointer to the Coroutine instance for the receiving coroutine.
/// @param condition A condition (Cocondition) that will allow for signalling
///   between coroutines when setting the done flag.
/// @param lock A mutex (Comutex) to guard the condition.
/// @param configured Whether or not the members of the message that requrie
///   initializatoin have been configured yet.
typedef struct Comessage {
  int type;
  void *data;
  size_t size;
  struct Comessage *next;
  bool waiting;
  bool done;
  bool inUse;
  Coroutine *from;
  Coroutine *to;
  Cocondition condition;
  Comutex lock;
  bool configured;
} Comessage;

/// @def coroutineResumable(coroutinePointer)
///
/// @brief Examines a coroutine to determine whether or not it can be resumed.
/// A coroutine can be resumed if it is not on the running or idle lists.
///
/// @param coroutinePointer A pointer to the Coroutine to examine.
///
/// @return Returns false when the coroutine has run to completion or when it is
/// blocked inside coroutineResume() and true otherwise.
#define coroutineResumable(coroutinePointer) \
  (((coroutinePointer) != NULL) && ((coroutinePointer)->next == NULL))

/// @def coroutineFinished(coroutinePointer)
///
/// @brief Examines a coroutine to determine whether or not it has completed.
///
/// @param coroutinePointer A pointer to the Coroutine to examine.
///
/// @return Returns true when the coroutine is allocated and its state
/// indicates that it's no longer running.
#define coroutineFinished(coroutinePointer) \
  (((coroutinePointer) != NULL) \
    && ((coroutinePointer)->state == COROUTINE_STATE_NOT_RUNNING))

/// @def coroutineRunning(coroutinePointer)
///
/// @brief Examines a coroutine to determine whether or not it is still running.
///
/// @param coroutinePointer A pointer to the Coroutine to examine.
///
/// @return Returns true when the coroutine is allocated and its state
/// indicates that it is still running.
#define coroutineRunning(coroutinePointer) \
  (((coroutinePointer) != NULL) \
    && ((coroutinePointer)->state != COROUTINE_STATE_NOT_RUNNING))


// Coroutine function prototypes.  Doxygen inline in source file.
int coroutineConfig(Coroutine *first, int stackSize);
Coroutine* coroutineCreate(CoroutineFunction func);
void* coroutineResume(Coroutine *targetCoroutine, void *arg);
void* coroutineYield(void *arg);
int coroutineSetId(Coroutine *coroutine, COROUTINE_ID_TYPE id);
COROUTINE_ID_TYPE coroutineId(Coroutine *coroutine);
CoroutineState coroutineState(Coroutine *coroutine);
#ifdef THREAD_SAFE_COROUTINES
void coroutineSetThreadingSupportEnabled(bool state);
bool coroutineThreadingSupportEnabled();
#endif
int coroutineTerminate(Coroutine *targetCoroutine, Comutex **mutexes);


// Coroutine mutex function prototypes.  Doxygen inline in source file.
int comutexInit(Comutex *mtx, int type);
int comutexLock(Comutex *mtx);
int comutexUnlock(Comutex *mtx);
void comutexDestroy(Comutex *mtx);
int comutexTimedLock(Comutex *mtx, const struct timespec *ts);
int comutexTryLock(Comutex *mtx);
void* comutexLastYieldValue(Comutex *mtx);


// Coroutine condition function prototypes.  Doxygen inline in source file.
int coconditionBroadcast(Cocondition *cond);
void coconditionDestroy(Cocondition *cond);
int coconditionInit(Cocondition *cond);
int coconditionSignal(Cocondition *cond);
int coconditionTimedWait(Cocondition *cond, Comutex *mtx,
  const struct timespec *ts);
int coconditionWait(Cocondition *cond, Comutex *mtx);
void* coconditionLastYieldValue(Cocondition *cond);


// Comessage queue functions
int comessageQueueDestroy(Coroutine *coroutine);
Comessage* comessageQueuePeek(void);
Comessage* comessageQueuePop(void);
Comessage* comessageQueuePopType(int type);
Comessage* comessageQueueWait(const struct timespec *ts);
Comessage* comessageQueueWaitForType(int type, const struct timespec *ts);
int comessageQueuePush(Coroutine *coroutine, Comessage *comessage);


// Comessage functions
int comessageDestroy(Comessage *comessage);
int comessageInit(
  Comessage *comessage, int type, void *data, size_t size, bool waiting);
int comessageRelease(Comessage *comessage);
int comessageSetDone(Comessage *comessage);
int comessageWaitForDone(Comessage *comessage, const struct timespec *ts);
Comessage* comessageWaitForReply(Comessage *sent, bool releaseAfterDone,
  const struct timespec *ts);
Comessage* comessageWaitForReplyWithType(Comessage *sent, bool releaseAfterDone,
  int type, const struct timespec *ts);


// Comessage accessors
#define comessageType(comessagePointer) \
  (((comessagePointer) != NULL) ? (comessagePointer)->type : 0)
#define comessageData(comessagePointer) \
  (((comessagePointer) != NULL) ? (comessagePointer)->data : NULL)
#define comessageSize(comessagePointer) \
  (((comessagePointer) != NULL) ? (comessagePointer)->size : 0)
// No accessor for next member element.
#define comessageWaiting(comessagePointer) \
  (((comessagePointer) != NULL) ? (comessagePointer)->waiting : false)
#define comessageDone(comessagePointer) \
  (((comessagePointer) != NULL) ? (comessagePointer)->done : true)
#define comessageInUse(comessagePointer) \
  (((comessagePointer) != NULL) ? (comessagePointer)->inUse : false)
#define comessageFrom(comessagePointer) \
  (((comessagePointer) != NULL) ? (comessagePointer)->from : NULL)
#define comessageTo(comessagePointer) \
  (((comessagePointer) != NULL) ? (comessagePointer)->to : NULL)
#define comessageConfigured(comessagePointer) \
  (((comessagePointer) != NULL) ? (comessagePointer)->configured : false)


#ifdef __cplusplus
} // extern "C"
#endif

#endif // COROUTINES_H
