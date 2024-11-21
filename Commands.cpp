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

#include "Commands.h"

// Defined at the bottom of this file:
extern CommandEntry commands[];
extern const int NUM_COMMANDS;

// Commands

void* ps(void *args) {
  (void) args;

  for (int ii = 0; ii < NANO_OS_NUM_COROUTINES; ii++) {
    if (coroutineResumable(runningCommands[ii].coroutine)) {
      printf("%d  %s\n",
        coroutineId(runningCommands[ii].coroutine),
        runningCommands[ii].name);
    }
  }

  releaseConsole();
  return NULL;
}

void* kill(void *args) {
  const char *processIdString = *((const char**) args);
  int processId = (int) strtol(processIdString, NULL, 10);

  if ((processId != NANO_OS_CONSOLE_PROCESS_ID)
    && (processId < NANO_OS_NUM_COROUTINES)
    && (coroutineResumable(runningCommands[processId].coroutine))
  ) {
    coroutineTerminate(runningCommands[processId].coroutine, NULL);
    runningCommands[processId].coroutine = NULL;
    runningCommands[processId].name = NULL;
    printf("Process terminated.\n");
  } else {
    printf("ERROR:  Invalid process ID.\n");
  }

  releaseConsole();
  return NULL;
}

void* echo(void *args) {
  char *argsBegin = (const char *) args;
  char *newlineAt = strchr(argsBegin, '\r');
  if (newlineAt == NULL) {
    newlineAt = strchr(argsBegin, '\n');
  }
  if (newlineAt) {
    *newlineAt = '\0';
  }

  size_t argsLength = strlen(argsBegin);
  if (argsLength < 255) {
    argsBegin[argsLength] = '\n';
    argsBegin[argsLength + 1] = '\0';
  } else {
    argsBegin[254] = '\n';
    argsBegin[255] = '\0';
  }

  printf(argsBegin);
  releaseConsole();
  return NULL;
}

unsigned int counter = 0;
void* showCounter(void *args) {
  (void) args;
  printf("Current counter value: %d\n", counter);
  printf("- SRAM left: %d\n", freeRamBytes());
  releaseConsole();
  return NULL;
}

void* runCounter(void *args) {
  (void) args;

  // This is a background process, so go ahead and release the console.
  releaseConsole();
  while (1) {
    counter++;
    coroutineYield(NULL);
  }
  return NULL;
}

void handleCommand(const char *consoleInput) {
  CommandEntry *commandEntry = NULL;
  int searchIndex = NUM_COMMANDS >> 1;
  for (int ii = 0, jj = NUM_COMMANDS - 1; ii <= jj;) {
    const char *commandName = commands[searchIndex].name;
    size_t commandNameLength = strlen(commandName);
    int comparisonValue = strncmp(commandName, consoleInput, commandNameLength);
    if (comparisonValue == 0) {
      commandEntry = &commands[searchIndex];
      consoleInput += commandNameLength + 1;
      break;
    } else if (comparisonValue < 0) {
      ii = searchIndex + 1;
    } else { // comparisonValue > 0
      jj = searchIndex - 1;
    }

    searchIndex = (ii + jj) >> 1;
  }

  if (commandEntry != NULL) {
    if (commandEntry->userProcess == true) {
      int jj = NANO_OS_FIRST_PROCESS_ID;
      for (; jj < NANO_OS_NUM_COROUTINES; jj++) {
        Coroutine *coroutine = runningCommands[jj].coroutine;
        if ((coroutine == NULL) || (coroutineFinished(coroutine))) {
          coroutine = coroutineCreate(commandEntry->function);
          coroutineSetId(coroutine, jj);

          runningCommands[jj].coroutine = coroutine;
          runningCommands[jj].name = commandEntry->name;

          coroutineResume(coroutine, consoleInput);
          break;
        }
      }

      if (jj == NANO_OS_NUM_COROUTINES) {
        // printf is blocking.  handleCommand is called from runConsole itself,
        // so we can't use a blocking call here.  Use the non-blocking
        // printConsole instead.
        printConsole("Out of memory to launch process.\n");
        releaseConsole();
      }
    } else {
      // We need to run this command from the main thread (that's running the
      // scheduler).  We can't block the console.
      Comessage *comessage = getAvailableMessage();
      while (comessage == NULL) {
        coroutineYield(NULL);
        comessage = getAvailableMessage();
      }

      comessage->type = (int) CALL_FUNCTION;
      comessage->funcData.func = commandEntry->function;
      memcpy(comessage->storage, &consoleInput, sizeof(consoleInput));
      comessage->handled = false;
      comessagePush(&mainCoroutine, comessage);
    }
  } else {
    // printf is blocking.  handleCommand is called from runConsole itself, so
    // we can't use a blocking call here.  Use the non-blocking printConsole
    // instead.
    printConsole("Unknown command.\n");
    releaseConsole();
  }

  return;
}

CommandEntry commands[] = {
  {
    .name = "echo",
    .function = echo,
    .userProcess = true
  },
  {
    .name = "kill",
    .function = kill,
    .userProcess = false
  },
  {
    .name = "ps",
    .function = ps,
    .userProcess = false
  },
  {
    .name = "runCounter",
    .function = runCounter,
    .userProcess = true
  },
  {
    .name = "showCounter",
    .function = showCounter,
    .userProcess = true
  }
};
const int NUM_COMMANDS = sizeof(commands) / sizeof(commands[0]);

