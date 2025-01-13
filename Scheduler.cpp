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
#include "NanoOs.h"
#include "Scheduler.h"
#include "Filesystem.h"

// Support prototypes.
void runScheduler(SchedulerState *schedulerState);

/// @def USB_SERIAL_PORT_SHELL_PID
///
/// @brief The process ID (PID) of the USB serial port shell.
#define USB_SERIAL_PORT_SHELL_PID 4

/// @def GPIO_SERIAL_PORT_SHELL_PID
///
/// @brief The process ID (PID) of the GPIO serial port shell.
#define GPIO_SERIAL_PORT_SHELL_PID 5

/// @def NUM_STANDARD_FILE_DESCRIPTORS
///
/// @brief The number of file descriptors a process usually starts out with.
#define NUM_STANDARD_FILE_DESCRIPTORS 3

/// @def STDIN_FILE_DESCRIPTOR_INDEX
///
/// @brief Index into a ProcessDescriptor's fileDescriptors array that holds the
/// FileDescriptor object that maps to the process's stdin FILE stream.
#define STDIN_FILE_DESCRIPTOR_INDEX 0

/// @def STDOUT_FILE_DESCRIPTOR_INDEX
///
/// @brief Index into a ProcessDescriptor's fileDescriptors array that holds the
/// FileDescriptor object that maps to the process's stdout FILE stream.
#define STDOUT_FILE_DESCRIPTOR_INDEX 1

/// @def STDERR_FILE_DESCRIPTOR_INDEX
///
/// @brief Index into a ProcessDescriptor's fileDescriptors array that holds the
/// FileDescriptor object that maps to the process's stderr FILE stream.
#define STDERR_FILE_DESCRIPTOR_INDEX 2

/// @var schedulerProcess
///
/// @brief Pointer to the main process object that's allocated in the main loop
/// function.
ProcessHandle schedulerProcess = NULL;

/// @var allProcesses
///
/// @brief Pointer to the allProcesses array that is part of the
/// SchedulerState object maintained by the scheduler process.  This is needed
/// in order to do lookups from process IDs to process object pointers.
static ProcessDescriptor *allProcesses = NULL;

/// @var standardKernelFileDescriptors
///
/// @brief The array of file descriptors that all kernel processes use.
const static FileDescriptor standardKernelFileDescriptors[
  NUM_STANDARD_FILE_DESCRIPTORS
] = {
  {
    // stdin
    // Kernel processes do not read from stdin, so clear out both pipes.
    .inputPipe = {
      .processId = PROCESS_ID_NOT_SET,
      .messageType = 0,
    },
    .outputPipe = {
      .processId = PROCESS_ID_NOT_SET,
      .messageType = 0,
    },
  },
  {
    // stdout
    // Uni-directional FileDescriptor, so clear the input pipe and direct the
    // output pipe to the console.
    .inputPipe = {
      .processId = PROCESS_ID_NOT_SET,
      .messageType = 0,
    },
    .outputPipe = {
      .processId = NANO_OS_CONSOLE_PROCESS_ID,
      .messageType = CONSOLE_WRITE_BUFFER,
    },
  },
  {
    // stderr
    // Uni-directional FileDescriptor, so clear the input pipe and direct the
    // output pipe to the console.
    .inputPipe = {
      .processId = PROCESS_ID_NOT_SET,
      .messageType = 0,
    },
    .outputPipe = {
      .processId = NANO_OS_CONSOLE_PROCESS_ID,
      .messageType = CONSOLE_WRITE_BUFFER,
    },
  },
};

/// @var standardUserFileDescriptors
///
/// @brief Pointer to the array of FileDescriptor objects (declared in the
/// startScheduler function on the scheduler's stack) that all processes start
/// out with.
//// static FileDescriptor *standardUserFileDescriptors = NULL;
const static FileDescriptor standardUserFileDescriptors[
  NUM_STANDARD_FILE_DESCRIPTORS
] = {
  {
    // stdin
    // Uni-directional FileDescriptor, so clear the output pipe and direct the
    // input pipe to the console.
    .inputPipe = {
      .processId = NANO_OS_CONSOLE_PROCESS_ID,
      .messageType = CONSOLE_WAIT_FOR_INPUT,
    },
    .outputPipe = {
      .processId = PROCESS_ID_NOT_SET,
      .messageType = 0,
    },
  },
  {
    // stdout
    // Uni-directional FileDescriptor, so clear the input pipe and direct the
    // output pipe to the console.
    .inputPipe = {
      .processId = PROCESS_ID_NOT_SET,
      .messageType = 0,
    },
    .outputPipe = {
      .processId = NANO_OS_CONSOLE_PROCESS_ID,
      .messageType = CONSOLE_WRITE_BUFFER,
    },
  },
  {
    // stderr
    // Uni-directional FileDescriptor, so clear the input pipe and direct the
    // output pipe to the console.
    .inputPipe = {
      .processId = PROCESS_ID_NOT_SET,
      .messageType = 0,
    },
    .outputPipe = {
      .processId = NANO_OS_CONSOLE_PROCESS_ID,
      .messageType = CONSOLE_WRITE_BUFFER,
    },
  },
};

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
  SchedulerState *schedulerState = NULL;
  ProcessDescriptor *poppedDescriptor = NULL;
  ProcessQueue *processQueue = NULL;
  
  if ((stateData == NULL) || (comutex == NULL)) {
    // We can't work like this.  Bail.
    return;
  } else if (comutex->head == NULL) {
    // This should be impossible.  If it happens, though, there's no point in
    // the rest of the function, so bail.
    return;
  }

  schedulerState = *((SchedulerState**) stateData);
  if (schedulerState == NULL) {
    // This won't fly either.  Bail.
    return;
  }

  processQueue = &schedulerState->waiting;
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
      processQueuePush(processQueue, poppedDescriptor);
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
void coconditionSignalCallback(void *stateData, Cocondition *cocondition) {
  SchedulerState *schedulerState = NULL;
  ProcessDescriptor *poppedDescriptor = NULL;
  ProcessQueue *processQueue = NULL;
  ProcessHandle cur = NULL;

  if ((stateData == NULL) || (cocondition == NULL)) {
    // We can't work like this.  Bail.
    return;
  } else if (cocondition->head == NULL) {
    // This should be impossible.  If it happens, though, there's no point in
    // the rest of the function, so bail.
    return;
  }

  schedulerState = *((SchedulerState**) stateData);
  if (schedulerState == NULL) {
    // This won't fly either.  Bail.
    return;
  }

  cur = cocondition->head;
  for (int ii = 0; (ii < cocondition->numSignals) && (cur != NULL); ii++) {
    processQueue = &schedulerState->waiting;
    while (1) {
      // See note above about using a member element in a for loop.
      for (uint8_t ii = 0; ii < processQueue->numElements; ii++) {
        poppedDescriptor = processQueuePop(processQueue);
        if (poppedDescriptor->processHandle == cur) {
          // We found the process that will get the lock next.  Push it onto the
          // ready queue and exit.
          processQueuePush(&schedulerState->ready, poppedDescriptor);
          goto endOfLoop;
        }
        processQueuePush(processQueue, poppedDescriptor);
      }

      if (processQueue == &schedulerState->waiting) {
        // We searched the entire waiting queue and found nothing.  Try the
        // timed waiting queue.
        processQueue = &schedulerState->timedWaiting;
      } else {
        // We've searched all the queues and found nothing.  We're done.
        break;
      }
    }

endOfLoop:
    cur = cur->nextToSignal;
  }

  return;
}

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

  //// printDebugStackDepth();

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
  // processMessage->from would normally be set when we do a
  // processMessageQueuePush. We're not using that mechanism here, so we have
  // to do it manually.  If we don't do this, then commands that validate that
  // the message came from the scheduler will fail.
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
  processMessageInit(
    &processMessage, type, &nanoOsMessage, sizeof(nanoOsMessage), true);

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

/// @fn void* schedulerResumeReallocMessage(void *ptr, size_t size)
///
/// @brief Send a MEMORY_MANAGER_REALLOC command to the memory manager process
/// by resuming it with the message and get a reply.
///
/// @param ptr The pointer to send to the process.
/// @param size The size to send to the process.
///
/// @return Returns the data pointer returned in the reply.
void* schedulerResumeReallocMessage(void *ptr, size_t size) {
  void *returnValue = NULL;
  
  ReallocMessage reallocMessage;
  reallocMessage.ptr = ptr;
  reallocMessage.size = size;
  reallocMessage.responseType = MEMORY_MANAGER_RETURNING_POINTER;
  
  ProcessMessage *sent = getAvailableMessage();
  if (sent == NULL) {
    // Nothing we can do.  The scheduler can't yield.  Bail.
    return returnValue;
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(sent);
  nanoOsMessage->data = (NanoOsMessageData) &reallocMessage;
  processMessageInit(sent, MEMORY_MANAGER_REALLOC,
    nanoOsMessage, sizeof(*nanoOsMessage), true);
  // sent->from would normally be set during processMessageQueuePush.  We're
  // not using that mechanism here, so we have to do it manually.  Things will
  // get messed up if we don't.
  sent->from = schedulerProcess;

  coroutineResume(
    allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].processHandle,
    sent);
  if (processMessageDone(sent) == true) {
    // The handler set the pointer back in the structure we sent it, so grab it
    // out of the structure we already have.
    returnValue = reallocMessage.ptr;
  } else {
    printString(
      "Warning!!!  Memory manager did not mark realloc message done.\n");
  }
  // The handler pushes the message back onto our queue, which is not what we
  // want.  Pop it off again.
  processMessageQueuePop();
  processMessageRelease(sent);

  // The message that was sent to us is the one that we allocated on the stack,
  // so, there's no reason to call processMessageRelease here.
  
  return returnValue;
}

/// @fn void* krealloc(void *ptr, size_t size)
///
/// @brief Reallocate a provided pointer to a new size.
///
/// @param ptr A pointer to the original block of dynamic memory.  If this value
///   is NULL, new memory will be allocated.
/// @param size The new size desired for the memory block at ptr.  If this value
///   is 0, the provided pointer will be freed.
///
/// @return Returns a pointer to size-adjusted memory on success, NULL on
/// failure or free.
void* krealloc(void *ptr, size_t size) {
  return schedulerResumeReallocMessage(ptr, size);
}

/// @fn void* kmalloc(size_t size)
///
/// @brief Allocate but do not clear memory.
///
/// @param size The size of the block of memory to allocate in bytes.
///
/// @return Returns a pointer to newly-allocated memory of the specified size
/// on success, NULL on failure.
void* kmalloc(size_t size) {
  return schedulerResumeReallocMessage(NULL, size);
}

/// @fn void* kcalloc(size_t nmemb, size_t size)
///
/// @brief Allocate memory and clear all the bytes to 0.
///
/// @param nmemb The number of elements to allocate in the memory block.
/// @param size The size of each element to allocate in the memory block.
///
/// @return Returns a pointer to zeroed newly-allocated memory of the specified
/// size on success, NULL on failure.
void* kcalloc(size_t nmemb, size_t size) {
  size_t totalSize = nmemb * size;
  void *returnValue = schedulerResumeReallocMessage(NULL, totalSize);
  
  if (returnValue != NULL) {
    memset(returnValue, 0, totalSize);
  }
  return returnValue;
}

/// @fn void kfree(void *ptr)
///
/// @brief Free a piece of memory using mechanisms available to the scheduler.
///
/// @param ptr The pointer to the memory to free.
///
/// @return This function returns no value.
void kfree(void *ptr) {
  ProcessMessage *sent = getAvailableMessage();
  if (sent == NULL) {
    // Nothing we can do.  The scheduler can't yield.  Bail.
    return;
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(sent);
  nanoOsMessage->data = (NanoOsMessageData) ((intptr_t) ptr);
  processMessageInit(sent, MEMORY_MANAGER_FREE,
    nanoOsMessage, sizeof(*nanoOsMessage), true);
  // sent->from would normally be set during processMessageQueuePush.  We're
  // not using that mechanism here, so we have to do it manually.  Things will
  // get messed up if we don't.
  sent->from = schedulerProcess;

  coroutineResume(
    allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].processHandle,
    sent);
  if (processMessageDone(sent) == false) {
    printString(
      "Warning!!!  Memory manager did not mark free message done.\n");
  }
  processMessageRelease(sent);

  return;
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

/// @fn int schedulerAssignPortInputToPid(
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
int schedulerAssignPortInputToPid(
  SchedulerState *schedulerState,
  uint8_t consolePort, ProcessId owner
) {
  ConsolePortPidUnion consolePortPidUnion;
  consolePortPidUnion.consolePortPidAssociation.consolePort
    = consolePort;
  consolePortPidUnion.consolePortPidAssociation.processId = owner;

  int returnValue = schedulerSendNanoOsMessageToPid(schedulerState,
    NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_ASSIGN_PORT_INPUT,
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
  ProcessMessage *processMessage = NULL;
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
    goto exit;
  }

  // It is possible, although unlikely, that an additional process is started
  // between the time we made the call above and the time that our message gets
  // handled below.  We allocated our return value based upon the size that was
  // returned above and, if we're not careful, it will be possible to overflow
  // the array.  Initialize processInfo->numProcesses so that
  // schedulerGetProcessInfoCommandHandler knows the maximum number of
  // ProcessInfoElements it can populated.
  processInfo->numProcesses = numProcessDescriptors;

  processMessage
    = sendNanoOsMessageToPid(NANO_OS_SCHEDULER_PROCESS_ID,
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
    goto releaseMessage;
  }

  if (processMessageRelease(processMessage) != processSuccess) {
    printf("ERROR!!!  Could not release message sent to scheduler for "
      "getting the number of running processes.\n");
  }

  return processInfo;

releaseMessage:
  if (processMessageRelease(processMessage) != processSuccess) {
    printf("ERROR!!!  Could not release message sent to scheduler for "
      "getting the number of running processes.\n");
  }

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
    (NanoOsMessageData) 0, (NanoOsMessageData) processId, true);
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
    NanoOsMessage *nanoOsMessage
      = (NanoOsMessage*) processMessageData(processMessage);
    returnValue = nanoOsMessage->data;
    if (returnValue == 0) {
      printf("Process terminated.\n");
    } else {
      printf("Process termination returned status \"%s\".\n",
        strerror(returnValue));
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

/// @fn int schedulerRunProcess(const CommandEntry *commandEntry,
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
int schedulerRunProcess(const CommandEntry *commandEntry,
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
      strerror(returnValue));
  }

  return returnValue;
}

/// @fn FileDescriptor* schedulerGetFileDescriptor(FILE *stream)
///
/// @brief Get the IoPipe object for a process given a pointer to the FILE
///   stream to write to.
///
/// @param stream A pointer to the desired FILE output stream (stdout or
///   stderr).
///
/// @return Returns the appropriate FileDescriptor object for the current
/// process on success, NULL on failure.
FileDescriptor* schedulerGetFileDescriptor(FILE *stream) {
  FileDescriptor *returnValue = NULL;
  uintptr_t fdIndex = (uintptr_t) stream;
  ProcessId runningProcessId = getRunningProcessId();

  if (fdIndex <= allProcesses[runningProcessId].numFileDescriptors) {
    returnValue = &allProcesses[runningProcessId].fileDescriptors[fdIndex - 1];
  } else {
    printString("ERROR:  Received request for unknown stream ");
    printInt((intptr_t) stream);
    printString(".\n");
  }

  return returnValue;
}

/// @fn int schedulerCloseAllFileDescriptors(void)
///
/// @brief Close all the open file descriptors for the currently-running
/// process.
///
/// @return Returns 0 on success, -1 on failure.
int schedulerCloseAllFileDescriptors(void) {
  ProcessMessage *processMessage = sendNanoOsMessageToPid(
    NANO_OS_SCHEDULER_PROCESS_ID, SCHEDULER_CLOSE_ALL_FILE_DESCRIPTORS,
    /* func= */ 0, /* data= */ 0, true);
  processMessageWaitForDone(processMessage, NULL);
  processMessageRelease(processMessage);

  return 0;
}

// Scheduler command handlers and support functions

/// @fn void handleOutOfSlots(ProcessMessage *processMessage, char *commandLine)
///
/// @brief Handle the exception case when we're out of free process slots to run
/// all the commands we've been asked to launch.  Releases all relevant messages
/// and frees all relevant memory.
///
/// @param processMessage A pointer to the ProcessMessage that was received
///   that contains the information about the process to run and how to run it.
/// @param commandLine The part of the console input that was being worked on
///   at the time of the failure.
///
/// @return This function returns no value.
void handleOutOfSlots(ProcessMessage *processMessage, char *commandLine) {
  CommandDescriptor *commandDescriptor
    = nanoOsMessageDataPointer(processMessage, CommandDescriptor*);

  // printf sends synchronous messages to the console, which we can't do.
  // Use the non-blocking printString instead.
  printString("Out of process slots to launch process.\n");
  sendNanoOsMessageToPid(commandDescriptor->callingProcess,
    SCHEDULER_PROCESS_COMPLETE, 0, 0, true);
  commandLine = stringDestroy(commandLine);
  free(commandDescriptor); commandDescriptor = NULL;
  if (processMessageRelease(processMessage) != processSuccess) {
    printString("ERROR!!!  "
      "Could not release message from handleSchedulerMessage "
      "for invalid message type.\n");
  }

  return;
}

/// @fn ProcessDescriptor* launchProcess(SchedulerState *schedulerState,
///   ProcessMessage *processMessage, CommandDescriptor *commandDescriptor,
///   ProcessDescriptor *processDescriptor, bool backgroundProcess)
///
/// @brief Run the specified command line with the specified ProcessDescriptor.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param processMessage A pointer to the ProcessMessage that was received
///   that contains the information about the process to run and how to run it.
/// @param commandDescriptor The CommandDescriptor that was sent via the
///   processMessage to the scueduler.
/// @param processDescriptor A pointer to the available processDescriptor to
///   run the command with.
/// @param backgroundProcess Whether or not the process is to be launched as a
///   background process.  If this value is false then it will not be assigned
///   to a console port.
///
/// @return Returns a pointer to the ProcessDescriptor used to launch the
/// process on success, NULL on failure.
static inline ProcessDescriptor* launchProcess(SchedulerState *schedulerState,
  ProcessMessage *processMessage, CommandDescriptor *commandDescriptor,
  ProcessDescriptor *processDescriptor, bool backgroundProcess
) {
  ProcessDescriptor *returnValue = processDescriptor;
  CommandEntry *commandEntry
    = nanoOsMessageFuncPointer(processMessage, CommandEntry*);

  if (processDescriptor != NULL) {
    processDescriptor->userId = schedulerState->allProcesses[
      processId(processMessageFrom(processMessage))].userId;
    processDescriptor->numFileDescriptors = NUM_STANDARD_FILE_DESCRIPTORS;
    processDescriptor->fileDescriptors
      = (FileDescriptor*) standardUserFileDescriptors;

    if (processCreate(&processDescriptor->processHandle,
      startCommand, processMessage) == processError
    ) {
      printString(
        "ERROR!!!  Could not configure process handle for new command.\n");
    }
    if (assignMemory(commandDescriptor->consoleInput,
      processDescriptor->processId) != 0
    ) {
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

    if (backgroundProcess == false) {
      if (schedulerAssignPortToPid(schedulerState,
        commandDescriptor->consolePort, processDescriptor->processId)
        != processSuccess
      ) {
        printString("WARNING:  Could not assign console port to process.\n");
      }
    }

    // Resume the coroutine so that it picks up all the pointers it needs.
    coroutineResume(processDescriptor->processHandle, NULL);

    // Put the process on the ready queue.
    processQueuePush(&schedulerState->ready, processDescriptor);
  } else {
    returnValue = NULL;
  }

  return returnValue;
}

/// @fn ProcessDescriptor* launchForegroundProcess(
///   SchedulerState *schedulerState, ProcessMessage *processMessage,
///   CommandDescriptor *commandDescriptor)
///
/// @brief Kill the sender of the ProcessMessage and use its ProcessDescriptor
/// to run the specified command line.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param processMessage A pointer to the ProcessMessage that was received
///   that contains the information about the process to run and how to run it.
/// @param commandDescriptor The CommandDescriptor that was sent via the
///   processMessage to the scueduler.
///
/// @return Returns a pointer to the ProcessDescriptor used to launch the
/// process on success, NULL on failure.
static inline ProcessDescriptor* launchForegroundProcess(
  SchedulerState *schedulerState, ProcessMessage *processMessage,
  CommandDescriptor *commandDescriptor
) {
  ProcessDescriptor *processDescriptor = &schedulerState->allProcesses[
    processId(processMessageFrom(processMessage))];
  // The process should be blocked in processMessageQueueWaitForType waiting
  // on a condition with an infinite timeout.  So, it *SHOULD* be on the
  // waiting queue.  Take no chances, though.
  (processQueueRemove(&schedulerState->waiting, processDescriptor) == 0)
    || (processQueueRemove(&schedulerState->timedWaiting, processDescriptor)
      == 0)
    || processQueueRemove(&schedulerState->ready, processDescriptor);

  // Protect the relevant memory from deletion below.
  if (assignMemory(commandDescriptor->consoleInput,
    NANO_OS_SCHEDULER_PROCESS_ID) != 0
  ) {
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
  processTerminate(processMessageFrom(processMessage));
  processSetId(
    processDescriptor->processHandle, processDescriptor->processId);

  // We don't want to wait for the memory manager to release the memory.  Make
  // it do it immediately.
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

  return launchProcess(schedulerState, processMessage, commandDescriptor,
    processDescriptor, false);
}

/// @fn ProcessDescriptor* launchBackgroundProcess(
///   SchedulerState *schedulerState, ProcessMessage *processMessage,
///   CommandDescriptor *commandDescriptor)
///
/// @brief Pop a process off of the free queue and use it to run the specified
/// command line.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param processMessage A pointer to the ProcessMessage that was received
///   that contains the information about the process to run and how to run it.
/// @param commandDescriptor The CommandDescriptor that was sent via the
///   processMessage to the scueduler.
///
/// @return Returns a pointer to the ProcessDescriptor used to launch the
/// process on success, NULL on failure.
static inline ProcessDescriptor* launchBackgroundProcess(
  SchedulerState *schedulerState, ProcessMessage *processMessage,
  CommandDescriptor *commandDescriptor
) {
  return launchProcess(schedulerState, processMessage, commandDescriptor,
    processQueuePop(&schedulerState->free), true);
}

/// @fn int closeProcessFileDescriptors(
///   SchedulerState *schedulerState, ProcessDescriptor *processDescriptor)
///
/// @brief Helper function to close out the file descriptors owned by a process
/// when it exits or is killed.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param processDescriptor A pointer to the ProcessDescriptor that holds the
///   fileDescriptors array to close.
///
/// @return Returns 0 on success, -1 on failure.
int closeProcessFileDescriptors(
  SchedulerState *schedulerState, ProcessDescriptor *processDescriptor
) {
  FileDescriptor *fileDescriptors = processDescriptor->fileDescriptors;
  if (fileDescriptors != standardUserFileDescriptors) {
    ProcessMessage *messageToSend = getAvailableMessage();
    while (messageToSend == NULL) {
      runScheduler(schedulerState);
      messageToSend = getAvailableMessage();
    }

    uint8_t numFileDescriptors = processDescriptor->numFileDescriptors;
    for (uint8_t ii = 0; ii < numFileDescriptors; ii++) {
      ProcessId waitingProcessId = fileDescriptors[ii].outputPipe.processId;
      if ((waitingProcessId == PROCESS_ID_NOT_SET)
        || (waitingProcessId == NANO_OS_CONSOLE_PROCESS_ID)
      ) {
        // Nothing waiting on output from this file descriptor.  Move on.
        continue;
      }
      ProcessDescriptor *waitingProcessDescriptor
        = &schedulerState->allProcesses[waitingProcessId];

      // Clear the processId of the waiting process's stdin file descriptor.
      waitingProcessDescriptor->fileDescriptors[
        STDIN_FILE_DESCRIPTOR_INDEX].
        inputPipe.processId = PROCESS_ID_NOT_SET;

      // Send an empty message to the waiting process so that it will become
      // unblocked.
      processMessageInit(messageToSend,
          fileDescriptors[ii].outputPipe.messageType,
          /*data= */ NULL, /* size= */ 0, /* waiting= */ false);
      processMessageQueuePush(
        waitingProcessDescriptor->processHandle, messageToSend);
      // Give the process a chance to unblock.
      coroutineResume(waitingProcessDescriptor->processHandle, NULL);

      // The function that was waiting should have released the message we
      // sent it.  Get another one.
      messageToSend = getAvailableMessage();
      while (messageToSend == NULL) {
        runScheduler(schedulerState);
        messageToSend = getAvailableMessage();
      }
    }

    // kfree will pull an available message.  Release the one we've been
    // using so that we're guaranteed it will be successful.
    processMessageRelease(messageToSend);
    kfree(fileDescriptors); processDescriptor->fileDescriptors = NULL;
  }

  return 0;
}

/// @fn FILE* kfopen(SchedulerState *schedulerState,
///   const char *pathname, const char *mode)
///
/// @brief Version of fopen for the scheduler.
///
/// @param schedulerState A pointer to the SchedulerState object maintained by
///   the scheduler process.
/// @param pathname A pointer to the C string with the full path to the file to
///   open.
/// @param mode A pointer to the C string that defines the way to open the file.
///
/// @return Returns a pointer to the opened file on success, NULL on failure.
FILE* kfopen(SchedulerState *schedulerState,
  const char *pathname, const char *mode
) {
  FILE *returnValue = NULL;
  ProcessMessage *processMessage = getAvailableMessage();
  while (processMessage == NULL) {
    runScheduler(schedulerState);
    processMessage = getAvailableMessage();
  }
  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->func = (intptr_t) mode;
  nanoOsMessage->data = (intptr_t) pathname;
  processMessageInit(processMessage, FILESYSTEM_OPEN_FILE,
    nanoOsMessage, sizeof(*nanoOsMessage), true);
  coroutineResume(
    schedulerState->allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID].processHandle,
    processMessage);

  while (processMessageDone(processMessage) == false) {
    runScheduler(schedulerState);
  }

  returnValue = nanoOsMessageDataPointer(processMessage, FILE*);

  processMessageRelease(processMessage);
  return returnValue;
}

/// @fn int kfclose(SchedulerState *schedulerState, FILE *stream)
///
/// @brief Version of fclose for the scheduler.
///
/// @param schedulerState A pointer to the SchedulerState object maintained by
///   the scheduler process.
///
/// @param stream A pointer to the FILE object that was previously opened.
///
/// @return Returns 0 on success, EOF on failure.  On failure, the value of
/// errno is also set to the appropriate error.
int kfclose(SchedulerState *schedulerState, FILE *stream) {
  int returnValue = 0;
  ProcessMessage *processMessage = getAvailableMessage();
  while (processMessage == NULL) {
    runScheduler(schedulerState);
    processMessage = getAvailableMessage();
  }
  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = (intptr_t) stream;
  processMessageInit(processMessage, FILESYSTEM_CLOSE_FILE,
    nanoOsMessage, sizeof(*nanoOsMessage), true);
  coroutineResume(
    schedulerState->allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID].processHandle,
    processMessage);

  while (processMessageDone(processMessage) == false) {
    runScheduler(schedulerState);
  }

  processMessageRelease(processMessage);
  return returnValue;
}

/// @fn int schedulerRunProcessCommandHandler(
///   SchedulerState *schedulerState, ProcessMessage *processMessage)
///
/// @brief Run a process in an appropriate process slot.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param processMessage A pointer to the ProcessMessage that was received
///   that contains the information about the process to run and how to run it.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerRunProcessCommandHandler(
  SchedulerState *schedulerState, ProcessMessage *processMessage
) {
  if (processMessage == NULL) {
    // This should be impossible, but there's nothing to do.  Return good
    // status.
    return 0;
  }

  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);
  CommandDescriptor *commandDescriptor
    = nanoOsMessageDataPointer(processMessage, CommandDescriptor*);
  char *consoleInput = commandDescriptor->consoleInput;
  if (assignMemory(consoleInput, NANO_OS_SCHEDULER_PROCESS_ID) != 0) {
    printString("WARNING:  Could not assign consoleInput to scheduler.\n");
    printString("Undefined behavior.\n");
  }
  commandDescriptor->schedulerState = schedulerState;
  bool backgroundProcess = false;
  char *charAt = NULL;
  ProcessDescriptor *curProcessDescriptor = NULL;
  ProcessDescriptor *prevProcessDescriptor = NULL;
  char *commandLine = NULL;

  if (consoleInput == NULL) {
    // We can't parse or handle NULL input.  Bail.
    handleOutOfSlots(processMessage, consoleInput);
    free(commandDescriptor); commandDescriptor = NULL;
    return 0;
  } else if (getNumPipes(consoleInput)
    > schedulerState->free.numElements
  ) {
    // We've been asked to run more processes chained together than we can
    // currently launch.  Fail.
    handleOutOfSlots(processMessage, consoleInput);
    free(commandDescriptor); commandDescriptor = NULL;
    return 0;
  }

  charAt = strchr(consoleInput, '&');
  if (charAt != NULL) {
    charAt++;
    if (charAt[strspn(charAt, " \t\r\n")] == '\0') {
      backgroundProcess = true;
    }
  }

  while (*consoleInput != '\0') {
    charAt = strrchr(consoleInput, '|');
    if (charAt == NULL) {
      // This is the usual case, so list it first.
      commandLine = (char*) kmalloc(strlen(consoleInput) + 1);
      strcpy(commandLine, consoleInput);
      *consoleInput = '\0';
    } else {
      // This is the last command in a chain of pipes.
      *charAt = '\0';
      charAt++;
      charAt = &charAt[strspn(charAt, " \t\r\n")];
      commandLine = (char*) kmalloc(strlen(charAt) + 1);
      strcpy(commandLine, charAt);
    }

    const CommandEntry *commandEntry = getCommandEntryFromInput(commandLine);
    nanoOsMessage->func = (intptr_t) commandEntry;
    commandDescriptor->consoleInput = commandLine;

    if (backgroundProcess == false) {
      // Task is a foreground process.  We're going to kill the caller and reuse
      // its process slot.  This is expected to be the usual case, so list it
      // first.
      curProcessDescriptor = launchForegroundProcess(
        schedulerState, processMessage, commandDescriptor);

      // Any process after the first one (if we're connecting pipes) will have
      // to be a background process, so set backgroundProcess to true.
      backgroundProcess = true;
    } else {
      // Task is a background process.  Get a process off the free queue.
      curProcessDescriptor = launchBackgroundProcess(
        schedulerState, processMessage, commandDescriptor);
    }
    if (curProcessDescriptor == NULL) {
      commandLine = stringDestroy(commandLine);
      handleOutOfSlots(processMessage, consoleInput);
      break;
    }

    if (prevProcessDescriptor != NULL) {
      // We're piping two or more commands together and we need to connect the
      // pipes.
      FileDescriptor *fileDescriptors = NULL;
      if (
        prevProcessDescriptor->fileDescriptors == standardUserFileDescriptors
      ) {
        // We need to make a copy of the previous process descriptor's file
        // descriptors.
        fileDescriptors = (FileDescriptor*) kmalloc(
          NUM_STANDARD_FILE_DESCRIPTORS * sizeof(FileDescriptor));
        memcpy(fileDescriptors, prevProcessDescriptor->fileDescriptors,
          NUM_STANDARD_FILE_DESCRIPTORS * sizeof(FileDescriptor));
        prevProcessDescriptor->fileDescriptors = fileDescriptors;
      }
      prevProcessDescriptor->fileDescriptors[
        STDIN_FILE_DESCRIPTOR_INDEX].inputPipe.processId
        = curProcessDescriptor->processId;
      prevProcessDescriptor->fileDescriptors[
        STDIN_FILE_DESCRIPTOR_INDEX].inputPipe.messageType
        = 0;

      fileDescriptors = (FileDescriptor*) kmalloc(
        NUM_STANDARD_FILE_DESCRIPTORS * sizeof(FileDescriptor));
      memcpy(fileDescriptors, standardUserFileDescriptors,
        NUM_STANDARD_FILE_DESCRIPTORS * sizeof(FileDescriptor));
      curProcessDescriptor->fileDescriptors = fileDescriptors;
      curProcessDescriptor->fileDescriptors[
        STDOUT_FILE_DESCRIPTOR_INDEX].outputPipe.processId
        = prevProcessDescriptor->processId;
      curProcessDescriptor->fileDescriptors[
        STDOUT_FILE_DESCRIPTOR_INDEX].outputPipe.messageType
        = CONSOLE_RETURNING_INPUT;
      if (schedulerAssignPortInputToPid(schedulerState,
        commandDescriptor->consolePort, curProcessDescriptor->processId)
        != processSuccess
      ) {
        printString(
          "WARNING:  Could not assign console port input to process.\n");
      }
    }

    prevProcessDescriptor = curProcessDescriptor;
  }

  // We're done with our copy of the console input.  The process(es) will free
  // its/their copy/copies.
  consoleInput = stringDestroy(consoleInput);

  processMessageRelease(processMessage);
  free(commandDescriptor); commandDescriptor = NULL;
  return 0;
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
  NanoOsMessage *nanoOsMessage
    = (NanoOsMessage*) processMessageData(processMessage);

  if ((processId >= NANO_OS_FIRST_USER_PROCESS_ID)
    && (processId < NANO_OS_NUM_PROCESSES)
    && (processRunning(allProcesses[processId].processHandle))
  ) {
    if ((allProcesses[processId].userId == callingUserId)
      || (callingUserId == ROOT_USER_ID)
    ) {
      ProcessDescriptor *processDescriptor = &allProcesses[processId];
      // Regardless of whether or not we succeed at terminating it, we have
      // to remove it from its queue.  We don't know which queue it's on,
      // though.  The fact that we're killing it makes it likely that it's hung.
      // The most likely reason is that it's waiting on something with an
      // infinite timeout, so it's most likely to be on the waiting queue.  The
      // second most likely reason is that it's in an infinite loop, so the
      // ready queue is the second-most-likely place it could be.  The least-
      // likely place for it to be would be the timed waiting queue with a very
      // long timeout.  So, attempt to remove from the queues in that order.
      (processQueueRemove(&schedulerState->waiting, processDescriptor) == 0)
      || (processQueueRemove(&schedulerState->ready, processDescriptor) == 0)
      || processQueueRemove(&schedulerState->timedWaiting, processDescriptor);

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

      // Close the file descriptors before we terminate the process so that
      // anything that gets sent to the process's queue gets cleaned up when
      // we terminate it.
      closeProcessFileDescriptors(schedulerState, processDescriptor);

      if (processTerminate(processDescriptor->processHandle)
        == processSuccess
      ) {
        processSetId(
          processDescriptor->processHandle, processDescriptor->processId);
        processDescriptor->name = NULL;
        processDescriptor->userId = NO_USER_ID;

        if ((processId != USB_SERIAL_PORT_SHELL_PID)
          && (processId != GPIO_SERIAL_PORT_SHELL_PID)
        ) {
          // The expected case.
          processQueuePush(&schedulerState->free, processDescriptor);
        } else {
          // The killed process is a shell command.  The scheduler is
          // responsible for detecting that it's not running and restarting it.
          // However, the scheduler only ever pops anything from the ready
          // queue.  So, push this back onto the ready queue instead of the free
          // queue this time.
          processQueuePush(&schedulerState->ready, processDescriptor);
        }
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
    } else {
      // Tell the caller that we've failed.
      nanoOsMessage->data = EACCES; // Permission denied
      if (processMessageSetDone(processMessage) != processSuccess) {
        printString("ERROR!!!  Could not mark message done in "
          "schedulerKillProcessCommandHandler.\n");
      }
      if (processMessageRelease(schedulerProcessCompleteMessage)
        != processSuccess
      ) {
        printString("ERROR!!!  "
          "Could not release schedulerProcessCompleteMessage.\n");
      }
    }
  } else {
    // Tell the caller that we've failed.
    nanoOsMessage->data = EINVAL; // Invalid argument
    if (processMessageSetDone(processMessage) != processSuccess) {
      printString("ERROR!!!  "
        "Could not mark message done in schedulerKillProcessCommandHandler.\n");
    }
    if (processMessageRelease(schedulerProcessCompleteMessage)
      != processSuccess
    ) {
      printString("ERROR!!!  "
        "Could not release schedulerProcessCompleteMessage.\n");
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
  ProcessId callingProcessId = processId(processMessageFrom(processMessage));
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) processMessageData(processMessage);
  if (callingProcessId < NANO_OS_NUM_PROCESSES) {
    nanoOsMessage->data = schedulerState->allProcesses[callingProcessId].userId;
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
/// @param processMessage A pointer to the ProcessMessage that was received.
///   This will be reused for the reply.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerSetProcessUserCommandHandler(
  SchedulerState *schedulerState, ProcessMessage *processMessage
) {
  int returnValue = 0;
  ProcessId callingProcessId = processId(processMessageFrom(processMessage));
  UserId userId = nanoOsMessageDataValue(processMessage, UserId);
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) processMessageData(processMessage);
  nanoOsMessage->data = -1;

  if (callingProcessId < NANO_OS_NUM_PROCESSES) {
    if ((schedulerState->allProcesses[callingProcessId].userId == -1)
      || (userId == -1)
    ) {
      schedulerState->allProcesses[callingProcessId].userId = userId;
      nanoOsMessage->data = 0;
    } else {
      nanoOsMessage->data = EACCES;
    }
  }

  processMessageSetDone(processMessage);

  // DO NOT release the message since the caller is waiting on the response.

  return returnValue;
}

/// @fn int schedulerCloseAllFileDescriptorsCommandHandler(
///   SchedulerState *schedulerState, ProcessMessage *processMessage)
///
/// @brief Get the number of processes that are currently running in the system.
///
/// @param schedulerState A pointer to the SchedulerState maintained by the
///   scheduler process.
/// @param processMessage A pointer to the ProcessMessage that was received.
///
/// @return Returns 0 on success, non-zero error code on failure.
int schedulerCloseAllFileDescriptorsCommandHandler(
  SchedulerState *schedulerState, ProcessMessage *processMessage
) {
  int returnValue = 0;
  ProcessId callingProcessId = processId(processMessageFrom(processMessage));
  ProcessDescriptor *processDescriptor
    = &schedulerState->allProcesses[callingProcessId];
  closeProcessFileDescriptors(schedulerState, processDescriptor);

  processMessageSetDone(processMessage);

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
  // SCHEDULER_CLOSE_ALL_FILE_DESCRIPTORS:
  schedulerCloseAllFileDescriptorsCommandHandler,
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
    SchedulerCommand messageType
      = (SchedulerCommand) processMessageType(message);
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
      processMessageQueuePush(getRunningProcess(), message);
    }
    lastReturnValue = returnValue;
  }

  return;
}

/// @fn void checkForTimeouts(SchedulerState *schedulerState)
///
/// @brief Check for anything that's timed out on the timedWaiting queue.
///
/// @param schedulerState A pointer to the SchedulerState object maintained by
///   the scheduler process.
///
/// @return This function returns no value.
void checkForTimeouts(SchedulerState *schedulerState) {
  ProcessQueue *timedWaiting = &schedulerState->timedWaiting;
  uint8_t numElements = timedWaiting->numElements;
  int64_t now = coroutineGetNanoseconds(NULL);

  for (uint8_t ii = 0; ii < numElements; ii++) {
    ProcessDescriptor *poppedDescriptor = processQueuePop(timedWaiting);
    Comutex *blockingComutex
      = poppedDescriptor->processHandle->blockingComutex;
    Cocondition *blockingCocondition
      = poppedDescriptor->processHandle->blockingCocondition;

    if ((blockingComutex != NULL) && (now >= blockingComutex->timeoutTime)) {
      processQueuePush(&schedulerState->ready, poppedDescriptor);
      continue;
    } else if ((blockingCocondition != NULL)
      && (now >= blockingCocondition->timeoutTime)
    ) {
      processQueuePush(&schedulerState->ready, poppedDescriptor);
      continue;
    }

    processQueuePush(timedWaiting, poppedDescriptor);
  }

  return;
}

/// @fn void runScheduler(SchedulerState *schedulerState)
///
/// @brief Run one (1) iteration of the main scheduler loop.
///
/// @param schedulerState A pointer to the SchedulerState object maintained by
///   the scheduler process.
///
/// @return This function returns no value.
void runScheduler(SchedulerState *schedulerState) {
  ProcessDescriptor *processDescriptor
    = processQueuePop(&schedulerState->ready);
  void *processReturnValue
    = coroutineResume(processDescriptor->processHandle, NULL);

  if (processReturnValue == COROUTINE_CORRUPT) {
    printString("ERROR!!!  Process corruption detected!!!\n");
    printString("          Removing process ");
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
      return;
    }

    ProcessMessage *freeProcessMemoryMessage = getAvailableMessage();
    if (freeProcessMemoryMessage != NULL) {
      NanoOsMessage *nanoOsMessage = (NanoOsMessage*) processMessageData(
        freeProcessMemoryMessage);
      nanoOsMessage->data = processDescriptor->processId;
      processMessageInit(freeProcessMemoryMessage,
        MEMORY_MANAGER_FREE_PROCESS_MEMORY,
        nanoOsMessage, sizeof(*nanoOsMessage), /* waiting= */ false);
      sendProcessMessageToProcess(
        schedulerState->allProcesses[
          NANO_OS_MEMORY_MANAGER_PROCESS_ID].processHandle,
        freeProcessMemoryMessage);
    } else {
      printString("WARNING:  Could not allocate "
        "freeProcessMemoryMessage.  Memory leak.\n");
    }

    return;
  }

  if (processRunning(processDescriptor->processHandle) == false) {
    schedulerSendNanoOsMessageToPid(schedulerState,
      NANO_OS_MEMORY_MANAGER_PROCESS_ID, MEMORY_MANAGER_FREE_PROCESS_MEMORY,
      /* func= */ 0, /* data= */ processDescriptor->processId);
  }

  // Check the shells and restart them if needed.
  if ((processDescriptor->processId == USB_SERIAL_PORT_SHELL_PID)
    && (processRunning(processDescriptor->processHandle) == false)
  ) {
    // Restart the shell.
    allProcesses[USB_SERIAL_PORT_SHELL_PID].numFileDescriptors
      = NUM_STANDARD_FILE_DESCRIPTORS;
    allProcesses[USB_SERIAL_PORT_SHELL_PID].fileDescriptors
      = (FileDescriptor*) standardUserFileDescriptors;
    if (processCreate(&processDescriptor->processHandle,
        runShell, schedulerState->hostname
      ) == processError
    ) {
      printString(
        "ERROR!!!  Could not configure process for USB shell.\n");
    }
    processDescriptor->name = "USB shell";
    coroutineResume(processDescriptor->processHandle, NULL);
  } else if ((processDescriptor->processId == GPIO_SERIAL_PORT_SHELL_PID)
    && (processRunning(processDescriptor->processHandle) == false)
  ) {
    // Restart the shell.
    allProcesses[GPIO_SERIAL_PORT_SHELL_PID].numFileDescriptors
      = NUM_STANDARD_FILE_DESCRIPTORS;
    allProcesses[GPIO_SERIAL_PORT_SHELL_PID].fileDescriptors
      = (FileDescriptor*) standardUserFileDescriptors;
    if (processCreate(&processDescriptor->processHandle,
        runShell, schedulerState->hostname
      ) == processError
    ) {
      printString(
        "ERROR!!!  Could not configure process for GPIO shell.\n");
    }
    processDescriptor->name = "GPIO shell";
    coroutineResume(processDescriptor->processHandle, NULL);
  }

  if (processReturnValue == COROUTINE_WAIT) {
    processQueuePush(&schedulerState->waiting, processDescriptor);
  } else if (processReturnValue == COROUTINE_TIMEDWAIT) {
    processQueuePush(&schedulerState->timedWaiting, processDescriptor);
  } else if (processFinished(processDescriptor->processHandle)) {
    processQueuePush(&schedulerState->free, processDescriptor);
  } else { // Process is still running.
    processQueuePush(&schedulerState->ready, processDescriptor);
  }

  checkForTimeouts(schedulerState);
  handleSchedulerMessage(schedulerState);

  return;
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
  extern ProcessMessage *messages;
  messages = messagesStorage;

  // Initialize the static NanoOsMessage storage.
  NanoOsMessage nanoOsMessagesStorage[NANO_OS_NUM_MESSAGES] = {};
  extern NanoOsMessage *nanoOsMessages;
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

  // Initialize all the kernel process file descriptors.
  for (ProcessId ii = 0; ii < NANO_OS_FIRST_USER_PROCESS_ID; ii++) {
    allProcesses[ii].numFileDescriptors = NUM_STANDARD_FILE_DESCRIPTORS;
    allProcesses[ii].fileDescriptors
      = (FileDescriptor*) standardKernelFileDescriptors;
  }

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

  // Create the filesystem process.
  processHandle = 0;
  if (processCreate(&processHandle, runFilesystem, NULL) != processSuccess) {
    printString("Could not start filesystem process.\n");
  }
  processSetId(processHandle, NANO_OS_FILESYSTEM_PROCESS_ID);
  allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID].processId
    = NANO_OS_FILESYSTEM_PROCESS_ID;
  allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID].processHandle = processHandle;
  allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID].name = "filesystem";
  allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID].userId = ROOT_USER_ID;

  processHandle = 0;
  if (processCreate(&processHandle, dummyProcess, NULL) != processSuccess) {
    printString("Could not double filesystem process's stack.\n");
  }

  // We need to do an initial population of all the commands because we need to
  // get to the end of memory to run the memory manager in whatever is left
  // over.
  for (ProcessId ii = NANO_OS_FIRST_USER_PROCESS_ID;
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
    ((uintptr_t) allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID].processHandle),
    ((uintptr_t) allProcesses[NANO_OS_CONSOLE_PROCESS_ID].processHandle))
    - sizeof(Coroutine)
  );
  printString(" bytes\n");

  printString("Coroutine stack size = ");
  printInt(ABS_DIFF(
    ((uintptr_t) allProcesses[NANO_OS_FIRST_USER_PROCESS_ID].processHandle),
    ((uintptr_t) allProcesses[NANO_OS_FIRST_USER_PROCESS_ID + 1].processHandle))
    - sizeof(Coroutine)
  );
  printString(" bytes\n");

  printString("Coroutine size = ");
  printInt(sizeof(Coroutine));
  printString("\n");

  printString("standardKernelFileDescriptors size = ");
  printInt(sizeof(standardKernelFileDescriptors));
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
  processQueuePush(&schedulerState.ready,
    &allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID]);
  for (ProcessId ii = NANO_OS_FIRST_USER_PROCESS_ID;
    ii < NANO_OS_NUM_PROCESSES;
    ii++
  ) {
    processQueuePush(&schedulerState.ready, &allProcesses[ii]);
  }

  // Get the memory manager and filesystem up and running.
  coroutineResume(
    allProcesses[NANO_OS_MEMORY_MANAGER_PROCESS_ID].processHandle,
    NULL);
  coroutineResume(
    allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID].processHandle,
    NULL);

  // Allocate memory for the hostname.
  schedulerState.hostname = (char*) kcalloc(1, 30);
  if (schedulerState.hostname != NULL) {
    strcpy(schedulerState.hostname, "localhost");
  } else {
    printString("ERROR!!!  schedulerState.hostname is NULL!!!\n");
  }
  //// FILE *hostnameFile = kfopen(&schedulerState, "/etc/hostname", "r");
  //// if (hostnameFile != NULL) {
  ////   if (nanoOsFGets(schedulerState.hostname, 30, hostnameFile)
  ////     != schedulerState.hostname
  ////   ) {
  ////     printString("ERROR!!!  fgets did not read hostname!!!\n");
  ////   }
  ////   if (strchr(schedulerState.hostname, '\r')) {
  ////     *strchr(schedulerState.hostname, '\r') = '\0';
  ////   } else if (strchr(schedulerState.hostname, '\n')) {
  ////     *strchr(schedulerState.hostname, '\n') = '\0';
  ////   } else if (*schedulerState.hostname == '\0') {
  ////     strcpy(schedulerState.hostname, "localhost");
  ////   }
  ////   kfclose(&schedulerState, hostnameFile);
  //// } else {
  ////   printString("ERROR!!!  kfopen of /etc/hostname returned NULL!!!\n");
  //// }

  // Run our scheduler.
  while (1) {
    runScheduler(&schedulerState);
  }
}

