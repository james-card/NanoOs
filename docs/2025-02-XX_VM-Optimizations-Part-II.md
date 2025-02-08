# XX-Feb-2025 - VM Optimizations Part II

To recap, there were four things I was thinking about to improve the performance of the VM in the general case:

1. File reads must be kept to an absolute minimum.
2. I need to see if I can increase the speed of SD card reads in general.
3. Data and code need to be in independently-addressable virtual memory segments.
4. Anything I can do to further optimize fseek is desirable.

I started with the last one since I already knew one thing to do there.  I put the optimization about not seeking backward when the target cluster and current cluster were the same back into the codebase.  Obviously, this has no effect with the current prograsm, but no reason not to have it in place now.  It also consumed virtually zero program space, so just go ahead and have it in there.

I took a look at the second item next.  I recalled that the driver I had put together with [Claude](https://claude.ai) had initialized the SPI at slow speed during card initialization, but I didn't recall that it had ever increased the speed before leaving the initialization function.  I took a look at the code and confirmed this was the case.  The base speed it ran at was 400 kHz.  So, I changed the code to reconfigure the SPI interface to 8 MHz before completing the SD card init function.  I then reconfigured the VM to use the screwy 144-byte cache buffer again to see what the performance difference would be.  Result:  About a 12.5% performance improvement.  Not bad.  I also checked what the performance difference was with the 256-byte cache.  Result:  About a 7.5% performance improvement.  The best case performance benchmark was now 2.267 kHz.

[Table of Contents](.)
