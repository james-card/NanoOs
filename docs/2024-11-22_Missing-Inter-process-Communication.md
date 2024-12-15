# 22-Nov-2024 - Missing Interprocess Communication

Working on this OS for the Arduino Nano made me realize something:  There's no mechanism for inter-thread communication defined in the C specification's threading model.  I based my coroutines library on C's thread model and, building the OS this week, it very quickly became apparent that this was a gap.  Inter-process communication is one of the basic things that operating systems are supposed to provide.  It seems like a pretty tremendous oversight to me that *THE* systems language doesn't natively provide a mechanism for doing it between threads.

So, a few things fell out of this.  First, I had to invent something for my coroutines.  That was a priority and a must.  The good news is that I succeeded and that it's working very well.  I was able to implement a version of printf that utilizes inter-process communication between the process calling printf and the console process that manages input and output.  So, success!

Second, though, I'm going to have to come up with an equivalent for threads and I'm going to have to propose it to the C standards committee.  This really has to be fixed.  Over the years, I've put together [an enormous set of code](https://github.com/james-card/cnext) that provides in C what other high-level languages provide natively.  Most of that stuff I'd consider optional.  This, however, isn't optional.  C needs to support inter-thread communication natively.  I really can't believe it doesn't already.

Thankfully, I have a contact who is on the C standard committee.  I've talked to them before about incorporating some of my ideas into the standard and got put on the back-burner, so to speak.  This is different.  This needs attention.  If C is going to support threads, it needs to support this.

So, I'm probably going to be putting in some time over the weekend to figure out how to do this.  The good news is that I've worked out most of the kinks with my coroutines message passing model, so I have a starting point.  The bad news is that the implementation won't map 1:1 because of some of the primitives that are used with threads vs. coroutines.  For example, in my coroutines library, a coroutine is an object with state and message passing can reference the object.  That won't work with threads because there's no such thing as a thread object that's visible to the programmer.  (There is a thrd\_t type, but that's really just an integer that's used as a key.  It doesn't hold any state information.)

More to do, as always.  I guess the advantage of me putting something together and proposing it is that, since it's never been done before, the area is wide open to any way I want to do it.  I already have some ideas for how I can manage this with the primitives that are provided by C's thread model.  It's a bit of a pain in the rear because the implementation will have to use compiler intrinsics, which I try to avoid.  However, since this is in the [threads library](https://github.com/james-card/cthreads), I already have environment-specific implementations and I can get away with compiler intrinsics from within this context.

To be continued...

[Table of Contents](.)
