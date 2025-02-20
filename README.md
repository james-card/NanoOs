# NanoOs

A multitasking nanokernel OS for an Arduino Nano.

## Goals

This work started out as an experiment to see if I could implement an operating system similar to an early version of UNIX in a similar environment.  The first version of UNIX ran on a PDP-7 with 8 kilowords of memory (where each word was 18-bits) and an approximately 571 kHz processor.  Arduinos are the closest modern devices to that kind of environment, so that's where I started.  This code was written for an Arduino Nano Every, which has a 20 MHz 8-bit processor and 6 KB of RAM.  Quite a bit faster, but much less RAM.  Definitely a challenging environment.

My first priority was getting a system that would support multiple concurrent processes.  Supporting multiple concurrent users was a non-starter without that ability.  Fortunately, I have a Coroutines library that works in this kind of environment, and I applied it here.

## Environmental Considerations

Stacks for the processes have to be tiny.  In the current implementation, each process has a 384-byte stack configured for it.  Fortunately, the Nano Every uses an 8-bit processor, so variables are also small.  Function calls don't consume much stack space because the values pushed onto the stack are so small.

The Coroutines library works by segmenting the main stack.  Coroutines are allocated by putting a Coroutine object at the top of each process's stack, so the total size consumed between processes is larger than the size of the stack that's configured.  There's also some additional space allocated due to the way the requested amount of space is broken into chunks.  As of the time of this writing (18-Feb-2024), the stack plus Coroutine object size is 517 bytes when a 384-byte stack is specified.  With this configuration, in 6 KB of RAM, NanoOs can support 11 processes slots.  However, the actual number of useful processes is far fewer than that (5 at the time of this running) due to the overhead requried by user processes.

## Architecture

NanoOs is a nanokernel architecture.  At a process level, there is no distinction between user space and kernel space; everything is kernel space.  There's no way to have any kind of memory protection in this kind of environment.

Processes in NanoOs use cooperative multitasking.  That means that each process runs until it either needs something from another process or it's done with its current work.  Preemptive multitasking is not feasible with the current libraries and hardware.

## Processes

One implication of the nanokernel arhitecture is that there basically is no kernel.  The system is really a collection of processes that communicate with each other in order to accomplish their work.  Each process has a specific task it is responsible for.  When a process needs something from another process, it sends a message to the designated process.  If it needs a response, it blocks waiting for a reply before continuing.

The metadata in the dynamic memory manager limits the number of concurrent processes to 15.  Support for more than 15 processes would require modification of some constants and data structures within the operating system.

### Kernel Processes

Currently, the following processes are always present and running:

1. The Scheduler - A multi-queue, round-robin scheduler
2. The I/O Manager - Manages I/O to the filesystem on the SD card and console
3. The Memory Manager - Responsible for dynamic memory allocation and deallocation

These processes work together to provide the basic kernel-level functionality.  These processes cannot be killed, even by the root user.

### User Processes

User processes are RV32I-compiled programs that are stored on the SD card.  They are run through an RV32I VM embedded into the OS.  In order to compile a program, you'll need to download and install an RV32I compiler.  This can be done as follows:

```
sudo mkdir /opt/riscv32i
sudo chown $USER /opt/riscv32i

git clone https://github.com/riscv/riscv-gnu-toolchain riscv-gnu-toolchain-rv32i
cd riscv-gnu-toolchain-rv32i

mkdir build; cd build
../configure --with-arch=rv32i --prefix=/opt/riscv32i
make -j$(nproc)
```

Once `/opt/riscv32i/bin` is in your path, a program named "Hello.c" could then be built as follows:

```
riscv32-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -nostartfiles -Ttext=0x1000 -e _start -o Hello.elf Hello.c
riscv32-unknown-elf-objcopy -O binary Hello.elf Hello.bin
riscv32-unknown-elf-objcopy -O binary --only-section=.text Hello.elf Hello.text
```

You will need to build the `addNanoOsExeMetadata` utility that is in the `usr/util` directory and run it on the `Hello.bin` program before NanoOs can parse and use it.  From the `usr/util` directory, you can built the utility with:

```
gcc -I../.. addNanoOsExeMetadata.c ../../NanoOsExe.c -o ../bin/addNanoOsExeMetadata
```

Once the `addNanoOsExeMetadata` utility is in your path, you can make the `Hello.bin` program runnable from the location of your `Hello.bin` and `Hello.text` files with:

```
addNanoOsExeMetadata Hello.bin Hello.text
```

Right now, only the NANO\_OS\_SYSCALL\_EXIT and NANO\_OS\_SYSCALL\_WRITE system calls are implemented.  The LIBC needed for more complete program development is in development.

### Foreground Processes

One of the challenges of having a shell in an embedded OS is consuming a process slot that could be used for other user processes.  To address this, NanoOs borrows a concept from early versions of UNIX.  Commands that are run in the foreground will **REPLACE** the shell process when they execute.  This allows all foreground processes to be guaranteed a slot to run in.  The shell is automatically restarted by the scheduler when a shell process or a process that replaces one exits.

### Background Processes

Earlier versions of NanoOs supported background processes.  However, the overhead of a VM consumed too much RAM to make this feasible in the current version.  A future version of NanoOs may support background processes again.

### Pipes

Piping of stdout from one process to stdin of another process was supported in an earlier version of NanoOs.  However, this ability relies on the ability to run background processes.  If NanoOs supports background processes in a future version, this ability may be returned to the OS.

## Multi-user Support

NanoOs supports two login shells:  One on the serial port available through the USB connection and one on the serial port available through the GPIO connection.  The shells are started at boot time and are automatically restarted by the scheduler if exit for any reason.  Like the first version of UNIX, this means that NanoOs can support two (2) concurrent users.

When the shells are started, they are unowned.  When a user logs in, the user takes over ownership of the shell.  Any processes started by the shell become owned by the user that owns the shell.  Only the same user or the root user can kill a process owned by a user.  Only the root user can kill an unowned shell process.

## Message Passing

NanoOs processes are coroutines.  As such, they use the primitives defined in the Coroutines library.  This includes using the library's message passing infrastructure to send and receive messages among them.  All kernel processes have well-known process IDs and handle incoming messages at least once for every iteration of their main loop.

When a user process needs something from one of the kernel processes, it prepares and sends a command message to the process.  If the command is asynchronous, the user process can immediately return to its other work.  If the command is synchronous, the user process can block waiting for a response from the kernel process.  All system operations are handled this way.

## Dynamic Memory

NanoOs does support dynamic memory, just not very much of it.  The memory manager is started as the last process and makes use of all of the remaining memory at that time.  As of 18-Feb-2025, that amount is about 2765 bytes.  Due to the way the data structures are organized, the maximum amount of dynamic memory supported is 4 KB.

The memory manager is a modified bump allocator that supports automatic memory compaction.  Any time the last pointer in the allocation list is freed, the next pointer is moved backward until a block that has not been freed is found or until the beginning of dynamic memory is reached.  The number of outstanding memory allocations is not tracked because it's unnecessary.  This allows partial reclamation of memory space without having to free all outstanding allocations.

The memory manager also tracks the size of each allocation.  This allows for realloc to function correctly.  If the size of the reallocation is less than or equal to the size already allocated, no action is taken.  If the size of the reallocation is greater than the size already allocated, the old memory can be copied to the new location before returning the new pointer to the user (as is supposed to happen for realloc).

The owning process of a piece of dynamically-allocated memory is also tracked.  By default, this is the process that originally made the allocation request.  However, the scheduler has the ability to reassign ownership on process creation.  This allows for all memory owned by an individual process to be freed in the event it is prematurely terminated.

## Filesystem

NanoOs uses a custom FAT16 implementation that's managed by the I/O process.  The implementation does not support subdirectories.  This cuts down on code space while still allowing the card to be read and written from a PC without any special requirements.

## Development History

The development history of this work is archived at this repository's [GitHub Pages](https://james-card.github.io/NanoOs/) site.

