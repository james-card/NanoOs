////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2025 James Card                     //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included    //
// in all copies or substantial portions of the Software.                     //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//                                 James Card                                 //
//                          http://www.jamescard.org                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// Doxygen marker
/// @file

#include "Messages.h"

#if LOGGING_ENABLED && CTHREADS_LOGGING_ENABLED
#include "LoggingLib.h"
#else
#undef printLog
#define printLog(...) {}
#define logFile stderr
#define LOG_MALLOC_FAILURE(...) {}
#endif

// Message functions

/// @fn int msg_start_use(msg_t *msg)
///
/// @brief Set all the member elements of a msg_t to their default values so
/// that we can begin using it.
///
/// @parm msg A pointer to the msg_t to configure and begin using.
///
/// @return Returns msg_success on success, msg_error on error.
static inline int msg_start_use(msg_t *msg) {
  int return_value = msg_success;
  
  if (msg != NULL) {
    if (msg->in_use == false) {
      msg->type = 0;
      msg->data = NULL;
      msg->size = 0;
      msg->next = NULL;
      msg->waiting = false;
      msg->done = true;
      msg->in_use = true;
      msg->from.coro = NULL;
#ifdef THREAD_SAFE_COROUTINES
      msg->from.thrd = 0;
#endif // THREAD_SAFE_COROUTINES
      msg->recipient = MESSAGE_RECIPIENT_NOT_SET;
      if (msg->configured == false) {
        msg->coro_init = (getRunningCoroutine() != NULL);
        if (msg->coro_init == true) {
          if (coconditionInit(&msg->coro_condition)
            == coroutineSuccess
          ) {
            if (comutexInit(&msg->coro_lock,
              comutexPlain | comutexTimed) == coroutineSuccess
            ) {
              msg->configured = true;
            } else {
              coconditionDestroy(&msg->coro_condition);
              return_value = msg_error;
              // msg->configured remains false
            }
          } else {
            return_value = msg_error;
            // msg->configured remains false
          }
        }

#ifdef THREAD_SAFE_COROUTINES
      if (return_value == msg_success) {
          memset(&msg->thrd_condition, 0, sizeof(msg->thrd_condition));
          memset(&msg->thrd_lock, 0, sizeof(msg->thrd_condition));
          if (cnd_init(&msg->thrd_condition) == thrd_success) {
            if (mtx_init(&msg->thrd_lock, mtx_plain | mtx_timed) == thrd_success) {
              msg->configured = true;
            } else {
              cnd_destroy(&msg->thrd_condition);
              return_value = msg_error;
              // msg->configured remains false
            }
          } else {
            return_value = msg_error;
            // msg->configured remains false
          }
        }
#endif // THREAD_SAFE_COROUTINES
      }
      // Don't touch msg->dynamically_allocated;
    } // Else this message is already setup
  } else {
    return_value = msg_error;
  }
  
  return return_value;
}

/// @fn msg_t* msg_create(void)
///
/// @brief Dynamically allocate a new msg_t and set all its member elements to
/// a default state.
///
/// @return Returns a pointer to a newly-allocated and configured msg_t on
/// success, NULL on failure.
msg_t* msg_create(void) {
  msg_t *msg = (msg_t*) calloc(1, sizeof(msg_t));
  if (msg_start_use(msg) == msg_success) {
    msg->dynamically_allocated = true;
  } else {
    // Some kind of problem.  Can't use this message.
    free(msg); msg = NULL;
  }
  
  return msg;
}

/// @fn msg_t* msg_destroy(msg_t *msg)
///
/// @brief If nothing is waiting on the message, deallocate all of the message's
/// resources and return NULL.  If something is waiting on the message, mark the
/// message as done and no longer in use and broadcast to all the waiters.  Note
/// that the msg_t itself will only be freed if it was allocated by msg_create.
///
/// @param msg A pointer to the msg_t to destroy.
///
/// @return Returns NULL if the msg_t was fully deallocated, a pointer to the
/// original msg_t otherwise.
msg_t* msg_destroy(msg_t *msg) {
  if (msg == NULL) {
    // Nothing to do.
    return msg;
  }
  
  // Don't touch msg->type.
  // Don't touch msg->data.
  // Don't touch msg->size.
  // Don't touch msg->next.
  // Don't touch msg->waiting.
  msg->in_use = false;
  // Don't touch from.
  if (msg->configured == true) {
    if (((msg->coro_init == false)
        || (comutexTryLock(&msg->coro_lock) == coroutineSuccess))
#ifdef THREAD_SAFE_COROUTINES
      && (mtx_trylock(&msg->thrd_lock) == thrd_success)
#endif // THREAD_SAFE_COROUTINES
    ) {
      msg->done = true;
    
      if (msg->waiting == false) {
        // Nothing is waiting.  Destroy the resources.
#ifdef THREAD_SAFE_COROUTINES
        mtx_unlock(&msg->thrd_lock);
        cnd_destroy(&msg->thrd_condition);
        mtx_destroy(&msg->thrd_lock);
#endif // THREAD_SAFE_COROUTINES
        if (msg->coro_init == true) {
          comutexUnlock(&msg->coro_lock);
          coconditionDestroy(&msg->coro_condition);
          comutexDestroy(&msg->coro_lock);
        }
        msg->configured = false;
        if (msg->dynamically_allocated == true) {
          free(msg); msg = NULL;
        }
      } else {
        // Something is waiting.  Signal the waiters.  It will be up to them to
        // destroy this message again later.
        if (msg->recipient == MESSAGE_RECIPIENT_COROUTINE) {
          if (msg->coro_init == true) {
            coconditionBroadcast(&msg->coro_condition);
            comutexUnlock(&msg->coro_lock);
          }
#ifdef THREAD_SAFE_COROUTINES
        } else if (msg->recipient == MESSAGE_RECIPIENT_THREAD) {
          cnd_broadcast(&msg->thrd_condition);
          mtx_unlock(&msg->thrd_lock);
#endif // THREAD_SAFE_COROUTINES
        }
      }
    } else {
      // We can't do any signalling.  Just tear down everything.
      msg->done = true;
      msg->waiting = false;
      if (msg->coro_init == true) {
        coconditionDestroy(&msg->coro_condition);
        comutexDestroy(&msg->coro_lock);
      }
#ifdef THREAD_SAFE_COROUTINES
      cnd_destroy(&msg->thrd_condition);
      mtx_destroy(&msg->thrd_lock);
#endif // THREAD_SAFE_COROUTINES
      msg->configured = false;
      if (msg->dynamically_allocated == true) {
        free(msg); msg = NULL;
      }
    }
  } else if (msg->dynamically_allocated == true) {
    // msg->thrd_lock and mtx->condition are not initialized, so notihing to destroy.
    free(msg); msg = NULL;
  }
  
  return msg;
}

/// @fn int msg_init(
///   msg_t *msg, int type, void *data, size_t size, bool waiting)
///
/// @brief Initialize a message to make it ready for sending to another thread.
///
/// @param msg A pointer to an allocated msg_t.  Will be properly configured if
///   not already so.
/// @param type The message type to use for the message.
/// @param data A pointer to the data for the message (if any).
/// @param size The number of bytes pointed to by the data pointer.
/// @param waiting Whether or not the caller of this function will be waiting on
///   a response to this message from the destination thread.
///
/// @return Returns msg_success on success, msg_error on failure.
int msg_init(
  msg_t *msg, int type, void *data, size_t size, bool waiting
) {
  int return_value = msg_error;
  
  if (msg == NULL) {
    // Nothing we can do.  Fail.
    return return_value; // msg_error
  } else if (msg_start_use(msg) != msg_success) {
    // Couldn't configure this message for use for some reason.  Fail.
    return return_value; // msg_error
  }
  
  msg->type = type;
  msg->data = data;
  msg->size = size;
  msg->next = NULL;
  msg->waiting = waiting;
  msg->done = false;
  // No need to set msg->in_use since we called msg_start_use above.
  // Don't touch msg->from.thrd in case this message is being reused.
  return_value = msg_success;
  
  return return_value;
}

/// @fn int msg_release(msg_t *msg)
///
/// @brief Release a message from use.
///
/// @param msg A pointer to a previously-allocated msg_t.
///
/// @return Returns msg_success on success, msg_error on failure.
int msg_release(msg_t *msg) {
  int return_value = msg_success;
  
  if (msg == NULL) {
    // A NULL message is already released.  Just return.
    return return_value; // msg_success
  }
  
  // Don't touch msg->type.
  // Don't touch msg->data.
  // Don't touch msg->size.
  // Don't touch msg->next.
  // Don't touch msg->waiting.
  msg->in_use = false;
  // Don't touch msg->from.thrd.
  if (msg->configured == true) {
    if (((msg->coro_init == false)
        || (comutexTryLock(&msg->coro_lock) == coroutineSuccess))
#ifdef THREAD_SAFE_COROUTINES
      && (mtx_trylock(&msg->thrd_lock) == thrd_success)
#endif // THREAD_SAFE_COROUTINES
    ) {
      msg->done = true;
      
      if (msg->waiting == true) {
        // Something is waiting.  Signal the waiters.  It will be up to them to
        // destroy this message again later.
        if (msg->recipient == MESSAGE_RECIPIENT_COROUTINE) {
          if (msg->coro_init == true) {
            coconditionBroadcast(&msg->coro_condition);
          }
#ifdef THREAD_SAFE_COROUTINES
        } else if (msg->recipient == MESSAGE_RECIPIENT_THREAD) {
          cnd_broadcast(&msg->thrd_condition);
#endif // THREAD_SAFE_COROUTINES
        }
      }
#ifdef THREAD_SAFE_COROUTINES
      mtx_unlock(&msg->thrd_lock);
#endif // THREAD_SAFE_COROUTINES
      if (msg->coro_init == true) {
        comutexUnlock(&msg->coro_lock);
      }
    } else {
      // Something is wrong here.  We're releasing a msg_t that is not owned
      // by us.  We can't do a broadcast.  Just mark it done and return an
      // error.
      msg->done = true;
      return_value = msg_error;
    }
  } else {
    // Nothing we can do but set the done flag.
    msg->done = true;
  }
  // Don't touch msg->thrd_condition.
  // Don't touch msg->thrd_lock.
  // Don't touch msg->configured.
  // Don't touch msg->dynamically_allocated.
  
  return return_value;
}

/// @fn int msg_set_done(msg_t *msg)
///
/// @brief Set the done flag for the message and signal any waiters.
///
/// @param msg A pointer to a previously-allocated msg_t.
///
/// @return Returns msg_success on success, msg_error on failure.
int msg_set_done(msg_t *msg) {
  int return_value = msg_error;
  
  if (msg == NULL) {
    // Invalid.
    return return_value; // msg_error
  }
  
  // Don't touch msg->type.
  // Don't touch msg->data.
  // Don't touch msg->size.
  // Don't touch msg->next.
  // Don't touch msg->waiting.
  // Don't touch msg->in_use.
  if (msg->configured == true) {
    if (msg->coro_init == true) {
      comutexLock(&msg->coro_lock);
    }
#ifdef THREAD_SAFE_COROUTINES
    mtx_lock(&msg->thrd_lock);
#endif // THREAD_SAFE_COROUTINES
    msg->done = true;
    
    if (msg->waiting == true) {
      // Something is waiting.  Signal the waiters.  It will be up to them to
      // destroy this message again later.
      if ((msg->coro_init == true)
        && (msg->recipient == MESSAGE_RECIPIENT_COROUTINE)
      ) {
        if (coconditionBroadcast(&msg->coro_condition)
          == coroutineSuccess
        ) {
          return_value = msg_success;
        } // else, return_value remains msg_error.
#ifdef THREAD_SAFE_COROUTINES
      } else if (msg->recipient == MESSAGE_RECIPIENT_THREAD) {
        if (cnd_broadcast(&msg->thrd_condition) == thrd_success) {
          return_value = msg_success;
        } // else return_value remains msg_error.
#endif // THREAD_SAFE_COROUTINES
      }
    } else {
      return_value = msg_success;
    }
#ifdef THREAD_SAFE_COROUTINES
    mtx_unlock(&msg->thrd_lock);
#endif // THREAD_SAFE_COROUTINES
    if (msg->coro_init == true) {
      comutexUnlock(&msg->coro_lock);
    }
  } else {
    // Nothing we can do but set the done flag.
    msg->done = true;
    return_value = msg_success;
  }
  // Don't touch msg->from.thrd.
  // Don't touch msg->thrd_condition.
  // Don't touch msg->thrd_lock.
  // Don't touch msg->configured.
  // Don't touch msg->dynamically_allocated.
  
  return return_value;
}

/// @fn int msg_wait_for_done(msg_t *msg, const struct timespec *ts)
///
/// @brief Wait on a message until another thread indicates that it's done
/// or until the specified time has been reached.
///
/// @param msg A pointer to a previously-allocated msg_t.
/// @param ts A pointer to a struct timespec that specifies the end of the time
///   period to wait for.  If this pointer is NULL, an infinite timeout will be
///   used.
///
/// @return Returns msg_success on success, msg_error on failure.
int msg_wait_for_done(msg_t *msg, const struct timespec *ts) {
  int return_value = msg_error;
  
  if (msg == NULL) {
    // Invalid.
    return return_value; // msg_error
  } else if (msg->configured == false) {
    // We can't do this.  Waiting for done requires the use of the lock and
    // condition in the message.  It doesn't make any sense for us to try and
    // initialize them at this point because whatever made this call is already
    // in a bad state.  We shouldn't try to fix things because we don't know
    // what's going on above us.  Just return bad status.
    return return_value; // msg_error
  }
  
  int lock_status = msg_error;
  int wait_status = msg_error;
  if (msg->done == true) {
    return_value = msg_success;
  } else {
    if ((msg->coro_init == true)
      && (msg->recipient == MESSAGE_RECIPIENT_COROUTINE)
    ) {
      if (ts == NULL) {
        lock_status = comutexLock(&msg->coro_lock);
      } else {
        lock_status = comutexTimedLock(&msg->coro_lock, ts);
      }
#ifdef THREAD_SAFE_COROUTINES
    } else if (msg->recipient == MESSAGE_RECIPIENT_THREAD) {
      if (ts == NULL) {
        lock_status = mtx_lock(&msg->thrd_lock);
      } else {
        lock_status = mtx_timedlock(&msg->thrd_lock, ts);
      }
#endif // THREAD_SAFE_COROUTINES
    }
    if (lock_status != msg_success) {
      // Either we timed out or there's a problem with the lock.  Either way, we
      // don't want to continue and we're going to exit with an error since we
      // never received the done flag.
      return return_value; // msg_error
    }
    
    msg->waiting = true;
    while (msg->done == false) {
      if ((msg->coro_init == true)
        && (msg->recipient == MESSAGE_RECIPIENT_COROUTINE)
      ) {
        if (ts == NULL) {
          wait_status = coconditionWait(&msg->coro_condition, &msg->coro_lock);
        } else {
          wait_status
            = coconditionTimedWait(&msg->coro_condition, &msg->coro_lock, ts);
        }
#ifdef THREAD_SAFE_COROUTINES
      } else if (msg->recipient == MESSAGE_RECIPIENT_THREAD) {
        if (ts == NULL) {
          wait_status = cnd_wait(&msg->thrd_condition, &msg->thrd_lock);
        } else {
          wait_status
            = cnd_timedwait(&msg->thrd_condition, &msg->thrd_lock, ts);
        }
#endif // THREAD_SAFE_COROUTINES
      }
      if (wait_status != msg_success) {
        // Either we timed out or there's a problem with the condition.  Again,
        // we don't want to proceed like this.
        break;
      }
    }
    msg->waiting = false;
    
    if (msg->done == true) {
      return_value = msg_success;
    }

    if ((msg->coro_init == true)
      && (msg->recipient == MESSAGE_RECIPIENT_COROUTINE)
    ) {
      comutexUnlock(&msg->coro_lock);
#ifdef THREAD_SAFE_COROUTINES
    } else if (msg->recipient == MESSAGE_RECIPIENT_THREAD) {
      mtx_unlock(&msg->thrd_lock);
#endif // THREAD_SAFE_COROUTINES
    }
  }
  
  return return_value;
}

#ifdef THREAD_SAFE_COROUTINES

/// @fn msg_t* msg_wait_for_reply_with_type_(
///   msg_t *sent, bool release, int *type, const struct timespec *ts)
///
/// @brief Wait for a reply from the thread recipient of a message.
///
/// @param sent The message that was originally sent to the recipient.
/// @param release Whether or not the provided sent message should be released
///   (*NOT* destroyed) after the recipient has indicated that they're done
///   processing our sent message.
/// @param type A pointer to an integer type of message that the caller is
///   waiting for.  If this parameter is NULL, no type will be considered.
/// @param ts A pointer to a struct timespec that holds the end time to wait
///   until for a reply.  If this parameter is NULL, then an infinite timeout
///   will be used.
///
/// @return Returns a pointer to the msg_t received from the recipient of
/// the original message on success, NULL on failure or if the provided timeout
/// time is reached.
msg_t* msg_wait_for_reply_with_type_thrd(
  msg_t *sent, bool release,
  int *type, const struct timespec *ts
) {
  msg_t *reply = NULL;

  thrd_msg_q_t *queue = get_thread_thrd_msg_q();
  if (queue == NULL) {
    // Something is wrong.  Bail.
    return reply; // NULL
  }

  if (sent == NULL) {
    // Invalid.
    return reply; // NULL
  }

  // We need to grab the original recipient of the message that was sent before
  // we wait for done in case the recipient reuses this message as the reply.
  thrd_t recipient = sent->to.thrd;

  if (msg_wait_for_done(sent, ts) != msg_success) {
    // Invalid state of the message.  Fail.
    return reply; // NULL
  }

  if (release == true) {
    // We're done with the message that was originally sent and the caller has
    // indicated that it is to be released now.
    msg_release(sent);
  }

  // Recipient has processed the message.  We now need to wait for their reply.
  int lock_status = msg_success;
  if (ts == NULL) {
    lock_status = mtx_lock(&queue->lock);
  } else {
    lock_status = mtx_timedlock(&queue->lock, ts);
  }
  if (lock_status != msg_success) {
    // Either we've timed out or there's a problem with the lock.  Either way,
    // we're done.  Bail.
    return reply; // NULL
  }

  // mtx_timedlock will return thrd_timedout if the timeout is
  // reached, so we'll never reach this point if we've exceeded our timeout.
  msg_t *prev = NULL;
  msg_t *cur = queue->head;
  msg_t **prev_next = &queue->head;
  int searchType = 0;
  if (type != NULL) {
    // This saves us from having to dereference the pointer in every iteration
    // of the loop below.
    searchType = *type;
  }

  // Enter our main wait loop.
  int wait_status = msg_success;
  while (reply == NULL) {
    while ((cur != NULL)
      && ((cur->from.thrd != recipient)
        || ((type != NULL) && (cur->type != searchType))
      )
    ) {
      prev = cur;
      prev_next = &cur->next;
      cur = cur->next;
    }

    if (cur != NULL) {
      // Desired reply was found.  Remove the message from the thread.
      reply = cur;
      *prev_next = cur->next;

      if (queue->head == NULL) {
        // Empty queue.  Set queue->tail to NULL too.
        queue->tail = NULL;
      }
      if (queue->tail == cur) {
        queue->tail = prev;
      }
      cur->next = NULL;
    } else {
      // Desired reply was not found.  Block until something else is pushed.
      if (ts == NULL) {
        wait_status = cnd_wait(&queue->condition, &queue->lock);
      } else {
        wait_status = cnd_timedwait(
          &queue->condition, &queue->lock, ts);
      }
      if (wait_status != msg_success) {
        // Something isn't as expected.  Bail.
        break;
      }
      // cnd_timedwait will return thrd_timedout if the timeout is
      // reached, so we won't continue the loop if we've exceeded our timeout.
    }

    prev = NULL;
    cur = queue->head;
    prev_next = &queue->head;
  }

  mtx_unlock(&queue->lock);

  return reply;
}

#endif // THREAD_SAFE_COROUTINES

/// @fn msg_t* msg_wait_for_reply_with_type_coro(msg_t *sent,
///   bool releaseAfterDone, int *type, const struct timespec *ts)
///
/// @brief Wait for a reply from the coroutine recipient of a message.
///
/// @param sent The message that was originally sent to the recipient.
/// @param releaseAfterDone Whether or not the provided sent message should be
///   released (*NOT* destroyed) after the recipient has indicated that they're
///   done processing our sent message.
/// @param type A pointer to an integer type of message that the caller is
///   waiting for.  If this parameter is NULL, no type will be considered.
/// @param ts A pointer to a struct timespec that holds the end time to wait
///   until for a reply.  If this parameter is NULL, then an infinite timeout
///   will be used.
///
/// @return Returns a pointer to the msg_t received from the recipient of
/// the original message on success, NULL on failure or if the provided timeout
/// time is reached.
msg_t* msg_wait_for_reply_with_type_coro(
  msg_t *sent, bool releaseAfterDone,
  int *type, const struct timespec *ts
) {
  msg_t *reply = NULL;

  Coroutine *coroutine = getRunningCoroutine();
  if (coroutine == NULL) {
    // Coroutines haven't been configured yet.
    return reply; // NULL
  }

  if (sent == NULL) {
    // Invalid.
    return reply; // NULL
  }

  // We need to grab the original recipient of the message that was sent before
  // we wait for done in case the recipient reuses this message as the reply.
  // message.
  Coroutine *recipient = sent->to.coro;

  if (msg_wait_for_done(sent, ts) != coroutineSuccess) {
    // Invalid state of the message.  Fail.
    return reply; // NULL
  }

  if (releaseAfterDone == true) {
    // We're done with the message that was originally sent and the caller has
    // indicated that it is to be released now.
    msg_release(sent);
  }

  // Recipient has processed the message.  We now need to wait for their reply.
  int lock_status = coroutineSuccess;
  if (sent->coro_init == true) {
    if (ts == NULL) {
      lock_status = comutexLock(&coroutine->messageLock);
    } else {
      lock_status = comutexTimedLock(&coroutine->messageLock, ts);
    }
  } else {
    lock_status = coroutineError;
  }
  if (lock_status != coroutineSuccess) {
    // Either we've timed out or there's a problem with the lock.  Either way,
    // we're done.  Bail.
    return reply; // NULL
  }

  // comutexTimedLock will return coroutineTimedout if the timeout is
  // reached, so we'll never reach this point if we've exceeded our timeout.
  msg_t *prev = NULL;
  msg_t *cur = coroutine->nextMessage;
  msg_t **prevNext = &coroutine->nextMessage;
  int searchType = 0;
  if (type != NULL) {
    // This saves us from having to dereference the pointer in every iteration
    // of the loop below.
    searchType = *type;
  }

  // Enter our main wait loop.
  int wait_status = coroutineSuccess;
  while (reply == NULL) {
    while ((cur != NULL)
      && ((cur->from.coro != recipient)
        || ((type != NULL) && (cur->type != searchType))
      )
    ) {
      prev = cur;
      prevNext = &cur->next;
      cur = cur->next;
    }

    if (cur != NULL) {
      // Desired reply was found.  Remove the message from the coroutine.
      reply = cur;
      *prevNext = cur->next;

      if (coroutine->nextMessage == NULL) {
        // Empty queue.  Set coroutine->lastMessage to NULL too.
        coroutine->lastMessage = NULL;
      }
      if (coroutine->lastMessage == cur) {
        coroutine->lastMessage = prev;
      }
      cur->next = NULL;
    } else {
      // Desired reply was not found.  Block until something else is pushed.
      // We exit with an error at the beginning of this function if
      // sent->coro_init is false, so no need to check it again here.
      if (ts == NULL) {
        wait_status = coconditionWait(
          &coroutine->messageCondition, &coroutine->messageLock);
      } else {
        wait_status = coconditionTimedWait(
          &coroutine->messageCondition, &coroutine->messageLock, ts);
      }
      if (wait_status != coroutineSuccess) {
        // Something isn't as expected.  Bail.
        break;
      }
      // coconditionTimedWait will return thrd_timedout if the timeout is
      // reached, so we won't continue the loop if we've exceeded our timeout.
    }

    prev = NULL;
    cur = coroutine->nextMessage;
    prevNext = &coroutine->nextMessage;
  }

  comutexUnlock(&coroutine->messageLock);

  return reply;
}

/// @fn msg_t* msg_wait_for_reply(
///   msg_t *sent, bool release, const struct timespec *ts)
///
/// @brief Block until a reply has been received from the original recipient of
/// the provided message or until a specified future time has been reached.
///
/// @param sent The message that was originally sent to the recipient.
/// @param release Whether or not the provided sent message should be released
///   (*NOT* destroyed) after the recipient has indicated that they're done
///   processing our sent message.
/// @param ts A pointer to a struct timespec that holds the end time to wait
///   until for a reply.  If this parameter is NULL, then an infinite timeout
///   will be used.
///
/// @return Returns a pointer to the msg_t received from the recipient of
/// the original message on success, NULL on failure.
msg_t* msg_wait_for_reply(msg_t *sent, bool release,
  const struct timespec *ts
) {
  if (sent == NULL) {
    return NULL;
  }

  if (sent->recipient == MESSAGE_RECIPIENT_COROUTINE) {
    return msg_wait_for_reply_with_type_coro(sent, release, NULL, ts);
#ifdef THREAD_SAFE_COROUTINES
  } else if (sent->recipient == MESSAGE_RECIPIENT_THREAD) {
    return msg_wait_for_reply_with_type_thrd(sent, release, NULL, ts);
#endif // THREAD_SAFE_COROUTINES
  }

  // Invalid recipient
  return NULL;
}

/// @fn msg_t* msg_wait_for_reply_with_type(
///   msg_t *sent, bool release, int type, const struct timespec *ts)
///
/// @brief Block until a reply of a specified type has been received from the
/// original recipient of the provided message or until a specified future time
/// has been reached.
///
/// @param sent The message that was originally sent to the recipient.
/// @param release Whether or not the provided sent message should be released
///   (*NOT* destroyed) after the recipient has indicated that they're done
///   processing our sent message.
/// @param type An integer type of message that the caller is waiting for.
/// @param ts A pointer to a struct timespec that holds the end time to wait
///   until for a reply.  If this parameter is NULL, then an infinite timeout
///   will be used.
///
/// @return Returns a pointer to the msg_t received from the recipient of
/// the original message of the specified tyep on success, NULL on failure or if
/// the provided timeout time is reached.
msg_t* msg_wait_for_reply_with_type(msg_t *sent, bool release,
  int type, const struct timespec *ts
) {
  if (sent == NULL) {
    return NULL;
  }

  if (sent->recipient == MESSAGE_RECIPIENT_COROUTINE) {
    return msg_wait_for_reply_with_type_coro(sent, release, &type, ts);
#ifdef THREAD_SAFE_COROUTINES
  } else if (sent->recipient == MESSAGE_RECIPIENT_THREAD) {
    return msg_wait_for_reply_with_type_thrd(sent, release, &type, ts);
#endif // THREAD_SAFE_COROUTINES
  }

  // Invalid recipient
  return NULL;
}

/// @fn void* msg_element(msg_t *msg, msg_element_t msg_element)
///
/// @brief Get a pointer to an element of a msg_t object.
///
/// @param msg A pointer to the msg_t to get the member of.
/// @param msg_element The member element of the msg_t to get.
///
/// @return Returns a pointer to the specified element on success, NULL on
/// failure.
void* msg_element(msg_t *msg, msg_element_t msg_element) {
  switch (msg_element) {
    case MSG_ELEMENT_TYPE: {
      return &msg->type;
      break;
    }

    case MSG_ELEMENT_DATA: {
      return &msg->data;
      break;
    }

    case MSG_ELEMENT_SIZE: {
      return &msg->size;
      break;
    }

    case MSG_ELEMENT_WAITING: {
      return &msg->waiting;
      break;
    }

    case MSG_ELEMENT_DONE: {
      return &msg->done;
      break;
    }

    case MSG_ELEMENT_IN_USE: {
      return &msg->in_use;
      break;
    }

    case MSG_ELEMENT_FROM: {
      return &msg->from;
      break;
    }

    case MSG_ELEMENT_TO: {
      return &msg->to;
      break;
    }

    default: {
      return NULL;
      break;
    }
  }
}

