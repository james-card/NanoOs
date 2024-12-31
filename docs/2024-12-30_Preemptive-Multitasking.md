# 30-Dec-2024 - Preemptive Multitasking

As I've mentioned before, there are two kinds of processes in NanoOs:  Kernel processes and user processes.  As I've also mentioned, NanoOs uses cooperative multitasking.  I never intended to change the multitasking model of the kernel processes.  My thought there was that, being kernel processes, only they can determine when it's safe to yield.  However, my intent was to change the user processes to preemptive multitasking when I could.

I started looking into this right after my last update.  The good news is that the Arduino Nano has a few high-resolution timers capable of doing interrupts.  My plan was to create an interrupt function that would be triggered by a timer and call yield on behalf of the user process currently executing.  The timer would be configured as a single-shot timer and would only be set up for non-user processes.

After doing a little research, I configured a timer and interrupt function to do exactly that.  I wasn't sure how long I should give the timer.  I figured I would start out short and then make it longer if I needed to, so I started with one millisecond.  Yeah, no.  That resulted in the console getting flooded with errors about corrupt Coroutine objects the instant the firmware booted.  So, I bumped it to 5 milliseconds.  I still got warnings at that rate too, so I bumped it up again to 10 milliseconds and then to 20 milliseconds just to be safe.

That got me past the corruption warnings, but that's about it.  The system was unstable.  Sometimes, I could login and sometimes I couldn't.  If I could login, I was usually unable to issue a command afterward.  I was initially pretty baffled by this.

Then, I realized something:  With the recent change to using four (4) process queues, I now had callbacks running from user processes that modified the state of the scheduler.  (Anyone remember how I said I wasn't happy with that model?  This is why.)  If the timer fired during one of those modifications, the state of the scheduler would become corrupted.  I had a sneaking suspicion that was the issue.

So, I modified the callbacks to disable the timer on entry and restore it if it was running.  Poof!!!  I could login and issue commands reliably!  So, my hunch was right.  OK, onto the real test.

The reason I wanted preemptive multitasking for the user processes is because I didn't want them to have to worry about the details of process management.  With only cooperative multitasking, user processes have to be responsible for calling yield directly.  I wanted to avoid this if at all possible.  So, the real test here was my runCounter command that sits in a tight loop incrementing the value of a variable.  I wanted to remove the yield call that happened at in every iteration of the loop.

So, I did just that.  After removing it and reloading it, I logged in and ran the command as a background task.  I got a prompt back!!!  That meant that I had successfully preempted the process with the hardware timer!!!  Then, I tried running a separate command to see the value of the counter.  Well...

This was not so successful.  In the terminal, I could see that it only read the first two letters of the command and then the newline.  I realized that what was happening was that it was reading the first two characters, then switching contexts to the preempted task.  In the 20 milliseconds that elapsed before it was interrupted, the rest of the characters overflowed the serial port buffer until the next time it was read again and the newline was picked up.  Great.

I started shortening the timer duration trying to see if I could find a happy medium where I could both not corrupt the coroutines and still read from the consoles reliably.  I could not.  I started corrupting coroutines before I was ever able to get to a point where I could reliably read from the terminals with the background process running.  Alas, it seems preemptive multitasking is not meant to be for NanoOs.

It's still a bit of a mystery to me as to why and how I was corrupting coroutines in the first place.  My best guess is that the timer was going off during a context switch that had been explicitly started.  I suppose it might be possible to break that if I added yet another callback that could be called to halt the hardware timer, but I don't think that's practical.  That would add time to every single context switch which, in an 8-bit system running at 20 MHz, is a pretty serious performance penalty.

So, cooperative multitasking for user processes is here to stay it seems.  For the most part, this really is not a big deal.  All the IO and memory management calls already take care of yielding when needed.  The only real danger is a process getting into a tight loop and never making a call that yields.  So, HOORAY!  My OS is as robust as Windows 3.x and MacOS 9 in that regard!

Maybe one day I'll come back and take another look at this but, for now, there are other things to move on to.  Specifically, next on my to-do list is enabling the MicroSD card reader I bought and being able to do real file IO.  That will enable even more possibilites, so it's a higher-priority item than continuing to put energy into this.  Let's move on.  To be continued...

[Table of Contents](.)
