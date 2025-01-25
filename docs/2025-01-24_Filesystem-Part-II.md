# 24-Jan-2025 - Filesystem Part II

OK, SdFat didn't get me very far.  The more functionality from the library I used, the more flash space the library consumed.  I was able to create fopen and fclose functions in my filesystem library that worked with proper message passing, but that's as much as I could do.  In order to things like fgets, I had to embed the SdFat calls directly into the fgets source code, which broke my process isolation model.  And, things like fseek and remove were just completely out.  Zero space available for things like that.

So, I had to come up with my own way to talk to a filesystem.  Well, first thing's first, let's start with the requirements.

1. It has to be simple.  I don't need a robust filesystem, I just need to be able to put data on a card from a PC, read it from the Nano, and maybe do some file updates on the Nano.
2. It has to be well-supported.  I want to be able to read and write it from basically anything.  This means I can't make up my own format.
3. It has to compile to the smallest possible binary.
4. It has to work with an SD card.

The fourth requirement actually throws a kink into things a bit.  Really, a filesystem should have nothing to do with the underlying storage medium.  With the SdFat library, as the name implies, both components were covered by the same library.  And, in order to accommodate the state needed for that, I had to join two processes together.  What I really wanted was one process to manage the device (the SD card) and a separate process to manage the logical filesystem level.

So, that's the direction I headed.  To get there, I had to start with the SD process.  After all, I can't manage reads and writes to files if I can't manage reads and writes of blocks to a device.

I broke the existing double-sized SdFat process down into its two processes and took one for the SD card.  Communicating with the card happens over SPI and, thankfully, I was able to use the Arduino's built-in library for SPI connectivity.  Now, I don't know the first thing about SD protocol and I really have no desire to.  It's not part of what I'm trying to learn or practice here.  So, back to [Claude](claude.ai) for help on this one.  I decided that Claude was actually valuable enough to start paying for, so I was able to get it to produce usable code without running out of messages this time.

Now, as I've mentioned before, I have not been super impressed with the output of these AI chatbots.  Claude has proven to be the best by a long way for what I do, but it still makes pretty dumb mistakes.  It's kind of like working with a junior engineer who knows a whole lot of facts but not necessarily the best way to put things together.  You have to test and code review absolutely everything it produces because you're pretty much guaranteed that it will not get things right on the first try.

The work I did with it on my new SD card process was no exception.  It made some naive - and incorrect - assumptions on its first implementations of the read and write functions.  Specifically, it assumed that the card would be ready for the next command immediately after one was sent.  This was, apparently, a violation of the protocol which said that the host was supposed to wait for good status to be returned before proceeding.  So, I had to work with it quite a bit to debug its routines to the point of being able to read and write reliably.  But, I got there quite a bit faster than I would have if I had had to research and learn all that in detail myself, so I really can't complain.

Now, at this point, I did not have anything filesystem-related to use for reading and writing.  I focused instead on getting the logic around the master boot record right.  I used the card reader in my laptop to put one 32 MB FAT12 partition on the card and used what I knew should be returned from that to verify the read and write functions at the SD card level.  This became the starting point for my filesystem library and process.

But, what filesystem should I actually implement?  I made the first partition FAT12 thinking that would probably be the simplest format to use.  After some additional research with Claude, though, I decided this was probably not the best idea.  I decided that some version of FAT would be necessary to meet my second requirement, and FAT12 would, indeed, produce the smallest metadata.  However, the bitwise logic required to operate on that metadata is actually more complicated than just using straight FAT16 metadata.  That means that FAT12 would violate the third requirement relative to FAT16.  Anything above FAT16 would violate both the first and third requirements.

OK, so FAT16 it is.  Again, I knew very little about the format and don't really care.  It would actually be more relevant for my purposes to invent my own filesystem than it would be to learn the details of FAT16, apart from just getting exposure to one way of implementing a filesystem.

So, back to Claude for help with the implementation.  Unlike the SD card implementation, this took a lot of time.  The problem was that I was trying to satisfy the third requirement while also implementing enough to give me the standard file I/O functions defined by C.  As I mentioned, just opening and closing files using the SdFat library pretty much exhaused my storage space.  Since this was my own process implementation this round, I needed open, close, read, write, seek, and remove at a minimum.  Everything else could be built on top of those or could even be simple macros.

At a minimum, all that had to fit into the storage space I had left on my Nano.  What I really, Really, **REALLY** wanted was a filesystem library that was around 1 KB.  So, I worked to optimize absolutely everything I could to reduce the final compiled size.  One of the things I did was to remove support for subdirectories.  I worked to make the functions as small as possible and simplify the math as much as possible.  Unfortunately, even with all those simplifications, I couldn't get anywhere close to my 1 KB goal.  The final version with all of the functionality is around 4.5 KB, but it does at least all fit in the program flash, which is considerably better than the SD or SdFat libraries.

As I was working on extending stdio functions to work with either the console or with files, I realized that the way I was using buffers for the console was sub-optimal.  I had joined two process slots for the console and allocated four I/O buffers within its state.  I realized, however, that usually only two were used (one per console).  The only time that more than that was used was when piping commands together.  So, I changed the console process to only allocate two static buffers and to allocate anything else that was needed from dynamic memory.  That freed up about 450 bytes of RAM.

Which was good because I needed a 512 byte buffer for the reads and writes of the filesystem.  Now, I could have allocated that buffer in the SD card process.  However, putting it there would mean that it would always have to be allocated because the SD card process would never know when a read or write would be necessary.  (Either that or it would have to allocate and release the buffer for every single operation, which would be enormously slow.)  I figured that the more-efficient route would be to allocate the buffer in the filesystem process and pass it into the SD card process.  That way, it would only need to be allocated when one or more files was opened and could be deallocated otherwise.

So, that's exactly what I did.  And, after all that, I have two process libraries that fit into the available flash and do all the file operations I need.  HUZZAH!!!  I can now open files, create them, write to them, append to them, read from them, and delete them!

That's good enough for now.  I may come back to this later and see if I can improve on the space used by the filesystem.  That will likely require coming up with a custom, extremely simple filesystem since it's pretty clear that I can't make things any smaller on FAT.  That will break my second requirement, but I could write a utility that could access it from a PC.  That's a problem for another day, though.

Next on the agenda is seeing if I can make a VM to run a binary that's loaded onto the SD card.  We shall see!  To be continued...

[Table of Contents](.)

