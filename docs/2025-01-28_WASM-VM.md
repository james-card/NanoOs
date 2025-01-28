# XX-Xxx-2025 - WASM VM

Let's first talk about the goal and why I have it in the first place.  The Arduino Nano Every - as well as other models of Arduino and other systems - is a Harvard architecture.  That means that it's impossible to load executable code into RAM and run it.  Translation:  There is absolutely no way for me to load code compiled for the Nano Every's processor (an ATMEGA4809) and run it natively.  So, if I'm going to run arbitrary commands that are read from the filesystem, my only option is to have something that can translate something in the command file to something that executes.  So, we're talking about a VM here.

One option would be to make a VM for the ATMEGA and compile binaries to that.  But, why?  There's nothing inherently great about that for what I'm doing.  These SD cards are enormous.  Once we're talking about accessing data on the card, the binaries can be basically any size and we don't care.  That's why getting to the point of having a usable filesystem was so important in the first place.

My idea was to make a VM for Web Assembly (WASM).  That has the potential to be a universal assembly language.  If I make a VM for that, anything that is compiled to WASM becomes something that my OS can run.  So, that's what I set out to do.

First thing's first:  Delete all the existing command infrastructure.  There were two reasons for this.  First:  I needed the space.  By the time I got done with the SD card and filesystem infrastructure, I only had a little less than 800 bytes of program flash left.  Deleting the existing infrastructure gave me about 8.5 KB to work with.  Second:  It will no longer be relevant.  All of the processes will have to run through the VM and the existing commands just flat out won't even work anymore.  So, get rid of it.

To be continued...

[Table of Contents](.)

