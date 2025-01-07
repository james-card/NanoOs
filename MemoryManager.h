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
/// being pushed onto the stack before initializeGlobals is called.  Note that
/// we need a larger stack when debugging to accommodate the logging calls.
//// #define MEMORY_MANAGER_DEBUG
#ifdef MEMORY_MANAGER_DEBUG
#define MEMORY_MANAGER_PROCESS_STACK_SIZE 192
#else
#define MEMORY_MANAGER_PROCESS_STACK_SIZE 128
#endif // MEMORY_MANAGER_DEBUG


/// @enum MemoryManagerCommandResponse
///
/// @brief Commands and responses recognized by the memory manager.
typedef enum MemoryManagerCommandREsponse {
  // Commands:
  MEMORY_MANAGER_REALLOC               = 0,
  MEMORY_MANAGER_FREE                  = 1,
  MEMORY_MANAGER_GET_FREE_MEMORY       = 2,
  MEMORY_MANAGER_FREE_PROCESS_MEMORY   = 3,
  // Responses:
  MEMORY_MANAGER_RETURNING_POINTER     = 4,
  MEMORY_MANAGER_RETURNING_FREE_MEMORY = 5,
  NUM_MEMORY_MANAGER_COMMAND_RESPONSES
} MemoryManagerCommand;

// Function prototypes
void* runMemoryManager(void *args);

void memoryManagerFree(const char *functionName, void *ptr);
#ifdef free
#undef free
#endif // free
#define free(ptr) memoryManagerFree(__func__, ptr)

void* memoryManagerRealloc(const char *functionName, void *ptr, size_t size);
#ifdef realloc
#undef realloc
#endif // realloc
#define realloc(ptr, size) memoryManagerRealloc(__func__, ptr, size)

void* memoryManagerMalloc(const char *functionName, size_t size);
#ifdef malloc
#undef malloc
#endif // malloc
#define malloc(size) memoryManagerMalloc(__func__, size)

void* memoryManagerCalloc(const char *functionName, size_t nmemb, size_t size);
#ifdef calloc
#undef calloc
#endif // calloc
#define calloc(nmemb, size) memoryManagerCalloc(__func__, nmemb, size)

size_t getFreeMemory(void);
int assignMemory(void *ptr, ProcessId pid);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // MEMORY_MANAGER_H

