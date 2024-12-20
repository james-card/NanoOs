# 2024-12-19 - Multiple Concurrent Users

Immediately after my last post, I started working on enhancing my coroutines infrastructure to be able to work with a more-advanced scheduler than a simple round-robin.  I'll talk more about that in a future post.  My work came to a screeching halt when I got a package from Amazon:  A breadboard and GPIO wire kit.

I've been doing my software development work on a Raspberry Pi.  My goal has always been to get the Pi to talk to the Nano over their GPIO UARTs at the same time as through the USB connection.  That would allow for true concurrent user access.  However, I didn't have the infrastructure to be able to do that until I got the shell working as I described in my last post.

The first step to getting communication working over the GPIO UARTs was enabling a separte virtual port in the console process.  I knew that the Arduino had built-in support for communicating over that port, but I didn't know what it looked like.  To my surprise, it used the same kind of communication object that the USB serial port uses.  (It's called "Serial1" instead of "Serial" in the code.)  The interface was identical.  That made things incredibly simple.  The console port code uses function pointers for the read and print functionality, so I simply refactored the existing function to allow for the appropriate object to be passed in as a parameter and then wrote four wrapper functions:  One pair for the USB serial port and one pair for the GPIO serial port.

Once I got the ports setup in the console, I had to put in another dedicated user process for a second shell that would be connected to the second serial port.  At that point, I could clearly see two shells running in my process list and, after a little debugging, I was able to login as a different user on each port!  HOORAY!!!  True multi-user support!!!  Woo hoo!!!

After quickly verifying that one user could not kill the other's running processes, I decided to test it out at doing what it was actually intended for:  Recovering a port in a bad state from the other one.  One of the changes I made recently was to add Bash-like command handling such that commands only run in the background if explicitly started with an ampersand ('&') at the end of the command line.  That allowed me to start a command that ran in an infinite loop in the foreground and never released the console.  So, on my new GPIO serial port, I logged in and ran such a command.  Then, from the original serial port, I logged in, found the process and killed it.  The command prompt came back instantly!  When I saw that, I just laughed with joy.  I couldn't ask for better!

Then, I decided to take it a step further and kill the running shell itself.  I figured that would probably result in bad behavior and I was right.  It resulted in the console the shell was serving completely losing the ability to accept input.  The shell is associated with the port at boot time so that if a port loses the process that owned it, it can reset back to the appropriate shell process.  Killing the shell process itself resulted in some circular logic that hung things up.

After cleaning that up and successfully verifying that killing a shell resulted in going back to a login state, I decided to try something a bit more advanced.  I again ran my command that hung the shell but, this time instead of killing the process that was in an infinite loop, I killed the shell it started from.  That didn't work so well because it had no effect at all on the foreground process, which still had a lock on the console.  When the shell restared, it couldn't take over ownership of the port becasue it was still in use.  So, then I killed the foreground process, logged back in as root, and then killed the shell again.  I got a login prompt immediately!  Hooray!!!

So, I can now officially say that I have true multi-user support in my little OS!  Not bad for a system with an 8-bit, 20 MHz processor and 6 KB of RAM!!!

I'll now go back and start working on the scheduler enhancements I was in the middle of before getting the breadboard.  To be continued...

[Table of Contents](.)
