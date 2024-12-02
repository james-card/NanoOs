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
#include "Scheduler.h"

/// @var runningCommands
///
/// @brief Pointer to the array of running commands that will be stored in the
/// main loop function's stack.
RunningCommand *runningCommands = NULL;

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
    = runningCommands[NANO_OS_RESERVED_PROCESS_ID].coroutine;
  if ((coroutine == NULL) || (coroutineFinished(coroutine))) {
    CommandEntry *commandEntry
      = nanoOsMessageFuncPointer(comessage, CommandEntry*);
    CoroutineFunction func = commandEntry->func;
    coroutine = coroutineCreate(func);
    coroutineSetId(coroutine, NANO_OS_RESERVED_PROCESS_ID);

    runningCommands[NANO_OS_RESERVED_PROCESS_ID].coroutine = coroutine;
    runningCommands[NANO_OS_RESERVED_PROCESS_ID].name = commandEntry->name;

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

void runScheduler(void) {
  Coroutine mainCoroutine;
  coroutineConfig(&mainCoroutine, NANO_OS_STACK_SIZE);
  coroutineSetId(&mainCoroutine, 0);

  // Initialize ourself in the array of running commands.
  runningCommands[0].coroutine = &mainCoroutine;
  runningCommands[0].name = "scheduler";

  // Create the console process.
  Coroutine *coroutine = NULL;
  coroutine = coroutineCreate(runConsole);
  coroutineSetId(coroutine, NANO_OS_CONSOLE_PROCESS_ID);
  runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine = coroutine;
  runningCommands[NANO_OS_CONSOLE_PROCESS_ID].name = "console";

  // We need to do an initial population of all the commands because we need to
  // get to the end of memory to run the memory manager in whatever is left
  // over.
  for (int ii = NANO_OS_RESERVED_PROCESS_ID; ii < NANO_OS_NUM_COMMANDS; ii++) {
    coroutine = coroutineCreate(dummyProcess);
    coroutineSetId(coroutine, ii);
    runningCommands[ii].coroutine = coroutine;
  }

  // Create the memory manager process.  !!! THIS MUST BE THE LAST PROCESS
  // CREATED BECAUSE WE WANT TO USE THE ENTIRE REST OF MEMORY FOR IT !!!
  coroutine = coroutineCreate(memoryManager);
  coroutineSetId(coroutine, NANO_OS_MEMORY_MANAGER_PROCESS_ID);
  runningCommands[NANO_OS_MEMORY_MANAGER_PROCESS_ID].coroutine = coroutine;
  runningCommands[NANO_OS_MEMORY_MANAGER_PROCESS_ID].name = "memory manager";

  // We're going to do a round-robin scheduler.  We don't want to use the array
  // of runningCommands because the scheduler process itself is in that list.
  // Instead, create an array of double-pointers to the coroutines that we'll
  // want to run in that array.
  Coroutine **scheduledCoroutines[NANO_OS_NUM_COMMANDS - 1];
  const int numScheduledCoroutines = NANO_OS_NUM_COMMANDS - 1;
  for (int ii = 0; ii < numScheduledCoroutines; ii++) {
    scheduledCoroutines[ii] = &runningCommands[ii + 1].coroutine;
  }

  // Start our round-robin scheduler.
  int coroutineIndex = 0;
  while (1) {
    coroutineResume(*scheduledCoroutines[coroutineIndex], NULL);
    handleMainCoroutineMessage();
    coroutineIndex++;
    coroutineIndex %= numScheduledCoroutines;
  }
}

