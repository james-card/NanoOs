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

// Doxygen marker
/// @file

#include "Commands.h"

// Defined at the bottom of this file:
extern CommandEntry commands[];
extern const int NUM_COMMANDS;

// Commands

/// @fn void* ps(void *args)
///
/// @brief Display a list of running processes and their process IDs.
///
/// @param args The rest of the console command line after the name of the
///   command, cast to a void*.  Unused by this function.
///
/// @return This function always returns NULL.
void* ps(void *args) {
  (void) args;
  // At this point, we were called by coroutineResume from handleCommand, so
  // it's still suspended.  We're not processing any arguments (the console
  // input), so we need to go ahead and yield now.
  coroutineYield(NULL);

  for (int ii = 0; ii < NANO_OS_NUM_COMMANDS; ii++) {
    if (coroutineResumable(runningCommands[ii].coroutine)) {
      printf("%d  %s\n",
        coroutineId(runningCommands[ii].coroutine),
        runningCommands[ii].name);
    }
  }

  printf("- Dynamic memory left: %d\n", getFreeMemory());
  releaseConsole();
  nanoOsExitProcess(NULL);
}

/// @fn void* kill(void *args)
///
/// @brief Kill a running process identified by its process ID.
///
/// @param args The rest of the console command line after the name of the
///   command, cast to a void*.  The process ID of the command to kill is parsed
///   from this before returning execution back to the console.
///
/// @return This function always returns NULL.
void* kill(void *args) {
  const char *processIdString = (const char*) args;
  int processId = (int) strtol(processIdString, NULL, 10);
  // We're done processing input from the console, so yield now.
  coroutineYield(NULL);

  if ((processId > NANO_OS_RESERVED_PROCESS_ID)
    && (processId < NANO_OS_NUM_COMMANDS)
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
  nanoOsExitProcess(NULL);
}

/// @fn void* echo(void *args)
///
/// @brief Echo a string from the user back to the console output.
///
/// @param args The rest of the console command line after the name of the
///   command, cast to a void*.  This is taken as the string to echo back to the
///   console output.
///
/// @return This function always returns NULL.
void* echo(void *args) {
  // The consoleBuffer is 256 characters.  Stacks are 512, so we can declare our
  // own buffer and still be safe.
  char consoleCopy[256];
  char *argsBegin = (char*) args;
  char *newlineAt = strchr(argsBegin, '\r');
  if (newlineAt == NULL) {
    newlineAt = strchr(argsBegin, '\n');
  }
  // One of the two must have succeeded, otherwise this command wouldn't have
  // been called.  So, we can safely dereference the pointer here.
  *newlineAt = '\0';
  strcpy(consoleCopy, argsBegin);
  // We're done with the console input, so yield now.
  coroutineYield(NULL);

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
  nanoOsExitProcess(NULL);
}

/// @fn void* echoSomething(void *args)
///
/// @brief Echo the word "Something" to the console output.  This function
/// exists to make sure that the binary search used for command lookup works
/// correctly.
///
/// @param args The rest of the console command line after the name of the
///   command, cast to a void*.  Ignored by this process.
///
/// @return This function always returns NULL.
void* echoSomething(void *args) {
  (void) args;
  // We're not processing conosle input, so immediately yield.
  coroutineYield(NULL);
  printf("Something\n");
  releaseConsole();
  nanoOsExitProcess(NULL);
}

/// @var counter
///
/// @brief Value that is continually incremented by the runCounter command and
/// shown via the showInfo command.
static unsigned int counter = 0;

/// @fn void* runCounter(void *args)
///
/// @brief Continually increment the global counter variable by one and then
/// yield control back to the scheduler.  This process exists as an example of
/// a multi-tasking command that runs in the background.
///
/// @param args The rest of the console command line after the name of the
///   command, cast to a void*.  Ignored by this process.
///
/// @return This function would always return NULL if it returned, however it
/// runs in an infinite loop and only stops when it is killed by the kill
/// command.
void* runCounter(void *args) {
  (void) args;
  // We're not processing conosle input, so immediately yield.
  coroutineYield(NULL);

  // This is a background process, so go ahead and release the console.
  releaseConsole();

  while (1) {
    counter++;
    coroutineYield(NULL);
  }

  nanoOsExitProcess(NULL);
}

/// @fn void* showInfo(void *args)
///
/// @brief Show various information about the state of the system.
///
/// @param args The rest of the console command line after the name of the
///   command, cast to a void*.  Ignored by this process.
///
/// @return This function always returns NULL.
void* showInfo(void *args) {
  (void) args;
  // We're not processing conosle input, so immediately yield.
  coroutineYield(NULL);

  printf("Current counter value: %u\n", counter);
  printf("- Dynamic memory left: %d\n", getFreeMemory());
  printf("- sizeof(Coroutine): %u\n", sizeof(Coroutine));
  printf("- sizeof(Comessage): %u\n", sizeof(Comessage));

  char *myString = (char*) malloc(16);
  strcpy(myString, "Hello, world!!!");
  printf("- myString: %p\n", myString);
  printf("- myString: '%s'", myString);
  printf("\n");
  printf("- strlen(myString): %u\n", strlen(myString));
  printf("- Dynamic memory left: %d\n", getFreeMemory());
  free(myString);
  printf("- myString after free: '%s'", myString);
  printf("\n");
  printf("- strlen(myString) after free: %u\n", strlen(myString));
  printf("- Dynamic memory left: %d\n", getFreeMemory());
  myString = (char*) malloc(16);
  printf("- Second myString: %p\n", myString);
  printf("- Second myString: '%s'", myString);
  printf("\n");
  printf("- Sescond strlen(myString): %u\n", strlen(myString));
  printf("- Dynamic memory left: %d\n", getFreeMemory());
  free(myString); myString = NULL;

  releaseConsole();
  nanoOsExitProcess(NULL);
}

/// @fn void* ver(void *args)
///
/// @brief Display the version of the OS on the console.
///
/// @param args The rest of the console command line after the name of the
///   command, cast to a void*.  Ignored by this process.
///
/// @return This function always returns NULL.
void* ver(void *args) {
  (void) args;
  // We're not processing conosle input, so immediately yield.
  coroutineYield(NULL);

  printf("NanoOs version " NANO_OS_VERSION "\n");

  releaseConsole();
  nanoOsExitProcess(NULL);
}

// Exported functions

/// @fn void handleCommand(char *consoleInput)
///
/// @brief Parse the command name out of the console input and run the command
/// using the rest of the input as an argument to the command.  The command will
/// be launched as a separate process, not run inline.
///
/// @param consoleInput A pointer to the beginning of the console buffer that
///   contains user input.
///
/// @return This function returns no value.
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
    // Send the found entry over to the scheduler.
    if (sendNanoOsMessageToPid(NANO_OS_SCHEDULER_PROCESS_ID, RUN_PROCESS,
      (NanoOsMessageData) commandEntry, (NanoOsMessageData) consoleInput,
      false) == NULL
    ) {
      printConsole("ERROR!!!  Could not communicate with scheduler.\n");
      releaseConsole();
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

/// @var commands
///
/// @brief Array of CommandEntry values that contain the names of the commands,
/// a pointer to the command handler functions, and information about whether
/// the command is a user process or a system process.
///
/// @details
/// REMINDER:  These commands have to be in alphabetical order so that the
///            binarysearch will work!!!
CommandEntry commands[] = {
  {
    .name = "echo",
    .func = echo,
    .userProcess = true
  },
  {
    .name = "echoSomething",
    .func = echoSomething,
    .userProcess = true
  },
  {
    .name = "kill",
    .func = kill,
    .userProcess = false
  },
  {
    .name = "ps",
    .func = ps,
    .userProcess = false
  },
  {
    .name = "runCounter",
    .func = runCounter,
    .userProcess = true
  },
  {
    .name = "showInfo",
    .func = showInfo,
    .userProcess = true
  },
  {
    .name = "ver",
    .func = ver,
    .userProcess = true
  },
};

/// @var NUM_COMMANDS
///
/// @brief Integer constant value that holds the number of commands in the
/// commands array above.  Used in the binary search that looks up commands by
/// their names.
const int NUM_COMMANDS = sizeof(commands) / sizeof(commands[0]);

