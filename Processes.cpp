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

/// @def SERIAL_PORT_SHELL_PID
///
/// @brief The process ID (PID) of the first user process, i.e. the first ID
/// after the last system process ID.
#define SERIAL_PORT_SHELL_PID 3

/// @struct CommandDescriptor
///
/// @brief Container of information for launching a process.
///
/// @param consolePort The index of the ConsolePort the input came from.
/// @param consoleInput The input as provided by the console.
/// @param callingProcess The process ID of the process that is launching the
///   command.
typedef struct CommandDescriptor {
  int                consolePort;
  char              *consoleInput;
  COROUTINE_ID_TYPE  callingProcess;
} CommandDescriptor;

/// @var mainCoroutine
///
/// @brief Pointer to the main coroutine that's allocated in the main loop
/// function.
Coroutine *mainCoroutine = NULL;

/// @var runningProcesses
///
/// @brief Pointer to the array of running commands that will be stored in the
/// main loop function's stack.
RunningProcess *runningProcesses = NULL;

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

  if (commandEntry->shellCommand == false) {
    if (backgroundProcess == false) {
    // The caller is still running and waiting to be told it can resume.  Notify
    // it via a message.
    sendNanoOsMessageToPid(commandDescriptor->callingProcess,
      SCHEDULER_PROCESS_COMPLETE, 0, 0, false);
    }
    runningProcesses[coroutineId(NULL)].userId = NO_USER_ID;
  }

  free(consoleInput); consoleInput = NULL;
  free(commandDescriptor); commandDescriptor = NULL;
  free(argv); argv = NULL;
  releaseConsole();
  if (comessageRelease(comessage) != coroutineSuccess) {
    printString("ERROR!!!  "
      "Could not release message from handleSchedulerMessage "
      "for invalid message type.\n");
  }

  // We need to clear the coroutine pointer.
  runningProcesses[coroutineId(NULL)].coroutine = NULL;

  return (void*) ((intptr_t) returnValue);
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
  return NULL;
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
  uint8_t numRunningProcesses = 0;

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

  numRunningProcesses = nanoOsMessageDataValue(comessage, COROUTINE_ID_TYPE);
  if (numRunningProcesses == 0) {
    printf("ERROR:  Number of running processes returned from the "
      "scheduler is 0.\n");
    goto releaseMessage;
  }

  // We need numRunningProcesses rows.
  processInfo = (ProcessInfo*) malloc(sizeof(ProcessInfo)
    + ((numRunningProcesses - 1) * sizeof(ProcessInfoElement)));
  if (processInfo == NULL) {
    printf(
      "ERROR:  Could not allocate memory for processInfo in getProcessInfo.\n");
  }

  // It is possible, although unlikely, that an additional process is started
  // between the time we made the call above and the time that our message gets
  // handled below.  We allocated our return value based upon the size that was
  // returned above and, if we're not careful, it will be possible to overflow
  // the array.  Initialize processInfo->numProcesses so that
  // handleGetProcessInfo knows the maximum number of ProcessInfoElements it can
  // populated.
  processInfo->numProcesses = numRunningProcesses;
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
  if ((processId < NANO_OS_FIRST_PROCESS_ID)
    || (processId >= NANO_OS_NUM_PROCESSES)
    || (coroutineResumable(runningProcesses[processId].coroutine) == false)
  ) {
    printString("ERROR:  Invalid process ID.\n");
    return 1;
  }

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
  struct timespec ts = { 0, 100000000 };
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
  commandDescriptor->callingProcess = coroutineId(NULL);

  if (sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_RUN_PROCESS,
    (NanoOsMessageData) commandEntry, (NanoOsMessageData) commandDescriptor,
    true) == NULL
  ) {
    printString("ERROR!!!  Could not communicate with scheduler.\n");
    return returnValue; // 1
  }

  Comessage *doneMessage
    = comessageQueueWaitForType(SCHEDULER_PROCESS_COMPLETE, NULL);
  // We don't need any data from the message.  Just release it.
  comessageRelease(doneMessage);

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

/// @fn Coroutine* getCoroutineByPid(unsigned int pid)
///
/// @brief Look up a croutine for a running command given its process ID.
///
/// @param pid The integer ID for the process.
///
/// @return Returns the found coroutine pointer on success, NULL on failure.
Coroutine* getCoroutineByPid(unsigned int pid) {
  if (pid >= NANO_OS_NUM_PROCESSES) {
    // Not a valid PID.  Fail.
    return NULL;
  }

  return runningProcesses[pid].coroutine;
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

  Coroutine *coroutine = runningProcesses[pid].coroutine;
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

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
///////////// SCHEDULER-PRIVATE FUNCTIONS ONLY BELOW THIS POINT!! //////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/// @fn int schedulerSendComessageToCoroutine(
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
int schedulerSendComessageToCoroutine(
  Coroutine *coroutine, Comessage *comessage
) {
  int returnValue = coroutineSuccess;
  if (coroutine == NULL) {
    printString(
      "ERROR:  Attempt to send scheduler comessage to NULL coroutine.\n");
    returnValue = coroutineError;
    return returnValue;
  } else if (comessage == NULL) {
    printString(
      "ERROR:  Attempt to send NULL scheduler comessage to coroutine.\n");
    returnValue = coroutineError;
    return returnValue;
  }
  // comessage->from would normally be set when we do a comessageQueuePush.
  // We're not using that mechanism here, so we have to do it manually.  If we
  // don't do this, then commands that validate that the message came from the
  // scheduler will fail.
  comessage->from = runningProcesses[NANO_OS_SCHEDULER_PROCESS_ID].coroutine;

  coroutineResume(coroutine, comessage);
  if (comessageDone(comessage) != true) {
    // This is our only indication from the called coroutine that something went
    // wrong.  Return an error status here.
    printString("ERROR:  Called coroutine did not mark sent message done.\n");
    returnValue = coroutineError;
  }

  return returnValue;
}

/// @fn int schedulerSendComessageToPid(unsigned int pid, Comessage *comessage)
///
/// @brief Look up a coroutine by its PID and send a message to it.
///
/// @param pid The ID of the process to send the message to.
/// @param comessage A pointer to the message to send to the destination
///   process.
///
/// @return Returns coroutineSuccess on success, coroutineError on failure.
int schedulerSendComessageToPid(unsigned int pid, Comessage *comessage) {
  Coroutine *coroutine = getCoroutineByPid(pid);
  // If coroutine is NULL, it will be detected as not running by
  // schedulerSendComessageToCoroutine, so there's no real point in checking for
  // NULL here.
  return schedulerSendComessageToCoroutine(coroutine, comessage);
}

/// @fn int schedulerSendNanoOsMessageToCoroutine(
///   Coroutine *coroutine, int type,
///   NanoOsMessageData func, NanoOsMessageData data)
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
/// @return Returns coroutineSuccess on success, a different coroutine status
/// on failure.
int schedulerSendNanoOsMessageToCoroutine(Coroutine *coroutine, int type,
  NanoOsMessageData func, NanoOsMessageData data
) {
  Comessage comessage;
  memset(&comessage, 0, sizeof(comessage));
  NanoOsMessage nanoOsMessage;

  nanoOsMessage.func = func;
  nanoOsMessage.data = data;

  // These messages are always waiting for done from the caller, so hardcode
  // the waiting parameter to true here.
  comessageInit(&comessage, type, &nanoOsMessage, sizeof(nanoOsMessage), true);

  int returnValue = schedulerSendComessageToCoroutine(coroutine, &comessage);

  return returnValue;
}

/// @fn int schedulerSendNanoOsMessageToPid(int pid, int type,
///   NanoOsMessageData func, NanoOsMessageData data)
///
/// @brief Send a NanoOsMessage to another process identified by its PID. Looks
/// up the process's Coroutine by its PID and then calls
/// schedulerSendNanoOsMessageToCoroutine.
///
/// @param pid The process ID of the destination process.
/// @param type The type of the message to send to the destination process.
/// @param func The function information to send to the destination process,
///   cast to a NanoOsMessageData.
/// @param data The data to send to the destination process, cast to a
///   NanoOsMessageData.
///
/// @return Returns coroutineSuccess on success, a different coroutine status
/// on failure.
int schedulerSendNanoOsMessageToPid(int pid, int type,
  NanoOsMessageData func, NanoOsMessageData data
) {
  int returnValue = coroutineError;
  if (pid >= NANO_OS_NUM_PROCESSES) {
    // Not a valid PID.  Fail.
    printString("ERROR!!!  ");
    printInt(pid);
    printString(" is not a valid PID.\n");
    return returnValue; // coroutineError
  }

  Coroutine *coroutine = runningProcesses[pid].coroutine;
  returnValue = schedulerSendNanoOsMessageToCoroutine(
    coroutine, type, func, data);
  return returnValue;
}

/// @fn int schedulerAssignPortToPid(
///   uint8_t consolePort, COROUTINE_ID_TYPE owner)
///
/// @brief Assign a console port to a process ID.
///
/// @param consolePort The ID of the consolePort to assign.
/// @param owner The ID of the process to assign the port to.
///
/// @return Returns coroutineSuccess on success, coroutineError on failure.
int schedulerAssignPortToPid(uint8_t consolePort, COROUTINE_ID_TYPE owner) {
  ConsolePortPidUnion consolePortPidUnion;
  consolePortPidUnion.consolePortPidAssociation.consolePort
    = consolePort;
  consolePortPidUnion.consolePortPidAssociation.processId = owner;

  int returnValue = schedulerSendNanoOsMessageToPid(
    NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_ASSIGN_PORT,
    /* func= */ 0, consolePortPidUnion.nanoOsMessageData);

  return returnValue;
}

/// @fn int schedulerSetPortShell(
///   uint8_t consolePort, COROUTINE_ID_TYPE shell)
///
/// @brief Assign a console port to a process ID.
///
/// @param consolePort The ID of the consolePort to set the shell for.
/// @param shell The ID of the shell process for the port.
///
/// @return Returns coroutineSuccess on success, coroutineError on failure.
int schedulerSetPortShell(uint8_t consolePort, COROUTINE_ID_TYPE shell) {
  int returnValue = coroutineError;

  if (shell >= NANO_OS_NUM_PROCESSES) {
    printString("ERROR:  schedulerSetPortShell called with invalid shell PID ");
    printInt(shell);
    printString("\n");
    return returnValue; // coroutineError
  }

  ConsolePortPidUnion consolePortPidUnion;
  consolePortPidUnion.consolePortPidAssociation.consolePort
    = consolePort;
  consolePortPidUnion.consolePortPidAssociation.processId = shell;

  returnValue = schedulerSendNanoOsMessageToPid(
    NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_SET_PORT_SHELL,
    /* func= */ 0, consolePortPidUnion.nanoOsMessageData);

  return returnValue;
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
  CommandDescriptor *commandDescriptor
    = nanoOsMessageDataPointer(comessage, CommandDescriptor*);
  char *consoleInput = commandDescriptor->consoleInput;
  int consolePort = commandDescriptor->consolePort;
  Coroutine *caller = comessageFrom(comessage);
  COROUTINE_ID_TYPE callingProcessId = coroutineId(caller);
  
  // Find an open slot.
  COROUTINE_ID_TYPE processId = NANO_OS_FIRST_PROCESS_ID;
  if (commandEntry->shellCommand == true) {
    // Not the normal case but the priority case, so handle it first.  We're
    // going to kill the caller and reuse its process slot.
    processId = callingProcessId;

    // Protect the relevant memory from deletion below.
    if (assignMemory(consoleInput, NANO_OS_SCHEDULER_PROCESS_ID) != 0) {
      printString(
        "WARNING:  Could not protect console input from deletion.\n");
      printString("Undefined behavior.\n");
    }
    if (assignMemory(commandDescriptor, NANO_OS_SCHEDULER_PROCESS_ID) != 0) {
      printString(
        "WARNING:  Could not protect command descriptor from deletion.\n");
      printString("Undefined behavior.\n");
    }

    // Kill and clear out the calling process.
    coroutineTerminate(caller, NULL);
    runningProcesses[processId].coroutine = NULL;
    runningProcesses[processId].name = NULL;

    // We don't want to wait for the memory manager to release the memory.  Make
    // it do it immediately.  We need to do this before we kill the process.
    if (schedulerSendNanoOsMessageToPid(NANO_OS_MEMORY_MANAGER_PROCESS_ID,
      MEMORY_MANAGER_FREE_PROCESS_MEMORY, /* func= */ 0, processId)
    ) {
      printString("WARNING:  Could not release memory for process ");
      printInt(processId);
      printString("\n");
      printString("Memory leak.\n");
    }
  } else {
    for (; processId < NANO_OS_NUM_PROCESSES; processId++) {
      Coroutine *coroutine = runningProcesses[processId].coroutine;
      if ((coroutine == NULL) || (coroutineFinished(coroutine))) {
        break;
      }
    }
  }

  if (processId < NANO_OS_NUM_PROCESSES) {
    runningProcesses[processId].userId
      = runningProcesses[callingProcessId].userId;

    Coroutine *coroutine = coroutineCreate(startCommand);
    coroutineSetId(coroutine, processId);
    if (assignMemory(consoleInput, processId) != 0) {
      printString(
        "WARNING:  Could not assign console input to new process.\n");
      printString("Memory leak.\n");
    }
    if (assignMemory(commandDescriptor, processId) != 0) {
      printString(
        "WARNING:  Could not assign command descriptor to new process.\n");
      printString("Memory leak.\n");
    }

    runningProcesses[processId].coroutine = coroutine;
    runningProcesses[processId].name = commandEntry->name;

    coroutineResume(coroutine, comessage);
    returnValue = 0;

    if (schedulerAssignPortToPid(consolePort, processId) != coroutineSuccess) {
      printString("WARNING:  Could not assign console port to process.\n");
    }
  } else {
    if (commandEntry->shellCommand == true) {
      // Don't call stringDestroy with consoleInput because we're going to try
      // this command again in a bit.
      returnValue = EBUSY;
    } else {
      // This is a user process, not a system process, so the user is just out
      // of luck.  *DO NOT* set returnValue to a non-zero value here as that
      // would result in an infinite loop.
      //
      // printf sends synchronous messages to the console, which we can't do.
      // Use the non-blocking printString instead.
      printString("Out of process slots to launch process.\n");
      sendNanoOsMessageToPid(commandDescriptor->callingProcess,
        SCHEDULER_PROCESS_COMPLETE, 0, 0, false);
      consoleInput = stringDestroy(consoleInput);
      free(commandDescriptor); commandDescriptor = NULL;
      if (comessageRelease(comessage) != coroutineSuccess) {
        printString("ERROR!!!  "
          "Could not release message from handleSchedulerMessage "
          "for invalid message type.\n");
      }
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

  UserId callingUserId
    = runningProcesses[coroutineId(comessageFrom(comessage))].userId;
  COROUTINE_ID_TYPE processId
    = nanoOsMessageDataValue(comessage, COROUTINE_ID_TYPE);
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);

  if ((processId >= NANO_OS_FIRST_PROCESS_ID)
    && (processId < NANO_OS_NUM_PROCESSES)
    && (coroutineResumable(runningProcesses[processId].coroutine))
  ) {
    if ((runningProcesses[processId].userId == callingUserId)
      || (callingUserId == ROOT_USER_ID)
    ) {
      if (coroutineTerminate(runningProcesses[processId].coroutine, NULL)
        == coroutineSuccess
      ) {
        runningProcesses[processId].coroutine = NULL;
        runningProcesses[processId].name = NULL;

        // Forward the message on to the memory manager to have it clean up the
        // process's memory.  *DO NOT* mark the message as done.  The memory
        // manager will do that.
        comessageInit(comessage, MEMORY_MANAGER_FREE_PROCESS_MEMORY,
          nanoOsMessage, sizeof(*nanoOsMessage), /* waiting= */ false);
        memoryManagerCast(comessage);
      } else {
        // Tell the caller that we've failed.
        nanoOsMessage->data = 1;
        if (comessageSetDone(comessage) != coroutineSuccess) {
          printString("ERROR!!!  "
            "Could not mark message done in handleKillProcess.\n");
        }
      }
    } else {
      // Tell the caller that we've failed.
      nanoOsMessage->data = EACCES; // Permission denied
      if (comessageSetDone(comessage) != coroutineSuccess) {
        printString("ERROR!!!  "
          "Could not mark message done in handleKillProcess.\n");
      }
    }
  } else {
    // Tell the caller that we've failed.
    nanoOsMessage->data = EINVAL; // Invalid argument
    if (comessageSetDone(comessage) != coroutineSuccess) {
      printString("ERROR!!!  "
        "Could not mark message done in handleKillProcess.\n");
    }
  }

  // DO NOT release the message since that's done by the caller.

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
  for (int ii = 0; ii < NANO_OS_NUM_PROCESSES; ii++) {
    if (coroutineRunning(runningProcesses[ii].coroutine)) {
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
  int maxProcesses = processInfo->numProcesses;
  ProcessInfoElement *processes = processInfo->processes;
  int idx = 0;
  for (int ii = 0; (ii < NANO_OS_NUM_PROCESSES) && (idx < maxProcesses); ii++) {
    if (coroutineResumable(runningProcesses[ii].coroutine)) {
      processes[idx].pid = (int) coroutineId(runningProcesses[ii].coroutine);
      processes[idx].name = runningProcesses[ii].name;
      processes[idx].userId = runningProcesses[ii].userId;
      idx++;
    }
  }

  // It's possible that a process completed between the time that processInfo
  // was allocated and now, so set the value of numProcesses to the value of
  // idx.
  processInfo->numProcesses = idx;

  comessageSetDone(comessage);

  // DO NOT release the message since the caller is waiting on the response.

  return returnValue;
}

/// @fn int handleGetProcessUser(Comessage *comessage)
///
/// @brief Get the number of processes that are currently running in the system.
///
/// @param comessage A pointer to the Comessage that was received.  This will be
///   reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int handleGetProcessUser(Comessage *comessage) {
  int returnValue = 0;
  COROUTINE_ID_TYPE processId = coroutineId(comessageFrom(comessage));
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);
  if (processId < NANO_OS_NUM_PROCESSES) {
    nanoOsMessage->data = runningProcesses[processId].userId;
  } else {
    nanoOsMessage->data = -1;
  }

  comessageSetDone(comessage);

  // DO NOT release the message since the caller is waiting on the response.

  return returnValue;
}

/// @fn int handleSetProcessUser(Comessage *comessage)
///
/// @brief Get the number of processes that are currently running in the system.
///
/// @param comessage A pointer to the Comessage that was received.  This will be
///   reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int handleSetProcessUser(Comessage *comessage) {
  int returnValue = 0;
  COROUTINE_ID_TYPE processId = coroutineId(comessageFrom(comessage));
  UserId userId = nanoOsMessageDataValue(comessage, UserId);
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);
  nanoOsMessage->data = -1;

  if (processId < NANO_OS_NUM_PROCESSES) {
    if ((runningProcesses[processId].userId == -1)
      || (userId == -1)
    ) {
      runningProcesses[processId].userId = userId;
      nanoOsMessage->data = 0;
    } else {
      nanoOsMessage->data = EACCES;
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
  handleGetProcessUser,
  handleSetProcessUser,
};

/// @fn void handleSchedulerMessage(void)
///
/// @brief Handle one (and only one) message from our message queue.  If
/// handling the message is unsuccessful, the message will be returned to the
/// end of our message queue.
///
/// @return This function returns no value.
void handleSchedulerMessage(void) {
  static int lastReturnValue = 0;
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
      if (lastReturnValue == 0) {
        // Only print out a message if this is the first time we've failed.
        printString("Scheduler command handler failed.\n");
        printString("Pushing message back onto our own queue.\n");
      }
      comessageQueuePush(NULL, message);
    }
    lastReturnValue = returnValue;
  }

  return;
}

/// @fn void startScheduler(void)
///
/// @brief Initialize and run the round-robin scheduler.
///
/// @return This function returns no value and, in fact, never returns at all.
__attribute__((noinline)) void startScheduler(void) {
  // Create the storage for the array of running commands and initialize the
  // array global pointer.
  RunningProcess runningProcessesStorage[NANO_OS_NUM_PROCESSES] = {};
  runningProcesses = runningProcessesStorage;

  // Create the storage for the array of Comessages and NanoOsMessages and
  // initialize the global array pointers and the back-pointers for the
  // NanoOsMessges.
  Comessage messagesStorage[NANO_OS_NUM_MESSAGES] = {};
  messages = messagesStorage;
  for (int ii = 0; ii < NANO_OS_NUM_MESSAGES; ii++) {
    // messages[ii].data will be initialized by getAvailableMessage.
    nanoOsMessages[ii].comessage = &messages[ii];
  }

  // Initialize ourself in the array of running commands.
  coroutineSetId(mainCoroutine, NANO_OS_SCHEDULER_PROCESS_ID);
  runningProcesses[NANO_OS_SCHEDULER_PROCESS_ID].coroutine = mainCoroutine;
  runningProcesses[NANO_OS_SCHEDULER_PROCESS_ID].name = "scheduler";
  runningProcesses[NANO_OS_SCHEDULER_PROCESS_ID].userId = ROOT_USER_ID;

  // Create the console process and start it.
  Coroutine *coroutine = NULL;
  coroutine = coroutineCreate(runConsole);
  coroutineSetId(coroutine, NANO_OS_CONSOLE_PROCESS_ID);
  runningProcesses[NANO_OS_CONSOLE_PROCESS_ID].coroutine = coroutine;
  runningProcesses[NANO_OS_CONSOLE_PROCESS_ID].name = "console";
  runningProcesses[NANO_OS_CONSOLE_PROCESS_ID].userId = ROOT_USER_ID;
  coroutineResume(coroutine, NULL);

  printString("\n");
  printString("Main stack size = ");
  printInt(ABS_DIFF(
    ((intptr_t) runningProcessesStorage),
    ((intptr_t) coroutine)
  ));
  printString(" bytes\n");

  // We need to do an initial population of all the commands because we need to
  // get to the end of memory to run the memory manager in whatever is left
  // over.
  for (int ii = NANO_OS_FIRST_PROCESS_ID; ii < NANO_OS_NUM_PROCESSES; ii++) {
    coroutine = coroutineCreate(dummyProcess);
    coroutineSetId(coroutine, ii);
    runningProcesses[ii].coroutine = coroutine;
  }
  printString("Coroutine stack size = ");
  printInt(ABS_DIFF(
    ((intptr_t) runningProcesses[NANO_OS_FIRST_PROCESS_ID].coroutine),
    ((intptr_t) runningProcesses[NANO_OS_FIRST_PROCESS_ID + 1].coroutine)
  ));
  printString(" bytes\n");

  // Create the memory manager process.  !!! THIS MUST BE THE LAST PROCESS
  // CREATED BECAUSE WE WANT TO USE THE ENTIRE REST OF MEMORY FOR IT !!!
  coroutine = coroutineCreate(runMemoryManager);
  coroutineSetId(coroutine, NANO_OS_MEMORY_MANAGER_PROCESS_ID);
  runningProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].coroutine = coroutine;
  runningProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].name = "memory manager";
  runningProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].userId = ROOT_USER_ID;
  // Assign the console ports to it.
  for (uint8_t ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (schedulerAssignPortToPid(ii, NANO_OS_MEMORY_MANAGER_PROCESS_ID)
      != coroutineSuccess
    ) {
      printString(
        "WARNING:  Could not assign console port to memory manager.\n");
    }
  }
  coroutineResume(coroutine, NULL);

  // Set the shells for the ports.
  if (schedulerSetPortShell(CONSOLE_SERIAL_PORT, SERIAL_PORT_SHELL_PID)
    != coroutineSuccess
  ) {
    printString("WARNING:  Could not set shell for serial port.\n");
    printString("          Undefined behavior will result.\n");
  }

  // Clear out all the dummy processes now.
  for (int ii = NANO_OS_FIRST_PROCESS_ID; ii < NANO_OS_NUM_PROCESSES; ii++) {
    coroutineResume(runningProcesses[ii].coroutine, NULL);
    runningProcesses[ii].coroutine = NULL;
    runningProcesses[ii].userId = NO_USER_ID;
  }

  // We're going to do a round-robin scheduler.  We don't want to use the array
  // of runningProcesses because the scheduler process itself is in that list.
  // Instead, create an array of double-pointers to the coroutines that we'll
  // want to run in that array.
  Coroutine **scheduledCoroutines[NANO_OS_NUM_PROCESSES - 1];
  const int numScheduledCoroutines = NANO_OS_NUM_PROCESSES - 1;
  for (int ii = 0; ii < numScheduledCoroutines; ii++) {
    scheduledCoroutines[ii] = &runningProcesses[ii + 1].coroutine;
  }

  // Start our round-robin scheduler.
  int coroutineIndex = 0;
  const int serialPortShellCoroutineIndex = SERIAL_PORT_SHELL_PID - 1;
  while (1) {
    coroutine = *scheduledCoroutines[coroutineIndex];
    coroutineResume(coroutine, NULL);
    if ((coroutineIndex == serialPortShellCoroutineIndex)
      && (coroutineRunning(coroutine) == false)
    ) {
      // Restart the shell.
      coroutine = coroutineCreate(runShell);
      coroutineSetId(coroutine, SERIAL_PORT_SHELL_PID);
      runningProcesses[SERIAL_PORT_SHELL_PID].coroutine = coroutine;
      runningProcesses[SERIAL_PORT_SHELL_PID].name = "shell";
    }
    handleSchedulerMessage();
    coroutineIndex++;
    coroutineIndex %= numScheduledCoroutines;
  }
}

