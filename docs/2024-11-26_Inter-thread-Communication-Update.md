# 26-Nov-2024 - Inter-thread Communication Update

Let's first go back in time a few weeks.  (There's a reason for this.  Bear with me.)  At the time, I hadn't yet purchased my Arduino, so I spent some time doing performance testing of [my product](https://skymond.io) on Windows.  It was a disaster.  The Windows version of my product ran at half the speed of the Linux version using the same database and hardware configuration.  There may be some performance differences between Linux and Windows (I haven't looked for the latest benchmarks), but there's no way the difference could be *THAT* severe.  This had to be a problem with my software.

Apart from the OS itself, there was one critical difference between the implementations:  thread-specific storage.  I was determined to use as much standard C as possible, so I only make use of the standard C mechanisms in my application-level code.  Unfortunately, the Windows concept of thread-specific storage doesn't align to the C model.  (Specifically, there's no way to specify a destructor to be called when a thread exits.)  So, I had to write [my own implementation](https://github.com/james-card/cthreads) of the standard calls to avoid resorting to OS-specific calls at the application layer.

Thread-specific storage is a two-level data structure lookup and there's no way around that.  The first level uses the storage ID as a key and the second level uses the thread ID as a key.  This lookup needs to be as blazingly fast as possible and, well, my implementation clearly did not fit that description.

I had used a mutex-guarded red-black tree of red-black trees for my Windows implementation.  This was a huge problem because the mutex locked the entire structure any time data was accessed in it.  So, basically, the entire application became single-threaded for any thread-specific storage calls.  My logging library makes extensive use of thread-specific storage and I log the entrance and exit of every function in every application-layer library, so... well... you get the idea.  Very, very slow.

OK, I had to fix this.  The fastest implementation would be an array of arrays but this seemed impractical to me.  Having an array for the storage ID keys is simple enough if you use 16-bit IDs, but I couldn't see any way to have a mutex-free second-level of arrays.  It seemed to me that you would constantly need to be resizing the arrays and there's no safe way to do that without using a mutex.  Maybe there's some clever way to do it that I couldn't think of but I gave up on the idea.

So, I needed a fast data structure.  None of my existing data structures fit the mold because they all rely on mutexes.  I needed one that was lock free, or close to it, where the data elements were as isolated as possible.

The solution I came up with was a radix tree.  The time complexity of a radix tree is determined by the the length of the key.  Since all the keys are the same length, this essentially made all the lookups O(1).  (Well, O(4) for a 32-bit key, which thread IDs are under Windows, but who's counting?).  I did have to make use of compiler intrinsics for compare-exchange and exchange functions, which I don't like, but there was just now way around that.

So, in the end, I came up with an array of radix trees for my Windows thread-specific storage.  This brought the performance to within 25% of Linux, which was good enough for me.  (I'll spare you the details on all my headaches with the exchange functions swapping out values that shouldn't have been swapped and how long all that took me to sort out.)

Now, fast-forward to me wanting to do inter-thread communication.  There's absolutely no provisions for this in C at all.  I realized quickly, though, that I was looking at a similar lookup as what I had done for the Windows thread-specific storage.  At a data structure level, the difference is that it's a tree of message queues instead of an array of storage trees.

It's critical that all the threads have visibility of the tree.  Unlike before, these queues are NOT private to each thread.  Yes, there is one per thread, but the idea here is that each thread has access to the queue of every other thread.  The sending thread will be pushing messages onto the queue of the destination thread.

Now, back to the little OS I'm writing for my Arduino.  As I mentioned before, there was no provision for inter-coroutine communication in my library because there had been no provision for C's threads.  So, my Arduino OS became my proving ground for the concepts I was working on.  My goal in the end was to have two implementations for the same concepts:  One for threads and one for coroutines.

So, I sat down and did a draft for what I thought I needed for my coroutines to be able to talk to each other and implemented that.  I got things working, but I made some mistakes.  Critically, I put some functionality into the kernel that really should have been part of the coroutines library.  So, when I did the draft of the counterpart for threads, I made sure to put in the functionality that I thought really should have been included in the coroutines library.  When I got the interface defined, I did a first-pass implementation using the coroutines implementation as a reference.  I grabbed the implementation logic out of the kernel for the things that I thought should be moved to the library.

When I got done with that, I went back and revised the coroutines library, putting things in place that I missed the first time and improving the algorithms based on what I had learned doing the threads implementation.  I tested the result in my OS along the way to make sure that what I was doing was sane and continued to work.  As I figured out more things that needed to be in the library level, I went back and updated the threads implementation again.

This process of going back and forth and carrying the learnings from one library into the other repeated for several iterations.  I've basically been doing it for the past week.  Having the OS for the Arduino Nano has really been a great thing because I've learned about some bad implications of some of my ideas before they made it into the threads version or before they got stuck in use in the coroutines version.

So, lather, rinse, repeat until I got something that I was decently satisfied with today.  After beating my head on my desk a few rounds with this with the coroutines implementation, I have decent confidence that the threads version will work.  I pulled the radix tree implementation out of my Windows C threads library into its own library so that I could use it for this task too.  That implementation is now being beaten to death just by using it on Windows, so I have relatively high confidence in that too, now.

So, after all that, I'm finally at a point where I feel like I can start playing with the threads version of this implementation.  Mind you, I'm not going to immediately because I want to document my OS before I go any further onto something else.  Everything in both the threads library and the coroutines library are fully documented with Doxygen and inline comments, but I haven't been as diligent with the OS code as I've gone through this.  So, that needs cleanup.

So, lots of progress, yet miles to go before I sleep.  To be continued...

[Table of Contents](.)
