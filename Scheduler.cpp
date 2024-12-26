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
/// @brief Pointer to the main coroutine that's allocated in the main loop
/// function.
Coroutine *schedulerProcess = NULL;

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

/// @fn Process* getProcessByPid(unsigned int pid)
///
/// @brief Look up a croutine for a running command given its process ID.
///
/// @param pid The integer ID for the process.
///
/// @return Returns the found coroutine pointer on success, NULL on failure.
Process* getProcessByPid(unsigned int pid) {
  Process *process = NULL;
  if (pid < NANO_OS_NUM_PROCESSES) {
    process = allProcesses[pid].coroutine;
  }

  return process;
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
  comessage->from = schedulerProcess;

  void *coroutineReturnValue = coroutineResume(coroutine, comessage);
  if (coroutineReturnValue == COROUTINE_CORRUPT) {
    printString("ERROR:  Called coroutine is corrupted!!!\n");
    returnValue = coroutineError;
    return returnValue;
  }

  if (comessageDone(comessage) != true) {
    // This is our only indication from the called coroutine that something went
    // wrong.  Return an error status here.
    printString("ERROR:  Called coroutine did not mark sent message done.\n");
    returnValue = coroutineError;
  }

  return returnValue;
}

/// @fn int schedulerSendComessageToPid(SchedulerState *schedulerState,
///   unsigned int pid, Comessage *comessage)
///
/// @brief Look up a coroutine by its PID and send a message to it.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param pid The ID of the process to send the message to.
/// @param comessage A pointer to the message to send to the destination
///   process.
///
/// @return Returns coroutineSuccess on success, coroutineError on failure.
int schedulerSendComessageToPid(SchedulerState *schedulerState,
  unsigned int pid, Comessage *comessage
) {
  (void) schedulerState;

  Coroutine *coroutine = getProcessByPid(pid);
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

/// @fn int schedulerSendNanoOsMessageToPid(
///   SchedulerState *schedulerState,
///   int pid, int type,
///   NanoOsMessageData func, NanoOsMessageData data)
///
/// @brief Send a NanoOsMessage to another process identified by its PID. Looks
/// up the process's Coroutine by its PID and then calls
/// schedulerSendNanoOsMessageToCoroutine.
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
/// @return Returns coroutineSuccess on success, a different coroutine status
/// on failure.
int schedulerSendNanoOsMessageToPid(
  SchedulerState *schedulerState,
  int pid, int type,
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

  Coroutine *coroutine = schedulerState->allProcesses[pid].coroutine;
  returnValue = schedulerSendNanoOsMessageToCoroutine(
    coroutine, type, func, data);
  return returnValue;
}

/// @fn int schedulerAssignPortToPid(
///   SchedulerState *schedulerState,
///   uint8_t consolePort, CoroutineId owner)
///
/// @brief Assign a console port to a process ID.
///
/// @param schedulerState A pointer to the SchedulerState object maintainted by
///   the scheduler.
/// @param consolePort The ID of the consolePort to assign.
/// @param owner The ID of the process to assign the port to.
///
/// @return Returns coroutineSuccess on success, coroutineError on failure.
int schedulerAssignPortToPid(
  SchedulerState *schedulerState,
  uint8_t consolePort, CoroutineId owner
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
///   uint8_t consolePort, CoroutineId shell)
///
/// @brief Assign a console port to a process ID.
///
/// @param schedulerState A pointer to the SchedulerState object maintainted by
///   the scheduler.
/// @param consolePort The ID of the consolePort to set the shell for.
/// @param shell The ID of the shell process for the port.
///
/// @return Returns coroutineSuccess on success, coroutineError on failure.
int schedulerSetPortShell(
  SchedulerState *schedulerState,
  uint8_t consolePort, CoroutineId shell
) {
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

  returnValue = schedulerSendNanoOsMessageToPid(schedulerState,
    NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_SET_PORT_SHELL,
    /* func= */ 0, consolePortPidUnion.nanoOsMessageData);

  return returnValue;
}

/// @fn int schedulerRunProcessCommandHandler(
///   SchedulerState *schedulerState, Comessage *comessage)
///
/// @brief Run a process in an appropriate process slot.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param comessage A pointer to the Comessage that was received that contains
///   the information about the process to run and how to run it.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerRunProcessCommandHandler(
  SchedulerState *schedulerState, Comessage *comessage
) {
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
  commandDescriptor->schedulerState = schedulerState;
  char *consoleInput = commandDescriptor->consoleInput;
  int consolePort = commandDescriptor->consolePort;
  Coroutine *caller = comessageFrom(comessage);
  
  // Find an open slot.
  ProcessDescriptor *processDescriptor = NULL;
  if (commandEntry->shellCommand == true) {
    // Not the normal case but the priority case, so handle it first.  We're
    // going to kill the caller and reuse its process slot.
    processDescriptor = &schedulerState->allProcesses[coroutineId(caller)];

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
      = schedulerState->allProcesses[coroutineId(caller)].userId;

    if (coroutineInit(processDescriptor->coroutine, startCommand) == NULL) {
      printString("ERROR!!!  Could not configure coroutine for new command.\n");
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

    coroutineResume(processDescriptor->coroutine, comessage);
    returnValue = 0;

    if (schedulerAssignPortToPid(schedulerState,
      consolePort, processDescriptor->processId) != coroutineSuccess
    ) {
      printString("WARNING:  Could not assign console port to process.\n");
    }

    if (commandEntry->shellCommand == false) {
      // Put the coroutine on the ready queue.
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
      if (comessageRelease(comessage) != coroutineSuccess) {
        printString("ERROR!!!  "
          "Could not release message from handleSchedulerMessage "
          "for invalid message type.\n");
      }
    }
  }
  
  return returnValue;
}

/// @fn int schedulerKillProcessCommandHandler(
///   SchedulerState *schedulerState, Comessage *comessage)
///
/// @brief Kill a process identified by its process ID.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param comessage A pointer to the Comessage that was received that contains
///   the information about the process to kill.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerKillProcessCommandHandler(
  SchedulerState *schedulerState, Comessage *comessage
) {
  int returnValue = 0;

  Comessage *schedulerProcessCompleteMessage = getAvailableMessage();
  if (schedulerProcessCompleteMessage == NULL) {
    // We have to have a message to send to unblock the console.  Fail and try
    // again later.
    return EBUSY;
  }

  UserId callingUserId
    = allProcesses[coroutineId(comessageFrom(comessage))].userId;
  CoroutineId processId
    = nanoOsMessageDataValue(comessage, CoroutineId);
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);

  if ((processId >= NANO_OS_FIRST_PROCESS_ID)
    && (processId < NANO_OS_NUM_PROCESSES)
    && (coroutineRunning(allProcesses[processId].coroutine))
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
      comessageInit(comessage, MEMORY_MANAGER_FREE_PROCESS_MEMORY,
        nanoOsMessage, sizeof(*nanoOsMessage), /* waiting= */ true);
      sendComessageToCoroutine(
        schedulerState->allProcesses[
          NANO_OS_MEMORY_MANAGER_PROCESS_ID].coroutine,
        comessage);

      if (coroutineTerminate(allProcesses[processId].coroutine, NULL)
        == coroutineSuccess
      ) {
        allProcesses[processId].name = NULL;
        allProcesses[processId].userId = NO_USER_ID;

        processQueuePush(&schedulerState->free, &allProcesses[processId]);
      } else {
        // Tell the caller that we've failed.
        nanoOsMessage->data = 1;
        if (comessageSetDone(comessage) != coroutineSuccess) {
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
      if (comessageSetDone(comessage) != coroutineSuccess) {
        printString("ERROR!!!  Could not mark message done in "
          "schedulerKillProcessCommandHandler.\n");
      }
    }
  } else {
    // Tell the caller that we've failed.
    nanoOsMessage->data = EINVAL; // Invalid argument
    if (comessageSetDone(comessage) != coroutineSuccess) {
      printString("ERROR!!!  "
        "Could not mark message done in schedulerKillProcessCommandHandler.\n");
    }
  }

  // DO NOT release the message since that's done by the caller.

  return returnValue;
}

/// @fn int schedulerGetNumProcessDescriptoresCommandHandler(
///   SchedulerState *schedulerState, Comessage *comessage)
///
/// @brief Get the number of processes that are currently running in the system.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param comessage A pointer to the Comessage that was received.  This will be
///   reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerGetNumProcessDescriptoresCommandHandler(
  SchedulerState *schedulerState, Comessage *comessage
) {
  int returnValue = 0;

  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);

  uint8_t numProcessDescriptores = 0;
  for (int ii = 0; ii < NANO_OS_NUM_PROCESSES; ii++) {
    if (coroutineRunning(schedulerState->allProcesses[ii].coroutine)) {
      numProcessDescriptores++;
    }
  }
  nanoOsMessage->data = numProcessDescriptores;

  comessageSetDone(comessage);

  // DO NOT release the message since the caller is waiting on the response.

  return returnValue;
}

/// @fn int schedulerGetProcessInfoCommandHandler(
///   SchedulerState *schedulerState, Comessage *comessage)
///
/// @brief Fill in a provided array with information about the currently-running
/// processes.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param comessage A pointer to the Comessage that was received.  This will be
///   reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerGetProcessInfoCommandHandler(
  SchedulerState *schedulerState, Comessage *comessage
) {
  int returnValue = 0;

  ProcessInfo *processInfo
    = nanoOsMessageDataPointer(comessage, ProcessInfo*);
  int maxProcesses = processInfo->numProcesses;
  ProcessInfoElement *processes = processInfo->processes;
  int idx = 0;
  for (int ii = 0; (ii < NANO_OS_NUM_PROCESSES) && (idx < maxProcesses); ii++) {
    if (coroutineRunning(schedulerState->allProcesses[ii].coroutine)) {
      processes[idx].pid
        = (int) coroutineId(schedulerState->allProcesses[ii].coroutine);
      processes[idx].name = schedulerState->allProcesses[ii].name;
      processes[idx].userId = schedulerState->allProcesses[ii].userId;
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

/// @fn int schedulerGetProcessUserCommandHandler(
///   SchedulerState *schedulerState, Comessage *comessage)
///
/// @brief Get the number of processes that are currently running in the system.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param comessage A pointer to the Comessage that was received.  This will be
///   reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerGetProcessUserCommandHandler(
  SchedulerState *schedulerState, Comessage *comessage
) {
  int returnValue = 0;
  CoroutineId processId = coroutineId(comessageFrom(comessage));
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);
  if (processId < NANO_OS_NUM_PROCESSES) {
    nanoOsMessage->data = schedulerState->allProcesses[processId].userId;
  } else {
    nanoOsMessage->data = -1;
  }

  comessageSetDone(comessage);

  // DO NOT release the message since the caller is waiting on the response.

  return returnValue;
}

/// @fn int schedulerSetProcessUserCommandHandler(
///   SchedulerState *schedulerState, Comessage *comessage)
///
/// @brief Get the number of processes that are currently running in the system.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param comessage A pointer to the Comessage that was received.  This will be
///   reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerSetProcessUserCommandHandler(
  SchedulerState *schedulerState, Comessage *comessage
) {
  int returnValue = 0;
  CoroutineId processId = coroutineId(comessageFrom(comessage));
  UserId userId = nanoOsMessageDataValue(comessage, UserId);
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(comessage);
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

  comessageSetDone(comessage);

  // DO NOT release the message since the caller is waiting on the response.

  return returnValue;
}

/// @var schedulerCommandHandlers
///
/// @brief Array of function pointers for commands that are understood by the
/// message handler for the main loop function.
int (*schedulerCommandHandlers[])(SchedulerState*, Comessage*) = {
  schedulerRunProcessCommandHandler,        // SCHEDULER_RUN_PROCESS
  schedulerKillProcessCommandHandler,       // SCHEDULER_KILL_PROCESS
  // SCHEDULER_GET_NUM_RUNNING_PROCESSES:
  schedulerGetNumProcessDescriptoresCommandHandler,
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
      comessageQueuePush(NULL, message);
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
  void *coroutineReturnValue = NULL;
  ProcessDescriptor *processDescriptor = NULL;

  while (1) {
    processDescriptor = processQueuePop(&schedulerState->ready);
    coroutineReturnValue = coroutineResume(processDescriptor->coroutine, NULL);
    if (coroutineReturnValue == COROUTINE_CORRUPT) {
      printString("ERROR!!!  Coroutine corruption detected!!!\n");
      printString("          Removing coroutine ");
      printInt(processDescriptor->processId);
      printString(" from process queues.\n");

      processDescriptor->name = NULL;
      processDescriptor->userId = NO_USER_ID;
      processDescriptor->coroutine->state = COROUTINE_STATE_NOT_RUNNING;

      Comessage *schedulerProcessCompleteMessage = getAvailableMessage();
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

      Comessage *memoryManagerFreeProcessMemoryMessage = getAvailableMessage();
      if (memoryManagerFreeProcessMemoryMessage != NULL) {
        NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(
          memoryManagerFreeProcessMemoryMessage);
        nanoOsMessage->data = processDescriptor->processId;
        comessageInit(memoryManagerFreeProcessMemoryMessage,
          MEMORY_MANAGER_FREE_PROCESS_MEMORY,
          nanoOsMessage, sizeof(*nanoOsMessage), /* waiting= */ false);
        sendComessageToCoroutine(
          schedulerState->allProcesses[
            NANO_OS_MEMORY_MANAGER_PROCESS_ID].coroutine,
          memoryManagerFreeProcessMemoryMessage);
      } else {
        printString("WARNING:  Could not allocate "
          "memoryManagerFreeProcessMemoryMessage.  Memory leak.\n");
      }

      continue;
    }

    // Check the shells and restart them if needed.
    if ((processDescriptor->processId == USB_SERIAL_PORT_SHELL_PID)
      && (coroutineRunning(processDescriptor->coroutine) == false)
    ) {
      // Restart the shell.
      if (coroutineInit(processDescriptor->coroutine, runShell) == NULL) {
        printString("ERROR!!!  Could not configure coroutine for shell.\n");
      }
      processDescriptor->name = "USB shell";
    } else if ((processDescriptor->processId == GPIO_SERIAL_PORT_SHELL_PID)
      && (coroutineRunning(processDescriptor->coroutine) == false)
    ) {
      // Restart the shell.
      if (coroutineInit(processDescriptor->coroutine, runShell) == NULL) {
        printString("ERROR!!!  Could not configure coroutine for shell.\n");
      }
      processDescriptor->name = "GPIO shell";
    }

    /* if (coroutineReturnValue == COROUTINE_WAIT) {
      processQueuePush(&schedulerState->waiting, processDescriptor);
    } else if (coroutineReturnValue == COROUTINE_TIMEDWAIT) {
      processQueuePush(&schedulerState->timedWaiting, processDescriptor)
    } else */ if (coroutineFinished(processDescriptor->coroutine)) {
      processQueuePush(&schedulerState->free, processDescriptor);
    } else { // Process is still running.
      processQueuePush(&schedulerState->ready, processDescriptor);
    }

    handleSchedulerMessage(schedulerState);
  }
}

/// @fn void startScheduler(SchedulerState *schedulerState)
///
/// @brief Initialize and run the round-robin scheduler.
///
/// @return This function returns no value and, in fact, never returns at all.
__attribute__((noinline)) void startScheduler(SchedulerState *schedulerState) {
  // Initialize the static Comessage storage.
  Comessage messagesStorage[NANO_OS_NUM_MESSAGES] = {};
  messages = messagesStorage;

  // Initialize the allProcesses pointer.
  allProcesses = schedulerState->allProcesses;

  // Initialize ourself in the array of running commands.
  coroutineSetId(schedulerProcess, NANO_OS_SCHEDULER_PROCESS_ID);
  allProcesses[NANO_OS_SCHEDULER_PROCESS_ID].processId
    = NANO_OS_SCHEDULER_PROCESS_ID;
  allProcesses[NANO_OS_SCHEDULER_PROCESS_ID].coroutine = schedulerProcess;
  allProcesses[NANO_OS_SCHEDULER_PROCESS_ID].name = "scheduler";
  allProcesses[NANO_OS_SCHEDULER_PROCESS_ID].userId = ROOT_USER_ID;

  // Create the console process and start it.
  Coroutine *coroutine = coroutineInit(NULL, runConsole);
  coroutineSetId(coroutine, NANO_OS_CONSOLE_PROCESS_ID);
  allProcesses[NANO_OS_CONSOLE_PROCESS_ID].processId
    = NANO_OS_CONSOLE_PROCESS_ID;
  allProcesses[NANO_OS_CONSOLE_PROCESS_ID].coroutine = coroutine;
  allProcesses[NANO_OS_CONSOLE_PROCESS_ID].name = "console";
  allProcesses[NANO_OS_CONSOLE_PROCESS_ID].userId = ROOT_USER_ID;
  coroutineResume(coroutine, NULL);

  printString("\n");
  printString("Main stack size = ");
  printInt(ABS_DIFF(
    ((intptr_t) schedulerProcess),
    ((intptr_t) allProcesses[NANO_OS_CONSOLE_PROCESS_ID].coroutine)
  ));
  printString(" bytes\n");
  printString("schedulerState size = ");
  printInt(sizeof(SchedulerState));
  printString(" bytes\n");
  printString("messagesStorage size = ");
  printInt(sizeof(Comessage) * NANO_OS_NUM_MESSAGES);
  printString(" bytes\n");

  // We need to do an initial population of all the commands because we need to
  // get to the end of memory to run the memory manager in whatever is left
  // over.
  for (ProcessId ii = NANO_OS_FIRST_PROCESS_ID;
    ii < NANO_OS_NUM_PROCESSES;
    ii++
  ) {
    coroutine = coroutineInit(NULL, dummyProcess);
    coroutineSetId(coroutine, ii);
    allProcesses[ii].processId = ii;
    allProcesses[ii].coroutine = coroutine;
  }
  printString("Coroutine stack size = ");
  printInt(ABS_DIFF(
    ((uintptr_t) allProcesses[NANO_OS_FIRST_PROCESS_ID].coroutine),
    ((uintptr_t) allProcesses[NANO_OS_FIRST_PROCESS_ID + 1].coroutine))
    - sizeof(Coroutine)
  );
  printString(" bytes\n");
  printString("Coroutine size = ");
  printInt(sizeof(Coroutine));
  printString("\n");

  // Create the memory manager process.  !!! THIS MUST BE THE LAST PROCESS
  // CREATED BECAUSE WE WANT TO USE THE ENTIRE REST OF MEMORY FOR IT !!!
  coroutine = coroutineInit(NULL, runMemoryManager);
  coroutineSetId(coroutine, NANO_OS_MEMORY_MANAGER_PROCESS_ID);
  allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].coroutine
    = coroutine;
  allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].processId
    = NANO_OS_MEMORY_MANAGER_PROCESS_ID;
  allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].name
    = "memory manager";
  allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].userId
    = ROOT_USER_ID;
  // Assign the console ports to it.
  for (uint8_t ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
    if (schedulerAssignPortToPid(schedulerState,
      ii, NANO_OS_MEMORY_MANAGER_PROCESS_ID) != coroutineSuccess
    ) {
      printString(
        "WARNING:  Could not assign console port to memory manager.\n");
    }
  }
  coroutineResume(coroutine, NULL);

  // Set the shells for the ports.
  if (schedulerSetPortShell(schedulerState,
    USB_SERIAL_PORT, USB_SERIAL_PORT_SHELL_PID) != coroutineSuccess
  ) {
    printString("WARNING:  Could not set shell for USB serial port.\n");
    printString("          Undefined behavior will result.\n");
  }
  if (schedulerSetPortShell(schedulerState,
    GPIO_SERIAL_PORT, GPIO_SERIAL_PORT_SHELL_PID) != coroutineSuccess
  ) {
    printString("WARNING:  Could not set shell for GPIO serial port.\n");
    printString("          Undefined behavior will result.\n");
  }

  // Clear out all the dummy processes.
  for (int ii = NANO_OS_FIRST_PROCESS_ID; ii < NANO_OS_NUM_PROCESSES; ii++) {
    coroutineResume(allProcesses[ii].coroutine, NULL);
    allProcesses[ii].userId = NO_USER_ID;
  }
  // Repopulate them so that we can set their IDs correctly.
  for (ProcessId ii = NANO_OS_FIRST_PROCESS_ID;
    ii < NANO_OS_NUM_PROCESSES;
    ii++
  ) {
    coroutine = coroutineInit(NULL, dummyProcess);
    coroutineSetId(coroutine, ii);
    allProcesses[ii].coroutine = coroutine;
    allProcesses[ii].processId = ii;
  }
  // The scheduler will take care of cleaning them up.

  processQueuePush(&schedulerState->ready,
    &allProcesses[NANO_OS_CONSOLE_PROCESS_ID]);
  processQueuePush(&schedulerState->ready,
    &allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID]);
  processQueuePush(&schedulerState->ready,
    &allProcesses[USB_SERIAL_PORT_SHELL_PID]);
  processQueuePush(&schedulerState->ready,
    &allProcesses[GPIO_SERIAL_PORT_SHELL_PID]);
  for (ProcessId ii = GPIO_SERIAL_PORT_SHELL_PID + 1;
    ii < NANO_OS_NUM_PROCESSES;
    ii++
  ) {
    processQueuePush(&schedulerState->ready, &allProcesses[ii]);
  }

  // Start our scheduler.
  runScheduler(schedulerState);
}

