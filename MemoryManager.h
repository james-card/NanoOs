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


/// @def MAIN_PROCESS_STACK_SIZE
///
/// @brief The stack size, in bytes, of the main memory manager process that
/// will handle messages.  This needs to be as small as possible.  The actual
/// stack size allocated will be slightly larger than this due to other things
/// being pushed onto the stack before initializeGlobals is called.
#define MAIN_PROCESS_STACK_SIZE 32


/// @enum MemoryManagerCommand
///
/// @brief Commands recognized by the memory manager.
typedef enum MemoryManagerCommand {
  MEMORY_MANAGER_FREE,
  MEMORY_MANAGER_REALLOC,
  NUM_MEMORY_MANAGER_COMMANDS
} MemoryManagerCommand;

/// @enum MemoryManagerResponse
///
/// @brief Possible responses from synchronous memory manager commands.
typedef enum MemoryManagerResponse {
  MEMORY_MANAGER_RETURNING_POINTER,
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


#ifdef __cplusplus
} // extern "C"
#endif

#endif // MEMORY_MANAGER_H

