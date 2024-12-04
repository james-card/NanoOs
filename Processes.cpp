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
#include "Processes.h"

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

/// @fn int getNumTokens(const char *input)
///
/// @brief Get the number of whitespace-delimited tokens in a string.
///
/// @param input A pointer to the input string to consider.
///
/// @return Returns the number of tokens discovered.
int getNumTokens(const char *input) {
  int numTokens = 0;
  if (input == NULL) {
    return numTokens;
  }

  while (*input != '\0') {
    numTokens++;
    input = &input[strcspn(input, " \t\r\n")];
    input = &input[strspn(input, " \t\r\n")];
  }

  return numTokens;
}

/// @fn int getNumLeadingBackslashes(char *strStart, char *strPos)
///
/// @brief Get the number of backslashes that precede a character.
///
/// @param strStart A pointer to the start of the string the character is in.
/// @param strPos A pointer to the character to look before.
int getNumLeadingBackslashes(char *strStart, char *strPos) {
  int numLeadingBackslashes = 0;

  strPos--;
  while ((((uintptr_t) strPos) >= ((uintptr_t) strStart))
    && (*strPos == '\\')
  ) {
    numLeadingBackslashes++;
    strPos--;
  }

  return numLeadingBackslashes;
}

/// @fn char *findEndQuote(char *input, char quote)
///
/// @brief Find the first double quote that is not escaped.
///
/// @brief input A pointer to the beginning of the string to search.
///
/// @return Returns a pointer to the end quote on success, NULL on failure.
char *findEndQuote(char *input, char quote) {
  char *quoteAt = strchr(input, quote);
  while ((quoteAt != NULL)
    && (getNumLeadingBackslashes(input, quoteAt) & 1)
  ) {
    input = quoteAt + 1;
    quoteAt = strchr(input, quote);
  }

  return quoteAt;
}

/// @fn char** parseArgs(char *consoleInput, int *argc)
///
/// @brief Parse a raw input string from the console into an array of individual
/// strings to pass as the argv array to a command function.
///
/// @param consoleInput The raw string of data read from the user's input by the
///   console.
/// @param argc A pointer to the integer where the number of parsed arguments
///   will be stored.
///
/// @return Returns a pointer to an array of strings on success, NULL on
/// failure.
char** parseArgs(char *consoleInput, int *argc) {
  char **argv = NULL;

  if ((consoleInput == NULL) || (argc == NULL)) {
    // Failure.
    return argv; // NULL
  }
  *argc = 0;
  char *endOfInput = &consoleInput[strlen(consoleInput)];

  // First, we need to declare an array that will hold all our arguments.  In
  // order to do this, we need to know the maximum number of arguments we'll be
  // working with.  That will be the number of tokens separated by whitespace.
  int maxNumArgs = getNumTokens(consoleInput);
  argv = (char**) malloc(maxNumArgs * sizeof(char*));
  if (argv == NULL) {
    // Nothing we can do.  Fail.
    return argv; // NULL
  }

  // Next, go through the input and fill in the elements of the argv array with
  // the addresses first letter of each argument and put a NULL byte at the end
  // of each argument.
  int numArgs = 0;
  char *endOfArg = NULL;
  while ((consoleInput != endOfInput) && (*consoleInput != '\0')) {
    if (*consoleInput == '"') {
      consoleInput++;
      endOfArg = findEndQuote(consoleInput, '"');
    } else if (*consoleInput == '\'') {
      consoleInput++;
      endOfArg = findEndQuote(consoleInput, '\'');
    } else {
      endOfArg = &consoleInput[strcspn(consoleInput, " \t\r\n")];
    }

    argv[numArgs] = consoleInput;
    numArgs++;

    if (endOfArg != NULL) {
      *endOfArg = '\0';
      if (endOfArg != endOfInput) {
        consoleInput = endOfArg + 1;
      } else {
        consoleInput = endOfInput;
      }
    } else {
      consoleInput += strlen(consoleInput);
    }

    consoleInput = &consoleInput[strspn(consoleInput, " \t\r\n")];
  }

  *argc = numArgs;
  return argv;
}

/// @fn void* startCommand(void *args)
///
/// @brief Wrapper coroutine that calls a command function.
///
/// @param args The message received from the console process that describes
///   the command to run, cast to a void*.
///
/// @return If the comamnd is run, returns the result of the command cast to a
/// void*.  If the command is not run, returns -1 cast to a void*.
void* startCommand(void *args) {
  // The scheduler is suspended because of the coroutineResume at the start
  // of this call.  So, we need to immediately yield and put ourselves back in
  // the round-robin array.
  coroutineYield(NULL);
  Comessage *comessage = (Comessage*) args;
  if (comessage == NULL) {
    printString("ERROR:  No arguments message provided to startCommand.\n");
    return (void*) ((intptr_t) -1);
  }

  int argc = 0;
  char **argv = NULL;
  CommandEntry *commandEntry
    = nanoOsMessageFuncPointer(comessage, CommandEntry*);
  char *consoleInput = nanoOsMessageDataPointer(comessage, char*);

  argv = parseArgs(consoleInput, &argc);
  if (argv == NULL) {
    // Fail.
    printString("ERROR:  Could not parse input into argc and argv.\n");
    consoleInput = stringDestroy(consoleInput);
    if (comessageRelease(comessage) != coroutineSuccess) {
      printString("ERROR!!!  "
        "Could not release message from handleSchedulerMessage "
        "for invalid message type.\n");
    }
    return (void*) ((intptr_t) -1);
  }

  int returnValue = commandEntry->func(argc, argv);
  free(consoleInput); consoleInput = NULL;
  free(argv); argv = NULL;
  if (comessageRelease(comessage) != coroutineSuccess) {
    printString("ERROR!!!  "
      "Could not release message from handleSchedulerMessage "
      "for invalid message type.\n");
  }

  // We need to clear the coroutine pointer.
  runningCommands[coroutineId(NULL)].coroutine = NULL;

  return (void*) ((intptr_t) returnValue);
}

/// @fn int handleRunProcess(Comessage *comessage)
///
/// @brief Run a process in an appropriate process slot.
///
/// @param comessage A pointer to the Comessage that was received that contains
///   the information about the process to run and how to run it.
///
/// @return Returns 0 on success, non-zero error code on failure.
int handleRunProcess(Comessage *comessage) {
  static int returnValue = 0;
  if (comessage == NULL) {
    // This should be impossible, but there's nothing to do.  Return good
    // status.
    return returnValue; // 0
  }

  CommandEntry *commandEntry
    = nanoOsMessageFuncPointer(comessage, CommandEntry*);
  char *consoleInput = nanoOsMessageDataPointer(comessage, char*);
  
  if (commandEntry->userProcess == false) {
    // This is not the expected case, but it's the priority case, so list it
    // first.
    Coroutine *coroutine
      = runningCommands[NANO_OS_RESERVED_PROCESS_ID].coroutine;
    if ((coroutine == NULL) || (coroutineFinished(coroutine))) {
      coroutine = coroutineCreate(startCommand);
      coroutineSetId(coroutine, NANO_OS_RESERVED_PROCESS_ID);
      if (assignMemory(consoleInput, NANO_OS_RESERVED_PROCESS_ID) != 0) {
        printString(
          "WARNING:  Could not assign console input to new process.\n");
        printString("Memory leak.\n");
      }

      runningCommands[NANO_OS_RESERVED_PROCESS_ID].coroutine = coroutine;
      runningCommands[NANO_OS_RESERVED_PROCESS_ID].name = commandEntry->name;

      coroutineResume(coroutine, comessage);
      returnValue = 0;
      if (comessageRelease(comessage) != coroutineSuccess) {
        printString("ERROR!!!  "
          "Could not release message from handleSchedulerMessage "
          "for invalid message type.\n");
      }
    } else {
      // Don't call stringDestroy with consoleInput because we're going to try
      // this command again in a bit.
      if (returnValue == 0) {
        releaseConsole();
      }
      returnValue = EBUSY;
    }
  } else {
    // Find an open non-reserved slot.
    int jj = NANO_OS_FIRST_PROCESS_ID;
    for (; jj < NANO_OS_NUM_COMMANDS; jj++) {
      Coroutine *coroutine = runningCommands[jj].coroutine;
      if ((coroutine == NULL) || (coroutineFinished(coroutine))) {
        coroutine = coroutineCreate(startCommand);
        coroutineSetId(coroutine, jj);
        if (assignMemory(consoleInput, jj) != 0) {
          printString(
            "WARNING:  Could not assign console input to new process.\n");
          printString("Memory leak.\n");
        }

        runningCommands[jj].coroutine = coroutine;
        runningCommands[jj].name = commandEntry->name;

        coroutineResume(coroutine, comessage);
        returnValue = 0;
        if (comessageRelease(comessage) != coroutineSuccess) {
          printString("ERROR!!!  "
            "Could not release message from handleSchedulerMessage "
            "for invalid message type.\n");
        }
        break;
      }
    }

    if (jj == NANO_OS_NUM_COMMANDS) {
      // printf is blocking.  handleCommand is called from runConsole itself,
      // so we can't use a blocking call here.  Use the non-blocking
      // printString instead.
      printString("Out of memory to launch process.\n");
      // This is a user process, not a system process, so the user is just out
      // of luck.  *DO NOT* set returnValue to a non-zero value here as that
      // would result in an infinite loop.
      consoleInput = stringDestroy(consoleInput);
      if (comessageRelease(comessage) != coroutineSuccess) {
        printString("ERROR!!!  "
          "Could not release message from handleSchedulerMessage "
          "for invalid message type.\n");
      }
      releaseConsole();
    }
  }

  return returnValue;
}

/// @fn int handleKillProcess(Comessage *comessage)
///
/// @brief Kill a process identified by its process ID.
///
/// @param comessage A pointer to the Comessage that was received that contains
///   the information about the process to kill.
///
/// @return Returns 0 on success, non-zero error code on failure.
int handleKillProcess(Comessage *comessage) {
  int returnValue = 0;

  COROUTINE_ID_TYPE processId
    = nanoOsMessageDataValue(comessage, COROUTINE_ID_TYPE);
  if ((processId > NANO_OS_RESERVED_PROCESS_ID)
    && (processId < NANO_OS_NUM_COMMANDS)
    && (coroutineResumable(runningCommands[processId].coroutine))
  ) {
    if (coroutineTerminate(runningCommands[processId].coroutine, NULL)
      == coroutineSuccess
    ) {
      runningCommands[processId].coroutine = NULL;
      runningCommands[processId].name = NULL;

      // Forward the message on to the memory manager to have it clean up the
      // process's memory.  *DO NOT* mark the message as done.  The memory
      // manager will do that.
      NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);
      comessageInit(comessage, MEMORY_MANAGER_FREE_PROCESS_MEMORY,
        nanoOsMessage, sizeof(*nanoOsMessage), /* waiting= */ false);
      memoryManagerCast(comessage);
    } else {
      // Tell the caller that we've failed.
      NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);
      nanoOsMessage->data = 1;
      if (comessageSetDone(comessage) != coroutineSuccess) {
        printString("ERROR!!!  "
          "Could not mark message done in handleKillProcess.\n");
      }
    }
  }

  // DO NOT release the message since that's done by the memory manager handler.

  return returnValue;
}

/// @fn int handleGetNumRunningProcesses(Comessage *comessage)
///
/// @brief Get the number of processes that are currently running in the system.
///
/// @param comessage A pointer to the Comessage that was received.  This will be
///   reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int handleGetNumRunningProcesses(Comessage *comessage) {
  int returnValue = 0;

  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);

  uint8_t numRunningProcesses = 0;
  for (int ii = 0; ii < NANO_OS_NUM_COMMANDS; ii++) {
    if (coroutineRunning(runningCommands[ii].coroutine)) {
      numRunningProcesses++;
    }
  }
  nanoOsMessage->data = numRunningProcesses;

  comessageSetDone(comessage);

  // DO NOT release the message since the caller is waiting on the response.

  return returnValue;
}

/// @fn int handleGetProcessInfo(Comessage *comessage)
///
/// @brief Fill in a provided array with information about the currently-running
/// processes.
///
/// @param comessage A pointer to the Comessage that was received.  This will be
///   reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int handleGetProcessInfo(Comessage *comessage) {
  int returnValue = 0;

  ProcessInfo *processInfo
    = nanoOsMessageDataPointer(comessage, ProcessInfo*);
  int idx = 0;
  for (int ii = 0; ii < NANO_OS_NUM_COMMANDS; ii++) {
    if (coroutineResumable(runningCommands[ii].coroutine)) {
      processInfo[idx].pid
        = (intptr_t) coroutineId(runningCommands[ii].coroutine);
      processInfo[idx].name = runningCommands[ii].name;
      idx++;
    }
  }

  comessageSetDone(comessage);

  // DO NOT release the message since the caller is waiting on the response.

  return returnValue;
}

/// @var schedulerCommandHandlers
///
/// @brief Array of function pointers for commands that are understood by the
/// message handler for the main loop function.
int (*schedulerCommandHandlers[])(Comessage*) = {
  handleRunProcess,
  handleKillProcess,
  handleGetNumRunningProcesses,
  handleGetProcessInfo,
};

/// @fn void handleSchedulerMessage(void)
///
/// @brief Handle one (and only one) message from our message queue.  If
/// handling the message is unsuccessful, the message will be returned to the
/// end of our message queue.
///
/// @return This function returns no value.
void handleSchedulerMessage(void) {
  Comessage *message = comessageQueuePop();
  if (message != NULL) {
    SchedulerCommand messageType = (SchedulerCommand) comessageType(message);
    if (messageType >= NUM_SCHEDULER_COMMANDS) {
      // Invalid.  Purge the message.
      if (comessageRelease(message) != coroutineSuccess) {
        printString("ERROR!!!  "
          "Could not release message from handleSchedulerMessage "
          "for invalid message type.\n");
      }
      return;
    }

    int returnValue = schedulerCommandHandlers[messageType](message);
    if (returnValue != 0) {
      printString("Scheduler command handler failed.\n");
      printString("Pushing message back onto our own queue.\n");
      // Processing the message failed.  We can't release it.  Put it on the
      // back of our own queue again and try again later.
      comessageQueuePush(NULL, message);
    }
  }

  return;
}

/// @fn void* dummyProcess(void *args)
///
/// @brief Dummy process that's loaded at startup to prepopulate the process
/// array with coroutines.
///
/// @param args Any arguments passed to this function.  Ignored.
///
/// @return This function always returns NULL.
void* dummyProcess(void *args) {
  (void) args;
  runningCommands[coroutineId(NULL)].coroutine = NULL;
  nanoOsExitProcess(0);
}

/// @fn void runScheduler(void)
///
/// @brief Initialize and run the round-robin scheduler.
///
/// @return This function returns no value and, in fact, never returns at all.
void runScheduler(void) {
  // Create the storage for the array of running commands and initialize the
  // array global pointer.
  RunningCommand runningCommandsStorage[NANO_OS_NUM_COMMANDS] = {};
  runningCommands = runningCommandsStorage;

  // Create the storage for the array of Comessages and NanoOsMessages and
  // initialize the global array pointers and the back-pointers for the
  // NanoOsMessges.
  Comessage messagesStorage[NANO_OS_NUM_MESSAGES] = {};
  messages = messagesStorage;
  NanoOsMessage nanoOsMessagesStorage[NANO_OS_NUM_MESSAGES] = {};
  nanoOsMessages = nanoOsMessagesStorage;
  for (int ii = 0; ii < NANO_OS_NUM_MESSAGES; ii++) {
    // messages[ii].data will be initialized by getAvailableMessage.
    nanoOsMessages[ii].comessage = &messages[ii];
  }

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
    handleSchedulerMessage();
    coroutineIndex++;
    coroutineIndex %= numScheduledCoroutines;
  }
}

/// @fn ProcessInfo* getProcessInfo(uint8_t *numRunningProcesses)
///
/// @brief Do all the inter-process communication with the scheduler required
/// to get information about the running processes.
///
/// @param numRunningProcesses A pointer to the storage for number of running
///   processes.
///
/// @return Returns a pointer to the dynamically-allocated ProcessInfo array
/// on success, NULL on failure.
ProcessInfo* getProcessInfo(uint8_t *numRunningProcesses) {
  Comessage *comessage = NULL;
  int waitStatus = coroutineSuccess;
  ProcessInfo *processInfo = NULL;
  NanoOsMessage *nanoOsMessage = NULL;

  // We don't know where our messages to the scheduler will be in its queue, so
  // we can't assume they will be processed immediately, but we can't wait
  // forever either.  Set a 100 ms timeout.
  struct timespec timeout = { 0, 100000000 };

  if (numRunningProcesses == NULL) {
    printf("ERROR:  numRunningProcesses cannot be NULL in getProcessInfo.\n");
    goto exit;
  }

  // Initialize numRunningProcesses to 0 if it's not already.
  *numRunningProcesses = 0;

  // Because the scheduler runs on the main coroutine, it doesn't have the
  // ability to yield.  That means it can't do anything that requires a
  // synchronus message exchange, i.e. allocating memory.  So, we need to
  // allocate memory from the current process and then pass that back to the
  // scheduler to populate.  That means we first need to know how many processes
  // are running so that we know how much space to allocate.  So, get that
  // first.
  comessage = sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_GET_NUM_RUNNING_PROCESSES,
    (NanoOsMessageData) 0, (NanoOsMessageData) 0, false);
  if (comessage == NULL) {
    printf("ERROR!!!  Could not communicate with scheduler.\n");
    goto exit;
  }

  waitStatus = comessageWaitForDone(comessage, &timeout);
  if (waitStatus != coroutineSuccess) {
    if (waitStatus == coroutineTimedout) {
      printf("Command to get the number of running commands timed out.\n");
    } else {
      printf("Command to get the number of running commands failed.\n");
    }

    // Without knowing how many processes there are, we can't continue.  Bail.
    goto releaseMessage;
  }

  *numRunningProcesses = nanoOsMessageDataValue(comessage, COROUTINE_ID_TYPE);
  if (*numRunningProcesses == 0) {
    printf("ERROR:  Number of running processes returned from the "
      "scheduler is 0.\n");
    goto releaseMessage;
  }

  // We need numRunningProcesses rows.
  processInfo
    = (ProcessInfo*) malloc(*numRunningProcesses * sizeof(ProcessInfo));
  nanoOsMessage = (NanoOsMessage*) comessageData(comessage);
  nanoOsMessage->data = (NanoOsMessageData) ((intptr_t) processInfo);
  if (comessageInit(comessage, SCHEDULER_GET_PROCESS_INFO,
    nanoOsMessage, sizeof(NanoOsMessage), /* waiting= */ true)
    != coroutineSuccess
  ) {
    printf(
      "ERROR:  Could not initialize message to send to get process info.\n");
    goto freeMemory;
  }

  if (sendComessageToPid(NANO_OS_SCHEDULER_PROCESS_ID, comessage)
    != coroutineSuccess
  ) {
    printf("ERROR:  Could not send scheduler message to get process info.\n");
    goto freeMemory;
  }

  waitStatus = comessageWaitForDone(comessage, &timeout);
  if (waitStatus != coroutineSuccess) {
    if (waitStatus == coroutineTimedout) {
      printf("Command to get process information timed out.\n");
    } else {
      printf("Command to get process information failed.\n");
    }

    // Without knowing the data for the processes, we can't display them.  Bail.
    goto freeMemory;
  }

  goto releaseMessage;

freeMemory:
  free(processInfo); processInfo = NULL;

releaseMessage:
  if (comessageRelease(comessage) != coroutineSuccess) {
    printf("ERROR!!!  Could not release message sent to scheduler for "
      "getting the number of running processes.\n");
  }

exit:
  return processInfo;
}

/// @fn int killProcess(COROUTINE_ID_TYPE processId)
///
/// @brief Do all the inter-process communication with the scheduler required
/// to kill a running process.
///
/// @param processId The ID of the process to kill.
///
/// @return Returns 0 on success, 1 on failure.
int killProcess(COROUTINE_ID_TYPE processId) {
  if ((processId <= NANO_OS_RESERVED_PROCESS_ID)
    || (processId >= NANO_OS_NUM_COMMANDS)
    || (coroutineResumable(runningCommands[processId].coroutine) == false)
  ) {
    printString("ERROR:  Invalid process ID.\n");
    return 1;
  }

  Comessage *comessage = sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_KILL_PROCESS,
    (NanoOsMessageData) 0, (NanoOsMessageData) processId, false);
  if (comessage == NULL) {
    printf("ERROR!!!  Could not communicate with scheduler.\n");
    releaseConsole();
    return 1;
  }

  // We don't know where our message to the scheduler will be in its queue, so
  // we can't assume it will be processed immediately, but we can't wait forever
  // either.  Set a 100 ms timeout.
  struct timespec ts = { 0, 100000000 };
  int waitStatus = comessageWaitForDone(comessage, &ts);
  int returnValue = 0;
  if (waitStatus == coroutineSuccess) {
    NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);
    returnValue = nanoOsMessage->data;
    if (returnValue == 0) {
      printf("Process terminated.\n");
    } else {
      printf("Process termination returned status %d.\n", returnValue);
    }
  } else {
    returnValue = 1;
    if (waitStatus == coroutineTimedout) {
      printf("Command to kill PID %d timed out.\n", processId);
    } else {
      printf("Command to kill PID %d failed.\n", processId);
    }
  }

  if (comessageRelease(comessage) != coroutineSuccess) {
    returnValue = 1;
    printf("ERROR!!!  "
      "Could not release message sent to scheduler for kill command.\n");
  }

  return returnValue;
}

/// @fn int runProcess(CommandEntry *commandEntry, char *consoleInput)
///
/// @brief Do all the inter-process communication with the scheduler required
/// to start a process.
///
/// @param commandEntry A pointer to the CommandEntry that describes the command
///   to run.
/// @param consoleInput The raw consoleInput that was captured for the command
///   line.
///
/// @return Returns 0 on success, 1 on failure.
int runProcess(CommandEntry *commandEntry, char *consoleInput) {
  int returnValue = 0;

  if (sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_RUN_PROCESS,
    (NanoOsMessageData) commandEntry, (NanoOsMessageData) consoleInput,
    false) == NULL
  ) {
    printString("ERROR!!!  Could not communicate with scheduler.\n");
    returnValue = 1;
  }

  return returnValue;
}

