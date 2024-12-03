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

/// @fn int ps(int argc, char **argv);
///
/// @brief Display a list of running processes and their process IDs.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return This function always returns 0.
int ps(int argc, char **argv) {
  (void) argc;
  (void) argv;

  for (int ii = 0; ii < NANO_OS_NUM_COMMANDS; ii++) {
    if (coroutineResumable(runningCommands[ii].coroutine)) {
      printf("%d  %s\n",
        coroutineId(runningCommands[ii].coroutine),
        runningCommands[ii].name);
    }
  }

  printf("- Dynamic memory left: %d\n", getFreeMemory());
  releaseConsole();
  nanoOsExitProcess(0);
}

/// @fn int kill(int argc, char **argv);
///
/// @brief Kill a running process identified by its process ID.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return Returns 0 on success, 1 on failure.
int kill(int argc, char **argv) {
  (void) argc;
  (void) argv;

  if (argc < 2) {
    printf("Usage:\n");
    printf("  kill <process ID>\n");
    printf("\n");
    return 1;
  }
  int processId = (int) strtol(argv[1], NULL, 10);

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
  nanoOsExitProcess(0);
}

/// @fn int echo(int argc, char **argv);
///
/// @brief Echo a string from the user back to the console output.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.  They will be echoed back to the console
///   output separated by a space.
///
/// @return This function always returns 0.
int echo(int argc, char **argv) {
  for (int ii = 1; ii < argc; ii++) {
    printConsole(argv[ii]);
    if (ii < (argc - 1)) {
      printConsole(" ");
    }
  }
  printConsole("\n");

  releaseConsole();
  nanoOsExitProcess(0);
}

/// @fn int echoSomething(int argc, char **argv);
///
/// @brief Echo the word "Something" to the console output.  This function
/// exists to make sure that the binary search used for command lookup works
/// correctly.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return This function always returns 0.
int echoSomething(int argc, char **argv) {
  (void) argc;
  (void) argv;

  printf("Something\n");
  releaseConsole();
  nanoOsExitProcess(0);
}

/// @var counter
///
/// @brief Value that is continually incremented by the runCounter command and
/// shown via the showInfo command.
static unsigned int counter = 0;

/// @fn int runCounter(int argc, char **argv);
///
/// @brief Continually increment the global counter variable by one and then
/// yield control back to the scheduler.  This process exists as an example of
/// a multi-tasking command that runs in the background.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return This function would always return 0 if it returned, however it
/// runs in an infinite loop and only stops when it is killed by the kill
/// command.
int runCounter(int argc, char **argv) {
  (void) argc;
  (void) argv;

  // This is a background process, so go ahead and release the console.
  releaseConsole();

  while (1) {
    counter++;
    coroutineYield(NULL);
  }

  nanoOsExitProcess(0);
}

/// @fn int showInfo(int argc, char **argv);
///
/// @brief Show various information about the state of the system.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return This function always returns 0.
int showInfo(int argc, char **argv) {
  (void) argc;
  (void) argv;

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
  free(myString); // We don't want to set it to NULL in this case.
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
  myString = stringDestroy(myString);

  releaseConsole();
  nanoOsExitProcess(0);
}

/// @fn int ver(int argc, char **argv);
///
/// @brief Display the version of the OS on the console.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return This function always returns 0.
int ver(int argc, char **argv) {
  (void) argc;
  (void) argv;

  printf("NanoOs version " NANO_OS_VERSION "\n");

  releaseConsole();
  nanoOsExitProcess(0);
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
  printString("Parsing consoleInput: ");
  printString(consoleInput);
  printString("\n");

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
      consoleInput = stringDestroy(consoleInput);
      releaseConsole();
    } else {
      printString("Sent consoleInput to scheduler.\n");
    }
  } else {
    // printf is blocking.  handleCommand is called from runConsole itself, so
    // we can't use a blocking call here.  Use the non-blocking printConsole
    // instead.
    printConsole("Unknown command.\n");
    consoleInput = stringDestroy(consoleInput);
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

