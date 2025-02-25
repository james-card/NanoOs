# 7-Feb-2025 - VM Optimizations Part I

When last we left, "Hello, world!" was taking 12 seconds to run... or so I thought.  I did some profiling and discovered that the real time was actually 13.802 seconds.  That brought the effective clock speed of the VM down to 9.85 Hz, so we really were talking single digits here.

I suspected that one of the issues was fseek.  As much as I had improved it, I was still seeking forward and backward 16 MB at a time to do stack operations and program reads.  So, I was completely ignoring the principle of locality of reference.  Absolutely nothing was making use of the cache in the virtual memory system.

I figured that I could gain pretty substantial performance improvements if I could avoid fseek as much as possible.  That obviously wasn't going to be possible as long as I had the program and the stack in the same virtual memory segment.  So, my first course of action was to decouple the two.

Unfortunately, there were a few problems with this.  Also when we left off, I was completely out of flash storage for more program logic.  Adding support for a third memory segment was obviously going to take more program space.  So, the first thing I had to do was free up some space.  There were a few different routes I had to do this, but the easiest way was just to remove print statements.  So, the first thing I did was go through the scheduler (which was the library with the most prints) and comment out all the print statements.  That reclaimed about 1.5 KB, which was enough to work with.

I added a third memory segment between physical memory and mapped memory and I made the new stack segment begin at the same address as the mapped memory.  Why?  Because the RV32I architecture uses a "subtract-then-reference" stack push mechanism.  That means that that the last address used by the stack segment would never be more than four bytes less than the starting address of the mapped memory address (since the size of a stack push is four bytes).

Mapped meory begins at offset 0x02000000 in my VM, which means that the first value on the stack would reside at offset 0x01FFFFFC.  I had previously defined the end of physical memory for the process to be 0x00FFFFFF.  Anyone see the pattern here?  The segment of memory used can be determined by doing a right-shift of 24 bits of the memory address.  i.e. Physical memory addresses use segment 0x00, stack addresses use segment 0x01, and mapped memory addresses use segment 0x02.

Unfortunately - and this is where the extra program logic comes in - I had to have a new function that could determine the memory segment and actual address offset to use given a memory address.  For physical memory addresses, there's literally no change.  All you have to do is grab the first eight bits of the address for the memory segment and you're done.  For mapped memory, you have to grab the first eight bits for the segment and then take the lower 24 bits for the offset into the segment.  For stack, it's a little more complicated.  You have to take the first 8 bits for the segment index and then take 0x02000000 minus the provided address to get the offset into the segment.  This allows for linear increase into the segment instead of working backward from the end of it.

New calculations added with new memory segments and it was time to give it a whirl.  First run:  4.798 seconds to run "Hello, world!".  That's almost a 3X improvement just based on that one change!  Still a long way to go to get to something useful, but not bad for a first pass and such a simple change!

I thought perhaps there might be another optimization I could make to fseek.  It occurred to me that the implementation always traversed the FAT chain when the new location was less than the current location.  This isn't necessary if the new location is still within the current cluster.  So I put in code to skip the traversal if the FAT traversal if the old cluster and new cluster were the same.  End result:  Zero impact.  The problem is that all the work I'm doing right now is still within the first cluster of each file.  That would be a valuable optimization if the virtual memory area I was working with was beyond the first cluster.  For now, though, it serves no purpose, so I removed it.

OK, what else?  Well, I knew from my previous debugging work that the program was jumping backward 28 bytes in the loop it was in.  That's beyond the 16 bytes that are available in the virtual memory state cache.  So I got curious what would happen if I doubled the size of the cache.  Result:  Immediately and consistently knocked another second off the runtime.  Cool!

Now, I had a problem, though.  Initially, I was talking about two virtual memory states with 16-byte caches.  That made the total size of the RV32I VM 257 bytes, which was acceptable in my 340-byte process stacks.  Now, I was talking about three virtual memory states with 32-byte caches.  The total size of the new VM state object was 348 bytes.  So, I was beyond the allowable size of a single process stack just with that one object.  Any function calls at all were just flat out of the question.  So, I needed to reduce the total size of the VM's state object.

The first idea that came to mind was that I could just allocate all of the virtual memory states in dynamic memory.  That would work, but it would be suboptimal for two reasons:  (1) We would now be talking about allocating 96 bytes plus the size of of the rest of three state objects in dynamic memory for a single process, which is quite a lot; and (2) I don't really need 32 bytes of cache for each memory segment.  32 bytes of cache for the physical memory segment definitely seemed to help, but it wouldn't be as useful for the stack or mapped memory segments.

So, I changed the internal buffer of the VirtualMemoryState object to be dynamically allocated and allocated 32 bytes for the physical memory, 16 bytes for the stack, and 4 bytes for the mapped memory.  With the buffer allocated as a pointer (and its size allocated as a `uint16_t`), this brought the total size of the VM's state object down to 264 bytes.  Still a little more than I would like, but acceptable.  If I run into more problems down the line, I can convert the entire virtual memory array into dynamic memory too.  Hopefully, that won't be necessary.

At this point, my execution time is down to about 3.75 seconds, or a little better than a 3.5X improvement in speed over the original baseline.  Much better, but still a long way to go.

Then...  I don't know what happened.  The program stopped running.  It wasn't even starting.  The VM state was being initialized just fine but it was never running.  I pulled the card and put it in my laptop.  Filesystem corrupted.  Great.  Literally no idea what happened.  It was in such a bad state that I couldn't even remove files.  OK, format, reload the program, and start over.

Strangely, when the system came back up, the program ran in less time.  With a 32-byte buffer, it was running in about 2.125 seconds.  With a 16-byte buffer, it ran in about 2.625 seconds.  Weird.  Well, at the 32-byte buffer rate, I was now at almost 6.5 times faster than baseline.  I'm not complaining, I'd just like to understand why the sudden improvement.  Meh.  Moving on.

I then realized that the best case scenario was that the entire program was stored in the virtual memory cache.  Since the whole thing was only 143 bytes, this was actually possible.  I made the buffer 144 bytes in size to see what would happen.  Result:  1.343 second runtime.  About 10.25 times better than baseline and an effective clock speed of about 101 Hz, but still a long, long way from useful.  I then made one more change and bumped the size of the stack segment's cache to 64 bytes.  Result:  1.306 second runtime.  So, it seemed that the best-case clock speed of the VM with this infrastructure was a little over 100 Hz (104.135 Hz if my math is right).

Why, though?  The entire program was in memory now.  What was taking so long?  I moved the time calculations and prints to the fetch part of the loop.  What I found surprised me.  There was a very clear pattern of seven fetches in a row taking 0 milliseconds and then one taking almost 40 milliseconds.  The pattern repeated several times, so I figured (correctly) that this represented the loop that walked through the string to compute the length.  Why was one load taking so much longer than the rest of them?  Then, I took a look at the execution times.  Exact same pattern.  Now, that really didn't make any sense.

I put in a debug print to print instructions that took longer than one millisecond to execute.  Then, I used Claude to decode the instruction.  It was a load instruction.  So, there were two memory accesses that were taking almost 40 milliseconds each.  Why, why, why?

I started adding prints into the virtual memory library.  And guess what...  It was doing not just one but two file reads:   One for the instruction fetch and one for the memory load.  That didn't make any sense to me.  So, then I added a print for the base address it was reading from.  The program was organized around a base address of 0x1000, so the base address for both should have been that as well.  But, what I found was that the base address of the instruction fetch was 0xFC0 and the base address for the memory load was 0x1050.  So, the program was basically being loaded in two pieces and having to read forward for the data and then backward for the instruction every eight instructions.

Then, I realized the mistake I made.  144 is not an even power of two.  The size of the buffer isn't just used to manage the size of the cache, it's also used as the alignment for the read and write addresses.  So, the system was using the nearest multiple of 144 to manage the buffer, which split the program into two.

So, I changed the buffer size to 256 bytes and tried again.  Result:  65 millisecond runtime.  **HOORAY!!!**  That's an effective clock speed of over 2 kHz!!!  I can live with that!

Now, unfortunately, this is the absolute best case since the entire program was in memory, and it's unsustainable.  However, there are a few key take-aways from this so far:

1. File reads must be kept to an absolute minimum.
2. I need to see if I can increase the speed of SD card reads in general.
3. Data and code need to be in independently-addressable virtual memory segments.
4. Anything I can do to further optimize fseek is desirable.

So, more optimizations to do to get this system to acceptable speeds in the general case, but I do have confidence that that's possible now.  We're getting there!  To be continued...

[Table of Contents](.)
