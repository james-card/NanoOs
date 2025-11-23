////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2025 James Card                     //
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

#include "../user/NanoOsApi.h"
#include "Commands.h"
#include "Console.h"
#include "MemoryManager.h"
#include "NanoOs.h"
#include "NanoOsOverlay.h"
#include "Processes.h"
#include "Scheduler.h"

// Must come last
#include "../user/NanoOsStdio.h"

// Defined at the bottom of this file:
extern const CommandEntry commands[];
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
    printf("ERROR: Could not get process information from scheduler.\n");
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
  char commandPath[26] = "/usr/bin/";
  strcat(commandPath, "echo");
  return runOverlayCommand(commandPath, argc, argv, NULL);
}

/// @fn int gettyCommandHandler(int argc, char **argv);
///
/// @brief Run the getty application from the filesystem.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return Returns 0 on success, 1 on failure.
int gettyCommandHandler(int argc, char **argv) {
  char commandPath[26] = "/usr/bin/";
  strcat(commandPath, "getty");
  return runOverlayCommand(commandPath, argc, argv, NULL);
}

/// @fn int grepCommandHandler(int argc, char **argv);
///
/// @brief Echo a line of text from standard input to the console output if it
/// contains the string the user is looking for.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.  They will be greped back to the console
///   output separated by a space.
///
/// @return This function always returns 0.
int grepCommandHandler(int argc, char **argv) {
  char buffer[96];

  if (argc < 2) {
    printf("Usage:  %s <string to find>\n", argv[0]);
    return 1;
  }

  while (fgets(buffer, sizeof(buffer), stdin)) {
    if (strstr(buffer, argv[1])) {
      fputs(buffer, stdout);
    }
  }

  if ((strlen(buffer) > 0) && (buffer[strlen(buffer) - 1] != '\n')) {
    fputs("\n", stdout);
  }

  return 0;
}

/// @fn int helloworldCommandHandler(int argc, char **argv);
///
/// @brief Run the "helloworld" command from the filesystem.
///
/// @param argc The number or arguments parsed from the command line, including
///   the name of the command.
/// @param argv The array of arguments parsed from the command line with one
///   argument per array element.
///
/// @return This function always returns 0.
int helloworldCommandHandler(int argc, char **argv) {
  char commandPath[26] = "/usr/bin/";
  strcat(commandPath, "helloworld");
  return runOverlayCommand(commandPath, argc, argv, NULL);
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
  char formatString[19];

  size_t maxCommandNameLength = 0;
  for (int ii = 0; ii < NUM_COMMANDS; ii++) {
    size_t commandNameLength = strlen(commands[ii].name);
    if (commandNameLength > maxCommandNameLength) {
      maxCommandNameLength = commandNameLength;
    }
  }
  maxCommandNameLength++;
  sprintf(formatString, "%%-%us %%s\n", (unsigned int) maxCommandNameLength);

  char *commandName = (char*) malloc(maxCommandNameLength + 2);
  for (int ii = 0; ii < NUM_COMMANDS; ii++) {
    strcpy(commandName, commands[ii].name);
    strcat(commandName, ":");
    const char *help = commands[ii].help;
    printf(formatString, commandName, help);
  }
  commandName = stringDestroy(commandName);

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
    fputs("WARNING: Could not clear owner of current process.\n", stderr);
  }

  return 0;
}

/// @fn const CommandEntry* getCommandEntryFromInput(char *consoleInput)
///
/// @brief Get the command specified by consoleInput.
///
/// @param consoleInput The raw input captured in the console buffer.
///
/// @return Returns a pointer to the found CommandEntry on success, NULL on
/// failure.
const CommandEntry* getCommandEntryFromInput(char *consoleInput) {
  const CommandEntry *commandEntry = NULL;
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
  const CommandEntry *commandEntry = getCommandEntryFromInput(consoleInput);
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
  const char *hostname = (char*) args;
  char commandBuffer[CONSOLE_BUFFER_SIZE];
  int consolePort = getOwnedConsolePort();
  while (consolePort < 0) {
    processYield();
    consolePort = getOwnedConsolePort();
  }

  if (schedulerGetProcessUser() < 0) {
    printf("\nNanoOs " NANO_OS_VERSION " %s console %d\n\n",
      hostname, consolePort);
    login();
  }

  UserId processUserId = schedulerGetProcessUser();
  const char *prompt = "$";
  if (processUserId == ROOT_USER_ID) {
    prompt = "#";
  }
  const char *processUsername = getUsernameByUserId(processUserId);
  while (1) {
    printf("%s@%s%s ", processUsername, hostname, prompt);
    fgets(commandBuffer, sizeof(commandBuffer), stdin);
    const CommandEntry *commandEntry = getCommandEntryFromInput(commandBuffer);
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
///            binary search will work:
const CommandEntry commands[] = {
  {
    .name = "echo",
    .func = echoCommandHandler,
    .help = "Echo a string back to the console."
  },
  {
    .name = "exit",
    .func = logoutCommandHandler,
    .help = "Exit the current shell."
  },
  {
    .name = "getty",
    .func = gettyCommandHandler,
    .help = "Run the getty application."
  },
  {
    .name = "grep",
    .func = grepCommandHandler,
    .help = "Find text in piped output."
  },
  {
    .name = "helloworld",
    .func = helloworldCommandHandler,
    .help = "Run the \"helloworld\" command from the filesystem."
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
};

/// @var NUM_COMMANDS
///
/// @brief Integer constant value that holds the number of commands in the
/// commands array above.  Used in the binary search that looks up commands by
/// their names.
const int NUM_COMMANDS = sizeof(commands) / sizeof(commands[0]);

