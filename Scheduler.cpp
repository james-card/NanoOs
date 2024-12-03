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

/// @var runningCommands
///
/// @brief Pointer to the array of running commands that will be stored in the
/// main loop function's stack.
RunningCommand *runningCommands = NULL;

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
  printString("startCommand starting.\n");
  // The scheduler is suspended because of the coroutineResume at the start
  // of this call.  So, we need to immediately yield and put ourselves back in
  // the round-robin array.
  coroutineYield(NULL);
  printString("startCommand returned from yield.\n");
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

  printString("Parsing: ");
  printString(consoleInput);
  printString("\n");
  argv = parseArgs(consoleInput, &argc);
  if (argv == NULL) {
    // Fail.
    printString("Could not parse input into argc and argv.\n");
    consoleInput = stringDestroy(consoleInput);
    if (comessageRelease(comessage) != coroutineSuccess) {
      printString("ERROR!!!  "
        "Could not release message from handleSchedulerMessage "
        "for invalid message type.\n");
    }
    return (void*) ((intptr_t) -1);
  }
  printString("Found ");
  printInt(argc);
  printString(" arguments.\n");

  int returnValue = commandEntry->func(argc, argv);
  consoleInput = stringDestroy(consoleInput);
  free(argv); argv = NULL;
  if (comessageRelease(comessage) != coroutineSuccess) {
    printString("ERROR!!!  "
      "Could not release message from handleSchedulerMessage "
      "for invalid message type.\n");
  }

  return (void*) ((intptr_t) returnValue);
}

/// @fn int runProcess(Comessage *comessage)
///
/// @brief Run a process in the slot for system processes.
///
/// @param comessage A pointer to the Comessage that was received that contains
///   the information about the process to run and how to run it.
///
/// @return Returns 0 on success, non-zero error code on failure.
int runProcess(Comessage *comessage) {
  printString("In runProcess handler.\n");
  static int returnValue = 0;
  if (comessage == NULL) {
    // This should be impossible, but there's nothing to do.  Return good
    // status.
    return returnValue; // 0
  }

  CommandEntry *commandEntry
    = nanoOsMessageFuncPointer(comessage, CommandEntry*);
  
  if (commandEntry->userProcess == false) {
    // This is not the expected case, but it's the priority case, so list it
    // first.
    Coroutine *coroutine
      = runningCommands[NANO_OS_RESERVED_PROCESS_ID].coroutine;
    if ((coroutine == NULL) || (coroutineFinished(coroutine))) {
      coroutine = coroutineCreate(startCommand);
      coroutineSetId(coroutine, NANO_OS_RESERVED_PROCESS_ID);

      runningCommands[NANO_OS_RESERVED_PROCESS_ID].coroutine = coroutine;
      runningCommands[NANO_OS_RESERVED_PROCESS_ID].name = commandEntry->name;

      printString("Launching ");
      printString(commandEntry->name);
      printString("\n");
      coroutineResume(coroutine, comessage);
      returnValue = 0;
    } else {
      // Don't call stringDestroy with consoleInput because we're going to try
      // this command in a bit.
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

        runningCommands[jj].coroutine = coroutine;
        runningCommands[jj].name = commandEntry->name;

        printString("Launching ");
        printString(commandEntry->name);
        printString("\n");
        coroutineResume(coroutine, comessage);
        returnValue = 0;
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
      char *consoleInput = nanoOsMessageDataPointer(comessage, char*);
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

/// @var schedulerCommandHandlers
///
/// @brief Array of function pointers for commands that are understood by the
/// message handler for the main loop function.
int (*schedulerCommandHandlers[])(Comessage*) = {
  runProcess,
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
  nanoOsExitProcess(0);
}

/// @fn void runScheduler(void)
///
/// @brief Initialize and run the round-robin scheduler.
///
/// @return This function returns no value and, in fact, never returns at all.
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
    handleSchedulerMessage();
    coroutineIndex++;
    coroutineIndex %= numScheduledCoroutines;
  }
}

