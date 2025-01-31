////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2025 James Card                     //
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
#include "Scheduler.h"

/// @var messages
///
/// @brief Pointer to the array of process messages that will be stored in the
/// scheduler function's stack.
ProcessMessage *messages = NULL;

/// @var nanoOsMessages
///
/// @brief Pointer to the array of NanoOsMessages that will be stored in the
/// scheduler function's stack.
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
/// @brief Wrapper process function that calls a command function.
///
/// @param args The message received from the console process that describes
///   the command to run, cast to a void*.
///
/// @return If the comamnd is run, returns the result of the command cast to a
/// void*.  If the command is not run, returns -1 cast to a void*.
void* startCommand(void *args) {
  // The scheduler may be suspended because of launching this process.
  // Immediately call processYield as a best practice to make sure the scheduler
  // goes back to its work.
  ProcessMessage *processMessage = (ProcessMessage*) args;
  if (processMessage == NULL) {
    printString("ERROR:  No arguments message provided to startCommand.\n");
    releaseConsole();
    schedulerCloseAllFileDescriptors();
    return (void*) ((intptr_t) -1);
  }
  CommandEntry *commandEntry
    = nanoOsMessageFuncPointer(processMessage, CommandEntry*);
  CommandDescriptor *commandDescriptor
    = nanoOsMessageDataPointer(processMessage, CommandDescriptor*);
  char *consoleInput = commandDescriptor->consoleInput;
  ProcessId callingProcessId = commandDescriptor->callingProcess;
  SchedulerState *schedulerState = commandDescriptor->schedulerState;
  processYield();

  int argc = 0;
  char **argv = NULL;

  argv = parseArgs(consoleInput, &argc);
  if (argv == NULL) {
    // Fail.
    printString("ERROR:  Could not parse input into argc and argv.\n");
    printString("Received consoleInput:  \"");
    printString(consoleInput);
    printString("\"\n");
    consoleInput = stringDestroy(consoleInput);
    releaseConsole();
    schedulerCloseAllFileDescriptors();
    return (void*) ((intptr_t) -1);
  }
  consoleInput = stringDestroy(consoleInput);

  bool backgroundProcess = false;
  char *ampersandAt = strchr(argv[argc - 1], '&');
  if (ampersandAt != NULL) {
    ampersandAt++;
    if (ampersandAt[strspn(ampersandAt, " \t\r\n")] == '\0') {
      backgroundProcess = true;
      releaseConsole();
      schedulerNotifyProcessComplete(callingProcessId);
    }
  }

  // Call the process function.
  int returnValue = commandEntry->func(argc, argv);
  free(argv); argv = NULL;

  if (callingProcessId != getRunningProcessId()) {
    // This command did NOT replace a shell process.
    releaseConsole();
    if (backgroundProcess == false) {
      // The caller is still running and waiting to be told it can resume.
      // Notify it via a message.
      schedulerNotifyProcessComplete(callingProcessId);
    }
    schedulerState->allProcesses[
      processId(getRunningProcess())].userId = NO_USER_ID;
  } else {
    // This is a foreground process that replaced the shell.  Just release the
    // console.
    releaseConsole();
  }

  schedulerCloseAllFileDescriptors();
  return (void*) ((intptr_t) returnValue);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/////////// NOTHING BELOW THIS LINE MAY CALL sendNanoOsMessageTo*!!! ///////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/// @fn int sendProcessMessageToProcess(
///   ProcessHandle processHandle, ProcessMessage *processMessage)
///
/// @brief Get an available ProcessMessage, populate it with the specified data,
/// and push it onto a destination process's queue.
///
/// @param processHandle A handle to the destination process to send the message
///   to.
/// @param processMessage A pointer to the message to send to the destination
///   process.
///
/// @return Returns processSuccess on success, processError on failure.
int sendProcessMessageToProcess(
  ProcessHandle processHandle, ProcessMessage *processMessage
) {
  int returnValue = processSuccess;
  if ((processHandle == NULL) || (processMessage == NULL)) {
    // Invalid.
    returnValue = processError;
    return returnValue;
  }

  returnValue = processMessageQueuePush(processHandle, processMessage);

  return returnValue;
}

/// @fn int sendProcessMessageToPid(unsigned int pid, ProcessMessage *processMessage)
///
/// @brief Look up a process by its PID and send a message to it.
///
/// @param pid The ID of the process to send the message to.
/// @param processMessage A pointer to the message to send to the destination
///   process.
///
/// @return Returns processSuccess on success, processError on failure.
int sendProcessMessageToPid(unsigned int pid, ProcessMessage *processMessage) {
  ProcessHandle processHandle = schedulerGetProcessByPid(pid);

  // If processHandle is NULL, it will be detected as not running by
  // sendProcessMessageToProcess, so there's no real point in checking for NULL
  // here.
  return sendProcessMessageToProcess(processHandle, processMessage);
}

/// ProcessMessage* getAvailableMessage(void)
///
/// @brief Get a message from the messages array that is not in use.
///
/// @return Returns a pointer to the available message on success, NULL if there
/// was no available message in the array.
ProcessMessage* getAvailableMessage(void) {
  ProcessMessage *availableMessage = NULL;

  for (int ii = 0; ii < NANO_OS_NUM_MESSAGES; ii++) {
    if (messages[ii].inUse == false) {
      availableMessage = &messages[ii];
      processMessageInit(availableMessage, 0,
        &nanoOsMessages[ii], sizeof(nanoOsMessages[ii]), false);
      break;
    }
  }

  return availableMessage;
}

/// @fn ProcessMessage* sendNanoOsMessageToProcess(
///   ProcessHandle processHandle, int type,
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
/// @return Returns a pointer to the sent ProcessMessage on success, NULL on failure.
ProcessMessage* sendNanoOsMessageToProcess(
  ProcessHandle processHandle, int type,
  NanoOsMessageData func, NanoOsMessageData data, bool waiting
) {
  ProcessMessage *processMessage = NULL;
  if (!processRunning(processHandle)) {
    // Can't send to a non-running process.
    printString("ERROR!!!  Could not send message from process ");
    printInt(processId(getRunningProcess()));
    printString("\n");
    if (processHandle == NULL) {
      printString("ERROR!!!  processHandle is NULL\n");
    } else {
      printString("ERROR!!!  Process ");
      printInt(processId(processHandle));
      printString(" is in state ");
      printInt(processState(processHandle));
      printString("\n");
    }
    return processMessage; // NULL
  }

  processMessage = getAvailableMessage();
  while (processMessage == NULL) {
    processYield();
    processMessage = getAvailableMessage();
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->func = func;
  nanoOsMessage->data = data;

  processMessageInit(processMessage, type,
    nanoOsMessage, sizeof(*nanoOsMessage), waiting);

  if (sendProcessMessageToProcess(processHandle, processMessage)
    != processSuccess
  ) {
    if (processMessageRelease(processMessage) != processSuccess) {
      printString("ERROR!!!  "
        "Could not release message from sendNanoOsMessageToProcess.\n");
    }
    processMessage = NULL;
  }

  return processMessage;
}

/// @fn ProcessMessage* sendNanoOsMessageToPid(int pid, int type,
///   NanoOsMessageData func, NanoOsMessageData data, bool waiting)
///
/// @brief Send a NanoOsMessage to another process identified by its PID. Looks
/// up the process's Coroutine by its PID and then calls
/// sendNanoOsMessageToProcess.
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
/// @return Returns a pointer to the sent ProcessMessage on success, NULL on failure.
ProcessMessage* sendNanoOsMessageToPid(int pid, int type,
  NanoOsMessageData func, NanoOsMessageData data, bool waiting
) {
  ProcessMessage *processMessage = NULL;
  if (pid >= NANO_OS_NUM_PROCESSES) {
    // Not a valid PID.  Fail.
    printString("ERROR!!!  ");
    printInt(pid);
    printString(" is not a valid PID.\n");
    return processMessage; // NULL
  }

  ProcessHandle process = schedulerGetProcessByPid(pid);
  processMessage
    = sendNanoOsMessageToProcess(process, type, func, data, waiting);
  if (processMessage == NULL) {
    printString("ERROR!!!  Could not send NanoOs message to process ");
    printInt(pid);
    printString("\n");
  }
  return processMessage;
}

/// @fn void* waitForDataMessage(
///   ProcessMessage *sent, int type, const struct timespec *ts)
///
/// @brief Wait for a reply to a previously-sent message and get the data from
/// it.  The provided message will be released when the reply is received.
///
/// @param sent A pointer to a previously-sent ProcessMessage the calling function is
///   waiting on a reply to.
/// @param type The type of message expected to be sent as a response.
/// @param ts A pointer to a struct timespec with the future time at which to
///   timeout if nothing is received by then.  If this parameter is NULL, an
///   infinite timeout will be used.
///
/// @return Returns a pointer to the data member of the received message on
/// success, NULL on failure.
void* waitForDataMessage(ProcessMessage *sent, int type, const struct timespec *ts) {
  void *returnValue = NULL;

  ProcessMessage *incoming = processMessageWaitForReplyWithType(sent, true, type, ts);
  if (incoming != NULL)  {
    returnValue = nanoOsMessageDataPointer(incoming, void*);
    if (processMessageRelease(incoming) != processSuccess) {
      printString("ERROR!!!  "
        "Could not release incoming message from waitForDataMessage.\n");
    }
  }

  return returnValue;
}

