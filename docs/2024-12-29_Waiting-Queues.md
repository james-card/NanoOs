# 29-Dec-2024 - Waiting Queues

When planning to evolve beyond the round-robin array of processes, I set my sights on four process queues:  A free queue, a ready queue, a waiting queue, and a timed-waiting queue.  Non-running processes would go on the free queue, running and non-blocked tasks would go on the ready queue, tasks blocked with an infinite timeout would go on the waiting queue, and tasks blocked with a defined timeout would go on the timed-waiting queue.  Only tasks on the ready queue would ever be resumed.

Moving things between the free queue and the ready queue was pretty straightforward.  When the task was initialized, it would move from the free queue to the ready queue.  When it completed, it would move from the ready queue to the free queue.

Moving things from the ready queue to one of the waiting queues required a little work but not much.  I already had a COROUTINE\_BLOCKED value that was yielded when a task yielded from a mutex lock or condition wait function.  I split that into COROUTINE\_WAIT and COROUTINE\_TIMEDWAIT values so that I could differentiate what kind of block was being yielded.  At that point, I had enough information to figure out which of the two queues to move a task onto from the ready queue.

Moving things from one of those queues back to the ready queue, though...  that took some work.  I recognized that what I needed was a callback system.  I also recognized that the callbacks would need to be called on mutex unlock and condition signal/broadcast.  That, in turn, made it clear that there would need to be two callbacks:  One for a mutex and one for a condition.

I had a few problems, though.  Coroutine objects did not have enough information to match them with the mutex or signal they were waiting on and mutexes did not have enough information to identify the queue of coroutines waiting for the lock.  Also, I recognized that the callbacks were going to need access to the queues in the scheduler's state object, so I needed a way to provide a state pointer to the Coroutines library and then pass it back into the callbacks.

I had an additional problem with calls involving timeouts:  Neither the mutex objects nor the condition objects contained any state information about when they were supposed to timeout.  That meant that, once I moved a process onto the timed-wait queue, I had nothing to use to evaluate whether or not one should come off because of a timeout.  In the round-robin scheduler, this didn't matter because the function just kept yielding until the timeout was reached.  But, with the scheduler only loading things from the ready queue, I had to have some way to track and evaluate this information.

This is what resulted in my Coroutine object expanding in size a few updates ago.  Tracking all this additional state information made the object grow quite a bit.  I wasn't willing to compromise on what needed to be tracked, though, so I paid the price with the stack size.

So, with the new state available, I updated the mutex lock and unlock functions and the condition wait, signal, and broadcast functions to update it correclty.  Then, I added function pointer and state pointer parameters to the coroutine configuration function and made the mutex unlock and condtion signal and broadcast functions pull the appropriate callbacks and state and call them if they were defined.

The mutex callback was pretty straightforward.  I simply walked the waiting and timed-waiting queues until I found the process that the mutex had at the head of its lock queue.  I removed it from its waiting queue and pushed it onto the ready queue.

The condition callback was a little more involved.  I needed to move the first N processes in the condition's signal queue from its waiting queue to the ready queue, where N was the number of processes being signalled.  A little more complex, but not a huge issue.

Callbacks implemented, I configured the Coroutines library with them and enabled the queues in the scheduler.  I was shocked that everything worked correctly the first time!  Hooray!!!

There is one thing I don't like about this setup, though.  The callbacks modify the scheduler's state through a state pointer.  I would much prefer them to send a message to the scheduler process and have the scheduler do it itself, but that's simply not possible.  The message system relies on conditions and mutexes to function.  Making the mutex and condition callback functions rely on message passing would result in infinite recursion or deadlock.

So, now, I *FINALLY* have the four-queue system that I set out to build over a week ago.  HUZZAH!!!  Now, let's see if I can make any progress on preemptive multitasking.  To be continued...

[Table of Contents](.)
