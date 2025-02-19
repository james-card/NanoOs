# XX-Xxx-2025 - User Shell

Time to make a shell for user space!  Well, actually, I needed to make a little more than that.  I also needed an init process to manage user logins.  Regardless, though, I needed a lot of infrastructure and I needed it both in kernel space and in user space.  Fortunately, I didn't need very much in kernel space because flash was very tight.

The first thing I started with was making a proper `strlen` function for my "Hello, world!" program rather calculating the length inline in the `_start` function.  This turned out to be a bigger problem than I thought it would be for a variety of reasons.  The first reason was that the linker put the `strlen` function at the beginning of the binary instead of the `_start` function, so my VM was crashing.  After I got that resolved, the program did run correctly.  It was significantly slower than before due to the extra function call and the different memory alignment, but it was still over 1 kHz, so I'm not going to complain.

Then, I decided to get crafty.  Since all the registers in the VM are 32-bit values, I thought it would probably take fewer instructions to do a version of `strlen` that used the `0x01010101` and `0x80808080` bit manipulation logic to evaluate 32-bits at a time instead of just a character at a time.  And I was right!  It took about a quarter of the total instructions (as one would expect).  However, this version of strlen came with two significant downsides:  (1) It took almost twice as long to run as the single-character version and (2) it didn't result in "Hello, world!" actually being printed to the console.

The reason for the performance penalty was obvious when I thought about it.  The Nano's processor is an 8-bit chip with an 8-bit bus and 8-bit registers.  On top of that, the algorithm was no longer doing a simple comparison against a value of 0, it was now doing bit manipulation logic.  There is simply no way for an 8-bit processor to do all that work as efficiently as it would compare a value to 0.

Because the performance penalty was so high, the algorithm was flat out unacceptable to use in my LIBC implementation.  Consequently, there was no reason for me to debug why the string wasn't being printed.  So, I abandoned the logic and went back to the simple algorithm.

To be continued...

[Table of Contents](.)
