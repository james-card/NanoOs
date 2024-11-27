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
    CommandEntry *commandEntry = comessageFunc(comessage, CommandEntry*);
    CoroutineFunction func = commandEntry->func;
    coroutine = coroutineCreate(func);
    coroutineSetId(coroutine, NANO_OS_SYSTEM_PROCESS_ID);

    runningCommands[NANO_OS_SYSTEM_PROCESS_ID].coroutine = coroutine;
    runningCommands[NANO_OS_SYSTEM_PROCESS_ID].name = commandEntry->name;

    coroutineResume(coroutine, comessageDataPointer(comessage));
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
int (*mainCoroutineCommandHandlers[])(Comessage*) {
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

void* dummy(void *args) {
  (void) args;
  (void) getFreeRamBytes();
  nanoOsExitProcess(NULL);
}

int freeRamBytes = INT_MAX;
int getFreeRamBytes(void) {
  extern int __heap_start,*__brkval;
  int v;
  int currentFreeRamBytes = (int)&v - (__brkval == 0
    ? (int)&__heap_start : (int) __brkval);
  if (currentFreeRamBytes < freeRamBytes) {
    freeRamBytes = currentFreeRamBytes;
  }
  return freeRamBytes;
}

long getElapsedMilliseconds(unsigned long startTime) {
  unsigned long now = millis();

  if (now < startTime) {
    return ULONG_MAX;
  }

  return now - startTime;
}

Comessage* getAvailableMessage(void) {
  Comessage *availableMessage = NULL;

  for (int ii = 0; ii < NANO_OS_NUM_MESSAGES; ii++) {
    if (messages[ii].inUse == false) {
      availableMessage = &messages[ii];
      comessageInit(availableMessage, 0, NULL, NULL, false);
      break;
    }
  }

  return availableMessage;
}

Comessage* sendDataMessageToCoroutine(
  Coroutine *coroutine, int type, void *data, bool waiting
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

  comessageInit(comessage, type, NULL, (intptr_t) data, waiting);

  if (comessageQueuePush(coroutine, comessage) != coroutineSuccess) {
    if (comessageRelease(comessage) != coroutineSuccess) {
      printString("ERROR!!!  "
        "Could not release message from handleMainCoroutineMessage\n");
    }
    comessage = NULL;
  }

  return comessage;
}

Comessage* sendDataMessageToPid(int pid, int type, void *data, bool waiting) {
  Comessage *comessage = NULL;
  if (pid >= NANO_OS_NUM_COROUTINES) {
    // Not a valid PID.  Fail.
    return comessage; // NULL
  }

  Coroutine *coroutine = runningCommands[pid].coroutine;
  comessage
    = sendDataMessageToCoroutine(coroutine, type, data, waiting);
  return comessage;
}

void* waitForDataMessage(Comessage *sent, int type, const struct timespec *ts) {
  void *returnValue = NULL;

  Comessage *incoming = comessageWaitForReplyWithType(sent, true, type, ts);
  if (incoming != NULL)  {
    returnValue = comessageDataPointer(incoming);
    if (comessageRelease(incoming) != coroutineSuccess) {
      printString("ERROR!!!  "
        "Could not release incoming message from handleMainCoroutineMessage\n");
    }
  }

  return returnValue;
}

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

