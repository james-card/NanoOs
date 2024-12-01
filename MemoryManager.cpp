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
  ((((uintptr_t) (ptr)) >= _mallocStart) && (((uintptr_t) (ptr)) <= _mallocEnd))

/// @var _mallocBuffer
///
/// @brief Pointer to the beginning of the buffer to use for memory allocation.
static char *_mallocBuffer = NULL;

/// @var _mallocNext
///
/// @brief Pointer to the next free segment of memory within
/// _mallocBuffer.
static char *_mallocNext = NULL;

/// @var _mallocStart
///
/// @brief Numerical address of the start of the static malloc buffer.
static uintptr_t _mallocStart = 0;

/// @var _mallocEnd
///
/// @brief Numerical address of the end of the static malloc buffer.
static uintptr_t _mallocEnd = 0;

#ifdef __cplusplus
extern "C"
{
#endif

/// @fn void localFree(void *ptr)
///
/// @brief Free a previously-allocated block of memory.
///
/// @param ptr A pointer to the block of memory to free.
///
/// @return This function always succeeds and returns no value.
void localFree(void *ptr) {
  char *charPointer = (char*) ptr;
  
  if (isDynamicPointer(ptr)) {
    // This is memory that was previously allocated from one of our allocators.
    
    // Check the size of the memory in case someone tries to free the same
    // pointer more than once.
    if (sizeOfMemory(ptr) > 0) {
      if ((charPointer + sizeOfMemory(ptr) + sizeof(MemNode)) == _mallocNext) {
        // Special case.  The value being freed is the last one that was
        // allocated.  Do memory compaction.
        _mallocNext = (char*) charPointer;
        for (MemNode *cur = memNode(ptr)->prev;
          (cur != NULL) && (cur->size == 0);
          cur = cur->prev
        ) {
          _mallocNext = (char*) &cur[1];
        }
      }
      
      // Clear out the size.
      memNode(charPointer)->size = 0;
    }
  } // else this is not something we can free.  Ignore it.
  
  return;
}

/// @fn void* localRealloc(void *ptr, size_t size)
///
/// @brief Reallocate a provided pointer to a new size.
///
/// @param ptr A pointer to the original block of dynamic memory.  If this value
///   is NULL, new memory will be allocated.
/// @param size The new size desired for the memory block at ptr.  If this value
///   is 0, the provided pointer will be freed.
///
/// @return Returns a pointer to size-adjusted memory on success, NULL on
/// failure or on free.
void* localRealloc(void *ptr, size_t size) {
  char *charPointer = (char*) ptr;
  char *returnValue = NULL;
  
  if (size == 0) {
    // In this case, there's no point in going through any path below.  Just
    // free it, return NULL, and be done with it.
    localFree(ptr);
    return NULL;
  }
  
  if (isDynamicPointer(ptr)) {
    // This pointer was allocated from our allocators.
    if (size <= sizeOfMemory(ptr)) {
      // We're fitting into a block that's larger than or equal to the size
      // being requested.  *DO NOT* update the size in this case.  Just
      // return the current pointer.
      return ptr;
    } else if ((charPointer + sizeOfMemory(ptr) + sizeof(MemNode))
      == _mallocNext
    ) {
      // The pointer we're reallocating is the last one allocated.  We have
      // an opportunity to just reuse the existing block of memory instead
      // of allocating a new block.
      if ((uintptr_t) (charPointer + size + sizeof(MemNode))
        <= _mallocEnd
      ) {
        // Update the size before returning the pointer.
        memNode(charPointer)->size = size;
        return ptr;
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
  if ((((uintptr_t) (_mallocNext + size + sizeof(MemNode))) <= _mallocEnd)) {
    returnValue = _mallocNext;
    memNode(returnValue)->size = size;
    _mallocNext += size + sizeof(MemNode);
    memNode(_mallocNext)->prev = memNode(returnValue);
    memNode(_mallocNext)->size = 0;
  } // else we don't have enough memory left to satisfy the request.
  
  if ((returnValue != NULL) && (ptr != NULL)) {
    // Because of the logic above, we're guaranteed that this means that the
    // address of returnValue is not the same as the address of ptr.  Copy
    // the data from the old memory to the new memory and free the old
    // memory.
    memcpy(returnValue, ptr, sizeOfMemory(ptr));
    localFree(ptr);
  }
  
  return returnValue;
}

#ifdef __cplusplus
} // extern "C"
#endif

/******************* End Custom Memory Management Functions *******************/

/// @fn int memoryManagerHandleFree(Comessage *incoming)
///
/// @brief Command handler for a MEMORY_MANAGER_FREE command.  Parses the
/// pointer out of the message and passes it to localFree.
///
/// @return Returns 0 on success, error code on failure.
int memoryManagerHandleFree(Comessage *incoming) {
  localFree(comessageData(incoming));
  // The client is *NOT* waiting on a reply and expects us to release the
  // message.  Don't disappoint them.
  comessageRelease(incoming);
  return 0;
}

/// @fn int memoryManagerHandleRealloc(Comessage *incoming)
///
/// @brief Command handler for a MEMORY_MANAGER_REALLOC command.  Extracts the
/// ReallocMessage from the message and passes the parameters to localRealloc.
///
/// @return Returns 0 on success, error code on failure.
int memoryManagerHandleRealloc(Comessage *incoming) {
  Comessage *response = getAvailableMessage();
  while (response == NULL) {
    coroutineYield(NULL);
    response = getAvailableMessage();
  }

  int returnValue = 0;
  ReallocMessage *reallocMessage = (ReallocMessage*) comessageData(incoming);
  void *clientReturnValue
    = localRealloc(reallocMessage->ptr, reallocMessage->size);
  size_t clientReturnSize
    = (clientReturnValue != NULL) ? reallocMessage->size : 0;
  
  comessageInit(response, MEMORY_MANAGER_RETURNING_POINTER,
    clientReturnValue, clientReturnSize, false);
  if (comessageQueuePush(comessageFrom(incoming), response)
    != coroutineSuccess
  ) {
    returnValue = -1;
    if (comessageRelease(response) != coroutineSuccess) {
      printString("ERROR!!!  "
        "Could not release message from sendNanoOsMessageToCoroutine.\n");
    }
  }
  
  // The client is waiting on us.  Mark the incoming message done now.  Do *NOT*
  // release it since the client is still using it.
  comessageSetDone(incoming);
  
  return returnValue;
}

/// @fn void initializeGlobals(jmp_buf returnBuffer)
///
/// @brief Initialize the global variables that will be needed by the memory
/// management functions and then resume execution in the main process function.
///
/// @param returnBuffer The jmp_buf that will be used to resume execution in the
///   main process function.
/// @param stack A pointer to the stack in allocateStack.  Passed just so that
///   the compiler doesn't optimize it out.
///
/// @return This function returns no value and, indeed, never actually returns.
void initializeGlobals(jmp_buf returnBuffer, char *stack) {
  char a = '\0';
  _mallocBuffer = &a;
  _mallocNext = _mallocBuffer + sizeof(MemNode);
  _mallocStart = (uintptr_t) _mallocNext;
  _mallocEnd = _mallocStart + (uintptr_t) getFreeRamBytes();
  
  printConsole("\n");
  printf("Using %u bytes of dynamic memory.\n", _mallocEnd - _mallocStart + 1);
  // Serial.print("Using ");
  // Serial.print(_mallocEnd - _mallocStart + 1);
  // Serial.print(" bytes of dynamic memory.\n");
  
  longjmp(returnBuffer, (int) stack);
}

/// @fn void allocateStack(jmp_buf returnBuffer)
///
/// @brief Allocate space on the stack for the main process and then call
/// initializeGlobals to finish the initialization process.
///
/// @param returnBuffer The jmp_buf that will be used to resume execution in the
///   main process function.
///
/// @return This function returns no value and, indeed, never actually returns.
void allocateStack(jmp_buf returnBuffer) {
  char stack[MAIN_PROCESS_STACK_SIZE];
  initializeGlobals(returnBuffer, stack);
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
  
  jmp_buf returnBuffer;
  if (setjmp(returnBuffer) == 0) {
    allocateStack(returnBuffer);
  }
  releaseConsole();
  
  while (1) {
    coroutineYield(NULL);
  }
  
  return NULL;
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
  sendComessageToPid(NANO_OS_MEMORY_MANAGER_PROCESS_ID,
    MEMORY_MANAGER_FREE, ptr, 0, false);
  
  // We're not interested in a reply for this.  The handler will actually
  // release the message when its done with it.  Just return now.
  
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
  void *returnValue = NULL;
  
  ReallocMessage reallocMessage;
  reallocMessage.ptr = ptr;
  reallocMessage.size = size;
  Comessage *sent = sendComessageToPid(NANO_OS_MEMORY_MANAGER_PROCESS_ID,
    MEMORY_MANAGER_REALLOC, &reallocMessage, sizeof(reallocMessage), true);
  if (sent == NULL) {
    // Failed to send the message.  Nothing we can do.
    return returnValue;
  }
  
  Comessage *response = comessageWaitForReplyWithType(sent, true,
    MEMORY_MANAGER_RETURNING_POINTER, NULL);
  returnValue = comessageData(response);
  comessageRelease(response);
  
  return returnValue;
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
  void *returnValue = NULL;
  
  ReallocMessage reallocMessage;
  reallocMessage.ptr = NULL;
  reallocMessage.size = size;
  Comessage *sent = sendComessageToPid(NANO_OS_MEMORY_MANAGER_PROCESS_ID,
    MEMORY_MANAGER_REALLOC, &reallocMessage, sizeof(reallocMessage), true);
  if (sent == NULL) {
    // Failed to send the message.  Nothing we can do.
    return returnValue;
  }
  
  Comessage *response = comessageWaitForReplyWithType(sent, true,
    MEMORY_MANAGER_RETURNING_POINTER, NULL);
  returnValue = comessageData(response);
  comessageRelease(response);
  
  return returnValue;
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
  void *returnValue = NULL;
  size_t totalSize = nmemb * size;
  
  ReallocMessage reallocMessage;
  reallocMessage.ptr = NULL;
  reallocMessage.size = totalSize;
  Comessage *sent = sendComessageToPid(NANO_OS_MEMORY_MANAGER_PROCESS_ID,
    MEMORY_MANAGER_REALLOC, &reallocMessage, sizeof(reallocMessage), true);
  if (sent == NULL) {
    // Failed to send the message.  Nothing we can do.
    return returnValue;
  }
  
  Comessage *response = comessageWaitForReplyWithType(sent, true,
    MEMORY_MANAGER_RETURNING_POINTER, NULL);
  returnValue = comessageData(response);
  comessageRelease(response);
  
  if (returnValue != NULL) {
    memset(returnValue, 0, totalSize);
  }
  return returnValue;
}

