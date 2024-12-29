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

// Coroutines support

/// @def USB_SERIAL_PORT_SHELL_PID
///
/// @brief The process ID (PID) of the USB serial port shell.
#define USB_SERIAL_PORT_SHELL_PID 3

/// @def GPIO_SERIAL_PORT_SHELL_PID
///
/// @brief The process ID (PID) of the GPIO serial port shell.
#define GPIO_SERIAL_PORT_SHELL_PID 4

/// @var schedulerProcess
///
/// @brief Pointer to the main process object that's allocated in the main loop
/// function.
ProcessHandle schedulerProcess = NULL;

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

/// @var allProcesses
///
/// @brief Pointer to the allProcesses array that is part of the
/// SchedulerState object maintained by the scheduler process.  This is needed
/// in order to do lookups from process IDs to process object pointers.
static ProcessDescriptor *allProcesses = NULL;

/// @fn int processQueuePush(
///   ProcessQueue *processQueue, ProcessDescriptor *processDescriptor)
///
/// @brief Push a pointer to a ProcessDescriptor onto a ProcessQueue.
///
/// @param processQueue A pointer to a ProcessQueue to push the pointer to.
/// @param processDescriptor A pointer to a ProcessDescriptor to push onto the
///   queue.
///
/// @return Returns 0 on success, ENOMEM on failure.
int processQueuePush(
  ProcessQueue *processQueue, ProcessDescriptor *processDescriptor
) {
  if ((processQueue == NULL)
    || (processQueue->numElements >= SCHEDULER_NUM_PROCESSES)
  ) {
    printString("ERROR!!!  Could not push process ");
    printInt(processDescriptor->processId);
    printString(" onto ");
    printString(processQueue->name);
    printString(" queue!!!\n");
    return ENOMEM;
  }

  processQueue->processes[processQueue->tail] = processDescriptor;
  processQueue->tail++;
  processQueue->tail %= SCHEDULER_NUM_PROCESSES;
  processQueue->numElements++;

  return 0;
}

/// @fn ProcessDescriptor* processQueuePop(ProcessQueue *processQueue)
///
/// @brief Pop a pointer to a ProcessDescriptor from a ProcessQueue.
///
/// @param processQueue A pointer to a ProcessQueue to pop the pointer from.
///
/// @return Returns a pointer to a ProcessDescriptor on success, NULL on
/// failure.
ProcessDescriptor* processQueuePop(ProcessQueue *processQueue) {
  ProcessDescriptor *processDescriptor = NULL;
  if ((processQueue == NULL) || (processQueue->numElements == 0)) {
    return processDescriptor; // NULL
  }

  processDescriptor = processQueue->processes[processQueue->head];
  processQueue->head++;
  processQueue->head %= SCHEDULER_NUM_PROCESSES;
  processQueue->numElements--;

  return processDescriptor;
}

/// @fn int processQueueRemove(
///   ProcessQueue *processQueue, ProcessDescriptor *processDescriptor)
///
/// @brief Remove a pointer to a ProcessDescriptor from a ProcessQueue.
///
/// @param processQueue A pointer to a ProcessQueue to remove the pointer from.
/// @param processDescriptor A pointer to a ProcessDescriptor to remove from the
///   queue.
///
/// @return Returns 0 on success, ENOMEM on failure.
int processQueueRemove(
  ProcessQueue *processQueue, ProcessDescriptor *processDescriptor
) {
  int returnValue = EINVAL;
  if ((processQueue == NULL) || (processQueue->numElements == 0)) {
    // Nothing to do.
    return returnValue; // EINVAL
  }

  ProcessDescriptor *poppedDescriptor = NULL;
  for (uint8_t ii = 0; ii < processQueue->numElements; ii++) {
    poppedDescriptor = processQueuePop(processQueue);
    if (poppedDescriptor == processDescriptor) {
      returnValue = ENOERR;
      break;
    }
    // This is not what we're looking for.  Put it back.
    processQueuePush(processQueue, poppedDescriptor);
  }

  return returnValue;
}

/// @fn void comutexUnlockCallback(void *stateData, Comutex *comutex)
///
/// @brief Function to be called when a mutex (Comutex) is unlocked.
///
/// @param stateData The coroutine state pointer provided when coroutineConfig
///   was called.
/// @param comutex A pointer to the Comutex object that has been unlocked.  At
///   the time this callback is called, the mutex has been unlocked but its
///   coroutine pointer has not been cleared.
///
/// @return This function returns no value, but if the head of the Comutex's
/// lock queue is found in one of the waiting queues, it is removed from the
/// waiting queue and pushed onto the ready queue.
void comutexUnlockCallback(void *stateData, Comutex *comutex) {
  if ((stateData == NULL) || (comutex == NULL)) {
    // We can't work like this.  Bail.
    return;
  } else if (comutex->head == NULL) {
    // This should be impossible.  If it happens, though, there's no point in
    // the rest of the function, so bail.
    return;
  }

  SchedulerState *schedulerState = *((SchedulerState**) stateData);
  if (schedulerState == NULL) {
    // This won't fly either.  Bail.
    return;
  }

  ProcessDescriptor *poppedDescriptor = NULL;
  ProcessQueue *processQueue = &schedulerState->waiting;
  while (1) {
    // NOTE:  It's bad practice to use a member element that's being updated
    // in the loop in the stop condition of a for loop.  But, (a) we're in a
    // very memory constrained environment and we need to avoid using any
    // variables we don't need and (b) the value remains constant between
    // iterations of the loop until we find what we're looking for.  However,
    // once we find what we're looking for, we exit the loop anyway, so it's
    // irrelevant.
    for (uint8_t ii = 0; ii < processQueue->numElements; ii++) {
      poppedDescriptor = processQueuePop(processQueue);
      if (poppedDescriptor->processHandle == comutex->head) {
        // We found the process that will get the lock next.  Push it onto the
        // ready queue and exit.
        processQueuePush(&schedulerState->ready, poppedDescriptor);
        return;
      }
    }

    if (processQueue == &schedulerState->waiting) {
      // We searched the entire waiting queue and found nothing.  Try the timed
      // waiting queue.
      processQueue = &schedulerState->timedWaiting;
    } else {
      // We've searched all the queues and found nothing.  We're done.
      break;
    }
  }

  return;
}
ComutexUnlockCallback comutexUnlockCallbackPointer = comutexUnlockCallback;

/// @fn void coconditionSignalCallback(
///   void *stateData, Cocondition *cocondition)
///
/// @brief Function to be called when a condition (Cocondition) is signalled.
///
/// @param stateData The coroutine state pointer provided when coroutineConfig
///   was called.
/// @param cocondition A pointer to the Cocondition object that has been
///   signalled.  At the time this callback is called, the number of signals has
///   been set to the number of waiters that will be signalled.
///
/// @return This function returns no value, but if the head of the Cocondition's
/// signal queue is found in one of the waiting queues, it is removed from the
/// waiting queue and pushed onto the ready queue.
void coconditionSignalCallback(void *stateData, Cocondition *coroutineObj) {
  if ((stateData == NULL) || (coroutineObj == NULL)) {
    // We can't work like this.  Bail.
    return;
  } else if (coroutineObj->head == NULL) {
    // This should be impossible.  If it happens, though, there's no point in
    // the rest of the function, so bail.
    return;
  }

  SchedulerState *schedulerState = *((SchedulerState**) stateData);
  if (schedulerState == NULL) {
    // This won't fly either.  Bail.
    return;
  }

  ProcessDescriptor *poppedDescriptor = NULL;
  ProcessQueue *processQueue = &schedulerState->waiting;
  while (1) {
    // See note above about using a member element in a for loop.
    for (uint8_t ii = 0; ii < processQueue->numElements; ii++) {
      poppedDescriptor = processQueuePop(processQueue);
      if (poppedDescriptor->processHandle == coroutineObj->head) {
        // We found the process that will get the lock next.  Push it onto the
        // ready queue and exit.
        processQueuePush(&schedulerState->ready, poppedDescriptor);
        return;
      }
    }

    if (processQueue == &schedulerState->waiting) {
      // We searched the entire waiting queue and found nothing.  Try the timed
      // waiting queue.
      processQueue = &schedulerState->timedWaiting;
    } else {
      // We've searched all the queues and found nothing.  We're done.
      break;
    }
  }

  return;
}
CoconditionSignalCallback coconditionSignalCallbackPointer
  = coconditionSignalCallback;

/// @fn ProcessHandle schedulerGetProcessByPid(unsigned int pid)
///
/// @brief Look up a croutine for a running command given its process ID.
///
/// @param pid The integer ID for the process.
///
/// @return Returns the found process handle on success, NULL on failure.
ProcessHandle schedulerGetProcessByPid(unsigned int pid) {
  ProcessHandle process = NULL;
  if (pid < NANO_OS_NUM_PROCESSES) {
    process = allProcesses[pid].processHandle;
  }

  printDebugStackDepth();

  return process;
}

/// @fn void* dummyProcess(void *args)
///
/// @brief Dummy process that's loaded at startup to prepopulate the process
/// array with processes.
///
/// @param args Any arguments passed to this function.  Ignored.
///
/// @return This function always returns NULL.
void* dummyProcess(void *args) {
  (void) args;
  return NULL;
}

/// @fn int schedulerSendProcessMessageToProcess(
///   ProcessHandle processHandle, ProcessMessage *processMessage)
///
/// @brief Get an available ProcessMessage, populate it with the specified data,
/// and push it onto a destination process's queue.
///
/// @param processHandle A ProcessHandle to the destination process to send the
///   message to.
/// @param processMessage A pointer to the message to send to the destination
///   process.
///
/// @return Returns processSuccess on success, processError on failure.
int schedulerSendProcessMessageToProcess(
  ProcessHandle processHandle, ProcessMessage *processMessage
) {
  int returnValue = processSuccess;
  if (processHandle == NULL) {
    printString(
      "ERROR:  Attempt to send scheduler processMessage to NULL process handle.\n");
    returnValue = processError;
    return returnValue;
  } else if (processMessage == NULL) {
    printString(
      "ERROR:  Attempt to send NULL scheduler processMessage to process handle.\n");
    returnValue = processError;
    return returnValue;
  }
  // processMessage->from would normally be set when we do a processMessageQueuePush.
  // We're not using that mechanism here, so we have to do it manually.  If we
  // don't do this, then commands that validate that the message came from the
  // scheduler will fail.
  processMessage->from = schedulerProcess;

  void *processReturnValue = coroutineResume(processHandle, processMessage);
  if (processReturnValue == COROUTINE_CORRUPT) {
    printString("ERROR:  Called process is corrupted!!!\n");
    returnValue = processError;
    return returnValue;
  }

  if (processMessageDone(processMessage) != true) {
    // This is our only indication from the called process that something went
    // wrong.  Return an error status here.
    printString("ERROR:  Called process did not mark sent message done.\n");
    returnValue = processError;
  }

  return returnValue;
}

/// @fn int schedulerSendProcessMessageToPid(SchedulerState *schedulerState,
///   unsigned int pid, ProcessMessage *processMessage)
///
/// @brief Look up a process by its PID and send a message to it.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param pid The ID of the process to send the message to.
/// @param processMessage A pointer to the message to send to the destination
///   process.
///
/// @return Returns processSuccess on success, processError on failure.
int schedulerSendProcessMessageToPid(SchedulerState *schedulerState,
  unsigned int pid, ProcessMessage *processMessage
) {
  (void) schedulerState;

  ProcessHandle processHandle = schedulerGetProcessByPid(pid);
  // If processHandle is NULL, it will be detected as not running by
  // schedulerSendProcessMessageToProcess, so there's no real point in
  //  checking for NULL here.
  return schedulerSendProcessMessageToProcess(processHandle, processMessage);
}

/// @fn int schedulerSendNanoOsMessageToProcess(
///   ProcessHandle processHandle, int type,
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
/// @return Returns processSuccess on success, a different process status
/// on failure.
int schedulerSendNanoOsMessageToProcess(ProcessHandle processHandle,
  int type, NanoOsMessageData func, NanoOsMessageData data
) {
  ProcessMessage processMessage;
  memset(&processMessage, 0, sizeof(processMessage));
  NanoOsMessage nanoOsMessage;

  nanoOsMessage.func = func;
  nanoOsMessage.data = data;

  // These messages are always waiting for done from the caller, so hardcode
  // the waiting parameter to true here.
  processMessageInit(&processMessage, type, &nanoOsMessage, sizeof(nanoOsMessage), true);

  int returnValue = schedulerSendProcessMessageToProcess(
    processHandle, &processMessage);

  return returnValue;
}

/// @fn int schedulerSendNanoOsMessageToPid(
///   SchedulerState *schedulerState,
///   int pid, int type,
///   NanoOsMessageData func, NanoOsMessageData data)
///
/// @brief Send a NanoOsMessage to another process identified by its PID. Looks
/// up the process's Coroutine by its PID and then calls
/// schedulerSendNanoOsMessageToProcess.
///
/// @param schedulerState A pointer to the SchedulerState object maintainted by
///   the scheduler.
/// @param pid The process ID of the destination process.
/// @param type The type of the message to send to the destination process.
/// @param func The function information to send to the destination process,
///   cast to a NanoOsMessageData.
/// @param data The data to send to the destination process, cast to a
///   NanoOsMessageData.
///
/// @return Returns processSuccess on success, a different process status
/// on failure.
int schedulerSendNanoOsMessageToPid(
  SchedulerState *schedulerState,
  int pid, int type,
  NanoOsMessageData func, NanoOsMessageData data
) {
  int returnValue = processError;
  if (pid >= NANO_OS_NUM_PROCESSES) {
    // Not a valid PID.  Fail.
    printString("ERROR!!!  ");
    printInt(pid);
    printString(" is not a valid PID.\n");
    return returnValue; // processError
  }

  ProcessHandle processHandle = schedulerState->allProcesses[pid].processHandle;
  returnValue = schedulerSendNanoOsMessageToProcess(
    processHandle, type, func, data);
  return returnValue;
}

/// @fn int schedulerAssignPortToPid(
///   SchedulerState *schedulerState,
///   uint8_t consolePort, ProcessId owner)
///
/// @brief Assign a console port to a process ID.
///
/// @param schedulerState A pointer to the SchedulerState object maintainted by
///   the scheduler.
/// @param consolePort The ID of the consolePort to assign.
/// @param owner The ID of the process to assign the port to.
///
/// @return Returns processSuccess on success, processError on failure.
int schedulerAssignPortToPid(
  SchedulerState *schedulerState,
  uint8_t consolePort, ProcessId owner
) {
  ConsolePortPidUnion consolePortPidUnion;
  consolePortPidUnion.consolePortPidAssociation.consolePort
    = consolePort;
  consolePortPidUnion.consolePortPidAssociation.processId = owner;

  int returnValue = schedulerSendNanoOsMessageToPid(schedulerState,
    NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_ASSIGN_PORT,
    /* func= */ 0, consolePortPidUnion.nanoOsMessageData);

  return returnValue;
}

/// @fn int schedulerSetPortShell(
///   SchedulerState *schedulerState,
///   uint8_t consolePort, ProcessId shell)
///
/// @brief Assign a console port to a process ID.
///
/// @param schedulerState A pointer to the SchedulerState object maintainted by
///   the scheduler.
/// @param consolePort The ID of the consolePort to set the shell for.
/// @param shell The ID of the shell process for the port.
///
/// @return Returns processSuccess on success, processError on failure.
int schedulerSetPortShell(
  SchedulerState *schedulerState,
  uint8_t consolePort, ProcessId shell
) {
  int returnValue = processError;

  if (shell >= NANO_OS_NUM_PROCESSES) {
    printString("ERROR:  schedulerSetPortShell called with invalid shell PID ");
    printInt(shell);
    printString("\n");
    return returnValue; // processError
  }

  ConsolePortPidUnion consolePortPidUnion;
  consolePortPidUnion.consolePortPidAssociation.consolePort
    = consolePort;
  consolePortPidUnion.consolePortPidAssociation.processId = shell;

  returnValue = schedulerSendNanoOsMessageToPid(schedulerState,
    NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_SET_PORT_SHELL,
    /* func= */ 0, consolePortPidUnion.nanoOsMessageData);

  return returnValue;
}

/// @fn int schedulerNotifyProcessComplete(ProcessId processId)
///
/// @brief Notify a waiting process that a running process has completed.
///
/// @param processId The ID of the process to notify.
///
/// @return Returns processSuccess on success, processError on failure.
int schedulerNotifyProcessComplete(ProcessId processId) {
  if (sendNanoOsMessageToPid(processId,
    SCHEDULER_PROCESS_COMPLETE, 0, 0, false) == NULL
  ) {
    return processError;
  }

  return processSuccess;
}

/// @fn int schedulerWaitForProcessComplete(void)
///
/// @brief Wait for another process to send us a message indicating that a
/// process is complete.
///
/// @return Returns processSuccess on success, processError on failure.
int schedulerWaitForProcessComplete(void) {
  ProcessMessage *doneMessage
    = processMessageQueueWaitForType(SCHEDULER_PROCESS_COMPLETE, NULL);
  if (doneMessage == NULL) {
    return processError;
  }

  // We don't need any data from the message.  Just release it.
  processMessageRelease(doneMessage);

  return processSuccess;
}

/// @fn ProcessId schedulerGetNumRunningProcesses(struct timespec *timeout)
///
/// @brief Get the number of running processes from the scheduler.
///
/// @param timeout A pointer to a struct timespec with the end time for the
///   timeout.
///
/// @return Returns the number of running processes on success, 0 on failure.
/// There is no way for the number of running processes to exceed the maximum
/// value of a ProcessId type, so it's used here as the return type.
ProcessId schedulerGetNumRunningProcesses(struct timespec *timeout) {
  ProcessMessage *processMessage = NULL;
  int waitStatus = processSuccess;
  ProcessId numProcessDescriptors = 0;

  processMessage = sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_GET_NUM_RUNNING_PROCESSES,
    (NanoOsMessageData) 0, (NanoOsMessageData) 0, true);
  if (processMessage == NULL) {
    printf("ERROR!!!  Could not communicate with scheduler.\n");
    goto exit;
  }

  waitStatus = processMessageWaitForDone(processMessage, timeout);
  if (waitStatus != processSuccess) {
    if (waitStatus == processTimedout) {
      printf("Command to get the number of running processes timed out.\n");
    } else {
      printf("Command to get the number of running processes failed.\n");
    }

    // Without knowing how many processes there are, we can't continue.  Bail.
    goto releaseMessage;
  }

  numProcessDescriptors = nanoOsMessageDataValue(processMessage, ProcessId);
  if (numProcessDescriptors == 0) {
    printf("ERROR:  Number of running processes returned from the "
      "scheduler is 0.\n");
    goto releaseMessage;
  }

releaseMessage:
  if (processMessageRelease(processMessage) != processSuccess) {
    printf("ERROR!!!  Could not release message sent to scheduler for "
      "getting the number of running processes.\n");
  }

exit:
  return numProcessDescriptors;
}

/// @fn ProcessInfo* schedulerGetProcessInfo(void)
///
/// @brief Get information about all processes running in the system from the
/// scheduler.
///
/// @return Returns a populated, dynamically-allocated ProcessInfo object on
/// success, NULL on failure.
ProcessInfo* schedulerGetProcessInfo(void) {
  int waitStatus = processSuccess;

  // We don't know where our messages to the scheduler will be in its queue, so
  // we can't assume they will be processed immediately, but we can't wait
  // forever either.  Set a 100 ms timeout.
  struct timespec timeout = {};
  timespec_get(&timeout, TIME_UTC);
  timeout.tv_nsec += 100000000;

  // Because the scheduler runs on the main coroutine, it doesn't have the
  // ability to yield.  That means it can't do anything that requires a
  // synchronus message exchange, i.e. allocating memory.  So, we need to
  // allocate memory from the current process and then pass that back to the
  // scheduler to populate.  That means we first need to know how many processes
  // are running so that we know how much space to allocate.  So, get that
  // first.
  ProcessId numProcessDescriptors = schedulerGetNumRunningProcesses(&timeout);

  // We need numProcessDescriptors rows.
  ProcessInfo *processInfo = (ProcessInfo*) malloc(sizeof(ProcessInfo)
    + ((numProcessDescriptors - 1) * sizeof(ProcessInfoElement)));
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
  processInfo->numProcesses = numProcessDescriptors;

  ProcessMessage *processMessage = sendNanoOsMessageToPid(NANO_OS_SCHEDULER_PROCESS_ID,
    SCHEDULER_GET_PROCESS_INFO, /* func= */ 0, (intptr_t) processInfo, true);

  if (processMessage == NULL) {
    printf("ERROR:  Could not send scheduler message to get process info.\n");
    goto freeMemory;
  }

  waitStatus = processMessageWaitForDone(processMessage, &timeout);
  if (waitStatus != processSuccess) {
    if (waitStatus == processTimedout) {
      printf("Command to get process information timed out.\n");
    } else {
      printf("Command to get process information failed.\n");
    }

    // Without knowing the data for the processes, we can't display them.  Bail.
    goto freeMemory;
  }

  goto exit;

freeMemory:
  free(processInfo); processInfo = NULL;

exit:
  return processInfo;
}

/// @fn int schedulerKillProcess(ProcessId processId)
///
/// @brief Do all the inter-process communication with the scheduler required
/// to kill a running process.
///
/// @param processId The ID of the process to kill.
///
/// @return Returns 0 on success, 1 on failure.
int schedulerKillProcess(ProcessId processId) {
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_KILL_PROCESS,
    (NanoOsMessageData) 0, (NanoOsMessageData) processId, false);
  if (processMessage == NULL) {
    printf("ERROR!!!  Could not communicate with scheduler.\n");
    return 1;
  }

  // We don't know where our message to the scheduler will be in its queue, so
  // we can't assume it will be processed immediately, but we can't wait forever
  // either.  Set a 100 ms timeout.
  struct timespec ts = { 0, 0 };
  timespec_get(&ts, TIME_UTC);
  ts.tv_nsec += 100000000;

  int waitStatus = processMessageWaitForDone(processMessage, &ts);
  int returnValue = 0;
  if (waitStatus == processSuccess) {
    NanoOsMessage *nanoOsMessage = (NanoOsMessage*) processMessageData(processMessage);
    returnValue = nanoOsMessage->data;
    if (returnValue == 0) {
      printf("Process terminated.\n");
    } else {
      printf("Process termination returned status \"%s\".\n",
        nanoOsStrError(returnValue));
    }
  } else {
    returnValue = 1;
    if (waitStatus == processTimedout) {
      printf("Command to kill PID %d timed out.\n", processId);
    } else {
      printf("Command to kill PID %d failed.\n", processId);
    }
  }

  if (processMessageRelease(processMessage) != processSuccess) {
    returnValue = 1;
    printf("ERROR!!!  "
      "Could not release message sent to scheduler for kill command.\n");
  }

  return returnValue;
}

/// @fn int schedulerRunProcess(CommandEntry *commandEntry,
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
int schedulerRunProcess(CommandEntry *commandEntry,
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
  commandDescriptor->callingProcess = processId(getRunningProcess());

  ProcessMessage *sent = sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_RUN_PROCESS,
    (NanoOsMessageData) commandEntry, (NanoOsMessageData) commandDescriptor,
    true);
  if (sent == NULL) {
    printString("ERROR!!!  Could not communicate with scheduler.\n");
    return returnValue; // 1
  }
  schedulerWaitForProcessComplete();

  if (processMessageDone(sent) == false) {
    // The called process was killed.  We need to release the sent message on
    // its behalf.
    processMessageRelease(sent);
  }

  returnValue = 0;
  return returnValue;
}

/// @fn UserId schedulerGetProcessUser(void)
///
/// @brief Get the ID of the user running the current process.
///
/// @return Returns the ID of the user running the current process on success,
/// -1 on failure.
UserId schedulerGetProcessUser(void) {
  UserId userId = -1;
  ProcessMessage *processMessage
    = sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_GET_PROCESS_USER,
    /* func= */ 0, /* data= */ 0, true);
  if (processMessage == NULL) {
    printString("ERROR!!!  Could not communicate with scheduler.\n");
    return userId; // -1
  }

  processMessageWaitForDone(processMessage, NULL);
  userId = nanoOsMessageDataValue(processMessage, UserId);
  processMessageRelease(processMessage);

  return userId;
}

/// @fn int schedulerSetProcessUser(UserId userId)
///
/// @brief Set the user ID of the current process to the specified user ID.
///
/// @return Returns 0 on success, -1 on failure.
int schedulerSetProcessUser(UserId userId) {
  int returnValue = -1;
  ProcessMessage *processMessage
    = sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_SET_PROCESS_USER,
    /* func= */ 0, /* data= */ userId, true);
  if (processMessage == NULL) {
    printString("ERROR!!!  Could not communicate with scheduler.\n");
    return returnValue; // -1
  }

  processMessageWaitForDone(processMessage, NULL);
  returnValue = nanoOsMessageDataValue(processMessage, int);
  processMessageRelease(processMessage);

  if (returnValue != 0) {
    printf("Scheduler returned \"%s\" for setProcessUser.\n",
      nanoOsStrError(returnValue));
  }

  return returnValue;
}

// Scheduler command handlers

/// @fn int schedulerRunProcessCommandHandler(
///   SchedulerState *schedulerState, ProcessMessage *processMessage)
///
/// @brief Run a process in an appropriate process slot.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param processMessage A pointer to the ProcessMessage that was received that contains
///   the information about the process to run and how to run it.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerRunProcessCommandHandler(
  SchedulerState *schedulerState, ProcessMessage *processMessage
) {
  static int returnValue = 0;
  if (processMessage == NULL) {
    // This should be impossible, but there's nothing to do.  Return good
    // status.
    return returnValue; // 0
  }

  CommandEntry *commandEntry
    = nanoOsMessageFuncPointer(processMessage, CommandEntry*);
  CommandDescriptor *commandDescriptor
    = nanoOsMessageDataPointer(processMessage, CommandDescriptor*);
  commandDescriptor->schedulerState = schedulerState;
  char *consoleInput = commandDescriptor->consoleInput;
  int consolePort = commandDescriptor->consolePort;
  ProcessHandle caller = processMessageFrom(processMessage);
  
  // Find an open slot.
  ProcessDescriptor *processDescriptor = NULL;
  if (commandEntry->shellCommand == true) {
    // Not the normal case but the priority case, so handle it first.  We're
    // going to kill the caller and reuse its process slot.
    processDescriptor = &schedulerState->allProcesses[processId(caller)];

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
    processTerminate(caller);

    // We don't want to wait for the memory manager to release the memory.  Make
    // it do it immediately.  We need to do this before we kill the process.
    if (schedulerSendNanoOsMessageToPid(
      schedulerState, NANO_OS_MEMORY_MANAGER_PROCESS_ID,
      MEMORY_MANAGER_FREE_PROCESS_MEMORY,
      /* func= */ 0, processDescriptor->processId)
    ) {
      printString("WARNING:  Could not release memory for process ");
      printInt(processDescriptor->processId);
      printString("\n");
      printString("Memory leak.\n");
    }
  } else {
    processDescriptor = processQueuePop(&schedulerState->free);
  }

  if (processDescriptor != NULL) {
    processDescriptor->userId
      = schedulerState->allProcesses[processId(caller)].userId;

    if (processCreate(&processDescriptor->processHandle,
      startCommand, processMessage) == processError
    ) {
      printString(
        "ERROR!!!  Could not configure process handle for new command.\n");
    }
    if (assignMemory(consoleInput, processDescriptor->processId) != 0) {
      printString(
        "WARNING:  Could not assign console input to new process.\n");
      printString("Memory leak.\n");
    }
    if (assignMemory(commandDescriptor, processDescriptor->processId) != 0) {
      printString(
        "WARNING:  Could not assign command descriptor to new process.\n");
      printString("Memory leak.\n");
    }

    processDescriptor->name = commandEntry->name;

    returnValue = 0;
    if (schedulerAssignPortToPid(schedulerState,
      consolePort, processDescriptor->processId) != processSuccess
    ) {
      printString("WARNING:  Could not assign console port to process.\n");
    }

    if (commandEntry->shellCommand == false) {
      // Put the process on the ready queue.
      processQueuePush(&schedulerState->ready, processDescriptor);
    }
    // else we replaced the running process and it's already on the ready queue.
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
        SCHEDULER_PROCESS_COMPLETE, 0, 0, true);
      consoleInput = stringDestroy(consoleInput);
      free(commandDescriptor); commandDescriptor = NULL;
      if (processMessageRelease(processMessage) != processSuccess) {
        printString("ERROR!!!  "
          "Could not release message from handleSchedulerMessage "
          "for invalid message type.\n");
      }
    }
  }
  
  return returnValue;
}

/// @fn int schedulerKillProcessCommandHandler(
///   SchedulerState *schedulerState, ProcessMessage *processMessage)
///
/// @brief Kill a process identified by its process ID.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param processMessage A pointer to the ProcessMessage that was received that contains
///   the information about the process to kill.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerKillProcessCommandHandler(
  SchedulerState *schedulerState, ProcessMessage *processMessage
) {
  int returnValue = 0;

  ProcessMessage *schedulerProcessCompleteMessage = getAvailableMessage();
  if (schedulerProcessCompleteMessage == NULL) {
    // We have to have a message to send to unblock the console.  Fail and try
    // again later.
    return EBUSY;
  }
  processMessageInit(schedulerProcessCompleteMessage,
    SCHEDULER_PROCESS_COMPLETE, 0, 0, false);

  UserId callingUserId
    = allProcesses[processId(processMessageFrom(processMessage))].userId;
  ProcessId processId
    = nanoOsMessageDataValue(processMessage, ProcessId);
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) processMessageData(processMessage);

  if ((processId >= NANO_OS_FIRST_PROCESS_ID)
    && (processId < NANO_OS_NUM_PROCESSES)
    && (processRunning(allProcesses[processId].processHandle))
  ) {
    if ((allProcesses[processId].userId == callingUserId)
      || (callingUserId == ROOT_USER_ID)
    ) {
      // Tell the console to release the port for us.  We will forward it
      // the message we acquired above, which it will use to send to the
      // correct shell to unblock it.  We need to do this before terminating
      // the process because, in the event the process we're terminating is one
      // of the shell process slots, the message won't get released because
      // there's no shell blocking waiting for the message.
      schedulerSendNanoOsMessageToPid(
        schedulerState,
        NANO_OS_CONSOLE_PROCESS_ID,
        CONSOLE_RELEASE_PID_PORT,
        (intptr_t) schedulerProcessCompleteMessage,
        processId);

      // Forward the message on to the memory manager to have it clean up the
      // process's memory.  *DO NOT* mark the message as done.  The memory
      // manager will do that.
      processMessageInit(processMessage, MEMORY_MANAGER_FREE_PROCESS_MEMORY,
        nanoOsMessage, sizeof(*nanoOsMessage), /* waiting= */ true);
      sendProcessMessageToProcess(
        schedulerState->allProcesses[
          NANO_OS_MEMORY_MANAGER_PROCESS_ID].processHandle,
        processMessage);

      if (processTerminate(allProcesses[processId].processHandle)
        == processSuccess
      ) {
        allProcesses[processId].name = NULL;
        allProcesses[processId].userId = NO_USER_ID;

        processQueuePush(&schedulerState->free, &allProcesses[processId]);
      } else {
        // Tell the caller that we've failed.
        nanoOsMessage->data = 1;
        if (processMessageSetDone(processMessage) != processSuccess) {
          printString("ERROR!!!  Could not mark message done in "
            "schedulerKillProcessCommandHandler.\n");
        }

        // Do *NOT* push the process back onto the free queue in this case.
        // If we couldn't terminate it, it's not valid to try and reuse it for
        // another process.
      }

      // Regardless of whether or not we succeeded at terminating it, we have
      // to remove it from the ready queue.
      if (processQueueRemove(
        &schedulerState->ready, &allProcesses[processId]) != 0
      ) {
        printString("ERROR!!!  Could not remove process ");
        printInt(processId);
        printString(" from ready queue!!!");
      }
    } else {
      // Tell the caller that we've failed.
      nanoOsMessage->data = EACCES; // Permission denied
      if (processMessageSetDone(processMessage) != processSuccess) {
        printString("ERROR!!!  Could not mark message done in "
          "schedulerKillProcessCommandHandler.\n");
      }
    }
  } else {
    // Tell the caller that we've failed.
    nanoOsMessage->data = EINVAL; // Invalid argument
    if (processMessageSetDone(processMessage) != processSuccess) {
      printString("ERROR!!!  "
        "Could not mark message done in schedulerKillProcessCommandHandler.\n");
    }
  }

  // DO NOT release the message since that's done by the caller.

  return returnValue;
}

/// @fn int schedulerGetNumProcessDescriptorsCommandHandler(
///   SchedulerState *schedulerState, ProcessMessage *processMessage)
///
/// @brief Get the number of processes that are currently running in the system.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param processMessage A pointer to the ProcessMessage that was received.  This will be
///   reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerGetNumProcessDescriptorsCommandHandler(
  SchedulerState *schedulerState, ProcessMessage *processMessage
) {
  int returnValue = 0;

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);

  uint8_t numProcessDescriptors = 0;
  for (int ii = 0; ii < NANO_OS_NUM_PROCESSES; ii++) {
    if (processRunning(schedulerState->allProcesses[ii].processHandle)) {
      numProcessDescriptors++;
    }
  }
  nanoOsMessage->data = numProcessDescriptors;

  processMessageSetDone(processMessage);

  // DO NOT release the message since the caller is waiting on the response.

  return returnValue;
}

/// @fn int schedulerGetProcessInfoCommandHandler(
///   SchedulerState *schedulerState, ProcessMessage *processMessage)
///
/// @brief Fill in a provided array with information about the currently-running
/// processes.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param processMessage A pointer to the ProcessMessage that was received.  This will be
///   reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerGetProcessInfoCommandHandler(
  SchedulerState *schedulerState, ProcessMessage *processMessage
) {
  int returnValue = 0;

  ProcessInfo *processInfo
    = nanoOsMessageDataPointer(processMessage, ProcessInfo*);
  int maxProcesses = processInfo->numProcesses;
  ProcessInfoElement *processes = processInfo->processes;
  int idx = 0;
  for (int ii = 0; (ii < NANO_OS_NUM_PROCESSES) && (idx < maxProcesses); ii++) {
    if (processRunning(schedulerState->allProcesses[ii].processHandle)) {
      processes[idx].pid
        = (int) processId(schedulerState->allProcesses[ii].processHandle);
      processes[idx].name = schedulerState->allProcesses[ii].name;
      processes[idx].userId = schedulerState->allProcesses[ii].userId;
      idx++;
    }
  }

  // It's possible that a process completed between the time that processInfo
  // was allocated and now, so set the value of numProcesses to the value of
  // idx.
  processInfo->numProcesses = idx;

  processMessageSetDone(processMessage);

  // DO NOT release the message since the caller is waiting on the response.

  return returnValue;
}

/// @fn int schedulerGetProcessUserCommandHandler(
///   SchedulerState *schedulerState, ProcessMessage *processMessage)
///
/// @brief Get the number of processes that are currently running in the system.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param processMessage A pointer to the ProcessMessage that was received.  This will be
///   reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerGetProcessUserCommandHandler(
  SchedulerState *schedulerState, ProcessMessage *processMessage
) {
  int returnValue = 0;
  ProcessId processId = processId(processMessageFrom(processMessage));
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) processMessageData(processMessage);
  if (processId < NANO_OS_NUM_PROCESSES) {
    nanoOsMessage->data = schedulerState->allProcesses[processId].userId;
  } else {
    nanoOsMessage->data = -1;
  }

  processMessageSetDone(processMessage);

  // DO NOT release the message since the caller is waiting on the response.

  return returnValue;
}

/// @fn int schedulerSetProcessUserCommandHandler(
///   SchedulerState *schedulerState, ProcessMessage *processMessage)
///
/// @brief Get the number of processes that are currently running in the system.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param processMessage A pointer to the ProcessMessage that was received.  This will be
///   reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerSetProcessUserCommandHandler(
  SchedulerState *schedulerState, ProcessMessage *processMessage
) {
  int returnValue = 0;
  ProcessId processId = processId(processMessageFrom(processMessage));
  UserId userId = nanoOsMessageDataValue(processMessage, UserId);
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = -1;

  if (processId < NANO_OS_NUM_PROCESSES) {
    if ((schedulerState->allProcesses[processId].userId == -1)
      || (userId == -1)
    ) {
      schedulerState->allProcesses[processId].userId = userId;
      nanoOsMessage->data = 0;
    } else {
      nanoOsMessage->data = EACCES;
    }
  }

  processMessageSetDone(processMessage);

  // DO NOT release the message since the caller is waiting on the response.

  return returnValue;
}

/// @var schedulerCommandHandlers
///
/// @brief Array of function pointers for commands that are understood by the
/// message handler for the main loop function.
int (*schedulerCommandHandlers[])(SchedulerState*, ProcessMessage*) = {
  schedulerRunProcessCommandHandler,        // SCHEDULER_RUN_PROCESS
  schedulerKillProcessCommandHandler,       // SCHEDULER_KILL_PROCESS
  // SCHEDULER_GET_NUM_RUNNING_PROCESSES:
  schedulerGetNumProcessDescriptorsCommandHandler,
  schedulerGetProcessInfoCommandHandler,    // SCHEDULER_GET_PROCESS_INFO
  schedulerGetProcessUserCommandHandler,    // SCHEDULER_GET_PROCESS_USER
  schedulerSetProcessUserCommandHandler,    // SCHEDULER_SET_PROCESS_USER
};

/// @fn void handleSchedulerMessage(SchedulerState *schedulerState)
///
/// @brief Handle one (and only one) message from our message queue.  If
/// handling the message is unsuccessful, the message will be returned to the
/// end of our message queue.
///
/// @param schedulerState A pointer to the SchedulerState object maintained by
///   the scheduler process.
///
/// @return This function returns no value.
void handleSchedulerMessage(SchedulerState *schedulerState) {
  static int lastReturnValue = 0;
  ProcessMessage *message = processMessageQueuePop();
  if (message != NULL) {
    SchedulerCommand messageType = (SchedulerCommand) processMessageType(message);
    if (messageType >= NUM_SCHEDULER_COMMANDS) {
      // Invalid.  Purge the message.
      if (processMessageRelease(message) != processSuccess) {
        printString("ERROR!!!  "
          "Could not release message from handleSchedulerMessage "
          "for invalid message type.\n");
      }
      return;
    }

    int returnValue = schedulerCommandHandlers[messageType](
      schedulerState, message);
    if (returnValue != 0) {
      // Processing the message failed.  We can't release it.  Put it on the
      // back of our own queue again and try again later.
      if (lastReturnValue == 0) {
        // Only print out a message if this is the first time we've failed.
        printString("Scheduler command handler failed.\n");
        printString("Pushing message back onto our own queue.\n");
      }
      processMessageQueuePush(NULL, message);
    }
    lastReturnValue = returnValue;
  }

  return;
}

/// @fn void runScheduler(SchedulerState *schedulerState)
///
/// @brief Run the main scheduler loop.
///
/// @param schedulerState A pointer to the SchedulerState object maintained by
///   the scheduler process.
///
/// @return This function returns no value and, in fact, never returns at all.
void runScheduler(SchedulerState *schedulerState) {
  void *processReturnValue = NULL;
  ProcessDescriptor *processDescriptor = NULL;

  while (1) {
    processDescriptor = processQueuePop(&schedulerState->ready);
    processReturnValue
      = coroutineResume(processDescriptor->processHandle, NULL);
    if (processReturnValue == COROUTINE_CORRUPT) {
      printString("ERROR!!!  Process corruption detected!!!\n");
      printString("          Removing process.");
      printInt(processDescriptor->processId);
      printString(" from process queues.\n");

      processDescriptor->name = NULL;
      processDescriptor->userId = NO_USER_ID;
      processDescriptor->processHandle->state = COROUTINE_STATE_NOT_RUNNING;

      ProcessMessage *schedulerProcessCompleteMessage = getAvailableMessage();
      if (schedulerProcessCompleteMessage != NULL) {
        schedulerSendNanoOsMessageToPid(
          schedulerState,
          NANO_OS_CONSOLE_PROCESS_ID,
          CONSOLE_RELEASE_PID_PORT,
          (intptr_t) schedulerProcessCompleteMessage,
          processDescriptor->processId);
      } else {
        printString("WARNING:  Could not allocate "
          "schedulerProcessCompleteMessage.  Memory leak.\n");
        // If we can't allocate the first message, we can't allocate the second
        // one either, so bail.
        continue;
      }

      ProcessMessage *memoryManagerFreeProcessMemoryMessage = getAvailableMessage();
      if (memoryManagerFreeProcessMemoryMessage != NULL) {
        NanoOsMessage *nanoOsMessage = (NanoOsMessage*) processMessageData(
          memoryManagerFreeProcessMemoryMessage);
        nanoOsMessage->data = processDescriptor->processId;
        processMessageInit(memoryManagerFreeProcessMemoryMessage,
          MEMORY_MANAGER_FREE_PROCESS_MEMORY,
          nanoOsMessage, sizeof(*nanoOsMessage), /* waiting= */ false);
        sendProcessMessageToProcess(
          schedulerState->allProcesses[
            NANO_OS_MEMORY_MANAGER_PROCESS_ID].processHandle,
          memoryManagerFreeProcessMemoryMessage);
      } else {
        printString("WARNING:  Could not allocate "
          "memoryManagerFreeProcessMemoryMessage.  Memory leak.\n");
      }

      continue;
    }

    // Check the shells and restart them if needed.
    if ((processDescriptor->processId == USB_SERIAL_PORT_SHELL_PID)
      && (processRunning(processDescriptor->processHandle) == false)
    ) {
      // Restart the shell.
      if (processCreate(&processDescriptor->processHandle, runShell, NULL)
          == processError
      ) {
        printString(
          "ERROR!!!  Could not configure process for USB shell.\n");
      }
      processDescriptor->name = "USB shell";
    } else if ((processDescriptor->processId == GPIO_SERIAL_PORT_SHELL_PID)
      && (processRunning(processDescriptor->processHandle) == false)
    ) {
      // Restart the shell.
      if (processCreate(&processDescriptor->processHandle, runShell, NULL)
        == processError
      ) {
        printString(
          "ERROR!!!  Could not configure process for GPIO shell.\n");
      }
      processDescriptor->name = "GPIO shell";
    }

    /* if (processReturnValue == COROUTINE_WAIT) {
      processQueuePush(&schedulerState->waiting, processDescriptor);
    } else if (processReturnValue == COROUTINE_TIMEDWAIT) {
      processQueuePush(&schedulerState->timedWaiting, processDescriptor)
    } else */ if (processFinished(processDescriptor->processHandle)) {
      processQueuePush(&schedulerState->free, processDescriptor);
    } else { // Process is still running.
      processQueuePush(&schedulerState->ready, processDescriptor);
    }

    handleSchedulerMessage(schedulerState);
  }
}

/// @fn void startScheduler(SchedulerState **coroutineStatePointer)
///
/// @brief Initialize and run the round-robin scheduler.
///
/// @return This function returns no value and, in fact, never returns at all.
__attribute__((noinline)) void startScheduler(
  SchedulerState **coroutineStatePointer
) {
  // Initialize the scheduler's state.
  SchedulerState schedulerState = {};
  schedulerState.ready.name = "ready";
  schedulerState.waiting.name = "waiting";
  schedulerState.timedWaiting.name = "timed waiting";
  schedulerState.free.name = "free";

  // Initialize the pointer that was used to configure coroutines.
  *coroutineStatePointer = &schedulerState;

  // Initialize the static ProcessMessage storage.
  ProcessMessage messagesStorage[NANO_OS_NUM_MESSAGES] = {};
  messages = messagesStorage;

  // Initialize the NanoOsMessage storage.
  NanoOsMessage nanoOsMessagesStorage[NANO_OS_NUM_MESSAGES] = {};
  nanoOsMessages = nanoOsMessagesStorage;

  // Initialize the allProcesses pointer.
  allProcesses = schedulerState.allProcesses;

  // Initialize ourself in the array of running commands.
  processSetId(schedulerProcess, NANO_OS_SCHEDULER_PROCESS_ID);
  allProcesses[NANO_OS_SCHEDULER_PROCESS_ID].processId
    = NANO_OS_SCHEDULER_PROCESS_ID;
  allProcesses[NANO_OS_SCHEDULER_PROCESS_ID].processHandle = schedulerProcess;
  allProcesses[NANO_OS_SCHEDULER_PROCESS_ID].name = "scheduler";
  allProcesses[NANO_OS_SCHEDULER_PROCESS_ID].userId = ROOT_USER_ID;

  // Create the console process.
  ProcessHandle processHandle = 0;
  if (processCreate(&processHandle, runConsole, NULL) != processSuccess) {
    printString("Could not create console process.\n");
  }
  processSetId(processHandle, NANO_OS_CONSOLE_PROCESS_ID);
  allProcesses[NANO_OS_CONSOLE_PROCESS_ID].processId
    = NANO_OS_CONSOLE_PROCESS_ID;
  allProcesses[NANO_OS_CONSOLE_PROCESS_ID].processHandle = processHandle;
  allProcesses[NANO_OS_CONSOLE_PROCESS_ID].name = "console";
  allProcesses[NANO_OS_CONSOLE_PROCESS_ID].userId = ROOT_USER_ID;

  // Double the size of the console's stack.
  processHandle = 0;
  if (processCreate(&processHandle, dummyProcess, NULL) != processSuccess) {
    printString("Could not double console process's stack.\n");
  }

  // Start the console by calling coroutineResume.
  coroutineResume(
    allProcesses[NANO_OS_CONSOLE_PROCESS_ID].processHandle, NULL);

  printString("\n");
  printString("Main stack size = ");
  printInt(ABS_DIFF(
    ((intptr_t) schedulerProcess),
    ((intptr_t) allProcesses[NANO_OS_CONSOLE_PROCESS_ID].processHandle)
  ));
  printString(" bytes\n");
  printString("schedulerState size = ");
  printInt(sizeof(SchedulerState));
  printString(" bytes\n");
  printString("messagesStorage size = ");
  printInt(sizeof(ProcessMessage) * NANO_OS_NUM_MESSAGES);
  printString(" bytes\n");
  printString("nanoOsMessagesStorage size = ");
  printInt(sizeof(NanoOsMessage) * NANO_OS_NUM_MESSAGES);
  printString(" bytes\n");
  printString("ConsoleState size = ");
  printInt(sizeof(ConsoleState));
  printString(" bytes\n");

  // We need to do an initial population of all the commands because we need to
  // get to the end of memory to run the memory manager in whatever is left
  // over.
  for (ProcessId ii = NANO_OS_FIRST_PROCESS_ID;
    ii < NANO_OS_NUM_PROCESSES;
    ii++
  ) {
    processHandle = 0;
    if (processCreate(&processHandle,
      dummyProcess, NULL) != processSuccess
    ) {
      printString("Could not create process ");
      printInt(ii);
      printString(".\n");
    }
    processSetId(processHandle, ii);
    allProcesses[ii].processId = ii;
    allProcesses[ii].processHandle = processHandle;
    allProcesses[ii].userId = NO_USER_ID;
  }
  printString("Console stack size = ");
  printInt(ABS_DIFF(
    ((uintptr_t) allProcesses[NANO_OS_FIRST_PROCESS_ID].processHandle),
    ((uintptr_t) allProcesses[NANO_OS_CONSOLE_PROCESS_ID].processHandle))
    - sizeof(Coroutine)
  );
  printString(" bytes\n");
  printString("Coroutine stack size = ");
  printInt(ABS_DIFF(
    ((uintptr_t) allProcesses[NANO_OS_FIRST_PROCESS_ID].processHandle),
    ((uintptr_t) allProcesses[NANO_OS_FIRST_PROCESS_ID + 1].processHandle))
    - sizeof(Coroutine)
  );
  printString(" bytes\n");
  printString("Coroutine size = ");
  printInt(sizeof(Coroutine));
  printString("\n");

  // Create the memory manager process.  !!! THIS MUST BE THE LAST PROCESS
  // CREATED BECAUSE WE WANT TO USE THE ENTIRE REST OF MEMORY FOR IT !!!
  processHandle = 0;
  if (processCreate(&processHandle,
    runMemoryManager, NULL) != processSuccess
  ) {
    printString("Could not create memory manager process.\n");
  }
  processSetId(processHandle, NANO_OS_MEMORY_MANAGER_PROCESS_ID);
  allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].processHandle = processHandle;
  allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].processId
    = NANO_OS_MEMORY_MANAGER_PROCESS_ID;
  allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].name = "memory manager";
  allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].userId = ROOT_USER_ID;

  // Start the memory manager by calling coroutineResume.
  coroutineResume(
    allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].processHandle, NULL);

  // Assign the console ports to it.
  for (uint8_t ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (schedulerAssignPortToPid(&schedulerState,
      ii, NANO_OS_MEMORY_MANAGER_PROCESS_ID) != processSuccess
    ) {
      printString(
        "WARNING:  Could not assign console port to memory manager.\n");
    }
  }

  // Set the shells for the ports.
  if (schedulerSetPortShell(&schedulerState,
    USB_SERIAL_PORT, USB_SERIAL_PORT_SHELL_PID) != processSuccess
  ) {
    printString("WARNING:  Could not set shell for USB serial port.\n");
    printString("          Undefined behavior will result.\n");
  }
  if (schedulerSetPortShell(&schedulerState,
    GPIO_SERIAL_PORT, GPIO_SERIAL_PORT_SHELL_PID) != processSuccess
  ) {
    printString("WARNING:  Could not set shell for GPIO serial port.\n");
    printString("          Undefined behavior will result.\n");
  }

  // The scheduler will take care of cleaning up the dummy processes.

  processQueuePush(&schedulerState.ready,
    &allProcesses[NANO_OS_CONSOLE_PROCESS_ID]);
  processQueuePush(&schedulerState.ready,
    &allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID]);
  for (ProcessId ii = NANO_OS_FIRST_PROCESS_ID;
    ii < NANO_OS_NUM_PROCESSES;
    ii++
  ) {
    processQueuePush(&schedulerState.ready, &allProcesses[ii]);
  }

  // Start our scheduler.
  runScheduler(&schedulerState);
}

