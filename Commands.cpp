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
#include "Scheduler.h"

// Defined at the bottom of this file:
extern CommandEntry commands[];
extern const int NUM_COMMANDS;

// Commands

/// @fn int psCommandHandler(int argc, char **argv);
///
/// @brief Display a list of running processes and their process IDs.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return This function always returns 0.
int psCommandHandler(int argc, char **argv) {
  (void) argc;
  (void) argv;
  int returnValue = 0;

  printf("- Dynamic memory left: %d\n", getFreeMemory());

  ProcessInfo *processInfo = schedulerGetProcessInfo();
  if (processInfo != NULL) {
    uint8_t numRunningProcesses = processInfo->numProcesses;
    ProcessInfoElement *processes = processInfo->processes;
    for (uint8_t ii = 0; ii < numRunningProcesses; ii++) {
      printf("%d  %s %s\n",
        processes[ii].pid,
        getUsernameByUserId(processes[ii].userId),
        processes[ii].name);
    }
    free(processInfo); processInfo = NULL;
  } else {
    printf("ERROR:  Could not get process information from scheduler.\n");
  }

  printf("- Dynamic memory left: %d\n", getFreeMemory());
  return returnValue;
}

/// @fn int killCommandHandler(int argc, char **argv);
///
/// @brief Kill a running process identified by its process ID.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return Returns 0 on success, 1 on failure.
int killCommandHandler(int argc, char **argv) {
  (void) argc;
  (void) argv;

  if (argc < 2) {
    printf("Usage:\n");
    printf("  kill <process ID>\n");
    printf("\n");
    return 1;
  }
  ProcessId processId = (ProcessId) strtol(argv[1], NULL, 10);

  int returnValue = schedulerKillProcess(processId);

  return returnValue;
}

/// @fn int echoCommandHandler(int argc, char **argv);
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
int echoCommandHandler(int argc, char **argv) {
  for (int ii = 1; ii < argc; ii++) {
    printConsole(argv[ii]);
    if (ii < (argc - 1)) {
      printConsole(" ");
    }
  }
  printConsole("\n");

  return 0;
}

/// @fn int echoSomethingCommandHandler(int argc, char **argv);
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
int echoSomethingCommandHandler(int argc, char **argv) {
  (void) argc;
  (void) argv;

  printf("Something\n");
  return 0;
}

/// @fn int helpCommandHandler(int argc, char **argv);
///
/// @brief Print the help strings for all the commands in the system.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return This function always returns 0.
int helpCommandHandler(int argc, char **argv) {
  (void) argc;
  (void) argv;
  char formatString[12];

  size_t maxCommandNameLength = 0;
  for (int ii = 0; ii < NUM_COMMANDS; ii++) {
    size_t commandNameLength = strlen(commands[ii].name);
    if (commandNameLength > maxCommandNameLength) {
      maxCommandNameLength = commandNameLength;
    }
  }
  maxCommandNameLength++;
  sprintf(formatString, "%%-%us %%s\n", maxCommandNameLength);

  char *commandName = (char*) malloc(maxCommandNameLength + 1);
  for (int ii = 0; ii < NUM_COMMANDS; ii++) {
    strcpy(commandName, commands[ii].name);
    strcat(commandName, ":");
    const char *help = commands[ii].help;
    printf(formatString, commandName, help);
  }
  commandName = stringDestroy(commandName);

  return 0;
}

/// @var counter
///
/// @brief Value that is continually incremented by the runCounter command and
/// shown via the showInfo command.
static unsigned int counter = 0;

/// @fn int runCounterCommandHandler(int argc, char **argv);
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
int runCounterCommandHandler(int argc, char **argv) {
  (void) argc;
  (void) argv;

  while (1) {
    counter++;
    processYield();
  }

  return 0;
}

/// @fn int showInfoCommandHandler(int argc, char **argv);
///
/// @brief Show various information about the state of the system.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return This function always returns 0.
int showInfoCommandHandler(int argc, char **argv) {
  (void) argc;
  (void) argv;

  printf("- Compile time: %s, %s\n", __DATE__, __TIME__);
  printf("- Current counter value: %u\n", counter);
  printf("- Dynamic memory left: %d\n", getFreeMemory());
  printf("- sizeof(Coroutine): %u\n", sizeof(Coroutine));
  printf("- sizeof(ProcessMessage): %u\n", sizeof(ProcessMessage));
  printf("- sizeof(SdFat): %u\n", sizeof(SdFat));
  printf("- sizeof(FatFile): %u\n", sizeof(FatFile));
  printf("\n");

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
  printf("\n");

  fprintf(stderr, "- stdin: %p\n", stdin);
  fprintf(stderr, "- stdout: %p\n", stdout);
  fprintf(stderr, "- stderr: %p\n", stderr);

  return 0;
}

/// @fn int verCommandHandler(int argc, char **argv);
///
/// @brief Display the version of the OS on the console.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return This function always returns 0.
int verCommandHandler(int argc, char **argv) {
  (void) argc;
  (void) argv;

  printf("NanoOs version " NANO_OS_VERSION "\n");

  return 0;
}

/// @fn int sha1SumCommandHandler(int argc, char **argv)
///
/// @brief Compute the SHA1 sum of an input string.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.  Must be at least 2.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.  argv[1] will be she string to compute.
///
/// @return Returns 0 on success, 1 on failure.
int sha1SumCommandHandler(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage:  %s <string>\n", argv[0]);
    return 1;
  }
  const char *inputString = argv[1];

  char *hexDigest = getHexDigest(inputString);
  printf("SHA1 sum:  %s\n", hexDigest);
  hexDigest = stringDestroy(hexDigest);

  return 0;
}

/// @fn int logoutCommandHandler(int argc, char **argv)
///
/// @brief Logout of a running shell.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.  Ignored by this function.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.  Ignored by this function.
///
/// @return This function always returns 0.
int logoutCommandHandler(int argc, char **argv) {
  (void) argc;
  (void) argv;

  if (schedulerSetProcessUser(NO_USER_ID) != 0) {
    fputs("WARNING:  Could not clear owner of current process.\n", stderr);
  }

  return 0;
}

/// @fn CommandEntry* getCommandEntryFromInput(char *consoleInput)
///
/// @brief Get the command specified by consoleInput.
///
/// @param consoleInput The raw input captured in the console buffer.
///
/// @return Returns a pointer to the found CommandEntry on success, NULL on
/// failure.
CommandEntry* getCommandEntryFromInput(char *consoleInput) {
  CommandEntry *commandEntry = NULL;
  if (*consoleInput != '\0') {
    int searchIndex = NUM_COMMANDS >> 1;
    size_t commandNameLength = strcspn(consoleInput, " \t\r\n&");
    for (int ii = 0, jj = NUM_COMMANDS - 1; ii <= jj;) {
      const char *commandName = commands[searchIndex].name;
      int comparisonValue
        = strncmp(commandName, consoleInput, commandNameLength);
      if (comparisonValue == 0) {
        // We need an exact match.  So, the character at index commandNameLength
        // of commandName needs to be zero.  If it's anything else, we don't
        // have an exact match and we need to continue our search.  Since the
        // order we're comparing is commandName - consoleInput, what we're
        // asserting is that consoleInput[commandNameLength] is a NULL byte (0).
        // However, it isn't really because of the whitespace inherent to
        // commands.  So, we can't do a literal subtration here.  However, since
        // it's assumed to be 0, this is the same as
        // commandName[commandNameLength] - 0, or simply
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
  }

  return commandEntry;
}

// Exported functions

/// @fn int handleCommand(int consolePort, char *consoleInput)
///
/// @brief Parse the command name out of the console input and run the command
/// using the rest of the input as an argument to the command.  The command will
/// be launched as a separate process, not run inline.
///
/// @param consolePort The index of the console port the input came from.
/// @param consoleInput A pointer to the beginning of the buffer that contains
/// user input.
///
/// @return This function returns no value.
int handleCommand(int consolePort, char *consoleInput) {
  int returnValue = processSuccess;
  CommandEntry *commandEntry = getCommandEntryFromInput(consoleInput);
  if (commandEntry != NULL) {
    // Send the found entry over to the scheduler.
    if (schedulerRunProcess(commandEntry, consoleInput, consolePort) != 0) {
      consoleInput = stringDestroy(consoleInput);
      releaseConsole();
    }
  } else {
    // printf is blocking.  handleCommand is called from runConsole itself, so
    // we can't use a blocking call here.  Use the non-blocking printString
    // instead.
    free(consoleInput); consoleInput = NULL;
    returnValue = processError;
  }

  return returnValue;
}

/// @fn void* runShell(void *args)
///
/// @brief Process function for interactive user shell.
///
/// @param args Any arguments passed by the scheduler.  Ignored by this
///   function.
///
/// @return This function never exits, but would return NULL if it did.
void* runShell(void *args) {
  (void) args;
  char commandBuffer[CONSOLE_BUFFER_SIZE];
  int consolePort = getOwnedConsolePort();
  while (consolePort < 0) {
    processYield();
    consolePort = getOwnedConsolePort();
  }

  if (schedulerGetProcessUser() < 0) {
    printf("\nNanoOs " NANO_OS_VERSION " localhost console %d\n\n",
      consolePort);
    login();
  }

  UserId processUserId = schedulerGetProcessUser();
  const char *prompt = "$";
  if (processUserId == ROOT_USER_ID) {
    prompt = "#";
  }
  const char *processUsername = getUsernameByUserId(processUserId);
  while (1) {
    printf("%s@localhost%s ", processUsername, prompt);
    fgets(commandBuffer, sizeof(commandBuffer), stdin);
    CommandEntry *commandEntry = getCommandEntryFromInput(commandBuffer);
    if (commandEntry == NULL) {
      printf("Unknown command.\n");
      continue;
    }

    char *newlineAt = strchr(commandBuffer, '\r');
    if (newlineAt == NULL) {
      newlineAt = strchr(commandBuffer, '\n');
    }
    if (newlineAt != NULL) {
      *newlineAt = '\0';
    }

    size_t bufferLength = strlen(commandBuffer);
    char *consoleInput = (char*) malloc(bufferLength + 1);
    strcpy(consoleInput, commandBuffer);
    if (schedulerRunProcess(commandEntry, consoleInput, consolePort) != 0) {
      consoleInput = stringDestroy(consoleInput);
    }
  }

  return NULL;
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
    .func = echoCommandHandler,
    .help = "Echo a string back to the console."
  },
  {
    .name = "echoSomething",
    .func = echoSomethingCommandHandler,
    .help = "Echo the word \"Something\" back to the console."
  },
  {
    .name = "exit",
    .func = logoutCommandHandler,
    .help = "Exit the current shell."
  },
  {
    .name = "help",
    .func = helpCommandHandler,
    .help = "Print this help message."
  },
  {
    .name = "kill",
    .func = killCommandHandler,
    .help = "Kill a running process."
  },
  {
    .name = "logout",
    .func = logoutCommandHandler,
    .help = "Logout of the system."
  },
  {
    .name = "ps",
    .func = psCommandHandler,
    .help = "List the running processes."
  },
  {
    .name = "runCounter",
    .func = runCounterCommandHandler,
    .help = "Increment a counter in a while loop."
  },
  {
    .name = "sha1Sum",
    .func = sha1SumCommandHandler,
    .help = "Compute the SHA1 sum of a value."
  },
  {
    .name = "showInfo",
    .func = showInfoCommandHandler,
    .help = "Show various pieces of information about the system."
  },
  {
    .name = "ver",
    .func = verCommandHandler,
    .help = "Show the version of the operating system."
  },
};

/// @var NUM_COMMANDS
///
/// @brief Integer constant value that holds the number of commands in the
/// commands array above.  Used in the binary search that looks up commands by
/// their names.
const int NUM_COMMANDS = sizeof(commands) / sizeof(commands[0]);

