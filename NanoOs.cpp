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

// Custom includes
#include "NanoOs.h"

// Coroutines support
RunningCommand *runningCommands = NULL;
Comessage *messages = NULL;

void callFunction(Comessage *comessage) {
  CoroutineFunction func = comessageFunc(comessage, CoroutineFunction);
  Coroutine *coroutine = coroutineCreate(func);
  coroutineSetId(coroutine, NANO_OS_RESERVED_PROCESS_ID);
  runningCommands[NANO_OS_RESERVED_PROCESS_ID].coroutine = coroutine;
  coroutineResume(coroutine, comessageDataPointer(comessage));
}

void (*mainCoroutineCommandHandlers[])(Comessage*) {
  callFunction,
};

void handleMainCoroutineMessage(void) {
  Comessage *message = comessageQueuePop(NULL);
  if (message != NULL) {
    MainCoroutineCommand messageType
      = (MainCoroutineCommand) comessageType(message);
    if (messageType >= NUM_MAIN_COROUTINE_COMMANDS) {
      // Invalid.
      return;
    }

    mainCoroutineCommandHandlers[messageType](message);
    releaseMessage(message);
  }

  return;
}

void* dummy(void *args) {
  (void) args;
  nanoOsExitProcess(NULL);
}

int freeRamBytes(void) {
  extern int __heap_start,*__brkval;
  int v;
  return (int)&v - (__brkval == 0  
    ? (int)&__heap_start : (int) __brkval);  
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
      comessageInit(availableMessage, 0, NULL, NULL);
      break;
    }
  }

  return availableMessage;
}

int releaseMessage(Comessage *comessage) {
  return comessageDestroy(comessage);
}

Comessage* sendDataMessageToCoroutine(
  Coroutine *coroutine, int type, void *data
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

  comessageInit(comessage, type, NULL, (intptr_t) data);

  if (comessageQueuePush(coroutine, comessage) != coroutineSuccess) {
    releaseMessage(comessage);
    comessage = NULL;
  }

  return comessage;
}

Comessage* sendDataMessageToPid(int pid, int type, void *data) {
  Comessage *comessage = NULL;
  if (pid >= NANO_OS_NUM_COROUTINES) {
    // Not a valid PID.  Fail.
    return comessage; // NULL
  }

  Coroutine *coroutine = runningCommands[pid].coroutine;
  comessage
    = sendDataMessageToCoroutine(coroutine, type, data);
  return comessage;
}

void* waitForDataMessage(Comessage *sent, int type) {
  void *returnValue = NULL;

  while (sent->done == false) {
    coroutineYield(NULL);
  }
  releaseMessage(sent);

  Comessage *incoming = comessageQueueWaitForType(NULL, type);
  if (incoming != NULL)  {
    returnValue = comessageDataPointer(incoming);
    releaseMessage(incoming);
  }

  return returnValue;
}

