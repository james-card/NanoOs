# 26-Dec-2024 - Reclaiming Memory

OK, I lied.  I did not work on adding the other queues to the scheduler like I said I would.  The reduction in dynamic memory was too big of a concern for me.  I used SHA1 as a password hash mechanism.  That one algorithm uses over 400 bytes of dynamic memory (because that was too much to have on a process's 512-byte stack) and is required in order to successfully login as a user.  Another place dynamic memory is used is in argument parsing for a command.  Then, there's just the regular dynamic memory usage that an arbitrary command could do on its own.  With only 650 bytes of dynamic memory available in the system, there was relatively high risk that the system would be unable to allocate enough memory to run the SHA1 algorithm and authenticate a second user in the event that a command in one shell hung or crashed.  That was unacceptable to me.

So, I needed to make more space available to the memory manager.  The memory manager process is the last one started and uses all of the remaining memory that's available at that point.  What's considered "remaining" is anything that has not already been allocated to a process (Coroutine) stack.

I should probably pause and explain how Coroutine objects are actually allocated.  There are two stacks that the library makes use of at all times:  The idle stack and the running stack.  Initially, both stacks are NULL.  When coroutines are configured, a pointer to a statically-allocated Coroutine object has to be passed in as the first one.  That Coroutine becomes the first thing on the running stack, but the idle stack remains empty.  When the first Coroutine is created, two (2) Coroutines are allocated:  One that is returned to the user for use and one that continues to be on the idle stack.  From that point onward, the idle stack is never empty.  When another Coroutine is created, the one on the top of the idle stack is popped and, if that is the last one, a new one is created at that time and put on the idle stack again.

Coroutines are allocated dynamically but without use of dynamic memory.  The full algorithm is extremely complicated but I'll simplify here.  jmp\_bufs are used to do segment the main stack.  When a new Coroutine is created, a longjmp is issued that takes the code to the highest place on the stack so far.  Then, a function is called that has a character array as the local variable.  From there, the main coroutine function is called.  One of the local variables in that function is the Coroutine object that is to be used.  The system then does a longjmp back down to the original place on the stack and passes the address of the local variable down to the caller.

The total distance from one Coroutine on the stack is the size of the Coroutine object plus the number of bytes in the character pointer plus the size of the values pushed onto the stack in order to make the function calls.  At the end of my last round of changes, the Coroutine object was 113 bytes on the Arduino (actual size depends on the compiler).  With a 512-byte character buffer, the total size between Coroutines was 639 bytes, wich was a 34-byte increase relative to before my changes.  Pretty big for a system with only 6 KB of RAM.

It was worse than that, though.  The messages that are exchanged between processes were statically allocated outside of the process stacks.  Those messages had gotten larger with my changes as well, and there was a pool of eight (8) of them.  That further reduced the amount of total memory.  Something had to be done about this.

I needed to decrease the total memory usage.  There were two things I could do to achieve this:  Decrease stack sizes and move large object allocation out of global space and onto process stacks.  These two things, though, were mutually exclusive, at least at first glance.  I had an idea for how to have my cake and eat it too, though.

I knew it was impossible to allocate any more objects on the scheduler process's existing 512-byte stack.  This was a problem because this was precisely where I would need to store the things that were in the global space.  It was therefore also impossible to put anything else on the stack if I decreased the size of all the stacks to try and conserve space.

BUT!  My goal here was to conserve TOTAL space.  I knew that most processes only used a few hundered bytes of stack space.  The scheduler process and the console process (where the console buffers were allocated) were the only exceptions to this.  The idea occurred to me that I could use two conjoined, smaller stacks for each one of these processes and a single smaller stack for all of the other processes.  So, basically, instead of allocating 8 639-byte stacks, I could allocate 10 smaller stacks.  I could then move the remainder of the global objects to the now-larger main stack and hide all the allocations under that.

So, that's what I set about doing yesterday.  There was one problem, though:  I didn't know how small I could make the stacks and still operate the user processes.  This was going to take some tweaking.

Which brings me back to that stack allocation algorithm in the Coroutines library that I mention earlier.  The algorithm was actually a little more sophisticated than that.  What it did was allocate the stack in 512-byte chunks.  If you specified a 1024-byte stack, it simply reduced the needed stack size by 512 bytes and called itself recursively until the needed stack size was 0.  It then called the main coroutine function.

I still wanted to use this approach but, obviously, 512 bytes at a time wasn't going to fly.  So, I was going to have to reduce the number of bytes allocated at a time and then set the total to... something???  Just have to play and see.  Obviously, the first thing I tried was a single, 256-byte stack per process.  Thankfully, I now have my Coroutine corruption system in place.  When trying that, the system immediately flagged coroutines as being corrupted and abandoned them.  OK, so one 256-byte chunk wasn't going to work for a stack size.

The next thing I tried was three 128-byte chunks for a total of 384 bytes.  This worked!  But... The total size of each allocation was 384 stack bytes plus 113 for the Coroutine, plus some number of bytes for the stack pushes for the recursive function calls.  Add all that up and multiply it by 10 processes and I was pretty much back where I started in terms of amount of free RAM in the system.

So, 256 bytes is too small to operate in and 384 bytes is too big to be practical.  The obvious compromise was to split the difference at 320 bytes per stack.  Thankfully, this seemed to work and also got me back over the 1 KB of free RAM threshold.  HOORAY!!!

Well, I may have spoken too soon.  This did seem to work at first because I didn't immediatly get complaints from the scheduler about corrupt Coroutine objects.  But, something weird happened when I logged in to the USB shell:  The Coroutine for the other shell became corrupt.  The processes are allocated back-to-back on the stack, so I knew that the stack was overflowing, but I couldn't figure out why or where.  I worked for a couple of hours tweaking things trying to get things to work right but finally just gave up, reverted all the changes back to 8 512-byte stacks, and went to bed.

Then, something happened that I'm sure is a familiar occurrence to most programmers.  Right before I got into bed, something dawned on me.  I quickly went to my office to confirm my suspicion.  I was right:  I had allocated the character buffers for the username and password in the login function on the stack.  I had allocated 50 bytes for the username and 96 bytes (!) for the password!!!  GAH!!!  What was I thinking?!?!  Well, at least I knew what the issue was.

As you might imagine, I didn't get a lot of sleep last night.  I was dog tired when I went to bed but I woke up at 3:30 AM and couldn't get back to sleep.  Welp!!!  Time to fix this problem, then!

I got to my office and immediately set the stack back to 5 64-byte chunks.  I allocated two process stacks for the main (scheduler) process and two process stacks for the console process.  I then changed the username and password variables to use dynamic memory instead of local variables on the stack.  Booted up, logged in...  and no other console crashed.  HUZZAH!!!

At that point, I felt confident in moving the allocation of the global objects into the scheduler's stack.  This was trivial compared to everything else.  When I got done moving things around, I had a little over 1,200 bytes of dynamic memory available.  Pretty big improvement!

There was one thing that still bugged me, though.  The fact that I was using 64-byte chunks meant that I had to do 5 recursive calls.  Each call level pushed the stack a little higher than it really needed to be.  I wanted to get this down some.

So, I reworked the allocation function.  I made versions with stack sizes in powers of two from 64 bytes up through 1 KB.  I then made each function call the next level function that closest to the number of remaining bytes left without exceeding it.  This allowed me to go from making 5 recursive calls to 2 (256 bytes and 64 bytes).  That, in turn, knocked 10 bytes off the total stack size allocated.  That may not seem like a lot, but 10 bytes times 10 process slots adds up to an additional 100 bytes that I was able to recover because of that.  So, in the end, I wound up with over 1,300 bytes of RAM left!  PARTY!!!

Now, finally, I can rest a little easier (literally).  I don't feel like my algorithms are at risk of failing due to lack of memory at this point and I've reclaimed everything I think I reasonably can.  So, maybe... just maybe... I'll be able to bring the other scheduler queues to life in the near future and get that chore off my to-do list.  We shall see.  To be continued...

[Table of Contents](.)