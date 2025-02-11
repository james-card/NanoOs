# XX-Feb-2025 - Cleanup

I needed to get to the point of being able to run arbitrary commands that were loaded from the filesystem.  In order to do that, I was going to have to add in some more system calls in the VM handler.  And, in order to do that, I needed more flash storage space.  Also, as I mentioned previously, running a VM process was taking up so much dynamic memory that my ability to run more than one process was questionable at best.  So, time to do some housekeeping.

There were three goals here:

1. Reclaim as much flash storage as possible
2. Reclaim as much RAM as possible any way I can
3. Keep the "Hello, world!" program running at 7 kHz

When I started this effort, I was consuming 48,843 of the available 49,152 bytes of flash, so I had only 309 bytes of program storage left.  One of the things I absolutely had to do was create a parser for whatever file format I came up with for my executables.  I wasn't sure if 309 bytes was enough, but I didn't want that to be all I had to work with because there were other things I needed to be able to do beyond that.

First thing's first:  Make a branch.  I did **NOT** want to mess up the VM branch.  So, I made a cleanup branch with the contents the rv32i-vm branch in it.  I should note that this committed me using the VM infrastructure going forward.

To be continued...

[Table of Contents](.)
