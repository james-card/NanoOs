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

/// @var runningCommands
///
/// @brief Pointer to the array of running commands that will be stored in the
/// main loop function's stack.
RunningCommand *runningCommands = NULL;

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

/// @fn int runSystemProcess(Comessage *comessage)
///
/// @brief Run a process in the slot for system processes.
///
/// @param comessage A pointer to the Comessage that was received that contains
///   the information about the process to run and how to run it.
///
/// @return Returns 0 on success, non-zero error code on failure.
int runSystemProcess(Comessage *comessage) {
  int returnValue = 0;

  Coroutine *coroutine
    = runningCommands[NANO_OS_SYSTEM_PROCESS_ID].coroutine;
  if ((coroutine == NULL) || (coroutineFinished(coroutine))) {
    CommandEntry *commandEntry
      = nanoOsMessageFuncPointer(comessage, CommandEntry*);
    CoroutineFunction func = commandEntry->func;
    coroutine = coroutineCreate(func);
    coroutineSetId(coroutine, NANO_OS_SYSTEM_PROCESS_ID);

    runningCommands[NANO_OS_SYSTEM_PROCESS_ID].coroutine = coroutine;
    runningCommands[NANO_OS_SYSTEM_PROCESS_ID].name = commandEntry->name;

    coroutineResume(coroutine, nanoOsMessageDataPointer(comessage, void*));
  } else {
    printConsole("ERROR:  System process already running.\n");
    returnValue = EBUSY;
  }

  return returnValue;
}

/// @var mainCoroutineCommandHandlers
///
/// @brief Array of function pointers for commands that are understood by the
/// message handler for the main loop function.
int (*mainCoroutineCommandHandlers[])(Comessage*) = {
  runSystemProcess,
};

/// @fn void handleMainCoroutineMessage(void)
///
/// @brief Handle one (and only one) message from our message queue.  If
/// handling the message is unsuccessful, the message will be returned to the
/// end of our message queue.
///
/// @return This function returns no value.
void handleMainCoroutineMessage(void) {
  Comessage *message = comessageQueuePop();
  if (message != NULL) {
    MainCoroutineCommand messageType
      = (MainCoroutineCommand) comessageType(message);
    if (messageType >= NUM_MAIN_COROUTINE_COMMANDS) {
      // Invalid.  Purge the message.
      if (comessageRelease(message) != coroutineSuccess) {
        printConsole("ERROR!!!  "
          "Could not release message from handleMainCoroutineMessage "
          "for invalid message type.\n");
      }
      return;
    }

    int returnValue = mainCoroutineCommandHandlers[messageType](message);
    if (returnValue == 0) {
      // Purge the message.
      if (comessageRelease(message) != coroutineSuccess) {
        printConsole("ERROR!!!  "
          "Could not release message from handleMainCoroutineMessage "
          "after handling message.\n");
      }
    } else {
      // Processing the message failed.  We can't release it.  Put it on the
      // back of our own queue again and try again later.
      printConsole("WARNING!  Message handler returned status ");
      printConsole(returnValue);
      printConsole(".  Returning unhandled message to back of queue.\n");
      comessageQueuePush(NULL, message);
    }
  }

  return;
}

/// @fn void* dummyProcess(void *args)
///
/// @brief Dummy process that's loaded at startup to prepopulate the process
/// array with coroutines.  Apart from that, the other purpose is to call
/// getFreeRamBytes so that we compute the actual amount of RAM left instead
/// of getting what each coroutine thinks it has.
///
/// @param args Any arguments passed to this function.  Ignored.
///
/// @return This function always returns NULL.
void* dummyProcess(void *args) {
  (void) args;
  (void) getFreeRamBytes();
  nanoOsExitProcess(NULL);
}

/// @var freeRamBytes
///
/// @brief The number of free bytes of RAM in the system.  Initialized to
/// INT_MAX and re-set every time a smaller value is computed.  The value of
/// this variable will be the value ultimately returned by getFreeRamBytes.
uintptr_t freeRamBytes = INT_MAX;

/// @fn uintptr_t getFreeRamBytes(void)
///
/// @brief Calculate the number of bytes that are available from the level of
/// the current function call.  If that value is smaller than the current value
/// of the freeRamBytes global variable, set freeRamBytes to the computed value.
///
/// @return Returns the value of freeRamBytes after any updates have been made.
uintptr_t getFreeRamBytes(void) {
  extern int __heap_start, *__brkval;
  int var;

  uintptr_t currentFreeRamBytes = (uintptr_t) (((uintptr_t) &var)
    - ((__brkval == NULL)
      ? (uintptr_t) &__heap_start
      : (uintptr_t) __brkval
    )
  );
  if (currentFreeRamBytes < freeRamBytes) {
    freeRamBytes = currentFreeRamBytes;
  }

  return freeRamBytes;
}

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

/// @fn Comessage* sendComessageToCoroutine(Coroutine *coroutine,
///   int type, void *data, size_t dataSize, bool waiting)
///
/// @brief Get an available Comessage, populate it with the specified data, and
/// push it onto a destination coroutine's queue.
///
/// @param coroutine A pointer to the destination coroutine to send the message
///   to.
/// @param type The integer type of the message.
/// @param data A pointer to the data contined in the message.
/// @param dataSize The number of bytes at the data pointer.
/// @param waiting Whether or not the caller is waiting on a reply to this
///   message.
///
/// @return Returns a pointer to the Comessage sent on success, NULL on failure.
Comessage* sendComessageToCoroutine(Coroutine *coroutine,
  int type, void *data, size_t dataSize, bool waiting
) {
  Comessage *comessage = NULL;
  if (!coroutineRunning(coroutine)) {
    // Can't send to a non-running coroutine.
    return comessage; // NULL
  }

  comessage = getAvailableMessage();
  while (comessage == NULL) {
    coroutineYield(NULL);
    comessage = getAvailableMessage();
  }

  comessageInit(comessage, type, data, dataSize, waiting);

  if (comessageQueuePush(coroutine, comessage) != coroutineSuccess) {
    if (comessageRelease(comessage) != coroutineSuccess) {
      printString("ERROR!!!  "
        "Could not release message from sendNanoOsMessageToCoroutine.\n");
    }
    comessage = NULL;
  }

  return comessage;
}

/// @fn Comessage* sendComessageToPid(unsigned int pid,
///   int type, void *data, size_t dataSize, bool waiting)
///
/// @brief Look up a coroutine by its PID and send a message to it.
///
/// @param pid The ID of the process to send the message to.
/// @param type The integer type of the message.
/// @param data A pointer to the data contined in the message.
/// @param dataSize The number of bytes at the data pointer.
/// @param waiting Whether or not the caller is waiting on a reply to this
///   message.
///
/// @return Returns a pointer to the Comessage sent on success, NULL on failure.
Comessage* sendComessageToPid(unsigned int pid,
  int type, void *data, size_t dataSize, bool waiting
) {
  Coroutine *coroutine = getCoroutineByPid(pid);
  // If coroutine is NULL, it will be detected as not running by
  // sendComessageToCoroutine, so there's no real point in checking for NULL
  // here.
  return sendComessageToCoroutine(coroutine, type, data, dataSize, waiting);
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

