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

/// @enum msg_recip_t
///
/// @brief Indicator of what kind of entity a message was sent to.
typedef enum msg_recip_t {
  MESSAGE_RECIPIENT_NOT_SET,
  MESSAGE_RECIPIENT_COROUTINE,
#ifdef THREAD_SAFE_COROUTINES
  MESSAGE_RECIPIENT_THREAD,
#endif // THREAD_SAFE_COROUTINES
  NUM_MESSAGE_RECIPIENTS
} msg_recip_t;

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
/// @param coro_from A pointer to the Coroutine instance for the sending
///   coroutine.
/// @param coro_to A pointer to the Coroutine instance for the receiving
///   coroutine.
/// @param coro_condition A condition (Cocondition) that will allow for
///   signalling between coroutines when setting the done flag.
/// @param coro_lock A mutex (Comutex) to guard the coroutine condition.
/// @param thrd_from The thrd_t value for the sending thread.
/// @param thrd_to The thrd_t value for the receiving thread.
/// @param thrd_condition A condition (cnd_t) that will allow for signalling
///   between threads when setting the done flag.
/// @param thrd_lock A mutex (mtx_t) to guard the thread condition.
/// @param configured Whether or not the members of the message that require
///   initialization have been configured yet.
/// @param dynamically_allocated Whether or not the message was dynamically
///   allocated with msg_create.
/// @param recipient A msg_recip_t that indicates the kind of entity that the
///   message was sent to, if any.
/// @param coro_init Whether or not coroutines are initialized on the system.
typedef struct msg_t {
  int type;
  void *data;
  size_t size;
  struct msg_t *next;
  bool waiting;
  bool done;
  bool in_use;
  Coroutine *coro_from;
  Coroutine *coro_to;
  Cocondition coro_condition;
  Comutex coro_lock;
#ifdef THREAD_SAFE_COROUTINES
  thrd_t thrd_from;
  thrd_t thrd_to;
  cnd_t thrd_condition;
  mtx_t thrd_lock;
#endif // THREAD_SAFE_COROUTINES
  bool configured;
  bool dynamically_allocated;
  msg_recip_t recipient;
  bool coro_init;
} msg_t;

#ifdef THREAD_SAFE_COROUTINES

/// @struct thrd_msg_q_t
///
/// @brief Definition for a thread's message queue.
///
/// @param head The head of the message queue.  Messages will be removed from
///   this pointer.
/// @param tail The tail of the message queue.  Messages will be added to this
///   pointer.
/// @param condition A condition (cnd_t) that will allow for signalling between
///   threads when adding a message to the queue.
/// @param lock A mutex (mtx_t) to guard the condition.
typedef struct thrd_msg_q_t {
  msg_t *head;
  msg_t *tail;
  cnd_t condition;
  mtx_t lock;
} thrd_msg_q_t;

// Defined and documented in CThreadsMessages.c
extern Trie *message_queues;

/// @fn thrd_msg_q_t* get_thread_thrd_msg_q(void)
///
/// @brief Get the message queue for the current thread.
///
/// @return Returns a pointer to the thrd_msg_q_t for the thread making this
/// call.  This should always be a non-NULL value unless something is wrong with
/// the system.
static inline thrd_msg_q_t* get_thread_thrd_msg_q(void) {
  thrd_t thr = thrd_current();
  return (thrd_msg_q_t*) trieGetValue(message_queues, &thr, sizeof(thr));
}

#endif // THREAD_SAFE_COROUTINES

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

// Message accessors
#define msg_type(msg_ptr) \
  (((msg_ptr) != NULL) ? (msg_ptr)->type : 0)
#define msg_data(msg_ptr) \
  (((msg_ptr) != NULL) ? (msg_ptr)->data : NULL)
#define msg_size(msg_ptr) \
  (((msg_ptr) != NULL) ? (msg_ptr)->size : 0)
// No accessor for next member element.
#define msg_waiting(msg_ptr) \
  (((msg_ptr) != NULL) ? (msg_ptr)->waiting : false)
#define msg_done(msg_ptr) \
  (((msg_ptr) != NULL) ? (msg_ptr)->done : true)
#define msg_in_use(msg_ptr) \
  (((msg_ptr) != NULL) ? (msg_ptr)->in_use : false)
#define msg_coro_from(msg_ptr) \
  (((msg_ptr) != NULL) ? (msg_ptr)->coro_from : 0)
#define msg_coro_to(msg_ptr) \
  (((msg_ptr) != NULL) ? (msg_ptr)->coro_to : 0)
#define msg_thrd_from(msg_ptr) \
  (((msg_ptr) != NULL) ? (msg_ptr)->thrd_from : 0)
#define msg_thrd_to(msg_ptr) \
  (((msg_ptr) != NULL) ? (msg_ptr)->thrd_to : 0)
#define msg_configured(msg_ptr) \
  (((msg_ptr) != NULL) ? (msg_ptr)->configured : false)


#ifdef __cplusplus
}
#endif

#endif // MESSAGES_H

