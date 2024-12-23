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

// NanoOs includes
#include "MemoryManager.h"

/****************** Begin Custom Memory Management Functions ******************/

/// @struct MemNode
///
/// @brief Metadata that's placed right before the memory pointer that's
/// returned by one one of the memory allocation functions.  These nodes are
/// used when the pointer is deallocated.
///
/// @param prev A pointer to the previous MemNode.
/// @param size The number of bytes allocated for this node.
/// @param owner The PID of the process that owns the memory (which is not
///   necessarily the process that allocated it).
typedef struct MemNode {
  struct MemNode    *prev;
  size_t             size:12;
  COROUTINE_ID_TYPE  owner:4;
} MemNode;

/// @def memNode
///
/// @brief Get a pointer to the MemNode for a memory address.
#define memNode(ptr) \
  (((ptr) != NULL) ? &((MemNode*) (ptr))[-1] : NULL)

/// @def sizeOfMemory
///
/// @brief Retrieve the size of a block of dynamic memory.  This information is
/// stored sizeof(MemNode) bytes before the pointer.
#define sizeOfMemory(ptr) \
  (((ptr) != NULL) ? memNode(ptr)->size : 0)

/// @def isDynamicPointer
///
/// @brief Determine whether or not a pointer was allocated from the allocators
/// in this library.
#define isDynamicPointer(ptr) \
  ((((uintptr_t) (ptr)) <= memoryManagerState->mallocStart) \
    && (((uintptr_t) (ptr)) >= memoryManagerState->mallocEnd))

#ifdef __cplusplus
extern "C"
{
#endif

/// @fn void localFreeProcessMemory(
///   MemoryManagerState *memoryManagerState, COROUTINE_ID_TYPE pid)
///
/// @brief Free *ALL* the memory owned by a process given its process ID.
///
/// @param memoryManagerState A pointer to the MemoryManagerState
///   structure that holds the values used for memory allocation and
///   deallocation.
/// @param pid The ID of the process to free the memory of.
///
/// @return This function always succeeds and returns no value.
void localFreeProcessMemory(
  MemoryManagerState *memoryManagerState, COROUTINE_ID_TYPE pid
) {
  //// printDebug("Freeing memory for process ");
  //// printDebug(pid);
  //// printDebug("\n");
  void *ptr = memoryManagerState->mallocNext;
  
  // We have to do two passes.  First pass:  Set the size of all the pointers
  // allocated by the process to zero and the pid to COROUTINE_ID_NOT_SET.
  for (MemNode *cur = memNode(ptr); cur != NULL; cur = cur->prev) {
    if (cur->owner == pid) {
      //// printDebug("Freeing ");
      //// printDebug(cur->size);
      //// printDebug(" bytes.\n");
      cur->size = 0;
      cur->owner = COROUTINE_ID_NOT_SET;
    }
  }
  
  // Second pass, move memoryManagerState->mallocNext back until we hit
  // something that's allocated.
  for (MemNode *cur = memNode(ptr); cur != NULL; cur = cur->prev) {
    if (cur->size != 0) {
      break;
    }
    memoryManagerState->mallocNext = (char*) &cur->prev[1];
    //// printDebug("Moving mallocNext to ");
    //// printDebug(memoryManagerState->mallocNext);
    //// printDebug("\n");
  }
  
  return;
}

/// @fn void localFree(MemoryManagerState *memoryManagerState, void *ptr)
///
/// @brief Free a previously-allocated block of memory.
///
/// @param memoryManagerState A pointer to the MemoryManagerState
///   structure that holds the values used for memory allocation and
///   deallocation.
/// @param ptr A pointer to the block of memory to free.
///
/// @return This function always succeeds and returns no value.
void localFree(MemoryManagerState *memoryManagerState, void *ptr) {
  char *charPointer = (char*) ptr;
  
  if (isDynamicPointer(ptr)) {
    // This is memory that was previously allocated from one of our allocators.
    
    // Check the size of the memory in case someone tries to free the same
    // pointer more than once.
    if (sizeOfMemory(ptr) > 0) {
      // Clear out the size.
      //// printDebug("Freeing ");
      //// printDebug(memNode(charPointer)->size);
      //// printDebug(" bytes for process ");
      //// printDebug(memNode(charPointer)->owner);
      //// printDebug("\n");
      memNode(charPointer)->size = 0;
      memNode(charPointer)->owner = COROUTINE_ID_NOT_SET;
      
      if (charPointer == memoryManagerState->mallocNext) {
        // Special case.  The value being freed is the last one that was
        // allocated.  Do memory compaction.
        for (MemNode *cur = memNode(ptr);
          (cur != NULL) && (cur->size == 0);
          cur = cur->prev
        ) {
          memoryManagerState->mallocNext = (char*) &cur->prev[1];
          //// printDebug("Moving mallocNext to ");
          //// printDebug(memoryManagerState->mallocNext);
          //// printDebug("\n");
        }
      }
    }
  } // else this is not something we can free.  Ignore it.
  
  return;
}

/// @fn void* localRealloc(MemoryManagerState *memoryManagerState,
///   void *ptr, size_t size, COROUTINE_ID_TYPE pid)
///
/// @brief Reallocate a provided pointer to a new size.
///
/// @param memoryManagerState A pointer to the MemoryManagerState
///   structure that holds the values used for memory allocation and
///   deallocation.
/// @param ptr A pointer to the original block of dynamic memory.  If this value
///   is NULL, new memory will be allocated.
/// @param size The new size desired for the memory block at ptr.  If this value
///   is 0, the provided pointer will be freed.
/// @param pid The ID of the process making the request.
///
/// @return Returns a pointer to size-adjusted memory on success, NULL on
/// failure or on free.
void* localRealloc(MemoryManagerState *memoryManagerState,
  void *ptr, size_t size, COROUTINE_ID_TYPE pid
) {
  char *charPointer = (char*) ptr;
  char *returnValue = NULL;
  
  if (size == 0) {
    // In this case, there's no point in going through any path below.  Just
    // free it, return NULL, and be done with it.
    localFree(memoryManagerState, ptr);
    return NULL;
  }
  
  if (isDynamicPointer(ptr)) {
    // This pointer was allocated from our allocators.
    if (size <= sizeOfMemory(ptr)) {
      // We're fitting into a block that's larger than or equal to the size
      // being requested.  *DO NOT* update the size in this case.  Just
      // return the current pointer.
      return ptr;
    } else if (charPointer == memoryManagerState->mallocNext) {
      // The pointer we're reallocating is the last one allocated.  We have
      // an opportunity to just extend the existing block of memory instead
      // of allocating an entirely new block.
      //// printDebug("Re-allocating ");
      //// printDebug(size);
      //// printDebug(" bytes for process ");
      //// printDebug(pid);
      //// printDebug("\n");
      if ((uintptr_t) (charPointer - size - sizeof(MemNode)
          + memNode(charPointer)->size)
        >= memoryManagerState->mallocEnd
      ) {
        size_t oldSize = memNode(ptr)->size;
        returnValue = charPointer - size + memNode(charPointer)->size;
        memNode(returnValue)->size = size;
        memNode(returnValue)->prev = memNode(charPointer)->prev;
        memNode(returnValue)->owner = memNode(charPointer)->owner;
        // Copy the contents of the old block to the new one.
        size_t ii = 0;
        for (char *newPointer = returnValue, *oldPointer = charPointer;
          ii < oldSize;
          newPointer++, oldPointer++
        ) {
          *newPointer = *oldPointer;
          ii++;
        }
        // Update memoryManagerState->mallocNext with the new last pointer.
        memoryManagerState->mallocNext = returnValue;
        //// printDebug("Moving mallocNext to ");
        //// printDebug(memoryManagerState->mallocNext);
        //// printDebug("\n");
        return returnValue;
      } else {
        // Out of memory.  Fail the request.
        return NULL;
      }
    }
  } else if (ptr != NULL) {
    // We're being asked to reallocate a pointer that was *NOT* allocated by
    // this allocator.  This is not valid and we cannot do this.  Fail.
    return NULL;
  }
  
  // We're allocating new memory.
  if ((((uintptr_t) (
      memoryManagerState->mallocNext - size - sizeof(MemNode))
    ) >= memoryManagerState->mallocEnd)
  ) {
    //// printDebug("Allocating ");
    //// printDebug(size);
    //// printDebug(" bytes for process ");
    //// printDebug(pid);
    //// printDebug("\n");
    returnValue = memoryManagerState->mallocNext - size - sizeof(MemNode);
    memNode(returnValue)->size = size;
    memNode(returnValue)->owner = pid;
    memNode(returnValue)->prev = memNode(memoryManagerState->mallocNext);
    memoryManagerState->mallocNext -= size + sizeof(MemNode);
    //// printDebug("Moving mallocNext to ");
    //// printDebug(memoryManagerState->mallocNext);
    //// printDebug("\n");
  } // else we don't have enough memory left to satisfy the request.
  
  if ((returnValue != NULL) && (ptr != NULL)) {
    // Because of the logic above, we're guaranteed that this means that the
    // address of returnValue is not the same as the address of ptr.  Copy
    // the data from the old memory to the new memory and free the old
    // memory.
    memcpy(returnValue, ptr, sizeOfMemory(ptr));
    localFree(memoryManagerState, ptr);
  }
  
  return returnValue;
}

#ifdef __cplusplus
} // extern "C"
#endif

/******************* End Custom Memory Management Functions *******************/

/// @fn int memoryManagerHandleRealloc(
///   MemoryManagerState *memoryManagerState, Comessage *incoming)
///
/// @brief Command handler for a MEMORY_MANAGER_REALLOC command.  Extracts the
/// ReallocMessage from the message and passes the parameters to localRealloc.
///
/// @param memoryManagerState A pointer to the MemoryManagerState
///   structure that holds the values used for memory allocation and
///   deallocation.
/// @param incoming A pointer to the message received from the requesting
///   process.
///
/// @return Returns 0 on success, error code on failure.
int memoryManagerHandleRealloc(
  MemoryManagerState *memoryManagerState, Comessage *incoming
) {
  // We're going to reuse the incoming message as the outgoing message.
  Comessage *response = incoming;

  int returnValue = 0;
  ReallocMessage *reallocMessage
    = nanoOsMessageDataPointer(incoming, ReallocMessage*);
  void *clientReturnValue
    = localRealloc(memoryManagerState,
      reallocMessage->ptr, reallocMessage->size, reallocMessage->pid);
  reallocMessage->ptr = clientReturnValue;
  reallocMessage->size = memNode(clientReturnValue)->size;
  
  Coroutine *from = comessageFrom(incoming);
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(incoming);
  
  // We need to mark waiting as true here so that comessageSetDone signals the
  // client side correctly.
  comessageInit(response, reallocMessage->responseType,
    nanoOsMessage, sizeof(*nanoOsMessage), true);
  if (comessageQueuePush(from, response) != coroutineSuccess) {
    returnValue = -1;
  }
  
  // The client is waiting on us.  Mark the incoming message done now.  Do *NOT*
  // release it since the client is still using it.
  if (comessageSetDone(incoming) != coroutineSuccess) {
    returnValue = -1;
  }
  
  return returnValue;
}

/// @fn int memoryManagerHandleFree(
///   MemoryManagerState *memoryManagerState, Comessage *incoming)
///
/// @brief Command handler for a MEMORY_MANAGER_FREE command.  Extracts the
/// pointer to free from the message and then calls localFree.
///
/// @param memoryManagerState A pointer to the MemoryManagerState
///   structure that holds the values used for memory allocation and
///   deallocation.
/// @param incoming A pointer to the message received from the requesting
///   process.
///
/// @return Returns 0 on success, error code on failure.
int memoryManagerHandleFree(
  MemoryManagerState *memoryManagerState, Comessage *incoming
) {
  int returnValue = 0;

  void *ptr = nanoOsMessageDataPointer(incoming, void*);
  localFree(memoryManagerState, ptr);
  if (comessageRelease(incoming) != coroutineSuccess) {
    printString("ERROR!!!  "
      "Could not release message from memoryManagerHandleFree.\n");
    returnValue = -1;
  }

  return returnValue;
}

/// @fn int memoryManagerHandleGetFreeMemory(
///   MemoryManagerState *memoryManagerState, Comessage *incoming)
///
/// @brief Command handler for MEMORY_MANAGER_GET_FREE_MEMORY.  Gets the amount
/// of free dynamic memory left in the system.
///
/// @param memoryManagerState A pointer to the MemoryManagerState
///   structure that holds the values used for memory allocation and
///   deallocation.
/// @param incoming A pointer to the message received from the requesting
///   process.
///
/// @return Returns 0 on success, error code on failure.
int memoryManagerHandleGetFreeMemory(
  MemoryManagerState *memoryManagerState, Comessage *incoming
) {
  // We're going to reuse the incoming message as the outgoing message.
  Comessage *response = incoming;

  int returnValue = 0;
  
  Coroutine *from = comessageFrom(incoming);
  uintptr_t dynamicMemorySize = (uintptr_t) memoryManagerState->mallocNext
    - memoryManagerState->mallocEnd + 1;
  
  // We need to mark waiting as true here so that comessageSetDone signals the
  // client side correctly.
  comessageInit(response, MEMORY_MANAGER_RETURNING_FREE_MEMORY,
    NULL, dynamicMemorySize, true);
  if (comessageQueuePush(from, response) != coroutineSuccess) {
    returnValue = -1;
  }
  
  // The client is waiting on us.  Mark the incoming message done now.  Do *NOT*
  // release it since the client is still using it.
  if (comessageSetDone(incoming) != coroutineSuccess) {
    returnValue = -1;
  }
  
  return returnValue;
}

/// @fn int memoryManagerHandleFreeProcessMemory(
///   MemoryManagerState *memoryManagerState, Comessage *incoming)
///
/// @brief Command handler for a MEMORY_MANAGER_FREE_PROCESS_MEMORY command.
/// Extracts the process ID from the message and then calls
/// localFreeProcessMemory.
///
/// @param memoryManagerState A pointer to the MemoryManagerState
///   structure that holds the values used for memory allocation and
///   deallocation.
/// @param incoming A pointer to the message received from the requesting
///   process.
///
/// @return Returns 0 on success, error code on failure.
int memoryManagerHandleFreeProcessMemory(
  MemoryManagerState *memoryManagerState, Comessage *incoming
) {
  int returnValue = 0;
  NanoOsMessage *nanoOsMessage = (NanoOsMessage*) comessageData(incoming);
  if (coroutineId(comessageFrom(incoming)) == NANO_OS_SCHEDULER_PROCESS_ID) {
    COROUTINE_ID_TYPE pid = nanoOsMessageDataValue(incoming, COROUTINE_ID_TYPE);
    localFreeProcessMemory(memoryManagerState, pid);
    nanoOsMessage->data = 0;
  } else {
    printString(
      "ERROR:  Only the scheduler may free another process's memory.\n");
    nanoOsMessage->data = 1;
    returnValue = -1;
  }
  
  // The client is waiting on us.  Mark the message as done.
  if (comessageSetDone(incoming) != coroutineSuccess) {
    printString("ERROR!!!  "
      "Could not mark message done in memoryManagerHandleFreeProcessMemory.\n");
    returnValue = -1;
  }
  
  return returnValue;
}

/// @var memoryManagerCommand
///
/// @brief Array of function pointers for handlers for commands that are
/// understood by this library.
int (*memoryManagerCommand[])(
  MemoryManagerState *memoryManagerState, Comessage *incoming
) = {
  memoryManagerHandleRealloc,
  memoryManagerHandleFree,
  memoryManagerHandleGetFreeMemory,
  memoryManagerHandleFreeProcessMemory,
};

/// @fn void handleMemoryManagerMessages(
///   MemoryManagerState *memoryManagerState)
///
/// @brief Handle memory manager messages from the process's queue until there
/// are no more waiting.
///
/// @param memoryManagerState A pointer to the MemoryManagerState
///   structure that holds the values used for memory allocation and
///   deallocation.
///
/// @return This function returns no value.
void handleMemoryManagerMessages(MemoryManagerState *memoryManagerState) {
  Comessage *comessage = comessageQueuePop();
  while (comessage != NULL) {
    MemoryManagerCommand messageType
      = (MemoryManagerCommand) comessageType(comessage);
    if (messageType >= NUM_MEMORY_MANAGER_COMMANDS) {
      comessage = comessageQueuePop();
      continue;
    }
    
    memoryManagerCommand[messageType](memoryManagerState, comessage);
    
    comessage = comessageQueuePop();
  }
  
  return;
}

/// @fn void initializeGlobals(MemoryManagerState *memoryManagerState,
///   jmp_buf returnBuffer, char *stack)
///
/// @brief Initialize the global variables that will be needed by the memory
/// management functions and then resume execution in the main process function.
///
/// @param memoryManagerState A pointer to the MemoryManagerState
///   structure that holds the values used for memory allocation and
///   deallocation.
/// @param returnBuffer The jmp_buf that will be used to resume execution in the
///   main process function.
/// @param stack A pointer to the stack in allocateMemoryManagerStack.  Passed
///   just so that the compiler doesn't optimize it out.
///
/// @return This function returns no value and, indeed, never actually returns.
void initializeGlobals(MemoryManagerState *memoryManagerState,
  jmp_buf returnBuffer, char *stack
) {
  extern int __heap_start, *__brkval;
  char mallocBufferStart = '\0';
  
  memoryManagerState->mallocBuffer = &mallocBufferStart;
  memoryManagerState->mallocNext = memoryManagerState->mallocBuffer;
  memNode(memoryManagerState->mallocNext)->prev = NULL;
  memoryManagerState->mallocStart
    = (uintptr_t) memoryManagerState->mallocNext;
  
  // We want to grab as much memory as we can support for the memory manager.
  // Get the delta between the address of mallocBufferStart and the end of
  // memory.
  uintptr_t memorySize
    = ((uintptr_t) (((uintptr_t) &mallocBufferStart)
        - ((__brkval == NULL)
          ? (uintptr_t) &__heap_start
          : (uintptr_t) __brkval
        )
      )
    )
    + 1;
  
  // The size element of a MemNode is stored as an 12-bit number, which means we
  // can only support a maximum of 4096 bytes.  Make sure we don't exceed that.
  if (memorySize > 4096) {
    memorySize = 4096;
  }
  
  // The value at memNode(memoryManagerState->mallocNext)->size needs to be
  // non-zero in order for the memory compaction algorithm in localFree to work
  // properly.
  memoryManagerState->mallocEnd
    = ((uintptr_t) memoryManagerState->mallocBuffer) - memorySize;
  memNode(memoryManagerState->mallocNext)->size = memorySize;
  
  longjmp(returnBuffer, (int) stack);
}

/// @fn void allocateMemoryManagerStack(MemoryManagerState *memoryManagerState,
///   jmp_buf returnBuffer, int stackSize, char *topOfStack)
///
/// @brief Allocate space on the stack for the main process and then call
/// initializeGlobals to finish the initialization process.
///
/// @details
/// This function is way more involved than it should be.  It really should just
/// declare a single buffer and then call initializeGlobals.  The problem was
/// that the compiler kept optimizing the stack out when it was that simple.
/// I guess it could detect that it was never used.  That won't work for our
/// purposes, so I had to make it more complicated.
///
/// @param memoryManagerState A pointer to the MemoryManagerState
///   structure that holds the values used for memory allocation and
///   deallocation.
/// @param returnBuffer The jmp_buf that will be used to resume execution in the
///   main process function.
/// @param stackSize The desired stack size to allocate.
/// @param topOfStack A pointer to the first stack pointer that gets created.
///
/// @return This function returns no value and, indeed, never actually returns.
void allocateMemoryManagerStack(MemoryManagerState *memoryManagerState,
  jmp_buf returnBuffer, int stackSize, char *topOfStack
) {
  char stack[MEMORY_MANAGER_PROCESS_STACK_CHUNK_SIZE];
  memset(stack, 0, MEMORY_MANAGER_PROCESS_STACK_CHUNK_SIZE);
  
  if (topOfStack == NULL) {
    topOfStack = stack;
  }
  
  if (stackSize > MEMORY_MANAGER_PROCESS_STACK_CHUNK_SIZE) {
    allocateMemoryManagerStack(
      memoryManagerState,
      returnBuffer,
      stackSize - MEMORY_MANAGER_PROCESS_STACK_CHUNK_SIZE,
      topOfStack);
  }
  
  initializeGlobals(memoryManagerState, returnBuffer, topOfStack);
}

/// @fn void printMemoryManagerState(MemoryManagerState *memoryManagerState)
///
/// @brief Debugging function to print all the values of the MemoryManagerState.
///
/// @param memoryManagerState A pointer to the MemoryManagerState maintained by
///   the process.
///
/// @return This function returns no value.
void printMemoryManagerState(MemoryManagerState *memoryManagerState) {
  extern int __heap_start;
  printString("memoryManagerState.mallocBuffer = ");
  printInt((uintptr_t) memoryManagerState->mallocBuffer);
  printString("\n");
  printString("memoryManagerState.mallocNext = ");
  printInt((uintptr_t) memoryManagerState->mallocNext);
  printString("\n");
  printString("memoryManagerState.mallocStart = ");
  printInt(memoryManagerState->mallocStart);
  printString("\n");
  printString("memoryManagerState.mallocEnd = ");
  printInt(memoryManagerState->mallocEnd);
  printString("\n");
  printString("&__heap_start = ");
  printInt((uintptr_t) &__heap_start);
  printString("\n");
  
  return;
}

/// @fn void* runMemoryManager(void *args)
///
/// @brief Main process for the memory manager that will configure all the
/// variables and be responsible for handling the messages.
///
/// @param args Any arguments passed by the scheduler.  Ignored by this
///   function.
///
/// @return This function never exits its main loop, so never returns, however
/// it would return NULL if it returned anything.
void* runMemoryManager(void *args) {
  (void) args;
  printConsole("\n");
  
  MemoryManagerState memoryManagerState;
  Comessage *schedulerMessage = NULL;
  jmp_buf returnBuffer;
  uintptr_t dynamicMemorySize = 0;
  if (setjmp(returnBuffer) == 0) {
    allocateMemoryManagerStack(&memoryManagerState, returnBuffer,
      MEMORY_MANAGER_PROCESS_STACK_SIZE, NULL);
  }
  
  printMemoryManagerState(&memoryManagerState);
  dynamicMemorySize
    = memoryManagerState.mallocStart - memoryManagerState.mallocEnd + 1;
  printConsole("Using ");
  printConsole(dynamicMemorySize);
  printConsole(" bytes of dynamic memory.\n");
  releaseConsole();
  
  while (1) {
    schedulerMessage = (Comessage*) coroutineYield(NULL);
    if (schedulerMessage != NULL) {
      // We have a message from the scheduler that we need to process.  This
      // is not the expected case, but it's the priority case, so we need to
      // list it first.
      MemoryManagerCommand messageType
        = (MemoryManagerCommand) comessageType(schedulerMessage);
      if (messageType < NUM_MEMORY_MANAGER_COMMANDS) {
        memoryManagerCommand[messageType](
          &memoryManagerState, schedulerMessage);
      } else {
        printString("ERROR!!!  Received unknown memory manager command ");
        printInt(messageType);
        printString(" from scheduler.\n");
      }
    } else {
      // No message from the scheduler.  Handle any user process messages in
      // our message queue.
      handleMemoryManagerMessages(&memoryManagerState);
    }
  }
  
  return NULL;
}

/// @fn size_t getFreeMemory(void)
///
/// @brief Send a MEMORY_MANAGER_GET_FREE_MEMORY command to the memory manager
/// process and wait for a reply.
///
/// @return Returns the size, in bytes, of available dynamic memory on success,
/// 0 on failure.
size_t getFreeMemory(void) {
  size_t returnValue = 0;
  
  Comessage sent;
  memset(&sent, 0, sizeof(sent));
  comessageInit(&sent, MEMORY_MANAGER_GET_FREE_MEMORY, NULL, 0, true);
  
  if (sendComessageToPid(NANO_OS_MEMORY_MANAGER_PROCESS_ID, &sent)
    != coroutineSuccess
  ) {
    // Nothing more we can do.
    return returnValue;
  }
  
  Comessage *response = comessageWaitForReplyWithType(&sent, false,
    MEMORY_MANAGER_RETURNING_FREE_MEMORY, NULL);
  returnValue = comessageSize(response);
  
  return returnValue;
}

/// @fn void *memoryManagerSendReallocMessage(void *ptr, size_t size)
///
/// @brief Send a MEMORY_MANAGER_REALLOC command to the memory manager process
/// and wait for a reply.
///
/// @param ptr The pointer to send to the process.
/// @param size The size to send to the process.
///
/// @return Returns the data pointer returned in the reply.
void *memoryManagerSendReallocMessage(void *ptr, size_t size) {
  void *returnValue = NULL;
  
  ReallocMessage reallocMessage;
  reallocMessage.ptr = ptr;
  reallocMessage.size = size;
  reallocMessage.pid = coroutineId(NULL);
  reallocMessage.responseType = MEMORY_MANAGER_RETURNING_POINTER;
  
  Comessage *sent = sendNanoOsMessageToPid(NANO_OS_MEMORY_MANAGER_PROCESS_ID,
    MEMORY_MANAGER_REALLOC, /* func= */ 0, (NanoOsMessageData) &reallocMessage,
    true);
  
  if (sent == NULL) {
    // Nothing more we can do.
    return returnValue; // NULL
  }
  
  Comessage *response = comessageWaitForReplyWithType(sent, false,
    MEMORY_MANAGER_RETURNING_POINTER, NULL);
  if (response == NULL) {
    // Something is wrong.  Fail.
    return returnValue; // NULL
  }
  
  // The handler set the pointer back in the structure we sent it, so grab it
  // out of the structure we already have.
  returnValue = reallocMessage.ptr;
  comessageRelease(sent);
  
  return returnValue;
}

/// @fn void memoryManagerFree(void *ptr)
///
/// @brief Free previously-allocated memory.  The provided pointer may have
/// been allocated either by the system memory functions or from our static
/// memory pool.
///
/// @param ptr A pointer to the block of memory to free.
///
/// @return This function always succeeds and returns no value.
void memoryManagerFree(void *ptr) {
  sendNanoOsMessageToPid(NANO_OS_MEMORY_MANAGER_PROCESS_ID, MEMORY_MANAGER_FREE,
    (NanoOsMessageData) 0, (NanoOsMessageData) ((intptr_t) ptr), false);
  return;
}

/// @fn void* memoryManagerRealloc(void *ptr, size_t size)
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
void* memoryManagerRealloc(void *ptr, size_t size) {
  return memoryManagerSendReallocMessage(ptr, size);
}

/// @fn void* memoryManagerMalloc(size_t size)
///
/// @brief Allocate but do not clear memory.
///
/// @param size The size of the block of memory to allocate in bytes.
///
/// @return Returns a pointer to newly-allocated memory of the specified size
/// on success, NULL on failure.
void* memoryManagerMalloc(size_t size) {
  return memoryManagerSendReallocMessage(NULL, size);
}

/// @fn void* memoryManagerCalloc(size_t nmemb, size_t size)
///
/// @brief Allocate memory and clear all the bytes to 0.
///
/// @param nmemb The number of elements to allocate in the memory block.
/// @param size The size of each element to allocate in the memory block.
///
/// @return Returns a pointer to zeroed newly-allocated memory of the specified
/// size on success, NULL on failure.
void* memoryManagerCalloc(size_t nmemb, size_t size) {
  size_t totalSize = nmemb * size;
  void *returnValue = memoryManagerSendReallocMessage(NULL, totalSize);
  
  if (returnValue != NULL) {
    memset(returnValue, 0, totalSize);
  }
  return returnValue;
}

/// @fn int assignMemory(void *ptr, COROUTINE_ID_TYPE pid)
///
/// @brief Assign ownership of a piece of memory to a specified process.
///
/// @note Only the scheduler may execute this function.  Requests from any other
/// process will fail.
///
/// @param ptr A pointer to the memory to assign.
/// @param pid The ID of the process to assign the memory to.
///
/// @return Returns 0 on success, -1 on failure.
int assignMemory(void *ptr, COROUTINE_ID_TYPE pid) {
  int returnValue = 0;
  
  if ((ptr != NULL) && (coroutineId(NULL) == NANO_OS_SCHEDULER_PROCESS_ID)) {
    memNode(ptr)->owner = pid;
  } else if (ptr != NULL) {
    printString(
      "ERROR:  Only the scheduler may assign memory to another process.\n");
    returnValue = -1;
  } else {
    printString("ERROR:  NULL pointer passed to assignMemory.\n");
    returnValue = -1;
  }
  
  return returnValue;
}

/// @fn void memoryManagerCast(Comessage *comessage)
///
/// @brief Send a prepared coroutine message to the memory manager process and
/// do not wait for a reply.  This function exists to support the scheduler and
/// to keep low-level calls that directly interact with the memory manager
/// process out of that library.
///
/// @param comessage A pointer to a Comessage to relay.
///
/// @return This function returns no value.
void memoryManagerCast(Comessage *comessage) {
  sendComessageToPid(NANO_OS_MEMORY_MANAGER_PROCESS_ID, comessage);

  return;
}

