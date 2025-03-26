///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              03.07.2025
///
/// @file              Messages.h
///
/// @brief             Message-only definitions and functionality.
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

#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdint.h>
#include <stdbool.h>
#include "Coroutines.h"

#ifdef THREAD_SAFE_COROUTINES
#include "Trie.h"
// Forward declarations needed for CThreadsMessages.h, which is eventually
// included from CThreads.h.
typedef struct Trie Trie;
typedef struct msg_t msg_t;
#ifdef __cplusplus
extern "C" {
#endif
  void* trieGetValue(Trie *tree, const volatile void *key, size_t keySize);
#ifdef __cplusplus
}
#endif
#include "CThreads.h"
#endif // THREAD_SAFE_COROUTINES

#ifdef __cplusplus
extern "C"
{
#endif

// Return statuses
#define msg_success    0
#define msg_error      1
#define msg_nomem      2
#define msg_timedout   3

/// @enum msg_endpoint_type_t
///
/// @brief Indicator of what kind of entity a message was sent to.
typedef enum msg_endpoint_type_t {
  MESSAGE_ENDPOINT_TYPE_NOT_SET,
  MESSAGE_ENDPOINT_TYPE_COROUTINE,
#ifdef THREAD_SAFE_COROUTINES
  MESSAGE_ENDPOINT_TYPE_THREAD,
#endif // THREAD_SAFE_COROUTINES
  NUM_MESSAGE_ENDPOINT_TYPES
} msg_endpoint_type_t;

/// @union msg_endpoint_t
///
/// @brief Union of all possible valid endpoints for a msg_t to be sent to (or
/// received from).
typedef union msg_endpoint_t {
    Coroutine *coro;
#ifdef THREAD_SAFE_COROUTINES
    thrd_t thrd;
#endif // THREAD_SAFE_COROUTINES
} msg_endpoint_t;

/// @struct msg_t
///
/// @brief Definition for a message that can be pushed onto a message queue.
///
/// @param type Integer value designating the type of message for the receiving
///   entity.
/// @param data A pointer to the data of the message.
/// @param size The number of bytes pointed to by the data pointer.
/// @param next A pointer to the next msg_t in a message queue.
/// @param waiting A Boolean flag to indicate whether or not the sender is
///   waiting on a response message from the recipient of the message.
/// @param done A Boolean flag to indicate whether or not the receiving entity
///   has handled the message yet.
/// @param in_use A Boolean flag to indicate whether or not this msg_t is in
///   use.
/// @param from A msg_endpoint_t that represents the sending entity.
/// @param to A msg_endpoint_t that represents the receiveing entity.
/// @param coro_condition A condition (Cocondition) that will allow for
///   signalling between coroutines when setting the done flag.
/// @param coro_lock A mutex (Comutex) to guard the coroutine condition.
/// @param thrd_condition A condition (cnd_t) that will allow for signalling
///   between threads when setting the done flag.
/// @param thrd_lock A mutex (mtx_t) to guard the thread condition.
/// @param configured Whether or not the members of the message that require
///   initialization have been configured yet.
/// @param dynamically_allocated Whether or not the message was dynamically
///   allocated with msg_create.
/// @param endpoint_type A msg_endpoint_type_t that indicates the kind of
///   entity that the message was sent to, if any.
/// @param coro_init Whether or not coroutines are initialized on the system.
typedef struct msg_t {
  int type;
  void *data;
  size_t size;
  struct msg_t *next;
  bool waiting;
  bool done;
  bool in_use;
  msg_endpoint_t from;
  msg_endpoint_t to;
  Cocondition coro_condition;
  Comutex coro_lock;
#ifdef THREAD_SAFE_COROUTINES
  cnd_t thrd_condition;
  mtx_t thrd_lock;
#endif // THREAD_SAFE_COROUTINES
  bool configured;
  bool dynamically_allocated;
  msg_endpoint_type_t endpoint_type;
  bool coro_init;
} msg_t;

/// @enum msg_element_t
///
/// @brief Enumeration of member elements of msg_t that are user-accessible.
typedef enum msg_element_t {
  MSG_ELEMENT_TYPE,
  MSG_ELEMENT_DATA,
  MSG_ELEMENT_SIZE,
  MSG_ELEMENT_WAITING,
  MSG_ELEMENT_DONE,
  MSG_ELEMENT_IN_USE,
  MSG_ELEMENT_FROM,
  MSG_ELEMENT_TO,
  NUM_MSG_ELEMENTS
} msg_element_t;

// Message functions
msg_t* msg_create(void);
msg_t* msg_destroy(msg_t *msg);
int msg_init(
  msg_t *msg, int type, void *data, size_t size, bool waiting);
int msg_release(msg_t *msg);
int msg_set_done(msg_t *msg);
int msg_wait_for_done(msg_t *msg, const struct timespec *ts);
msg_t* msg_wait_for_reply(msg_t *sent, bool release, const struct timespec *ts);
msg_t* msg_wait_for_reply_with_type(msg_t *sent, bool release, int type,
  const struct timespec *ts);

// Message element accessors
void* msg_element(msg_t *msg, msg_element_t msg_element);
#define msg_type(msg_ptr) \
  (*((int*) msg_element((msg_ptr), MSG_ELEMENT_TYPE)))
#define msg_data(msg_ptr) \
  (*((void**) msg_element((msg_ptr), MSG_ELEMENT_DATA)))
#define msg_size(msg_ptr) \
  (*((size_t*) msg_element((msg_ptr), MSG_ELEMENT_SIZE)))
#define msg_waiting(msg_ptr) \
  (*((bool*) msg_element((msg_ptr), MSG_ELEMENT_WAITING)))
#define msg_done(msg_ptr) \
  (*((bool*) msg_element((msg_ptr), MSG_ELEMENT_DONE)))
#define msg_in_use(msg_ptr) \
  (*((bool*) msg_element((msg_ptr), MSG_ELEMENT_IN_USE)))
#define msg_from(msg_ptr) \
  (*((msg_endpoint_t*) msg_element((msg_ptr), MSG_ELEMENT_FROM)))
#define msg_to(msg_ptr) \
  (*((msg_endpoint_t*) msg_element((msg_ptr), MSG_ELEMENT_TO)))


#ifdef __cplusplus
}
#endif

#endif // MESSAGES_H

