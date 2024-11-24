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
  const char *processIdString = (const char*) args;
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
  char *argsBegin = (char*) args;
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

void* echoSomething(void *args) {
  (void) args;
  printf("Something\n");
  releaseConsole();
  return NULL;
}

unsigned int counter = 0;
void* showCounter(void *args) {
  (void) args;
  printf("Current counter value: %u\n", counter);
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

void handleCommand(char *consoleInput) {
  CommandEntry *commandEntry = NULL;
  int searchIndex = NUM_COMMANDS >> 1;
  size_t commandNameLength = strcspn(consoleInput, " \t\r\n");
  for (int ii = 0, jj = NUM_COMMANDS - 1; ii <= jj;) {
    const char *commandName = commands[searchIndex].name;
    int comparisonValue = strncmp(commandName, consoleInput, commandNameLength);
    if (comparisonValue == 0) {
      // We need an exact match.  So, the character at index commandNameLength
      // of commandName needs to be zero.  If it's anything else, we don't have
      // an exact match and we need to continue our search.  Since the order
      // we're comparing is commandName - consoleInput, what we're asserting
      // is that consoleInput[commandNameLength] is a NULL byte (0).  However,
      // it isn't really because of the whitespace inherent to commands.  So, we
      // can't do a literal subtration here.  However, since it's assumed to be
      // 0, this is the same as commandName[commandNameLength] - 0, or simply
      // commandName[commandNameLength].  So, just use that value as the final
      // comparison value here.
      comparisonValue = ((int) commandName[commandNameLength]);
    }

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

      comessageInit(comessage, CALL_FUNCTION,
        commandEntry->function, (intptr_t) consoleInput);
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

// REMINDER:  These commands have to be in alphabetical order so that the binary
//            search will work!!!
CommandEntry commands[] = {
  {
    .name = "echo",
    .function = echo,
    .userProcess = true
  },
  {
    .name = "echoSomething",
    .function = echoSomething,
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

