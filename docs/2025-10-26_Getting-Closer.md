# 26-Oct-2025 - Getting Closer

Things finally slowed down in my life enough for me to pick this up again.  I was never happy about the fact that I wasn't able to get very close to a Unix-like OS and I really wanted to fix that.  So, here we go!

The first step was to get hardware that was a better fit.  There were two limitations of the Arduino&reg; Nano Every that I had initially used:  storage size and RAM size.  Of the two, RAM was definitely the bigger concern.  48 KB of storage was definitely not enough and would likely cause issues as well, but 6 KB of RAM was simply never going to work.  Also, the Nano Every was a Harvard architecture, which meant that I couldn't run compiled code from RAM.  My experiments with a VM told me that that approach wasn't practical, so I had to find something that was a Von Neumann architecture as well.

After a little research, I decided to go with the Arduino&reg; Nano 33 IoT.  This is a Von Neumann system with 32 KB of RAM and 256 KB of on-board storage.  256 KB is definitely not enough for a full Unix-like OS, even an early one.  (I think the early systems consumed around 500 to 750 KB for the OS.)  However, my plan is to use the SD card as the storage for all the userspace utilities.  The on-board storage only needs to be big enough for core components and 256 KB should be large enough for that.

The 33 IoT is a 48 MHz, 32-bit, Arm&reg; Cortex&reg; M0 processor while the Every was a 20 MHz, 8-bit AVR&reg; ATMega4809 processor.  This immediately caused problems when I loaded my build onto the new board.  Being an 8-bit processor, there were no memory alignment issues on the Every and I hadn't had anything in my code related to alignment.  All memory accesses on the 33 IoT have to be aligned to the width of the data type.  My failure to have done that caused the processor to lock up **HARD** the instant it started.  Due to the space constraints I had on the Every, I had turned off all debugging in my branch, so when I first loaded the build, I just got no output at all.  To make matters worse, the way that the processor locked up made it very difficult to load a new build.  It took several rounds of failed loads and a lot more debugging before I could even get output to work.  I eventually discovered that my initial culprit was my memory manager and the fact that the addresses that my malloc implementation was returning were not 32-bit aligned.  Fixing that got me much further.  While I was at it, I also added a seven-second delay to the startup code so that I could load new firmware if I uploaded a bad build again.

Once I got past that, the next issue was the size of the stacks.  On the Every, integers and memory addresses were only two bytes wide and so each stack push consumed only two bytes.  On the 33 IoT, integers and memory addresses are four bytes wide and the stack pushes reflected that.  That meant that all of my function calls consumed at least double the stack space that they had on the Every.  The coroutine library I use uses stack segmentation to achieve its magic and this increased consumption meant that processes were corrupting the stacks of other processes in pretty short order.  I had to a little more than double the stack size to address this.  Once I did that, I was at least not seeing any more memory issues.

However, at this point, I was not able to read anything from my SD card.  The changes I'd made to the code were minimal at this point and, without debug messages enabled, I was still able to make a build that would fit on the Every, so I swapped out the 33 IoT with the Every to make sure my changes hadn't broken anything in software.  Everything still worked fine on the Every, so it had something to do with the 33 IoT.

The morning I had soldered the pins onto the 33 IoT, my hands had been shaking from excitement.  On top of that, I had accidentally soldered one of the rows at a slight angle and had had to bend the pins to get them to fit in the breadboard properly.  I figured I had probably damaged a solder joint somewhere in the process, so I ordered second board and tried again.  This time, I made certain that I soldered the pins absolutely perfectly, however I was still met with failure to read from the card.

After quite a long battle, I finally noticed something in the data sheet that had escaped me initally:  The 33 IoT is a 3.3 V-only board and does not support 5 V at all.  The Every is 5 V.  I went back and checked my SD card reader and it is a 5 V-only board.  Luckily, I happened to have a 3.3 V card reader handy.  I swapped out the card reader and BINGO!  I was finally able to read the /etc/hostname file again.

I was never happy with my filesystem implementation in my first draft.  For one thing, it had a bug in it that was corrupting the filesystem when creating a new file.  Also, though, being FAT16, I was limited to 2 GB partitions, which meant I wasn't able to use most of the SD card that I had (and wouldn't be able to use most of any modern SD card).  I decided to do this right this time and make an exFAT driver that would address both issues.

Getting to the point of being able to read from the entire card (I bought a new 1 TB SD card for this effort) was pretty straightforward.  Fixing the file corruption bug when writing a new file, however, turned out to be a pretty big pain.  To recap, I had used Claude&trade; AI for my original FAT16 filesystem driver and I used it again for the exFAT driver.  However, it initally made the same mistake in the exFAT driver that it had made in the FAT16 driver that was causing it to corrupt the filesystem when creating a new file.  It took me about a week of night and weekend debug work to trace down what the issue was.  In the end, it was the checksum algorithm.  FAT is case-insensitive and it had done a case-sensitive checksum.  Once that got resolved, creating files worked fine!

So, OS back to a working state and a better filesystem in use, it was time to figure out how to load a program from the filesystem into RAM and run it.  Like any good programmer, I started out with a simple "Hello, world!" program.  I say, "simple," but in truth it was not at all clear to me initially how this was going to work.  There were a few inter-related things that I had to work out:

1. What was going to be the base address for the executables?
2. How were the programs going to access kernel functionality?
3. How much RAM could I realistically afford to allocate to a running program?
4. Regardless of how much I could afford, how could I keep the programs as small as possible?
5. How could I get multiple user programs to run concurrently?

The fact that I was now working with a 32-bit processor made issues 3 and 4 more difficult for me than Ken Thompson had to deal with for early Unix.  The PDP-11 was a 16-bit processor.  So, my user programs are likely to be double the size of the user programs on early Unix.  That's annoying.

The M0 processor on the 33 IoT has no MMU, so virtual memory is completely out of the question.  However, the PDP-11/20 that early Unix ran on didn't have an MMU either, so there was clearly a way around this.  I had heard that the early versions used overlays, so I started looking into what was involved with that.  Short answer:  It just means breaking up a single program into segments that utilize the same piece of physical memory.  All segments are organized around the same base address.  Segments are loaded and unloaded as needed by the program.  The consequence of this to me meant that I was going to have to have a way for a user program to call a function in a different physical segment of the program.  OK, fine, I'll make an API for that.  While perhaps not ideal, this did provide a solution to issue 5.

This bugged me, though.  In the PDP-11/20, maybe they only had one physical memory segment that they could use due to the limited amount of memory they had available.  By the time of Windows&reg; 3.0, however, that was definitely not the case.  By that time, they were working with hundreds of KB or a few MB of RAM and they ran programs concurrently out of memory without swapping from disk.  Windows 3.0 also supported running on an 8086, which did not have an MMU.  So, how did they do concurrent execution in that time frame?

I did some research into this and did not like what I found.  I knew the .exe format used by early Windows&reg; (the "new executable format") was different than the one used by DOS, but I never knew what that difference was or why the formats were different.  Now, I do.  The Windows&reg; .exe format contained information about the addresses of functions in the binary.  When the Windows&reg; loader loaded an executable into RAM, it went through and adjusted all the addresses to align to wherever the program had been loaded into memory.  The base address of the program was largely irrelevant because of this.  This is also why starting programs in early Windows&reg; took so long.

This is not something I could do in my environment without either re-using the Windows&reg; executable format or inventing my own, neither of which I wanted to do because that would cut into the amount of space that executable code could use in memory.  My research did give me an idea, though.  Part of the fixups that Windows&reg; did to a program was to provide it the memory addresses of Windows&reg; functions that the program relied on.  What I could do was provide the program with function pointers into the OS code for it to use for its functionality.  That would address issue 2.  Also, I could implement the bulk of the C standard library in the OS storage space and provide function pointers for those functions as well, thereby providing a solution to issue 4.

At this point, I only needed to resolve issues 1 and 3, which were more trivial.  The total amount of RAM available in this system is 32 KB, which is a little more than what was available in the the PDP-11/20 that early Unix ran on (it had 24 KB).  I would have preferred to divide total RAM into 16 KB for the OS and 16 KB for user space and then divide user space into 8 KB per console so that I could have two dedicated process spaces.  This, however, was not feasible.  I definitely need 16 KB for the OS at this point given the increase in stack sizes, but not all of the remaining 16 KB is available to me.  It turns out that the Arduino&reg; libraries use their own dynamic memory allocation that starts out at the bottom of the available RAM.  I had missed that because my custom memory allocator didn't start from the bottom of RAM.  I only discovered this because I modified my allocator to start at the bottom and wound up corrupting state that the Arduino libraries were using.  So, I have to cut back on what I'm using for my own purposes.  I decided to go with a single 8 KB block for user space applications for now.  Maybe I'll be able to extend it in the future.  That, however, did simplify issue 1.  With only a single block to worry about, I don't have to do anything special to adjust for the applications being at different base addresses.  Just use the one address and we're good.

With all 5 issues resolved, it was time to start coding my "Hello, world!" user program.  To get access to the kernel printf implementation (and a bunch of other functions), I built a structure of function pointers to my implementations.  I then built a header in the user program that's placed at the beginning of the compiled binary at the fixed base address.  The program loader reads the program into memory and then populates the program's structure pointer with the structure that's built into the OS.  In the user application, I wrote the standard C headers with function macros that actually dereference the function pointers in the header structure.  So, when the application calls:

```C
printf("Hello, world!\n");
```

What it was actually calling was:

```C
overlayMap.header.stdCApi->printf("Hello, world!\n");
```

After a bit of fighting with the compiler and makefiles, I got a version of my binary that was stripped down to just the basics.  It was a whopping 108 bytes!  I wrote the loader in the OS and, for now, just wrote a command handler for the helloworld program so that it could be found by the existing shell stub.  I reloaded the SD card and OS and gave it a shot and guess what...  It worked the first time!  I couldn't believe it.

I decided to get a little fancy and implement my echo command handler as a user program too.  I literally just copied-and-pasted the logic from what was hardcoded into the OS into its own user application and then changed the OS command handler to load the binary from disk.  It worked perfectly as well!  Can you beat that?!

OK, user programs working, it was time to start getting more serious about really implementing Unix.  The first thing I needed to do was replace the login mechanism with one that was loaded from disk like it should be.  So, I started researching how this works in Linux.

I was pretty surprised by what I found.  It's a multi-stage process where execution is passed from one program to another.  It starts with getty displaying the parsed contents of `/etc/issue` and a `login:` prompt and getting the username that's entered.  At that point, it execs the login program, which takes the username as an argument and prompts for a password (which is not echoed to the terminal).  Once the password is read, the user credentials are validated.  If the user is authenticated, execution is then handed off to the user's shell.  If not, execution is handed back to getty.

Well, one thing at a time, here.  I started with getty.  This turned out to be a bit more involved than I had expected and expanded the scope of my original standard C API.  It turns out that the contents of `/etc/issue` can contain escape sequences that are replaced by getty.  There are various things that can be displayed about the system such as the name of the OS, the version, the hostname, etc.  The thing is, the calls that need to be made to get these pieces of information are not standard C calls, they're POSIX or Unix calls.  Well, OK, great!  My goal is to get closer to Unix, so this seems like a wonderful place to start!

I renamed my stdCApi structure of function pointers to unixApi and expanded it to include the functions I needed to provide the information in the escape sequences.  I then went about writing my getty implementation as a user program.  After a few days and back and forth between OS space and user space, I had a `/etc/issue` file that looked like this:

```
\s \r \n \l

```

And output that looked like this:

```
NanoOs 0.2.0 james-nano33iot-1 tty1

login:
```

Pretty cool!

There were several OS things that had to be developed here.  I was already reading in the hostname at boot time, but I hadn't created a way to retrieve that information from a regular process, so that came when I went to implement the `gethostname` function.  I also ran into a problem when implementing these functions.  Being that this is a single-core system that had previously had everything running in "kernel space" (if you can consider it that), I had made `errno` a global integer the way it was in the original C spec.  With programs now being swapped in and out of memory dynamically and switching between kernel operations and user operations, that was no longer adequate.  `errno` was converted to thread-local storage in C11 due to multi-threading primitives coming into the mix.  I wound up having to implement a process-local version of errno for my processes and creating an appropriate define in user space.  So, now, when a program checks `errno`, what it actually checks is `(*overlayMap.header.unixApi->errno_())`.  Functions within the OS reference `(*errno_())` instead.

Still lots more to do here.  I now need to write a formal `exec` implementation and figure out a way to transfer the username read in from the getty application to the login application.  And, login will have to do something intelligent with /etc/passwd.  Still a way to go before I get to a shell.

One of the things I did recently was to write a formal install script.  The user applications are build with makefiles but the full thing needs to format the SD card with exFAT, build the directory tree, copy the commands, and populate the config files.  Having the installer greatly streamlined all that.

I am worried about the shell, though.  That could pretty easily exceed the current 8 KB limit.  That may be the first point at which I'll have to break a single application into multiple overlays.  We'll see.

To be continued...

[Table of Contents](.)
