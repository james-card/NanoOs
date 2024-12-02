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

// We need to wrap memory management functions for printStackTrace.  The reason
// is that this function may be called when the system is in a degraded state
// (i.e. there's something that is preventing memory management from working
// correctly).  While printStackTrace is running, we need to avoid calling the
// real functions.  Other than that, we need to pass through to the real calls.

/// @struct MemNode
///
/// @brief Metadata that's placed right before the memory pointer that's
/// returned by one one of the memory allocation functions.  These nodes are
/// used when the pointer is deallocated.
///
/// @param prev A pointer to the previous MemNode.
/// @param size The number of bytes allocated for this node.
typedef struct MemNode {
  struct MemNode *prev;
  size_t          size;
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
      if (charPointer == memoryManagerState->mallocNext) {
        // Special case.  The value being freed is the last one that was
        // allocated.  Do memory compaction.
        memoryManagerState->mallocNext
          += sizeOfMemory(ptr) + sizeof(MemNode);
        for (MemNode *cur = memNode(ptr)->prev;
          (cur != NULL) && (cur->size == 0);
          cur = cur->prev
        ) {
          memoryManagerState->mallocNext = (char*) &cur[1];
        }
      }
      
      // Clear out the size.
      memNode(charPointer)->size = 0;
    }
  } // else this is not something we can free.  Ignore it.
  
  return;
}

/// @fn void* localRealloc(MemoryManagerState *memoryManagerState,
///   void *ptr, size_t size)
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
///
/// @return Returns a pointer to size-adjusted memory on success, NULL on
/// failure or on free.
void* localRealloc(MemoryManagerState *memoryManagerState,
  void *ptr, size_t size
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
      if ((uintptr_t) (charPointer - size - sizeof(MemNode)
          + memNode(charPointer)->size)
        >= memoryManagerState->mallocEnd
      ) {
        size_t oldSize = memNode(ptr)->size;
        returnValue = charPointer - size + memNode(charPointer)->size;
        memNode(returnValue)->size = size;
        memNode(returnValue)->prev = memNode(charPointer)->prev;
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
    returnValue = memoryManagerState->mallocNext - size - sizeof(MemNode);
    memNode(returnValue)->size = size;
    memNode(returnValue)->prev = memNode(memoryManagerState->mallocNext);
    memoryManagerState->mallocNext -= size + sizeof(MemNode);
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
///
/// @return Returns 0 on success, error code on failure.
int memoryManagerHandleRealloc(
  MemoryManagerState *memoryManagerState, Comessage *incoming
) {
  // We're going to reuse the incoming message as the outgoing message.
  Comessage *response = incoming;

  int returnValue = 0;
  ReallocMessage *reallocMessage = (ReallocMessage*) comessageData(incoming);
  void *clientReturnValue
    = localRealloc(memoryManagerState,
      reallocMessage->ptr, reallocMessage->size);
  size_t clientReturnSize
    = (clientReturnValue != NULL) ? reallocMessage->size : 0;
  
  Coroutine *from = comessageFrom(incoming);
  
  // We need to mark waiting as true here so that comessageSetDone signals the
  // client side correctly.
  comessageInit(response, MEMORY_MANAGER_RETURNING_POINTER,
    clientReturnValue, clientReturnSize, true);
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

/// @var memoryManagerCommand
///
/// @brief Array of function pointers for handlers for commands that are
/// understood by this library.
int (*memoryManagerCommand[])(
  MemoryManagerState *memoryManagerState, Comessage *incoming
) = {
  memoryManagerHandleRealloc,
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
      printf("ERROR!!!  Received invalid memory manager message type %d.\n",
        messageType);
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
/// @param stack A pointer to the stack in allocateStack.  Passed just so that
///   the compiler doesn't optimize it out.
///
/// @return This function returns no value and, indeed, never actually returns.
void initializeGlobals(MemoryManagerState *memoryManagerState,
  jmp_buf returnBuffer, char *stack
) {
  extern int __heap_start, *__brkval;
  char mallocBufferStart = '\0';
  
  memoryManagerState->mallocBuffer = &mallocBufferStart;
  memoryManagerState->mallocNext = memoryManagerState->mallocBuffer;
  memNode(memoryManagerState->mallocNext)->size = 0;
  memNode(memoryManagerState->mallocNext)->prev = NULL;
  memoryManagerState->mallocStart
    = (uintptr_t) memoryManagerState->mallocNext;
  
  // We want to grab as much memory as possible for the memory manager.  If we
  // call getFreeRamBytes() from here, we will get the amount available minus
  // the amount used on the stack to call the function, which will be less than
  // the total amount available.  Rather than doing that, directly reimplement
  // the logic here so that we get the maximum value.
  memoryManagerState->mallocEnd
    = ((uintptr_t) memoryManagerState->mallocBuffer)
    - ((uintptr_t) (((uintptr_t) &mallocBufferStart)
        - ((__brkval == NULL)
          ? (uintptr_t) &__heap_start
          : (uintptr_t) __brkval
        )
      )
    )
    + 1;
  
  longjmp(returnBuffer, (int) stack);
}

/// @fn void allocateStack(MemoryManagerState *memoryManagerState,
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
void allocateStack(MemoryManagerState *memoryManagerState,
  jmp_buf returnBuffer, int stackSize, char *topOfStack
) {
  char stack[MEMORY_MANAGER_PROCESS_STACK_CHUNK_SIZE];
  memset(stack, 0, MEMORY_MANAGER_PROCESS_STACK_CHUNK_SIZE);
  
  if (topOfStack == NULL) {
    topOfStack = stack;
  }
  
  if (stackSize > MEMORY_MANAGER_PROCESS_STACK_CHUNK_SIZE) {
    allocateStack(
      memoryManagerState,
      returnBuffer,
      stackSize - MEMORY_MANAGER_PROCESS_STACK_CHUNK_SIZE,
      topOfStack);
  }
  
  initializeGlobals(memoryManagerState, returnBuffer, topOfStack);
}

void printMemoryManagerState(MemoryManagerState *memoryManagerState) {
  extern int __heap_start;
  printf("memoryManagerState.mallocBuffer = %p\n",
    memoryManagerState->mallocBuffer);
  printf("memoryManagerState.mallocNext = %p\n",
    memoryManagerState->mallocNext);
  printf("memoryManagerState.mallocStart = 0x%04x\n",
    memoryManagerState->mallocStart);
  printf("memoryManagerState.mallocEnd = 0x%04x\n",
    memoryManagerState->mallocEnd);
  printf("&__heap_start = %p\n", &__heap_start);
  
  return;
}

/// @fn void* memoryManager(void *args)
///
/// @brief Main process for the memory manager that will configure all the
/// variables and be responsible for handling the messages.
///
/// @param args Any arguments passed by the scheduler.  Ignored by this
///   function.
///
/// @return This function never exits its main loop, so never returns, however
/// it would return NULL if it returned anything.
void* memoryManager(void *args) {
  (void) args;
  printConsole("\n");
  
  MemoryManagerState memoryManagerState;
  jmp_buf returnBuffer;
  uintptr_t dynamicMemorySize = 0;
  if (setjmp(returnBuffer) == 0) {
    allocateStack(&memoryManagerState, returnBuffer,
      MEMORY_MANAGER_PROCESS_STACK_SIZE, NULL);
  }
  
  printMemoryManagerState(&memoryManagerState);
  dynamicMemorySize
    = memoryManagerState.mallocStart - memoryManagerState.mallocEnd + 1;
  printf("Using %u bytes of dynamic memory.\n", dynamicMemorySize);
  releaseConsole();
  
  while (1) {
    coroutineYield(NULL);
    handleMemoryManagerMessages(&memoryManagerState);
  }
  
  return NULL;
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
  
  Comessage sent;
  memset(&sent, 0, sizeof(sent));
  comessageInit(&sent, MEMORY_MANAGER_REALLOC,
    &reallocMessage, sizeof(reallocMessage), true);
  
  if (sendComessageToPid(NANO_OS_MEMORY_MANAGER_PROCESS_ID, &sent)
    != coroutineSuccess
  ) {
    // Nothing more we can do.
    return returnValue;
  }
  
  Comessage *response = comessageWaitForReplyWithType(&sent, false,
    MEMORY_MANAGER_RETURNING_POINTER, NULL);
  returnValue = comessageData(response);
  
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
  memoryManagerSendReallocMessage(ptr, 0);
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

