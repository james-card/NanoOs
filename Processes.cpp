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

extern Comessage *messages;
extern NanoOsMessage *nanoOsMessages;

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
  CommandDescriptor *commandDescriptor
    = nanoOsMessageDataPointer(comessage, CommandDescriptor*);
  char *consoleInput = commandDescriptor->consoleInput;

  argv = parseArgs(consoleInput, &argc);
  if (argv == NULL) {
    // Fail.
    printString("ERROR:  Could not parse input into argc and argv.\n");
    printString("Received consoleInput:  \"");
    printString(consoleInput);
    printString("\"\n");
    consoleInput = stringDestroy(consoleInput);
    if (comessageRelease(comessage) != coroutineSuccess) {
      printString("ERROR!!!  "
        "Could not release message from handleSchedulerMessage "
        "for invalid message type.\n");
    }
    releaseConsole();
    return (void*) ((intptr_t) -1);
  }

  bool backgroundProcess = false;
  if (commandEntry->shellCommand == false) {
    char *ampersandAt = strchr(argv[argc - 1], '&');
    if (ampersandAt != NULL) {
      ampersandAt++;
      if (ampersandAt[strspn(ampersandAt, " \t\r\n")] == '\0') {
        backgroundProcess = true;
        releaseConsole();
        sendNanoOsMessageToPid(commandDescriptor->callingProcess,
          SCHEDULER_PROCESS_COMPLETE, 0, 0, false);
      }
    }
  }

  // Call the process function.
  int returnValue = commandEntry->func(argc, argv);

  if (commandDescriptor->callingProcess != coroutineId(getRunningCoroutine())) {
    // This command did NOT replace a shell process.
    releaseConsole();
    if (backgroundProcess == false) {
      // The caller is still running and waiting to be told it can resume.
      // Notify it via a message.
      sendNanoOsMessageToPid(commandDescriptor->callingProcess,
        SCHEDULER_PROCESS_COMPLETE, 0, 0, false);
    }
    commandDescriptor->schedulerState->allProcesses[
      coroutineId(getRunningCoroutine())].userId = NO_USER_ID;
  } else {
    // This command DID replace a shell process.  We need to release the
    // message that was sent because there's no shell that is waiting to
    // release it.
    if (comessageRelease(comessage) != coroutineSuccess) {
      printString("ERROR!!!  "
        "Could not release message from handleSchedulerMessage "
        "for invalid message type.\n");
    }
    releaseConsole();
  }

  free(consoleInput); consoleInput = NULL;
  free(commandDescriptor); commandDescriptor = NULL;
  free(argv); argv = NULL;

  return (void*) ((intptr_t) returnValue);
}

/// @fn ProcessInfo* getProcessInfo(void)
///
/// @brief Do all the inter-process communication with the scheduler required
/// to get information about the running processes.
///
/// @return Returns a pointer to the dynamically-allocated ProcessInfo object
/// on success, NULL on failure.
ProcessInfo* getProcessInfo(void) {
  Comessage *comessage = NULL;
  int waitStatus = coroutineSuccess;
  ProcessInfo *processInfo = NULL;
  NanoOsMessage *nanoOsMessage = NULL;
  uint8_t numProcessDescriptores = 0;

  // We don't know where our messages to the scheduler will be in its queue, so
  // we can't assume they will be processed immediately, but we can't wait
  // forever either.  Set a 100 ms timeout.
  struct timespec timeout = { 0, 100000000 };

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

  numProcessDescriptores = nanoOsMessageDataValue(comessage, CoroutineId);
  if (numProcessDescriptores == 0) {
    printf("ERROR:  Number of running processes returned from the "
      "scheduler is 0.\n");
    goto releaseMessage;
  }

  // We need numProcessDescriptores rows.
  processInfo = (ProcessInfo*) malloc(sizeof(ProcessInfo)
    + ((numProcessDescriptores - 1) * sizeof(ProcessInfoElement)));
  if (processInfo == NULL) {
    printf(
      "ERROR:  Could not allocate memory for processInfo in getProcessInfo.\n");
  }

  // It is possible, although unlikely, that an additional process is started
  // between the time we made the call above and the time that our message gets
  // handled below.  We allocated our return value based upon the size that was
  // returned above and, if we're not careful, it will be possible to overflow
  // the array.  Initialize processInfo->numProcesses so that
  // schedulerGetProcessInfoCommandHandler knows the maximum number of
  // ProcessInfoElements it can populated.
  processInfo->numProcesses = numProcessDescriptores;
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

/// @fn int killProcess(CoroutineId processId)
///
/// @brief Do all the inter-process communication with the scheduler required
/// to kill a running process.
///
/// @param processId The ID of the process to kill.
///
/// @return Returns 0 on success, 1 on failure.
int killProcess(CoroutineId processId) {
  Comessage *comessage = sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_KILL_PROCESS,
    (NanoOsMessageData) 0, (NanoOsMessageData) processId, false);
  if (comessage == NULL) {
    printf("ERROR!!!  Could not communicate with scheduler.\n");
    return 1;
  }

  // We don't know where our message to the scheduler will be in its queue, so
  // we can't assume it will be processed immediately, but we can't wait forever
  // either.  Set a 100 ms timeout.
  struct timespec ts = { 0, 0 };
  timespec_get(&ts, TIME_UTC);
  ts.tv_nsec += 100000000;

  int waitStatus = comessageWaitForDone(comessage, &ts);
  int returnValue = 0;
  if (waitStatus == coroutineSuccess) {
    NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);
    returnValue = nanoOsMessage->data;
    if (returnValue == 0) {
      printf("Process terminated.\n");
    } else {
      printf("Process termination returned status \"%s\".\n",
        nanoOsStrError(returnValue));
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

/// @fn int runProcess(CommandEntry *commandEntry,
///   char *consoleInput, int consolePort)
///
/// @brief Do all the inter-process communication with the scheduler required
/// to start a process.
///
/// @param commandEntry A pointer to the CommandEntry that describes the command
///   to run.
/// @param consoleInput The raw consoleInput that was captured for the command
///   line.
/// @param consolePort The index of the console port the process is being
///   launched from.
///
/// @return Returns 0 on success, 1 on failure.
int runProcess(CommandEntry *commandEntry,
  char *consoleInput, int consolePort
) {
  int returnValue = 1;
  CommandDescriptor *commandDescriptor
    = (CommandDescriptor*) malloc(sizeof(CommandDescriptor));
  if (commandDescriptor == NULL) {
    printString("ERROR!!!  Could not allocate CommandDescriptor.\n");
    return returnValue; // 1
  }
  commandDescriptor->consoleInput = consoleInput;
  commandDescriptor->consolePort = consolePort;
  commandDescriptor->callingProcess = coroutineId(getRunningCoroutine());

  Comessage *sent = sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_RUN_PROCESS,
    (NanoOsMessageData) commandEntry, (NanoOsMessageData) commandDescriptor,
    true);
  if (sent == NULL) {
    printString("ERROR!!!  Could not communicate with scheduler.\n");
    return returnValue; // 1
  }

  Comessage *doneMessage
    = comessageQueueWaitForType(SCHEDULER_PROCESS_COMPLETE, NULL);
  // We don't need any data from the message.  Just release it.
  comessageRelease(doneMessage);
  if (comessageDone(sent) == false) {
    // The called process was killed.  We need to release the sent message on
    // its behalf.
    comessageRelease(sent);
  }

  returnValue = 0;
  return returnValue;
}

/// @fn UserId getProcessUser(void)
///
/// @brief Get the ID of the user running the current process.
///
/// @return Returns the ID of the user running the current process on success,
/// -1 on failure.
UserId getProcessUser(void) {
  UserId userId = -1;
  Comessage *comessage
    = sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_GET_PROCESS_USER,
    /* func= */ 0, /* data= */ 0, true);
  if (comessage == NULL) {
    printString("ERROR!!!  Could not communicate with scheduler.\n");
    return userId; // -1
  }

  comessageWaitForDone(comessage, NULL);
  userId = nanoOsMessageDataValue(comessage, UserId);
  comessageRelease(comessage);

  return userId;
}

/// @fn int setProcessUser(UserId userId)
///
/// @brief Set the user ID of the current process to the specified user ID.
///
/// @return Returns 0 on success, -1 on failure.
int setProcessUser(UserId userId) {
  int returnValue = -1;
  Comessage *comessage
    = sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_SET_PROCESS_USER,
    /* func= */ 0, /* data= */ userId, true);
  if (comessage == NULL) {
    printString("ERROR!!!  Could not communicate with scheduler.\n");
    return returnValue; // -1
  }

  comessageWaitForDone(comessage, NULL);
  returnValue = nanoOsMessageDataValue(comessage, int);
  comessageRelease(comessage);

  if (returnValue != 0) {
    printf("Scheduler returned \"%s\" for setProcessUser.\n",
      nanoOsStrError(returnValue));
  }

  return returnValue;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/////////// NOTHING BELOW THIS LINE MAY CALL sendNanoOsMessageTo*!!! ///////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

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
  Coroutine *coroutine = getProcessByPid(pid);

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
    printString("ERROR!!!  Could not send message from process ");
    printInt(processId(getRunningProcess()));
    printString("\n");
    if (coroutine == NULL) {
      printString("ERROR!!!  Coroutine is NULL\n");
    } else {
      printString("ERROR!!!  Coroutine ");
      printInt(coroutineId(coroutine));
      printString(" is in state ");
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

  if (sendComessageToCoroutine(coroutine, comessage) != coroutineSuccess) {
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
  if (pid >= NANO_OS_NUM_PROCESSES) {
    // Not a valid PID.  Fail.
    printString("ERROR!!!  ");
    printInt(pid);
    printString(" is not a valid PID.\n");
    return comessage; // NULL
  }

  ProcessHandle process = getProcessByPid(pid);
  comessage
    = sendNanoOsMessageToCoroutine(process, type, func, data, waiting);
  if (comessage == NULL) {
    printString("ERROR!!!  Could not send NanoOs message to process ");
    printInt(pid);
    printString("\n");
  }
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

