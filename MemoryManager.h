///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              11.30.2024
///
/// @file              MemoryManager.h
///
/// @brief             Dynamic memory management functionality for NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2024 James Card
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
/// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///                                James Card
///                         http://www.jamescard.org
///
///////////////////////////////////////////////////////////////////////////////

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

// Standard C includes
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <setjmp.h>

// NanoOs includes
#include "NanoOs.h"

#ifdef __cplusplus
extern "C"
{
#endif


/// @def MEMORY_MANAGER_PROCESS_STACK_CHUNK_SIZE
///
/// @brief The size, in bytes, of one chunk of the main memory process's stack.
#define MEMORY_MANAGER_PROCESS_STACK_CHUNK_SIZE 32

/// @def MEMORY_MANAGER_PROCESS_STACK_SIZE
///
/// @brief The stack size, in bytes, of the main memory manager process that
/// will handle messages.  This needs to be as small as possible.  The actual
/// stack size allocated will be slightly larger than this due to other things
/// being pushed onto the stack before initializeGlobals is called.
#define MEMORY_MANAGER_PROCESS_STACK_SIZE 128


/// @enum MemoryManagerCommand
///
/// @brief Commands recognized by the memory manager.
typedef enum MemoryManagerCommand {
  MEMORY_MANAGER_REALLOC,
  MEMORY_MANAGER_GET_FREE_MEMORY,
  NUM_MEMORY_MANAGER_COMMANDS
} MemoryManagerCommand;

/// @enum MemoryManagerResponse
///
/// @brief Possible responses from synchronous memory manager commands.
typedef enum MemoryManagerResponse {
  MEMORY_MANAGER_RETURNING_POINTER,
  MEMORY_MANAGER_RETURNING_FREE_MEMORY,
  NUM_MEMORY_MANAGER_RESPONSES
} MemoryManagerResponse;

/// @struct ReallocMessage
///
/// @brief Structure that holds the data needed to make a request to reallocate
/// an existing pointer.
typedef struct ReallocMessage {
  void *ptr;
  size_t size;
} ReallocMessage;

/// @struct MemoryManagerState
///
/// @brief State metadata the memory manager process uses for allocations and
/// deallocations.
///
/// @parm mallocBuffer A pointer to the first character of the buffer to
///   allocate memory from.
/// @param mallocNext A pointer to the next free piece of memory.
/// @param mallocStart The numeric value of the first address available to
///   allocate memory from.
/// @param mallocEnd The numeric value of the last address available to allocate
///   memory from.
typedef struct MemoryManagerState {
  char *mallocBuffer;
  char *mallocNext;
  uintptr_t mallocStart;
  uintptr_t mallocEnd;
} MemoryManagerState;

// Function prototypes
void* memoryManager(void *args);

void memoryManagerFree(void *ptr);
#ifdef free
#undef free
#endif // free
#define free memoryManagerFree

void* memoryManagerRealloc(void *ptr, size_t size);
#ifdef realloc
#undef realloc
#endif // realloc
#define realloc memoryManagerRealloc

void* memoryManagerMalloc(size_t size);
#ifdef malloc
#undef malloc
#endif // malloc
#define malloc memoryManagerMalloc

void* memoryManagerCalloc(size_t nmemb, size_t size);
#ifdef calloc
#undef calloc
#endif // calloc
#define calloc memoryManagerCalloc

size_t getFreeMemory(void);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // MEMORY_MANAGER_H

