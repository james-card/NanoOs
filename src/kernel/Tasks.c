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
#include "Console.h"
#include "NanoOs.h"
#include "NanoOsOverlay.h"
#include "Tasks.h"
#include "Scheduler.h"
#include "../user/NanoOsLibC.h"

// Must come last
#include "../user/NanoOsStdio.h"

/// @var messages
///
/// @brief Pointer to the array of task messages that will be stored in the
/// scheduler function's stack.
TaskMessage *messages = NULL;

/// @var nanoOsMessages
///
/// @brief Pointer to the array of NanoOsMessages that will be stored in the
/// scheduler function's stack.
NanoOsMessage *nanoOsMessages = NULL;

/// @fn ExecArgs* execArgsDestroy(ExecArgs *execArgs)
///
/// @brief Free all of an ExecArgs structure.
///
/// @param execArgs A pointer to an ExecArgs structure.
///
/// @return This function always succeeds and always returns NULL.
ExecArgs* execArgsDestroy(ExecArgs *execArgs) {
  free(execArgs->pathname);

  char **argv = execArgs->argv;
  // argv *SHOULD* never be NULL, but check just in case.
  if (argv != NULL) {
    for (int ii = 0; argv[ii] != NULL; ii++) {
      free(argv[ii]);
    }
    free(argv);
  }

  char **envp = execArgs->envp;
  if (envp != NULL) {
    for (int ii = 0; envp[ii] != NULL; ii++) {
      free(envp[ii]);
    }
    free(envp);
  }

  // We don't need to and SHOULD NOT touch execArgs->schedulerState.

  free(execArgs);
  return NULL;
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
/// @brief Wrapper task function that calls a command function.
///
/// @param args The message received from the console task that describes
///   the command to run, cast to a void*.
///
/// @return If the comamnd is run, returns the result of the command cast to a
/// void*.  If the command is not run, returns -1 cast to a void*.
void* startCommand(void *args) {
  // The scheduler may be suspended because of launching this task.
  // Immediately call taskYield as a best practice to make sure the scheduler
  // goes back to its work.
  TaskMessage *taskMessage = (TaskMessage*) args;
  if (taskMessage == NULL) {
    printString("ERROR: No arguments message provided to startCommand.\n");
    releaseConsole();
    schedulerCloseAllFileDescriptors();
    return (void*) ((intptr_t) -1);
  }
  CommandEntry *commandEntry
    = nanoOsMessageFuncPointer(taskMessage, CommandEntry*);
  CommandDescriptor *commandDescriptor
    = nanoOsMessageDataPointer(taskMessage, CommandDescriptor*);
  char *consoleInput = commandDescriptor->consoleInput;
  TaskId callingTaskId = commandDescriptor->callingTask;
  SchedulerState *schedulerState = commandDescriptor->schedulerState;
  taskYield();

  int argc = 0;
  char **argv = NULL;

  argv = parseArgs(consoleInput, &argc);
  if (argv == NULL) {
    // Fail.
    printString("ERROR: Could not parse input into argc and argv.\n");
    printString("Received consoleInput:  \"");
    printString(consoleInput);
    printString("\"\n");
    consoleInput = stringDestroy(consoleInput);
    releaseConsole();
    schedulerCloseAllFileDescriptors();
    return (void*) ((intptr_t) -1);
  }
  consoleInput = stringDestroy(consoleInput);

  bool backgroundTask = false;
  char *ampersandAt = strchr(argv[argc - 1], '&');
  if (ampersandAt != NULL) {
    ampersandAt++;
    if (ampersandAt[strspn(ampersandAt, " \t\r\n")] == '\0') {
      backgroundTask = true;
      releaseConsole();
      schedulerNotifyTaskComplete(callingTaskId);
    }
  }

  // Call the task function.
  int returnValue = commandEntry->func(argc, argv);
  free(argv); argv = NULL;

  if (callingTaskId != getRunningTaskId()) {
    // This command did NOT replace a shell task.
    releaseConsole();
    if (backgroundTask == false) {
      // The caller is still running and waiting to be told it can resume.
      // Notify it via a message.
      schedulerNotifyTaskComplete(callingTaskId);
    }
    schedulerState->allTasks[
      taskId(getRunningTask())].userId = NO_USER_ID;
  } else {
    // This is a foreground task that replaced the shell.  Just release the
    // console.
    releaseConsole();
  }

  schedulerCloseAllFileDescriptors();

  // Gracefully clear out our message queue.  We have to do this after closing
  // our file descriptors (which is a blocking call) because some other task
  // may be in the middle of sending us data and if we were to do this first,
  // it could turn around and send us more data again.
  msg_t *msg = taskMessageQueuePop();
  while (msg != NULL) {
    taskMessageSetDone(msg);
    msg = taskMessageQueuePop();
  }

  return (void*) ((intptr_t) returnValue);
}

/// @fn void* execCommand(void *args)
///
/// @brief Wrapper task function that calls a command function.
///
/// @param args The message received from the console task that describes
///   the command to run, cast to a void*.
///
/// @return If the comamnd is run, returns the result of the command cast to a
/// void*.  If the command is not run, returns -1 cast to a void*.
void* execCommand(void *args) {
  // The scheduler may be suspended because of launching this task.
  // Immediately call taskYield as a best practice to make sure the scheduler
  // goes back to its work.
  ExecArgs *execArgs = (ExecArgs*) args;
  if (execArgs == NULL) {
    printString("ERROR: No arguments message provided to execCommand.\n");
    releaseConsole();
    schedulerCloseAllFileDescriptors();
    return (void*) ((intptr_t) -1);
  }
  // Let the caller finish its work.
  taskYield();
  char *pathname = execArgs->pathname;
  char **argv = execArgs->argv;
  char **envp = execArgs->envp;
  SchedulerState *schedulerState = execArgs->schedulerState;

  if ((argv == NULL) || (argv[0] == NULL)) {
    // Fail.
    printString("ERROR: Invalid argv.\n");
    releaseConsole();
    schedulerCloseAllFileDescriptors();
    return (void*) ((intptr_t) -1);
  }
  int argc = 0;
  for (; argv[argc] != NULL; argc++);

  // Call the task function.
  int returnValue = runOverlayCommand(pathname, argc, argv, envp);

  if (execArgs->callingTaskId != getRunningTaskId()) {
    // This command did NOT replace a shell task.  Mark its slot in the
    // task table as unowned.
    schedulerState->allTasks[
      taskId(getRunningTask()) - 1].userId = NO_USER_ID;
  }

  execArgs = execArgsDestroy(execArgs);

  releaseConsole();

  schedulerCloseAllFileDescriptors();

  // Gracefully clear out our message queue.  We have to do this after closing
  // our file descriptors (which is a blocking call) because some other task
  // may be in the middle of sending us data and if we were to do this first,
  // it could turn around and send us more data again.
  msg_t *msg = taskMessageQueuePop();
  while (msg != NULL) {
    taskMessageSetDone(msg);
    msg = taskMessageQueuePop();
  }

  return (void*) ((intptr_t) returnValue);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/////////// NOTHING BELOW THIS LINE MAY CALL sendNanoOsMessageTo*: ///////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/// @fn int sendTaskMessageToTask(
///   TaskDescriptor *taskDescriptor, TaskMessage *taskMessage)
///
/// @brief Get an available TaskMessage, populate it with the specified data,
/// and push it onto a destination task's queue.
///
/// @param taskDescriptor A pointer to the destination task to send the
///   message to.
/// @param taskMessage A pointer to the message to send to the destination
///   task.
///
/// @return Returns taskSuccess on success, taskError on failure.
int sendTaskMessageToTask(
  TaskDescriptor *taskDescriptor, TaskMessage *taskMessage
) {
  int returnValue = taskSuccess;
  if ((taskDescriptor == NULL) || (taskDescriptor->taskHandle == NULL)
    || (taskMessage == NULL)
  ) {
    // Invalid.
    returnValue = taskError;
    return returnValue;
  }

  returnValue = taskMessageQueuePush(taskDescriptor, taskMessage);

  return returnValue;
}

/// @fn int sendTaskMessageToPid(unsigned int pid, TaskMessage *taskMessage)
///
/// @brief Look up a task by its PID and send a message to it.
///
/// @param pid The ID of the task to send the message to.
/// @param taskMessage A pointer to the message to send to the destination
///   task.
///
/// @return Returns taskSuccess on success, taskError on failure.
int sendTaskMessageToPid(unsigned int pid, TaskMessage *taskMessage) {
  TaskDescriptor *taskDescriptor = schedulerGetTaskByPid(pid);

  // If taskDescriptoris NULL, it will be detected as not running by
  // sendTaskMessageToTask, so there's no real point in checking for NULL
  // here.
  return sendTaskMessageToTask(taskDescriptor, taskMessage);
}

/// TaskMessage* getAvailableMessage(void)
///
/// @brief Get a message from the messages array that is not in use.
///
/// @return Returns a pointer to the available message on success, NULL if there
/// was no available message in the array.
TaskMessage* getAvailableMessage(void) {
  TaskMessage *availableMessage = NULL;

  for (int ii = 0; ii < NANO_OS_NUM_MESSAGES; ii++) {
    if (msg_in_use(&messages[ii]) == false) {
      availableMessage = &messages[ii];
      taskMessageInit(availableMessage, 0,
        &nanoOsMessages[ii], sizeof(nanoOsMessages[ii]), false);
      break;
    }
  }

  return availableMessage;
}

/// @fn TaskMessage* sendNanoOsMessageToTask(
///   TaskDescriptor *taskDescriptor, int type,
///   NanoOsMessageData func, NanoOsMessageData data, bool waiting)
///
/// @brief Send a NanoOsMessage to another task identified by its Coroutine.
///
/// @param pid The task ID of the destination task.
/// @param type The type of the message to send to the destination task.
/// @param func The function information to send to the destination task,
///   cast to a NanoOsMessageData.
/// @param data The data to send to the destination task, cast to a
///   NanoOsMessageData.
/// @param waiting Whether or not the sender is waiting on a response from the
///   destination task.
///
/// @return Returns a pointer to the sent TaskMessage on success, NULL on failure.
TaskMessage* sendNanoOsMessageToTask(
  TaskDescriptor *taskDescriptor, int type,
  NanoOsMessageData func, NanoOsMessageData data, bool waiting
) {
  TaskMessage *taskMessage = NULL;
  if (taskDescriptor == NULL) {
    return taskMessage; // NULL
  } else if (!taskRunning(taskDescriptor)) {
    // Can't send to a non-running task.
    printString("ERROR: Could not send message from task ");
    printInt(taskId(getRunningTask()));
    printString("\n");
    if (taskDescriptor->taskHandle == NULL) {
      printString("ERROR: taskHandle is NULL\n");
    } else {
      printString("ERROR: Task ");
      printInt(taskId(taskDescriptor));
      printString(" is in state ");
      printInt(taskState(taskDescriptor));
      printString("\n");
    }
    return taskMessage; // NULL
  }

  taskMessage = getAvailableMessage();
  while (taskMessage == NULL) {
    taskYield();
    taskMessage = getAvailableMessage();
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) taskMessageData(taskMessage);
  nanoOsMessage->func = func;
  nanoOsMessage->data = data;

  taskMessageInit(taskMessage, type,
    nanoOsMessage, sizeof(*nanoOsMessage), waiting);

  if (sendTaskMessageToTask(taskDescriptor, taskMessage)
    != taskSuccess
  ) {
    if (taskMessageRelease(taskMessage) != taskSuccess) {
      printString("ERROR: "
        "Could not release message from sendNanoOsMessageToTask.\n");
    }
    taskMessage = NULL;
  }

  return taskMessage;
}

/// @fn TaskMessage* sendNanoOsMessageToPid(int pid, int type,
///   NanoOsMessageData func, NanoOsMessageData data, bool waiting)
///
/// @brief Send a NanoOsMessage to another task identified by its PID. Looks
/// up the task's Coroutine by its PID and then calls
/// sendNanoOsMessageToTask.
///
/// @param pid The task ID of the destination task.
/// @param type The type of the message to send to the destination task.
/// @param func The function information to send to the destination task,
///   cast to a NanoOsMessageData.
/// @param data The data to send to the destination task, cast to a
///   NanoOsMessageData.
/// @param waiting Whether or not the sender is waiting on a response from the
///   destination task.
///
/// @return Returns a pointer to the sent TaskMessage on success, NULL on failure.
TaskMessage* sendNanoOsMessageToPid(int pid, int type,
  NanoOsMessageData func, NanoOsMessageData data, bool waiting
) {
  TaskMessage *taskMessage = NULL;
  if (pid >= NANO_OS_NUM_TASKS) {
    // Not a valid PID.  Fail.
    printString("ERROR: ");
    printInt(pid);
    printString(" is not a valid PID.\n");
    return taskMessage; // NULL
  }

  TaskDescriptor *task = schedulerGetTaskByPid(pid);
  taskMessage
    = sendNanoOsMessageToTask(task, type, func, data, waiting);
  if (taskMessage == NULL) {
    printString("ERROR: Could not send NanoOs message to task ");
    printInt(pid);
    printString("\n");
  }
  return taskMessage;
}

/// @fn void* waitForDataMessage(
///   TaskMessage *sent, int type, const struct timespec *ts)
///
/// @brief Wait for a reply to a previously-sent message and get the data from
/// it.  The provided message will be released when the reply is received.
///
/// @param sent A pointer to a previously-sent TaskMessage the calling function is
///   waiting on a reply to.
/// @param type The type of message expected to be sent as a response.
/// @param ts A pointer to a struct timespec with the future time at which to
///   timeout if nothing is received by then.  If this parameter is NULL, an
///   infinite timeout will be used.
///
/// @return Returns a pointer to the data member of the received message on
/// success, NULL on failure.
void* waitForDataMessage(TaskMessage *sent, int type, const struct timespec *ts) {
  void *returnValue = NULL;

  TaskMessage *incoming = taskMessageWaitForReplyWithType(sent, true, type, ts);
  if (incoming != NULL)  {
    returnValue = nanoOsMessageDataPointer(incoming, void*);
    if (taskMessageRelease(incoming) != taskSuccess) {
      printString("ERROR: "
        "Could not release incoming message from waitForDataMessage.\n");
    }
  }

  return returnValue;
}

