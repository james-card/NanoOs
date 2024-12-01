# NanoOs

## Goals

This work started out as an experiment to see if I could implement an operating system similar to an early version of UNIX in a similar environment.  Early versions of UNIX ran on a PDP-11 with 16 KB of memory and an approximately 1 MHz processor.  Arduinos are the closest modern devices to that kind of environment, so that's where I started.  This code was written for an Arduino Nano Every, which has a 20 MHz processor and 6 KB of RAM.  Quite a bit faster, and a little less than half the RAM.  Definitely a challenging environment.

My first priority was getting a system that would support multiple concurrent processes.  Supporting multiple concurrent users is a non-starter without that ability.  The only way to achieve that in this kind of environment is with cooperative multitasking.  Fortunately, I have a Coroutines library that will work in this kind of environment, and I applied it here.

## Environmental Considerations

Stacks for the coroutines have to be tiny.  In the current implementation, each coroutine has a 512-byte stack available to it.  Fortunately, the Nano Every uses an 8-bit processor, so variables are also small.  Function calls don't consume much stack space because the values pushed onto the stack are so small.

The Coroutines library works by segmenting the main stack.  Coroutines are allocated by putting a Coroutine object at the top of each coroutine's stack, so the total size consumed between coroutines is a little larger than the size of the stack that's configured.  As of the time of this writing (29-Nov-2024), the stack plus Coroutine object size is 605 bytes when a 512-byte stack is specified.  With this configuration, in 6 KB of RAM, NanoOs can support 7 processes plus the scheduler.

## Architecture

NanoOs is a nanokernel architecture.  Specifically, what this means is that there is no distinction between user space and kernel space.  Everything is kernel space.  There's no way to have any kind of memory protection in this kind of environment.

Another implication of this arhitecture is that there basically is no kernel.  The system is really a collection of processes that communicate with each other in order to accomplish their work.  Each process has a specific task it is responsible for.  When a process needs something from another process, it sends a message to the designated process.  If it needs a response, it blocks waiting for a reply before continuing.

## Special-Purpose Processes

Currently, the following processes are always present and running:

1. The Scheduler - A round-robin scheduler
2. The Console - Polls for user input and displays process output

In addition, there is a "reserved" process slot in which system processes (i.e. not general-purpose user processes) run.  Note that only one system process may be running at a time.  System processes are intended to be very short-lived so that they do not consume the slot for an extended period of time.

### Future Processes

A future process for managing dynamically-allocated memory is planned.  To be continued...

## Message Passing

NanoOs processes are coroutines.  As such, they use the primitives defined in the Coroutines library.  This includes using the library's message passing infrastructure to send and receive messages among them.