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

// The setup function runs once when you press reset or power the board.  This
// is to be used for Arduino-specific setup.  *ANYTHING* that requires use of
// coroutines needs to be done in the loop function.
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  // start serial port at 9600 bps:
  Serial.begin(9600);
  // wait for serial port to connect. Needed for native USB port only.
  while (!Serial);

  digitalWrite(LED_BUILTIN, HIGH);
}

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

// In a normal Arduino sketch, the loop function runs over and over again
// forever.  For NanoOs, it will be called once and never exit.  This is because
// we want to do everything related to coroutines in this function.  The reason
// we want to do this is because we want to hide as much of the coroutine
// metadata storage as we can within the stack of the main loop.  Every stack,
// including the stack for the main loop, is NANO_OS_STACK_SIZE bytes in size.
// Because we need a stack for the main loop, there are effectively
// NANO_OS_NUM_COROUTINES + 1 stacks.  If we do nothing with this stack, we're
// just wasting space.  Also, making the storage for these things global
// consumes precious memory.  It's much more efficient to just store the
// pointers in the global address space and put the real storage in the main
// loop.  However, that means that we have to do all the one-time setup from
// within the main loop.  That, in turn, means that we can never exit this
// function.  So, we will do all the one-time setup and then enter an infinite
// loop from within this function that will be responsible for the task
// switching.
void loop() {
  Coroutine mainCoroutine;
  coroutineConfig(&mainCoroutine, NANO_OS_STACK_SIZE);

  RunningCommand runningCommandsStorage[NANO_OS_NUM_COROUTINES] = {};
  runningCommands = runningCommandsStorage;
  Comessage messagesStorage[NANO_OS_NUM_MESSAGES] = {};
  messages = messagesStorage;

  Coroutine *coroutine = NULL;
  for (int ii = 0; ii < NANO_OS_NUM_COROUTINES; ii++) {
    if (ii == NANO_OS_CONSOLE_PROCESS_ID) {
      continue;
    }
    coroutine = coroutineCreate(dummy);
    coroutineSetId(coroutine, ii);
    runningCommands[ii].coroutine = coroutine;
  }

  coroutine = coroutineCreate(runConsole);
  coroutineSetId(coroutine, NANO_OS_CONSOLE_PROCESS_ID);
  runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine = coroutine;
  runningCommands[NANO_OS_CONSOLE_PROCESS_ID].name = "runConsole";

  printConsole("\n");
  printConsole("Setup complete.\n");
  printConsole("> ");

  int coroutineIndex = 0;
  while (1) {
    coroutineResume(runningCommands[coroutineIndex].coroutine, NULL);
    handleMainCoroutineMessage();
    coroutineIndex++;
    coroutineIndex %= NANO_OS_NUM_COROUTINES;
  }
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

  Comessage *incoming = comessageQueuePopType(NULL, type);
  if (incoming != NULL)  {
    returnValue = comessageDataPointer(incoming);
    releaseMessage(incoming);
  }

  return returnValue;
}

