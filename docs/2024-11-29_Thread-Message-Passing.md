# 2024-11-29 - Thread Message Passing

Two days ago, I spent the fist half of the day documenting my OS code as I had committed to in my previous post.  I spent the second half working on testing out the threads version of message passing.

I decided that the fastest way to get thorough testing on the threads version would be to convert my cooperative-multitasking OS into a multi-threaded application.  Since there's a 1:1 mapping between my [coroutines API](https://github.com/james-card/coroutines) and C's threads API (now that I've added message passing to it), this was mostly a process of running sed commands on a copy of the codebase.  I also had to change the IO calls since the application works with stdio calls instead of the serial port, but all of the other logic remained the same.  Critically, all the message passing logic was identical.

So, I spent a few hours Wednesday afternoon doing the conversion.  The first time I ran the converted application, I hit a bug within three commands.  This, however, was a bug in the OS logic, not the message passing logic.  I found it and fixed it and then everything worked just fine.  Then, I ran it through valgrind.  I fully expected that I would find a memory leak in my radix tree implementation.  However, when running it, the only error I found was a variable I had forgotten to initialize in the OS, not in the messaging or radix tree.  Once I fixed that, there was nothing.  I was extremely surprised by that, but very pleasantly so.

Late in the afternoon yesterday, I had a non-trivial realization:  My design for the message data structures was too specific to my application.  I had basically made OS/Application-specific information elements members of the message data structure.  Groan...  I couldn't bear to redo the work yesterday after having gone to all that trouble, so I let it lie.

This morning, I picked it up again.  I spent the morning making the messages generic and pulling the OS-specific elements out into their own data structure and passing pointers to those structures via the messages.  It was a pain in the rear, but it worked flawlessly when I got done with it.  I again ran everything through valgrind and still no issues, so I think I'm good at this point.

One realization I had in the course of doing this is that there really needs to be a mechanism to pass messages between any two entities.  Coroutines have their own messages and threads have their own messages.  That's necessary because the signalling mechanisms are different between the two systems.  But, there's nothing between processes.  The signalling mechanisms between coroutines and threads would be best represented as a socket at the process level... but the C standard has no support for sockets.  Networking is an OS-specific extension.  This has never made any sense to me.  Networking has been around for almost as long as C has, so why the language never defined a mechanism for it is beyond me.  In my opinion, that's something else that needs to be addressed by the standard committee, but I suspect that will be even more difficult for them to address than inter-thread communication.  I digress.

At any rate, I think I have something now that I can use as a base proposal to my contact on the committee.  I won't do it right away.  I need to sit with this for a bit in case I have any dope-slap realizations about my design.  I do want to get to it relatively soon, though.

That's it for now.  I think I'll write a dynamic memory manager for the OS next.  :-)  To be continued...

[Table of Contents](.)
