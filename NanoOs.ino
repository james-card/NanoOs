////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2024 James Card                     //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included    //
// in all copies or substantial portions of the Software.                     //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//                                 James Card                                 //
//                          http://www.jamescard.org                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// Custom includes
#include "NanoOs.h"

// The setup function runs once when you press reset or power the board.  This
// is to be used for Arduino-specific setup.  *ANYTHING* that requires use of
// coroutines needs to be done in the loop function.
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  // Start the USB serial port at 1000000 bps:
  Serial.begin(1000000);
  // wait for serial port to connect. Needed for native USB port only.
  while (!Serial);

  // Start the secondary serial port at 1000000 bps:
  Serial1.begin(1000000);

  digitalWrite(LED_BUILTIN, HIGH);

  // Initialize stdin, stdout, and stderr to something we can at least tell the
  // difference between.
  stdin  = (FILE*) ((intptr_t) 0x0);
  stdout = (FILE*) ((intptr_t) 0x1);
  stderr = (FILE*) ((intptr_t) 0x2);
}

// In a normal Arduino sketch, the loop function runs over and over again
// forever.  For NanoOs, it will be called once and never exit.  This is because
// we want to do everything related to coroutines in this stack.  The reason
// we want to do this is because we want to hide as much of the coroutine
// metadata storage as we can within the stack of the main loop.  Every stack,
// including the stack for the main loop, is NANO_OS_STACK_SIZE bytes in size.
// If we declare no variables in this stack, we're just wasting space.  Also,
// making the storage for the metadata global consumes precious memory.  It's
// much more efficient to just store the pointers in the global address space
// and put the real storage in the main loop's stack.  However, that means that
// we have to do all the one-time setup from within the main function.  That, in
// turn, means that we can never exit this function.  So, we will do all the
// one-time setup and then run our scheduler loop from within this call.
void loop() {
  // Externs
  extern Coroutine *schedulerProcess;
  extern Comessage *messages;

  // Prototypes and externs we need that are not exported from the Processes
  // library.
  void* dummyProcess(void *args);
  extern NanoOsMessage *nanoOsMessages;

  // We want the address of the first coroutine to be as close to the base as
  // possible.  Because of that, we need to create the first one before we enter
  // the scheduler.  That means we need to allocate the main coroutine here,
  // configure it, and then create and run one before we ever enter the
  // scheduler.
  Coroutine _mainCoroutine;
  schedulerProcess = &_mainCoroutine;
  coroutineConfig(&_mainCoroutine, NANO_OS_STACK_SIZE, NULL, NULL, NULL);
  Coroutine *coroutine = coroutineCreate(dummyProcess);
  coroutineResume(coroutine, NULL);

  // Additional variables we need to allocate before going into the scheduler.
  Comessage messagesStorage[NANO_OS_NUM_MESSAGES] = {};
  messages = messagesStorage;
  NanoOsMessage nanoOsMessagesStorage[NANO_OS_NUM_MESSAGES] = {};
  nanoOsMessages = nanoOsMessagesStorage;
  for (int ii = 0; ii < NANO_OS_NUM_MESSAGES; ii++) {
    // messages[ii].data will be initialized by getAvailableMessage.
    nanoOsMessages[ii].comessage = &messages[ii];
  }
  SchedulerState schedulerState = {};

  // Enter the scheduler.  This never returns.
  startScheduler(&schedulerState);
}

