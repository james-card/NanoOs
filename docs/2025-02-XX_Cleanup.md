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

One of the things that I discovered while doing the cleanup was that the FAT16 implementation that Claude had put together was re-computing the directory offset of the file blocks every time it needed to update a directory entry.  It did this by looking up the filename in the directory every time.  To make matters worse, it had "stored" the filename by using strdup on the name of the file when the file was opened.  I have no idea what strdup does in the Arduino, but it doesn't use dynamic memory.  I changed the code to store the directory offset when the file is opened and just use that instead of looking up the file each time.  This fixed the strdup bug and resulted in about an 11% performance improvement!  Cool!!!



To be continued...

[Table of Contents](.)
