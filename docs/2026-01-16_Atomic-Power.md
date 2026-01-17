# 16-Jan-2026 - Atomic Power

Successfully enabling preemptive multitasking introduced a problem into my coroutines library.  Prior to that, there way no way for any of the lock/unlock mechanisms to be interrupted in their critical sections.  Now, there was.  This meant that I had to use atomic test-and-set, load, and store functions.

Fortunately for me, C11 introduced standard function calls for these operations when they added threads to the standard.  So, the first pass was just a matter of replacing the relevant existing logic with the its atomic equivalent.  I tested this out in the simulator and ran my regression tests to make sure I didn't break anything.  Everything was fine.

I then moved to the Nano 33 IoT.  This compiled fine but - to my surprise - didn't link.  The problem was that the linker couldn't find GCC's `__atomic_compare_exchange_4` function.  In my code, I had used the standard C `atomic_compare_exchange_strong` function.  That's implemented as a C generic and `__atomic_compare_exchange_4` is GCC's implementation when 4-byte values are involved.  It turns out that the Cortex M0 doesn't have an atomic compare-exchange instruction.  Without that, the software doesn't know how to safely do a compare and exchange operation.  The best it could do is disable ALL interrupts, implement the logic in software, then enable them again.  This, however, runs the risk of missing an interrupt entirely and causing system issues.  That would make it impossible for the compiler to guarantee the correctness of a compiled program, and so it simply can't do it at all.

I then tried to compile on the Nano Every.  The situation was even worse here.  On this platofrm, the linker complained about missing `__atomic_compare_exchange_2`, `__atomic_store_2`, and `__atomic_load_2`.  Since pointers on this platform are 16-bit values, the `__atomic_compare_exchange_2` was not unexpected, however I was initially confused about why store and load weren't already atomic operations.  I very quickly remembered that this is an 8-bit platform and so each of these operations requires two memory accesses and are therefore not atomic.

The solution here was obviously an OS-level solution, not a hardware- or compiler-level solution.  Only the OS knows how to safely ensure these operations are atomic and has the ability to make it happen.  The problem was that I didn't have a way to do that at the time.  The reason was that I needed the ability to cancel a timer and get the state that it was in in a single call and I hadn't thought to add that into my HAL.

Well, OK, so, I added the call.  Fortunately, I had had the foresight to store the time that a timer was configured into its state metadata, so implementing the function was fairly straightforward.  While I was at it, I also added a call to get the remaining time for a timer although it's not clear to me at the moment that there's actually a use case for this.

New function in hand, I implemented the `__atomic_compare_exchange_4` function for the Nano 33 IoT to stop the preemption timer, perform the logic, then enable it again for the time that was remaining if it was active.  I rebuilt the project and it successfully linked!  I loaded it onto the board and ran it again and there were no issues on the first try!  I then started working on the missing functions for the Nano Every.  `__atomic_compare_exchange_2` was just a reimplementation of `__atomic_compare_exchange_4` using `uint16_t` values instead of `uint32_t` values.  `__atomic_store_2` and `__atomic_load_2` were even simpler.  Because of the size that the OS now is and the size limitations of the Nano Every, it's not possible to load and try the build on the board, but it does compile and I have no reason to doubt that it would work fine.

So, now I have software atomicity in an environment where it's not guaranteed in hardware.  How cool is that?!  And, now my coroutines primitives are guaranteed to be atomic.  Overall, I'm pretty pleased with how well this turned out.

Something that's fallen out of this is that my coroutine primitives are now full replacements for C's thread primitives within the context of the OS.  When I get around to it, `mtx_t` and `cnd_t` can be fully implemented by my `Comutex` and `Cocondition` constructs.  Because I based all this on C's thread model, there are 1:1 mappings for all the objects and functionality now.

So, maybe now I can finally get back to the work I was doing on overlays and userspace processes.  We'll see.

To be continued...

[Table of Contents](.)
