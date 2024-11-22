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

// Coroutines support
Coroutine mainCoroutine;
RunningCommand runningCommands[NANO_OS_NUM_COROUTINES] = {0};
Comessage messages[NANO_OS_NUM_MESSAGES] = {0};

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  coroutineConfig(&mainCoroutine, NANO_OS_STACK_SIZE);

  // start serial port at 9600 bps:
  Serial.begin(9600);
  // wait for serial port to connect. Needed for native USB port only.
  while (!Serial);

  Coroutine *coroutine = coroutineCreate(runConsole);
  coroutineSetId(coroutine, NANO_OS_CONSOLE_PROCESS_ID);
  runningCommands[NANO_OS_CONSOLE_PROCESS_ID].coroutine = coroutine;
  runningCommands[NANO_OS_CONSOLE_PROCESS_ID].name = "runConsole";

  printConsole("\n");
  printConsole("Setup complete.\n");
  printConsole("> ");

  digitalWrite(LED_BUILTIN, HIGH);
}

void callFunction(Comessage *comessage) {
  CoroutineFunction func = comessageFunc(comessage);
  Coroutine *coroutine = coroutineCreate(func);
  coroutineSetId(coroutine, NANO_OS_RESERVED_PROCESS_ID);
  runningCommands[NANO_OS_RESERVED_PROCESS_ID].coroutine = coroutine;
  coroutineResume(coroutine, comessageStorage(comessage));
}

void (*mainCoroutineCommandHandlers[])(Comessage*) {
  callFunction,
};

static inline void handleMainCoroutineMessage(void) {
  Comessage *message = comessagePop(NULL);
  if (message != NULL) {
    MainCoroutineCommand messageType
      = (MainCoroutineCommand) comessageType(message);
    if (messageType >= NUM_MAIN_COROUTINE_COMMANDS) {
      // Invalid.
      return;
    }

    mainCoroutineCommandHandlers[messageType](message);
    releaseMessage(message);
  }

  return;
}

// the loop function runs over and over again forever
int coroutineIndex = 0;
void loop() {
  coroutineResume(runningCommands[coroutineIndex].coroutine, NULL);
  handleMainCoroutineMessage();
  coroutineIndex++;
  coroutineIndex %= NANO_OS_NUM_COROUTINES;
}
