# XX-Feb-2025 - Cleanup

I needed to get to the point of being able to run arbitrary commands that were loaded from the filesystem.  In order to do that, I was going to have to add in some more system calls in the VM handler.  And, in order to do that, I needed more flash storage space.  Also, as I mentioned previously, running a VM process was taking up so much dynamic memory that my ability to run more than one process was questionable at best.  So, time to do some housekeeping.

When I started this effort, I was consuming 48,350 of the available 49,152 bytes of flash without debug prints, so I had only 802 bytes of program storage left.  One of the things I absolutely had to do was create a parser for whatever file format I came up with for my executables.  I wasn't sure if 802 bytes was enough, but I didn't want that to be all I had to work with because there were other things I needed to be able to do beyond that.

I had four goals in mind when I started this:

1. Reclaim enough RAM to be able to run at least 4 VM processes in parallel
2. Reclaim enough flash storage to implement an executable parser and loader
3. Reclaim enough flash storage to implement as much of standard C as possible
4. Keep the "Hello, world!" program running at 7 kHz (or better)

First thing's first:  Make a branch.  I did **NOT** want to mess up the VM branch.  So, I made a cleanup branch with the contents the rv32i-vm branch in it.  I should note that this committed me using the VM infrastructure going forward.

My primary idea for space reclamation (both RAM and flash) was to consolidate processes.  The way this would reclaim RAM was pretty straightforward:  Run fewer total processes in the kernel and there would be more dynamic memory available.  For flash, what I realized was that the FAT16 library and the SD card library were always serial.  So, there was really no point in having them as two separate processes which, in turn, meant that there was no point in having two separate libraries.  And, because there wouldn't be two processes, there would be no need for any of the infrastructure to allow the filesystem process to call into the SD card process.

Consolidating processes is a really bad idea from an architectural perspective because it makes the kernel less modular.  If I was working with gigabytes or even megabytes of RAM and storage, I would absolutely **NOT** do this and I will be the first one to recommend against it to anyone else who considers an architecture like this.  But, I have to also be practical here.  Bytes matter in both RAM and program storage and I really don't have the luxury of being a purist here, so I needed to take a more practical approach.

I started a new library with the pieces of the FAT16 and SD card libraries.  This provided me with a way to have one, consolidated place for all disk-based I/O functions.  This was good because I was also able to consolidate the console library into the new library as well.

One of the things I had been unhappy with was the division of I/O functions across three libraries:  The filesystem library, the console library, and the LIBC implementation.  The new structure gave me an opportunity to restructure things and clean up a lot.  I was able to put all the handler logic into the new I/O library and all the calling logic into the LIBC implementation.

After all my consolidation, my total flash usage was at 46,839 bytes.  So, I reclaimed about 1.5 KB.  I wasn't super happy with that, but it's more than I had.  I can still trim out a little more by taking out some more error messages, but I'll avoid doing that unless I absolutely have to.

One of the things that I discovered while doing the cleanup was that the FAT16 implementation that Claude had put together was re-computing the directory offset of the file blocks every time it needed to update a directory entry.  It did this by looking up the filename in the directory every time.  To make matters worse, it had "stored" the filename by using strdup on the name of the file when the file was opened.  I have no idea what strdup does in the Arduino, but it doesn't use dynamic memory.  I changed the code to store the directory offset when the file is opened and just use that instead of looking up the file each time.  This fixed the strdup bug and resulted in about an 11% performance improvement!  The program actually ran at 8 kHz!  Cool!!!

When Claude had put together the handler to do a print to the console, it had used the raw `Serial.write()` call in the Arduino libraries.  I needed to convert it to using the standard `fwrite()` function so that writes would go to the console owned by the VM process instead of always going to the USB serial port.  There was a problem with this, though:  My `fwrite()` implementation only handled writes to the filesystem, not to the console.  I first had to extend the infrastructure to handle writes to `stdout` and `stderr` correctly.  Once I got everything put together, it worked on the first try!  Unfortunately, the additonal level of indirection and context switching cost me the 11% gain I got from the filesystem improvement, but I was willing to live with that.  I was actually just glad I had gotten the gain so that there was no net loss.

Once I got all that working, it was time for the big test:  Running one "Hello, world!" program per console.  And guess what!  Stack overflow.  \*sigh\*

Then began a rather lengthy attempt at optimizing memory usage.  At the end of the day, there are only two possible places to store the state needed for the VMs:  On the VM's process stack or in dynamic memory.  Because a process's virtual memory is file backed, 512 bytes of dynamic memory (plus dynamic memory metadata) was needed for a block buffer to do any file operations at all.  That, I had accounted for.  However, in order to avoid stack overflows, I wound up consuming 556 bytes of dynamic memory with the data I shifted from the stack.  That was way more than expected.  I could, of course, trade speed for space and reduce the size of the virtual memory buffers used, and I did try that.  Try as I might, however, I could not reduce the amount of virtual memory to the point where even three processes could be run in parallel.  Four processes was just flat out of the question.

So, it looks like I will have to scale down the number of concurrent user processes supported to two.  While this is not what I want and makes it impossible to pipe the output of one command to the input of another (because there's no way to run any other process than the foreground processes the users are using), it does not violate my goal of reproducing the functionality of the first version of UNIX since that version did not support more than two concurrent processes and did not support pipes.  On the plus side, the lack of a need to support pipes means that I can remove the infrastructure I have in place to support that, thereby reclaiming some additional flash space.

I went ahead and added code to handle stdin as the input FILE stream in my fread implementation.  This is not being used by the current code in any way, but it will be needed for the next phase of my development, which will be a user space shell.

To be continued...

[Table of Contents](.)
