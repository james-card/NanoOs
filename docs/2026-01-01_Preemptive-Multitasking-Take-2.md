# 1-Jan-2026 - Preemptive Multitasking - Take 2

This is not the post I intended to be writing at this point.  I'd been working on enhancing my overlay system but I kept having this irritating thought that I really needed to get preemptive multitasking working if I was going to have serious user space applications.  The thing about doing the overlays is that it enables arbitrary code execution.  Up to the point that overlays are fully functional, all of the code in the entire OS will be in my control.  Once overlays work, that's no longer necessarily true.  Having working overlays means that it will be possible for other developers to write applications for the OS.  Those applications may not necessarily be well behaved in terms of yielding like they need to.  So, I decided it was time to take a second look at preemptive multitasking.

Before I get into this, I should note that I did this work on December 30th and December 31st.  I had not gone back and looked at my blog before I started doing this and I had no idea that I was doing it one year to the day after my first attempt at preemptive multitasking.  What a coincidence, eh???

So, as I [previously mentioned](2024-12-30_Preemptive-Multitasking.md) at the end of my last attempt at this, I suspected that the issue I was running into was preemptively yielding in the middle of a cooperative yield.  I also mentioned that it might be possible to add a callback to the yield function and that I was resistent to doing that performance reasons.  Well, I decided to give it a shot anyway.

Truthfully, there was another reason that I was reluctant to add the callback:  I was getting increasingly uncomfortable with coroutineConfig.  By that point, it took five (5) parameters, which is pretty ridiculous.  I did not want to add a sixth one.  Last week, however, I did some cleanup to the Coroutines library.  There is really only one (1) required parameter, which is the pointer to the first coroutine to use.  All of the other parameters are optional and have default values if they're not provided.  So, I created a CoroutineConfigOptions struct and reorganized coroutineConfig to take only two (2) parameters: A pointer to the first coroutine and a pointer to the options structure.  This made things much cleaner.

Once I got done doing that reorganization, adding another member element to the options structure was pretty trvial and I didn't have any objection to adding another callback based on aesthetics.  So, I decided to give it a shot and see what happened.  I added in the member variable and added in all the support to have a callback.  Within coroutineYield, I checked to see if a callback was set and, if so, called it right before the context switch.

This allowed me to do what wasn't possible in my last attempt:  Cancel a hardware timer prior to doing a context switch.  This, in turn, would prevent the situation I believed I was running into where the timer fired and invoked a context switch while a context switch was already in progress.

Well, it would once I had that logic in place.  A lot has changed since I first tried this a year ago.  At the time, I was thinking only in terms of the Arduino Nano Every and I had baked calls to its timer functionality directly into my scheduler.  I won't allow that kind of thing in the OS now.  For one thing, my main target platfrom is now the Arduino Nano 33 IoT.  More importantly from an architectural standpoint, though, is that timers are hardware specific and belong in the HAL now.

I set about adding timer support to my HAL abstraction.  My immediate goal was to add full support for hardware timers on the 33 IoT and to just get it to compile on the Every and on the simulator.  So, for the latter two, I just put in stub implementations that returned `-ENOTSUP` for most things to indicate there was no support for the operations and indicated that there were zero timers available.  Indicating the number of timers available became critical later.

Thus began my work on getting hardware timers to preempt a task in progress.  I added six (6) new timer-related functions to the HAL abstraction:
- `int getNumTimers(void)`
- `int setNumTimers(int numTimers)`
- `int initTimer(int timer)`
- `int configTimer(int timer, uint32_t microseconds, void (*callback)(void))`
- `bool isTimerActive(int timer)`
- `int cancelTimer(int timer)`

`getNumTimers` actually turned out to be a bit more tricky than one might think.  On the surface of it, it's up to the programmer (i.e. me) to return whatever they want to support.  The challenge for me was figuring out which timers were even available.  Basically, there are several timers supported by this board, but some of them are reserved for system use (for things like keeping track of time) and some of them are *sometimes* used depending on what functionality is accessed.  Anyway, long story short, I wound up supporting two timers.

Now, some might be wondering why I added `setNumTimers`.  There are a few reasons.  One is that a call to `initTimer` might fail, and so fewer than all possible timers would be availabe on the system.  The software shouldn't expect to need to call anything other than `getNumTimers`, so `setNumTimers` can be used in that case to limit the number of timers returned by `getNumTimers`.  The second reason is that the implementation may want to not support timers at all and thus return 0, or even return a specific error code to indicate something to the rest of the system.  So, `setNumTimers` allows for configuring those scenarios as well.

`cancelTimer` was the magic that I needed for my yield callback.  I added a `coroutineYieldCallback` function to my scheduler and configured the Coroutines library to use it when I initialized it.  That is the only thing that function does.  Right before a context switch, coroutineYield calls the callback (if it's set, which is is in NanoOs), which ensures that the timer can't go off and attempt to do a context switch in the middle of a context switch, which is what was biting me a year ago.

`configTimer` and the mechanics of what happens when a timer is triggered is where I spent the bulk of my development and debug work.  On the surface of it, it's pretty simple:  When the timer goes off, the timer interrupt (which is either `TC3_Handler` or `TC4_Handler` for what I'm doing on the Nano 33 IoT) clears the interrupt and calls `coroutineYield`.  In order to have proper abstraction, the timer needs to call a configured callback instead of directly calling yield itself, so, OK, configTimer takes a `callback` parameter to call when the timer fires.

In practice, it was a lot more difficult.  The thing is that on the Cortex&reg; M0, even though the interrupt can be cleared directly, other interrupts on the system cannot.  The hardware won't re-enable interrupts on the system again until the handler returns.  This had multiple consequences.  For one thing, even though the system would continue to be responsive during a forced yield, killing a process permanently hung the system.  Also, only one process could be preempted.  Attempting to run two background processes also hung the system.

Getting around this was ugly and, unfortunately, involved assembly.  The full sequence worked as follows:
1. Save the return context directly in the timer interrupt handler into a temporary global variable.
2. Modify the return address to point to the real handler we want to invoke.
3. Return from the timer handler.  This re-enables interrupts.
4. In the real handler, save the return context into a stack variable and then do the work necessary to update the timer metadata and call the callback if it's set.
5. Once the callback returns (which will be returning from the yield in this case, so the timer is active again), restore the context and jump back to the original return address that was saved by the timer handler.

I was able to save the context without using assembly, but I used assembly to restore it.  I had been going for an OS that was totally C, i.e. used no direct assembly, but that goal fianlly broke here.  Strictly speaking, it **MIGHT** be possible to avoid assembly by directly modifying the members of a `jmp_buf`, however this would be compiler specific.  Having compiler-specific C code is worse than having machine-specific assembly, so I opted for the assembly.  On the plus side, the assembly is isolated to the HAL code, which is the right place for it.

In the process of all this, I learned that I had a design problem with my Coroutines library and I am now **SO GLAD** that I haven't submitted it to the C standard committee for consideration yet.  I based the library on the C11 thread primitives.  One of the things I struggled with was what to do for an equivalent to thread-specific storage that didn't rely on dynamic memory.  My solution at the time was to allow the coroutines to have IDs set.  My thought was that the ID could be used as an index into an array of storage elements so that the coroutine could look up its own data.

I realized that that was a silly way to do things.  What I should have done was have a private context pointer in the coroutine.  That would allow direct lookup of whatever information was relevant and specific to the coroutine.  It's much more straightforward to do:

```C
getRunningCoroutineContext()->someMember = someValue;
```

than it is to do:

```C
someArray[getRunningCoroutineId()] = someValue;
```

What really brought this need to my attention, however, was my implementation of the mutex unlock and condition signal callbacks.  Those callbacks only have knowledge of the Coroutines library primitives.  What I really need for them, however, is knowledge of the processes that the coroutines are a part of.  So, what I was doing was something like:

```C
ProcessDescriptor *processDescriptor = &processArray[coroutineId(comutex->head)];
```

It would be much more clear to just write:

```C
ProcessDescriptor *processDescriptor = (ProcessDescriptor*) coroutineContext(comutex->head);
```

So, I refactored the Coroutines library to this effect.  That was actually considerabley less work than I was expecting.  What this ultimately enabled was far-simpler implementations of the existing callbacks.  I added a ProcessQueue pointer member variable to the ProcessDescriptor structure to keep track of which queue a process was assigned to and set the context of all the coroutines to be their parent process descriptors.  Since I was now directly interacting with the `ProcessDescriptor`, I was able to directly remove it from the queue it was on and put it on the ready queue.  That cut out quite a bit of code and a rather lengthy search for the Coroutine in all of the process queues.

One last bit of cleanup I did this round was to my Processes library.  I realized that a lot of my functions and macros directly took a `ProcessHandle` (which is really just a pointer to a `Coroutine`) when they really should be operating on the higher-level `ProcessDescriptor` structure.  That allowed me to make a lot of the calling code much simpler as well.

Once I **FINALLY** finished all the cleanup and got everything put back together, I had a system that allowed me to preempt user processes.  Before I resume a process, I now check to see if the process is a user process and, if so, I set a timer for two (2) milliseconds.  Irrespective of whether it voluntarily calls coroutineYield itself or if the timer goes off and forces a yield, the callback is called before a context switch happens and that clears the timer.  No further action is requried on the part of the scheduler.

Back to `getNumTimers`, what I did was configure the timer when there's at least one to use and skip it entirely when there isn't one.  This allows the system to remain fully multitasking irrespective of whether or not there are hardware timers available.  The only question is whether a process can be preempted.  If there are no hardware timers, the system is fully cooperative multitasking.  If a hardware timer is available, user processes will be forced to yield if their timer expires.  If they voluntarily yield the processor before the timer is up then they just behave as regular cooperative tasks.  As mentioned when I tried this last year, kernel processes remain fully cooperative at all times on the grounds that only they know when it's safe to yield.

So, hooray!!!  I now have a preemptive multitasking system that has the ability to be every bit as stable as Windows 95!  Granted, that's not a terrific bar for stability, but it's a heck of a lot better than Windows 3.x, which is where I was before this work.  I'll take it and run with it!!

I may do a little bit more cleanup before resuming my work on overlays.  It's occurred to me now that, at this point, what I really have is a preempted thread and not necessarily a full process.  I may do some renaming of "process" items to be "task" items such that they can really be any kind of entity whether cooperative or preemptive, thread or process.  That's work for another day, though.  For now, I'm pleased that I've gotten this far.  I feel much better going into overlay work that enables arbitrary user program execution knowing that user programs run through that mechanism cannot bring the entire system to its knees just by running in a tight loop.  Another win for NanoOs.

To be continued...

[Table of Contents](.)
