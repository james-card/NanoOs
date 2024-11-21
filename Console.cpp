#include "Console.h"
#include "Commands.h"

void consoleWriteChar(void *data) {
  const char *message = (const char *) data;
  Serial.print(*message);

  return;
}

void consoleWriteUChar(void *data) {
  const unsigned char *message = (const unsigned char *) data;
  Serial.print(*message);

  return;
}

void consoleWriteInt(void *data) {
  int *message = (int *) data;
  Serial.print(*message);

  return;
}

void consoleWriteUInt(void *data) {
  unsigned int *message = (unsigned int *) data;
  Serial.print(*message);

  return;
}

void consoleWriteLongInt(void *data) {
  long int *message = (long int *) data;
  Serial.print(*message);

  return;
}

void consoleWriteLongUInt(void *data) {
  long int *message = (long int *) data;
  Serial.print(*message);

  return;
}

void consoleWriteFloat(void *data) {
  float *message = (float *) data;
  Serial.print(*message);

  return;
}

void consoleWriteDouble(void *data) {
  double *message = (double *) data;
  Serial.print(*message);

  return;
}

void consoleWriteString(void *data) {
  const char *message = (const char *) data;
  Serial.print(message);

  return;
}

void (*consoleCommandHandlers[])(void*) {
  consoleWriteChar,
  consoleWriteUChar,
  consoleWriteInt,
  consoleWriteUInt,
  consoleWriteLongInt,
  consoleWriteLongUInt,
  consoleWriteFloat,
  consoleWriteDouble,
  consoleWriteString,
};

static inline void handleConsoleMessages(void) {
  Comessage *message = comessagePop(NULL);
  while (message != NULL) {
    ConsoleCommand messageType = (ConsoleCommand) message->type;
    if (messageType >= NUM_CONSOLE_COMMANDS) {
      // Invalid.
      message = comessagePop(NULL);
      continue;
    }

    consoleCommandHandlers[messageType](message->funcData.data);
    message->handled = true;
    releaseMessage(message);
    message = comessagePop(NULL);
  }

  return;
}

unsigned long startTime = 0;
int ledStates[2] = {LOW, HIGH};
int ledStateIndex = 0;
void blink() {
  if (getElapsedMilliseconds(startTime) >= (LED_CYCLE_TIME_MS >> 1)) {
    // toggle the LED state
    digitalWrite(LED_BUILTIN, ledStates[ledStateIndex]);
    ledStateIndex ^= 1;
    startTime = getElapsedMilliseconds(0);
  }

  return;
}

char consoleBuffer[256];
unsigned char consoleIndex = 0;
void* runConsole(void *args) {
  (void) args;
  int serialData = -1;

  while (1) {
    blink();

    serialData = Serial.read();
    if (serialData > -1) {
      consoleBuffer[consoleIndex] = (char) serialData;
      Serial.print(consoleBuffer[consoleIndex]);
      consoleIndex++;
    }

    if ((serialData == ((int) '\n')) || (serialData == ((int) '\r'))) {
      // NULL-terminate the buffer.
      consoleBuffer[consoleIndex] = '\0';
      if (serialData == ((int) '\r')) {
        Serial.println("");
      }
      handleCommand(consoleBuffer);
      // If the command has already returned or wrote to the console before its
      // first yield, we may need to display its output.  Handle the next
      // next message in our queue just in case.
      handleConsoleMessages();
      consoleIndex = 0;
    }
    
    coroutineYield(NULL);
    handleConsoleMessages();
  }

  return NULL;
}
