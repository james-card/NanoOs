# NanoOs

A nanokernel for an Arduino Nano.

## Goals

This work started out as an experiment to see if I could implement an operating system similar to an early version of UNIX in a similar environment.  Early versions of UNIX ran on a PDP-11 with 16 KB of memory and an approximately 1 MHz processor.  Arduinos are the closest modern devices to that kind of environment, so that's where I started.  This code was written for an Arduino Nano Every, which has a 20 MHz processor and 6 KB of RAM.  Quite a bit faster, and a little less than half the RAM.  Definitely a challenging environment.

My first priority was getting a system that would support multiple concurrent processes.  Supporting multiple concurrent users is a non-starter without that ability.  The only way to achieve that in this kind of environment is with cooperative multitasking.  Fortunately, I have a Coroutines library that will work in this kind of environment, and I applied it here.

## Environmental Considerations

Stacks for the coroutines have to be tiny.  In the current implementation, each coroutine has a 512-byte stack available to it.  Fortunately, the Nano Every uses an 8-bit processor, so variables are also small.  Function calls don't consume much stack space because the values pushed onto the stack are so small.

The Coroutines library works by segmenting the main stack.  Coroutines are allocated by putting a Coroutine object at the top of each coroutine's stack, so the total size consumed between coroutines is a little larger than the size of the stack that's configured.  As of the time of this writing (29-Nov-2024), the stack plus Coroutine object size is 605 bytes when a 512-byte stack is specified.  With this configuration, in 6 KB of RAM, NanoOs can support 8 concurrent processes.

## Architecture

NanoOs is a nanokernel architecture.  Specifically, what this means is that there is no distinction between user space and kernel space.  Everything is kernel space.  There's no way to have any kind of memory protection in this kind of environment.


## Processes

One implication of the nanokernel arhitecture is that there basically is no kernel.  The system is really a collection of processes that communicate with each other in order to accomplish their work.  Each process has a specific task it is responsible for.  When a process needs something from another process, it sends a message to the designated process.  If it needs a response, it blocks waiting for a reply before continuing.

As mentioned, the current implementation supports up to 8 concurrent processes, however other configurations (with smaller stacks) are possible.  The metadata in the dynamic memory manager limits the number of concurrent processes to 15.  Support for more than 15 processes is possible, but would require modification of some constants and data structures within the operating system.

### OS Processes

Currently, the following processes are always present and running:

1. The Scheduler - A round-robin scheduler
2. The Console - Polls for user input and displays process output
3. The Memory Manager - Responsible for dynamic memory allocation and deallocation

### The Shell and Shell Processes

NanoOs has a very simple command line shell.  The shell is not considered one of the special OS processes.  It would be considered a "user space" process if there were any such thing.

One of the challenges of having a shell in an embedded OS is consuming a process slot that could be used for other user processes.  This is of special concern for processes that need to do process maintenance such as listing running processes or killing a process.  If the shell always launches a command in a new process slot, all slots can be consumed with user processes and process maintenance will become impossible.

To address this, NanoOs borrows a concept from early versions of UNIX.  The command entries in the Commands library have an attribute called "shellCommand".  Commands that have this attribute set to "true" will *REPLACE* the shell process when they execute.  All other commands will run in a different process slot.  This allows mission-critical processes to be guaranteed a slot to run in while doing normal process management for most processes.

### Background Processes

Since NanoOs aims to emulate parts of UNIX, background user processes are supported.  As in UNIX-like environments, launching a process in the background is achieved in a similar manner to UNIX shells:  By appending an ampersand ('&') to the end of the command line.

*NOTE:*  Background processes do not have access to stdout/stderr.  Their only communication mechanism is inter-process communication.

## Message Passing

NanoOs processes are coroutines.  As such, they use the primitives defined in the Coroutines library.  This includes using the library's message passing infrastructure to send and receive messages among them.  All system processes have well-known process IDs and handle incoming messages at least once for every iteration of their main loop.

When a user process needs something from one of the system processes, it prepares and sends a command message to the process.  If the command is asynchronous, the user process can immediately return to its other work.  If the command is synchronous, the user process can block waiting for a response from the system process.  All system operations are handled this way.

## Dynamic Memory

NanoOs does support dynamic memory, just not very much of it.  The memory manager is started as the last coroutine and makes use of all of the remaining memory at that time.  As of today, that amount is a little less than 1 KB.  Due to the way the data structures are organized, the maximum amount of dynamic memory supported is 4 KB.

The memory manager is a modified bump allocator that supports automatic memory compaction.  Any time the last pointer in the allocation list is freed, the next pointer is moved backward until a block that has not been freed is found or until the beginning of dynamic memory is reached.  The number of outstanding memory allocations is not tracked because it's unnecessary.  This allows partial reclamation of memory space without having to free all outstanding allocations.

The memory manager also tracks the size of each allocation.  This allows for realloc to function correctly.  If the size of the reallocation is less than or equal to the size already allocated, no action is taken.  If the size of the reallocation is greater than the size already allocated, the old memory can be copied to the new location before returning the new pointer to the user (as is supposed to happen for realloc).

The owning process of a piece of dynamically-allocated memory is also tracked.  By default, this is the process that originally made the allocation request.  However, the scheduler has the ability to reassign ownership on process creation.  This allows for all memory owned by an individual process to be freed in the event it is prematurely terminated.
