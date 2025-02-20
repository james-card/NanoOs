# XX-Xxx-2025 - User Shell

Time to make a shell for user space!  Well, actually, I needed to make a little more than that.  I also needed an init process to manage user logins.  Regardless, though, I needed a lot of infrastructure and I needed it both in kernel space and in user space.  Fortunately, I didn't need very much in kernel space because flash was very tight.

The first thing I started with was making a proper `strlen` function for my "Hello, world!" program rather calculating the length inline in the `_start` function.  This turned out to be a bigger problem than I thought it would be for a variety of reasons.  The first reason was that the linker put the `strlen` function at the beginning of the binary instead of the `_start` function, so my VM was crashing.  After I got that resolved, the program did run correctly.  It was significantly slower than before due to the extra function call and the different memory alignment, but it was still over 1 kHz, so I'm not going to complain.

Then, I decided to get crafty.  Since all the registers in the VM are 32-bit values, I thought it would probably take fewer instructions to do a version of `strlen` that used the `0x01010101` and `0x80808080` bit manipulation logic to evaluate 32-bits at a time instead of just a character at a time.  And I was right!  It took about a quarter of the total instructions (as one would expect).  However, this version of strlen came with two significant downsides:  (1) It took almost twice as long to run as the single-character version and (2) it didn't result in "Hello, world!" actually being printed to the console.

The reason for the performance penalty was obvious when I thought about it.  The Nano's processor is an 8-bit chip with an 8-bit bus and 8-bit registers.  On top of that, the algorithm was no longer doing a simple comparison against a value of 0, it was now doing bit manipulation logic.  There is simply no way for an 8-bit processor to do all that work as efficiently as it would compare a value to 0.  Because the performance penalty was so high, the algorithm was flat out unacceptable to use in my libc implementation.  Consequently, there was no reason for me to debug why the string wasn't being printed.  So, I abandoned the logic and went back to the simple algorithm.

The next step was to **STOP!**  I needed to create a proper build environment before I went any further with this.  For one thing, I was getting increasingly nervous about the need to keep all the commands straight for building a program.  For another, I was about to enter the realm of building libraries and I definitely did not want to start that work without having an automated way to build.  So, I put makefiles in place for the work I'd done so far and some pre-enablements for the work that was coming.

Makefiles in place, it was time to start putting in more pieces of a libc implementation.  I pulled the print logic out into an `fputs` function and pulled the exit logic out into an `exit` function.  Compiled everything and gave it a shot.  It was a little slower because of the increased number of instructions, but it ran just fine.  So far so good.

Then, I thought I would go one step furhter and pull the write logic out into a formal `fwrite` function and make `fputs` call `fwrite`.  For this, I decided to go all the way and actually make a `for` loop that would properly print everything.  In order to do that, I had to multiply the `size` and `nmemb` parameters to the function to get a total length and then break the total lengths into chunks that could be managed by the OS.  This turned out to be a problem.  Skipping over the debugging details, the issue turned out to be that multiplication and division are not considered to be part of the base RV32I instruction set.  Those instructions are considered to be the RV32M extension.

One option would be to implement the logic in the libc implementation in terms of addition and subtraction.  This was just not acceptable to me, however.  The performance penalty for that would make any application unusable.  I can live without floating point operation support, but multiplication and division instructions are a must.

So, back to Claude to see what it would take to implement support for those operations.  As I figured, it was more code than I wanted, but not a horrible amount.  I added the suggested code to the OS and it did fit in the available storage space but it used almost all of it.  I'm going to have to figure out how to get more space soon.

Multiplication and division in place, I gave compilation another shot.  It worked.  Ran the program and that worked too.  So far so good.  However, at this point, the "Hello, world!" program was taking about 275 milliseconds to run with all the function call overhead.  I wanted to see if I could bring the total execution time down.

My idea for how to do that was to make the standard C calls static inline functions.  That would (a) avoid a lot of stack I/O and (b) keep the code logic closer together.  Size of program binaries is basically irrelevant with this system.  If all the logic is inlined, that's totally fine.

I restructured my program so that all the standard C calls were static inline functions, rebuilt and re-ran.  And guess what... it crashed.  After a little debugging, I discovered that making the library calls inline had caused the linker to put the `_start` function somewhere in the middle of the binary again.  REALLY?!  OK, back to Claude to see how I can get gcc to stop putting the start symbol in the wrong place.  It recommended adding a few attributes to the `_start` function.  That got it to put the code in the right place (FINALLY!) but the program was still crashing.

To be continued...

[Table of Contents](.)
