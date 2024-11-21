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
  CoroutineFunction func = comessage->funcData.func;
  Coroutine *coroutine = coroutineCreate(func);
  coroutineSetId(coroutine, NANO_OS_RESERVED_PROCESS_ID);
  runningCommands[NANO_OS_RESERVED_PROCESS_ID].coroutine = coroutine;
  coroutineResume(coroutine, comessage->storage);
}

void (*mainCoroutineCommandHandlers[])(Comessage*) {
  callFunction,
};

static inline void handleMainCoroutineMessage(void) {
  Comessage *message = comessagePop(NULL);
  if (message != NULL) {
    MainCoroutineCommand messageType = (MainCoroutineCommand) message->type;
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
