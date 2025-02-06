# XX-Feb-2025 - VM Optimizations

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

Now, I had a problem, though.  Initially, I was talking about two virtual memory states with 16-byte caches.  That made the total size of the RV32I VM 257 bytes, which was acceptable in my 340-byte process stacks.  Now, I was talking about three virtual memory states with 32-byte caches.  The total size of the new VM state object was 348 bytes.  So, I was beyond the allowable size of a single process stack just with that one object.  Any function calls at all were just flat out of the question.

To be continued...

[Table of Contents](.)
