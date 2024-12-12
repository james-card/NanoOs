# 18-Nov-2024 - Starting Out

I've been toying with the idea of trying to see if I could write something similar to one of the early versions of UNIX for an Arduino for a while.  Early versions of UNIX ran on a PDP-11 with 16 KB of RAM and a 1 MHz processor.  Arduinos are the closest modern devices to that kind of environment, which is why I was thinking of using one as the platform.

Early versions of UNIX did not do preemptive multitasking.  In fact, the earliest version didn't do multitasking at all.  When multitasking was introduced, cooperative multitasking was used.  So, that's what I need to use too, at least initially.

Since this is an Arduino, the implementation will be limited to what the Arduino environment supports.  Specifically, this means that I'm limited to the C and C++ functionality provided by their libraries, plus anything I write myself.

Today, I started this effort.  I used my [coroutines library](https://github.com/james-card/coroutines) for the cooperative multitasking support.  This works by subdividing the main stack into smaller stacks.  The board I'm using is an Arduino Nano Every, which only has 6 KB of RAM.  So, the individual coroutine stacks have to be tiny.  After a bit of tinkering with the stack sizes, I was able to get a console process and three user processes running concurrently!!!  Granted, the processes themselves don't actually do much, but it's a great proof of concept to start with!

To be continued...
