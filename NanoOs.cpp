////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2024 James Card                     //
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

// Custom includes
#include "NanoOs.h"

// Coroutines support

/// @var messages
///
/// @brief pointer to the array of coroutine messages that will be stored in the
/// main loop function's stack.
Comessage *messages = NULL;

/// @var nanoOsMessages
///
/// @brief Pointer to the array of NanoOsMessages that will be stored in the
/// main loop function's stack.
NanoOsMessage *nanoOsMessages = NULL;

/// @fn long getElapsedMilliseconds(unsigned long startTime)
///
/// @brief Get the number of milliseconds that have elapsed since a specified
/// time in the past.
///
/// @param startTime The time in the past to calcualte the elapsed time from.
///
/// @return Returns the number of milliseconds that have elapsed since the
/// provided start time.
long getElapsedMilliseconds(unsigned long startTime) {
  unsigned long now = millis();

  if (now < startTime) {
    return ULONG_MAX;
  }

  return now - startTime;
}

/// @fn Coroutine* getCoroutineByPid(unsigned int pid)
///
/// @brief Look up a croutine for a running command given its process ID.
///
/// @param pid The integer ID for the process.
///
/// @return Returns the found coroutine pointer on success, NULL on failure.
Coroutine* getCoroutineByPid(unsigned int pid) {
  if (pid >= NANO_OS_NUM_COMMANDS) {
    // Not a valid PID.  Fail.
    return NULL;
  }

  return runningCommands[pid].coroutine;
}

/// @fn int sendComessageToCoroutine(
///   Coroutine *coroutine, Comessage *comessage)
///
/// @brief Get an available Comessage, populate it with the specified data, and
/// push it onto a destination coroutine's queue.
///
/// @param coroutine A pointer to the destination coroutine to send the message
///   to.
/// @param comessage A pointer to the message to send to the destination
///   coroutine.
///
/// @return Returns coroutineSuccess on success, coroutineError on failure.
int sendComessageToCoroutine(
  Coroutine *coroutine, Comessage *comessage
) {
  int returnValue = coroutineSuccess;
  if ((coroutine == NULL) || (comessage == NULL)) {
    // Invalid.
    returnValue = coroutineError;
    return returnValue;
  }

  returnValue = comessageQueuePush(coroutine, comessage) != coroutineSuccess;

  return returnValue;
}

/// @fn int sendComessageToPid(unsigned int pid, Comessage *comessage)
///
/// @brief Look up a coroutine by its PID and send a message to it.
///
/// @param pid The ID of the process to send the message to.
/// @param comessage A pointer to the message to send to the destination
///   process.
///
/// @return Returns coroutineSuccess on success, coroutineError on failure.
int sendComessageToPid(unsigned int pid, Comessage *comessage) {
  Coroutine *coroutine = getCoroutineByPid(pid);
  // If coroutine is NULL, it will be detected as not running by
  // sendComessageToCoroutine, so there's no real point in checking for NULL
  // here.
  return sendComessageToCoroutine(coroutine, comessage);
}

/// Comessage* getAvailableMessage(void)
///
/// @brief Get a message from the messages array that is not in use.
///
/// @return Returns a pointer to the available message on success, NULL if there
/// was no available message in the array.
Comessage* getAvailableMessage(void) {
  Comessage *availableMessage = NULL;

  for (int ii = 0; ii < NANO_OS_NUM_MESSAGES; ii++) {
    if (messages[ii].inUse == false) {
      availableMessage = &messages[ii];
      comessageInit(availableMessage, 0,
        &nanoOsMessages[ii], sizeof(nanoOsMessages[ii]), false);
      break;
    }
  }

  return availableMessage;
}

/// @fn Comessage* sendNanoOsMessageToCoroutine(Coroutine *coroutine, int type,
///   NanoOsMessageData func, NanoOsMessageData data, bool waiting)
///
/// @brief Send a NanoOsMessage to another process identified by its Coroutine.
///
/// @param pid The process ID of the destination process.
/// @param type The type of the message to send to the destination process.
/// @param func The function information to send to the destination process,
///   cast to a NanoOsMessageData.
/// @param data The data to send to the destination process, cast to a
///   NanoOsMessageData.
/// @param waiting Whether or not the sender is waiting on a response from the
///   destination process.
///
/// @return Returns a pointer to the sent Comessage on success, NULL on failure.
Comessage* sendNanoOsMessageToCoroutine(Coroutine *coroutine, int type,
  NanoOsMessageData func, NanoOsMessageData data, bool waiting
) {
  Comessage *comessage = NULL;
  if (!coroutineRunning(coroutine)) {
    // Can't send to a non-resumable coroutine.
    printString("ERROR!!!  Coroutine ");
    printInt(coroutineId(coroutine));
    printString(" is not running.\n");
    if (coroutine == NULL) {
      printString("coroutine is NULL\n");
    } else {
      printString("coroutine is in state ");
      printInt(coroutine->state);
      printString("\n");
    }
    return comessage; // NULL
  }

  comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);
  nanoOsMessage->func = func;
  nanoOsMessage->data = data;

  comessageInit(comessage, type,
    nanoOsMessage, sizeof(*nanoOsMessage), waiting);

  if (comessageQueuePush(coroutine, comessage) != coroutineSuccess) {
    if (comessageRelease(comessage) != coroutineSuccess) {
      printString("ERROR!!!  "
        "Could not release message from sendNanoOsMessageToCoroutine.\n");
    }
    comessage = NULL;
  }

  return comessage;
}

/// @fn Comessage* sendNanoOsMessageToPid(int pid, int type,
///   NanoOsMessageData func, NanoOsMessageData data, bool waiting)
///
/// @brief Send a NanoOsMessage to another process identified by its PID. Looks
/// up the process's Coroutine by its PID and then calls
/// sendNanoOsMessageToCoroutine.
///
/// @param pid The process ID of the destination process.
/// @param type The type of the message to send to the destination process.
/// @param func The function information to send to the destination process,
///   cast to a NanoOsMessageData.
/// @param data The data to send to the destination process, cast to a
///   NanoOsMessageData.
/// @param waiting Whether or not the sender is waiting on a response from the
///   destination process.
///
/// @return Returns a pointer to the sent Comessage on success, NULL on failure.
Comessage* sendNanoOsMessageToPid(int pid, int type,
  NanoOsMessageData func, NanoOsMessageData data, bool waiting
) {
  Comessage *comessage = NULL;
  if (pid >= NANO_OS_NUM_COMMANDS) {
    // Not a valid PID.  Fail.
    printString("ERROR!!!  ");
    printInt(pid);
    printString(" is not a valid PID.\n");
    return comessage; // NULL
  }

  Coroutine *coroutine = runningCommands[pid].coroutine;
  comessage
    = sendNanoOsMessageToCoroutine(coroutine, type, func, data, waiting);
  return comessage;
}

/// @fn void* waitForDataMessage(
///   Comessage *sent, int type, const struct timespec *ts)
///
/// @brief Wait for a reply to a previously-sent message and get the data from
/// it.  The provided message will be released when the reply is received.
///
/// @param sent A pointer to a previously-sent Comessage the calling function is
///   waiting on a reply to.
/// @param type The type of message expected to be sent as a response.
/// @param ts A pointer to a struct timespec with the future time at which to
///   timeout if nothing is received by then.  If this parameter is NULL, an
///   infinite timeout will be used.
///
/// @return Returns a pointer to the data member of the received message on
/// success, NULL on failure.
void* waitForDataMessage(Comessage *sent, int type, const struct timespec *ts) {
  void *returnValue = NULL;

  Comessage *incoming = comessageWaitForReplyWithType(sent, true, type, ts);
  if (incoming != NULL)  {
    returnValue = nanoOsMessageDataPointer(incoming, void*);
    if (comessageRelease(incoming) != coroutineSuccess) {
      printString("ERROR!!!  "
        "Could not release incoming message from waitForDataMessage.\n");
    }
  }

  return returnValue;
}

/// @fn void timespecFromDelay(struct timespec *ts, long int delayMs)
///
/// @brief Initialize the value of a struct timespec with a time in the future
/// based upon the current time and a specified delay period.  The timespec
/// will hold the value of the current time plus the delay.
///
/// @param ts A pointer to a struct timespec to initialize.
/// @param delayMs The number of milliseconds in the future the timespec is to
///   be initialized with.
///
/// @return This function returns no value.
void timespecFromDelay(struct timespec *ts, long int delayMs) {
  if (ts == NULL) {
    // Bad data.  Do nothing.
    return;
  }

  timespec_get(ts, TIME_UTC);
  ts->tv_sec += (delayMs / 1000);
  ts->tv_nsec += (delayMs * 1000000);

  return;
}

