# 3-Mar-2025 - Retrospective

As I prepare to shelve this work for a bit, and as I approach the four-month mark of this effort, I thought it would be good to do a bit of a retrospective on how this has gone, what I've learned thus far, and what my thoughts are going forward.

- [User Space](#user-space)
- [Persistent Storage](#persistent-storage)
- [Distributed Computing](#distributed-computing)
- ["Everything is a... process???"](#everything-is-a-process)
- [Kernel Architecture](#kernel-architecture)
- [Permissions](#permissions)

## User Space

My final push to get arbitrary processes running in "user space" opened my eyes to the value of a kernel that supports subsystems.  But, my research into other operating systems left me with the question, "What defines a subsystem?"  The term is heavily overloaded in the industry.

What's particularly interesting about that question is the way the answer has changed for Windows Subsystem for Linux (WSL).  The answer for WSL version 1 was, "a translation layer," meaning a layer of software that converted Linux system calls to their Windows equivalents (the inverse of Wine).  The answer for WSL version 2 is, "a hardware virtual machine," meaning that a complete Linux kernel runs in a dedicated VM.

Another related technology is [z/OS](https://en.wikipedia.org/wiki/Z/OS)'s [Unix System Services (USS)](https://en.wikipedia.org/wiki/UNIX_System_Services).  Unlike the Windows approach, this provides API compatibility, not ABI compatibility.  POSIX-compliant programs will run if they're recompiled to run within z/OS's USS environment, but it doesn't run a native binary from any other platform.  So, in that context, a subsystem could be, "a compiler and set of libraries."

The work I did to develop the RV32I VM was super educational, but not terribly useful in its current form.  The user space functionality depends on services provided by the kernel.  Ultimately, if the kernel can't provide the functionality needed by user space, the ability to execute the instructions is of no value.  I didn't have enough kernel program space to get to the point of needing kernel services with the [WASM](https://en.wikipedia.org/wiki/WebAssembly) VM, but I would have run into the same problem there.  So, I think that, regardless of the definition of, "subsystem," it has to include services provided by the kernel at some level.

The direction I was headed with my RV32I VM most-closely resembled the WSL version 1 approach to a subsystem.  The use of kernel services happened via the use of ECALL instructions (instructions that switched from the user context to the kernel context).  In my case, I was defining system calls that were unique to NanoOs, but I could have just as well defined calls that were compatible with Linux or Windows or any other OS.

There is a reason, however, that Microsoft abandoned that approach in WSL version 2:  It's not generic or versatile.  Intercepting calls at that level means that you forever play catch-up with whatever OS you're emulating.  Supporting multiple OS environments means constatnly keeping up with each OS vendor.  Intercepting calls at the hardware interface level, however, gives the developer more freedom.  As long as you emulate hardware that's supported by the guest OS, you can run any application that the OS supports.  The developer can choose to standardize on a particular set of hardware or hardware that conforms to industry standards and not have to update their subsystem with each new patch that comes out for the supported OS.  And, any OS that runs on the emulated hardware can (in theory) be supported.

But, even that isn't good enough for mission-critical applications.  Hosting a guest OS exposes the application to all of the bugs that come along with the OS.  Code running in a VM will also never be as performant as native code.  In fact, the translation layer approach might actually be more stable and performant.  Really, for a mission-critical application where both stability and performance are absolutely paramount, the only suitable choice is to compile the application for the host OS.  This is why IBM went with API compatiblity for z/OS's USS instead of ABI compatibility.

z/OS does have "z/OS Container Extensions (zCX)," though.  That feature provides a Docker-compatible container runtime environment, which allows unmodified, x86-64 Linux binaries to run within z/OS.  I thought this concept was especially interesting because it's almost identical to the way I had designed my VM in that it both emulates the x86-64 instructions in software and intercepts the system calls to enable Linux functionality.  The neat thing about this is that I didn't find out about this feature until after I had already built my VM.  Great minds think alike!!!

All of these approaches have something in common:  The use of virtual memory.  The real power of virtual memory was never apparent to me before my efforts to implement the RV32I VM.  Virtual memory is really what enables multitasking to work.  It's what allows every program to have the same memory model and address organization.  Without virtual memory, modern operating systems wouldn't even be possible.  We'd all still be stuck using one program at a time!

One of the other things I learned is that "static" linking on Windows isn't truly static the way it is on Linux.  On Windows, even programs that are statically linked still make calls into dynamically-linked system libraries.  That's what makes the subsystem model possible on Windows.  Programs literally have no alternative but to go through OS libraries for some things.  They cannot directly call into the kernel.  I liked that idea a lot.

Unfortunately, I don't think it's realistic.  It's only possible on Windows because Microsoft controls the entire environment and has all the way back to Windows 1.0.  For everyone else in the world, I think the only feasible intercept point is at the system call boundary the way zCX and my current VM implementation have done things.

So, I think for me, a "subsystem" would be "a virtual address space using a specific instruction set and system call set."  That seems like the simplest and cleanest model to me at the moment.  That's likely the direction I would pursue if I continue down the road of extending my user space model.  That "specific instruction set" doesn't necessarily have to be software emulated like it is now.  From where I'm sitting right now, I don't see any reason why it couldn't be the native instruction set of the hardware running the OS (assuming that the hardware had adequate memory protection), but it could also continue to be software emulated.

And, the option for software emulation of an instruction set leaves open the possibility of supporting software written for virtual machines.  The work I was doing to support [WASM](https://en.wikipedia.org/wiki/WebAssembly) could be resurrected.  Subsystems for [Java](https://en.wikipedia.org/wiki/Java_\(programming_language\)) and [.NET](https://en.wikipedia.org/wiki/.NET) would also be possible.  "Native" support for those platforms would be really cool.

## Persistent Storage

The more I learned about z/OS, the more it blew my mind.  It bears absolutely zero resemblance to UNIX.  I realized in learning about it how much the UNIX model has affected both my own mindset and the development of other major operating systems.  As much as [Dave Cutler](https://en.wikipedia.org/wiki/Dave_Cutler) claims to hate UNIX, both Windows and [VMS](https://en.wikipedia.org/wiki/OpenVMS) are much more like UNIX than z/OS is.

One of the main ways that z/OS is radically different is in how persistent storage is managed.  Prior to z/OS, all of the operating systems I'd come in contact with had a concept of a "file" that was a collection of bytes that resided in a "filesystem".  z/OS has no such concept natively.  USS provides something that looks like files in a filesystem, but there's a compatibility layer that does translation to the real underlying storage.

z/OS's concept of a unit of persistent storage is a "record".  I'm still struggling to understand what, exactly, a record is.  I understand it's akin to a record in a database, but the specifics elude me.  As in a database, records are managed in transactions that are atomic.  This is one of the features of the OS that helps it be so reliable.  Data corruption is basically a non-issue.

With that in mind, I wondered if maintining raw storage in a filesystem was really what I should do.  I definitely want to support POSIX.  But, as I mentioned in a previous post, POSIX defines user space, not kernel space.  AS IBM has clearly shown with USS, it's certainly possible to have a POSIX-compliant subsystem that has a completely different underlying architecture.  And, given z/OS's reputation as the most stable operating system in the world, there's certainly reason to challenge the idea that the OS has to natively be organized around a filesystem.

Another reason to challenge the idea is [WinFS](https://en.wikipedia.org/wiki/WinFS).  This was Microsoft's attempt to fix the problem of related information being distributed across different places in a filesystem hierarchy.  Like z/OS's native storage, it's based around a database model (specifically a relational database model in this case).  Bill Gates once mentioned the failure to get WinFS adopted as his greatest regret about his time at Microsoft.  In his view, it was an idea that was ahead of its time.  Be that as it may, the fact that it shares similarity with z/OS's storage should give pause at the least.

For me personally, however, deviating from the filesystem model comes with risk.  The filesystem model has been battle hardened over the past 50 years via thousands of person years.  The database model, while potentially much more stable and powerful, is proprietary.  If I'm going to try and recreate that model (or any other model), I would be on my own.  The fact that even the operating systems that Dave Cutler has made are still basically organized around the concept of files says something about the value of the idea.

So, I don't know on this one.  I'm in love with the idea of a persistent storage system that makes data corruption impossible.  And, given my preference for mandating that all processes be run in a subsystem, there's no reason why that subsystem can't present a filesystem to the application when none really exists internally.  There would definitely be a lot of work to do here if I really want to go down the road of database-like storage, but maybe it would be worth it?  Not sure.  I'm definitely going to consider it but I don't have a firm idea one way or the other right now.

## Distributed Computing

When I started this effort, I had not yet added message passing to my coroutines library or to my C threads libraries.  That has proven to be the single-most-significant aspect of this work.  Nothing else I've done throughout this effort would have been possible without that development... at least, not without significantly more-complex and messier code.  Using that infrastructure was a huge help and simplification to the rest of the design.  The advent of that functionality alone has made this effort worthwhile.

So, my libraries now have message passing between coroutines and between threads.  If I can ever get the attention of the C standard committee, maybe those things will become standard parts of C at some point in the future.  C has no mechanism for inter-process messaging, though, and that's very, very unfortunate.  POSIX does define a mechanism, but I've never liked having to rely on POSIX for things at the process level.  I don't particularly like POSIX's design for message queues either.  I digress, though.

I will eventually have to add in support for networking into my OS if I take it far enough.  I've been reading a lot of [Alan Kay's](https://en.wikipedia.org/wiki/Alan_Kay) thoughts on computing recently.  In his original concept of "object-oriented programming," an "object" was more like a process or even an entire operating system than the objects we know today in languages like C++.  In his vision, an object is something that receives messages, does something with them, and sends messages either back to the original entity or to the next object that's appropriate for processing.

All this is to say that, as I consider the future direction of my OS, I want to keep in mind that its main responsibility is to facilitate the processing of data by processes.  From the standpoint of the OS, the messaging required to do that may be between processes running within the same OS or between processes running on different hosts.  I would **REALLY** like it if the processes didn't know the difference.

That means that one of the things I'll have to consider is what an inter-process message looks like.  In the embedded version of the OS, it's just a coroutine message and that's all that will ever make sense.  In that model, "processes" are addressed by their numeric process IDs which are then translated to the real Coroutine objects that manage the state of the process.

Extending this to a networked operating system, though, begs the question of how processes should be identified.  POSIX is insufficent here.  It only provides a mechanism to address a queue, not a process.  So, it will be up to me to come up with something that makes more sense.

I should note that the format of the messages themselves is really not up for debate for me.  I spent a lot of time ironing that out during the development of the OS.  The contents of the thread messages and the coroutine messages are identical.  It's only the functional implementation that's different.  I would use the same format between processes as well.

What would become necessary (and perhaps it alredy is) is a serialization/deserialization mechanism for the messages.  The possibility of sending data over the wire necessitates a standard encoding and decoding scheme between OS instances.  Within the same OS, memory can simply be copied, of course.

IP networking definitely would have to be supported, as would some reliable data exchange protocol.  TCP would probably be fine, but SCTP is probably more appropriate given that we're talking discrete packets of information.  In either case, we're talking about an IP address and a port number to identify a running instance of an OS.

But, how do you identify a process?  In the embedded OS, the kernel processes have well-known PIDs, so they're easy to address.  In a distributed environment, that's really not sufficient.  Really what you'd want is a service in the OS that a process can register with to receive messages.  Processes would probably want to register with a name.  If two processes register with the same name, then you'd want to have an index into the processes registered with that name.  So, I think a full "process ID" would be an IP address, port number, process name, and process index.

There's an open question of how different OS instances would become aware of each other.  This matters because it has the potential to affect how processes address one another.  In lieu of an IP address, a domain name might be a more-appropriate addressing mechanism at the host level.  However, that would be a "central authority" based addressing scheme.  Peer-to-peer mechanisms could also be possible.  This is getting off into the weeds, though, and is really a topic for further down the road.

## "Everything is a... process???"

One of the things I did in the embedded OS - and one of the things that brought the UNIX model of "everything is a file" into question for me - was that I made a process directly responsible for managing a device (the MicroSD card) and defined a message-passing API for other processes to interface with the device.  (Note that I wound up collapsing the console, the filesystem, and the SD card into a single process in the RV32I VM version of the OS to conserve space, but that was out of desperation and not because I thought it was a good architectural decision.)  For the purposes of POSIX compliance in user space, I would present devices under `/dev` as per usual in UNIX environments.  In kernel space, however, having them as processes is a very convenient and easy-to-manage abstraction.  Calls that addressed devices by file path from user space would be translated to the appropriate process and process API in kernel space.

The console manager, the kernel memory manager, and the filesystem were also all processes.  Even the scheduler was a process.  The organization of functionality into processes plus the use of the message passing infrastructure made "separation of concerns" and construction of well-defined APIs both pretty trivial.  This was a very clean model and I would definitely keep it in a more-extensive OS.

This smacks a bit of functional programming... which is probably to be expected since I just came off a project that was using a functional programming language.  Pure functional programming or any other paradigm is not a goal, though.  I am by no means an idealist.  But, I do try to apply useful concepts where it makes sense.  Process-oriented organization and message passing between processes makes a lot of sense for an operating system.  Having things oriented around processes also facilitates the longer-term goal of distributed computing.

## Kernel Architecture

NanoOs was intended to be a nanokernel, which means that there is no kernel.  There are processes that provided kernel services, but there was no distinction between kernel space and user space in the original design, and there was certainly no memory protection.

That said, the architecture didn't really fit the term "nanokernel" as it applied to MacOS 9 and prior.  In that environment, there was really just a collection of libraries that provided the functionality that a kernel would provide.  To  my knowledge, that system wasn't designed around the idea of message passing between processes.  The fact that NanoOs was organized around process boundaries with a message passing system gave it properties of a microkernel.

I am extremely leery of microkernels.  One of the things I researched was the GNU Hurd kernel.  I was appalled by what I learned.  I had always heard that it worked poorly but I had never looked into what the issues were or why they existed.  The problem, as I discovered, is that just about everything is in user space, including hardware drivers.  To make matters worse, there's no direct communication between processes.  EVERYTHING has to go through the kernel to communicate with another process.  And, on top of that, every message has to be copied \*TWICE\*:  Once from the sender into kernel space and again from kernel space to the recipient.

This is not the way NanoOs works, and there is absolutely no way I would ever build a system like that.  Processes today have the ability to communicate directly with each other and I have no intention to change that.  NanoOs also doesn't do any copying of messages.  The memory used to send a message from one process is the same memory that's accessed in the receiving process.  I have no intention of changing that either.  There may be some kind of permissions operations that happen on the memory that's used, but there's no way I'd require a copy... and certainly not \*TWO\* copies!

Both z/OS and Linux are monolithic kernels, as was the the original UNIX kernel and some of the BSD implementations that remain today.  The z/OS kernel is considered a "hardware assisted" monolithic kernel.  The first version of Linux was too.  I have to wonder if [Linus Torvalds](https://en.wikipedia.org/wiki/Linus_Torvalds) would have chosen a purely monolithic kernel architecture if he had known he would have to relinquish that in future versions.  Linux has proven that it's possible for a monolithic kernel to be portable to different hardware, but (a) that wasn't the goal when Linux was first created and (b) I'm not sure that a monolithic kernel would be the best choice if that was a goal.  I have heard (although I have no direct evidence of this) that the monolithic architecture of Linux does make it more challenging to port it to different hardware than it would be if the core of it had been more of a microkernel.

Consolidating the console, filesystem, and MicroSD card processes into a single I/O process made NanoOs more like a monolithc kernel.  I was not a fan of the result.  First and foremost, doing that blatantly violated the separation of concerns princple of opertaing systems.  More practically, however, it made the code more difficult to manage and maintain.  If I extend the OS into a system with more resources, I will definitely separate them out again.

My VM implementation and the direction I favor with subsystem design, however, does make the OS function *conceptually* like a monolithic kernel from the standpoint of a process.  In my concept of a subsystem, it would be the subsystem's responsibility to construct and pass a message to the appropriate process.

In this architecture, there would be no such thing as a hardware abstraction "layer," nor would there be any need for one.  A process that directly interacted with a piece of hardware would be responsible for the abstraction.  Like the MicroSD card process, a device driver would be a process with a well-defined message API.  Processes that needed access to the hardware would interface with it via (zero-copy) messages.

I watched an interview with Dave Cutler in which he stated that UNIX is extended by adding things to the filesystem while Windows is extended by adding new objects.  I think that's taking things a bit far but, in that example, this architecture would be closer to the Windows model than the UNIX model if a process is an "object" (as it would be under Alan Kay's definition of the term).  New functionality could be added to the OS at any time just by launching a new kernel process.

So, I'm not really sure what to call this architecture.  Is it a "hybrid kernel"?  It almost seems like the inverse of what usually goes by that name.  Is it a "modular monolithic kernel"?  It kind of looks that way from the standpoint of a user space program.  Is it something else entirely?  ::shrug::  I don't know and maybe the name isn't important.  It's an architecture that seems to scale well from the embedded space upward.  Not sure what to call that.

## Permissions

I only got into the area of permissions a very little bit this round.  It's fine for the embedded version, but I would need to take this much, much further (and do it right) if I were to extend the OS.

I did develop a concept of memory ownership, but I didn't get into the area of memory permissions.  I would need to do that in order to properly manage zero-copy messages.

Then, there's the issue of permissions on resources such as files and devices.  These are POSIX entities, though.  Exposing these things via a subsystem puts some amount of enforcement burden on the subsystem, not just the kernel.  I would need to work out both the division of enforcement responsibility as well as the communication of permissions between the kernel and the subsystem.

Regardless, I would need to work out some sort of [Access Control List (ACL)](https://en.wikipedia.org/wiki/Access-control_list) mechanism.  One of the things I wondered about was what the permissions between processes should be.  Should every kernel process be allowed to talk to every other kernel process?  How should a process receiving a message determine whether or not the sending process was authorized for particular operation?  How would the permissions of operations be communicated?  What would do the communication?  What happens to permissions when a process exits?

This is all to say there would be a whole lot to do in this area.  I definitely do \*NOT\* want to build an OS that doesn't take security into account from the beginning.  Even the embedded OS has some concept of permissions because there are certain operations that only the scheduler is allowed to perform.  This is an area that deserves a lot of thought and attention.

The risk here is that I could make an oversight.  Security and permissions concepts have come a very long way since the early versions of UNIX.  There's a reason why Windows NT's ACLs were more advanced than UNIX's at the time NT was created and why Linux has extended things with [SELinux](https://en.wikipedia.org/wiki/Security-Enhanced_Linux).  I really need to do my homework here.

[Table of Contents](.)
