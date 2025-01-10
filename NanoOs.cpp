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

// Custom includes
#include "NanoOs.h"
#include "Scheduler.h"

// Externs
extern User users[];
extern const int NUM_USERS;

/// @fn ProcessId getNumPipes(const char *commandLine)
///
/// @brief Get the number of pipes in a commandLine.
///
/// @param commandLine The command line as read in from a console port.
///
/// @return Returns the number of pipe characters found in the command line.
ProcessId getNumPipes(const char *commandLine) {
  ProcessId numPipes = 0;
  const char *pipeAt = NULL;

  do {
    pipeAt = strchr(commandLine, '|');
    if (pipeAt != NULL) {
      numPipes++;
      commandLine = pipeAt + 1;
    }
  } while (pipeAt != NULL);

  return numPipes;
}

/// @var processStorage
///
/// @brief File-local variable to hold the per-process storage.
static void *processStorage[
  NANO_OS_NUM_PROCESSES - NANO_OS_FIRST_USER_PROCESS_ID][
  NUM_PROCESS_STORAGE_KEYS] = {};

/// @fn void *getProcessStorage(uint8_t key)
///
/// @brief Get a previously-set value from per-process storage.
///
/// @param key The index into the process's per-process storage to retrieve.
///
/// @return Returns the previously-set value on success, NULL on failure.
void *getProcessStorage(uint8_t key) {
  void *returnValue = NULL;
  if (key >= NUM_PROCESS_STORAGE_KEYS) {
    // Key is out of range.
    return returnValue; // NULL
  }

  int processIndex
    = ((int) getRunningProcessId()) - NANO_OS_FIRST_USER_PROCESS_ID;
  if ((processIndex >= 0)
    && (processIndex < (NANO_OS_NUM_PROCESSES - NANO_OS_FIRST_USER_PROCESS_ID))
  ) {
    // Calling process is not supported and does not have storage.
    returnValue = processStorage[processIndex][key];
  }

  return returnValue;
}

/// @fn int setProcessStorage_(uint8_t key, void *val, int processId, ...)
///
/// @brief Set the value of a piece of per-process storage.
///
/// @param key The index into the process's per-process storage to retrieve.
/// @param val The pointer value to set for the storage.
/// @param processId The ID of the process to set.  This value may only be set
///   by the scheduler.
///
/// @return Returns coroutineSuccess on success, coroutineError on failure.
int setProcessStorage_(uint8_t key, void *val, int processId, ...) {
  int returnValue = coroutineError;
  if (key >= NUM_PROCESS_STORAGE_KEYS) {
    // Key is out of range.
    return returnValue; // coroutineError
  }

  if (processId < 0) {
    if (getRunningProcessId() == NANO_OS_SCHEDULER_PROCESS_ID) {
      processId = (int) getRunningProcessId();
    } else {
      return returnValue; // coroutineError
    }
  }
  int processIndex = processId - NANO_OS_FIRST_USER_PROCESS_ID;
  if ((processIndex >= 0)
    && (processIndex < (NANO_OS_NUM_PROCESSES - NANO_OS_FIRST_USER_PROCESS_ID))
  ) {
    // Calling process is not supported and does not have storage.
    processStorage[processIndex][key] = val;
    returnValue = coroutineSuccess;
  }

  return returnValue;
}

/// @fn void timespecFromDelay(struct timespec *ts, long int delayMs)
///
/// @brief Initialize the value of a struct timespec with a time in the future
/// based upon the current time and a specified delay period.  The timespec
/// will hold the value of the current time plus the delay.
///
/// @param ts A pointer to a struct timespec to initialize.
/// @param delayMs The number of milliseconds in the future the timespec is to
///   be initialized with.
///
/// @return This function returns no value.
void timespecFromDelay(struct timespec *ts, long int delayMs) {
  if (ts == NULL) {
    // Bad data.  Do nothing.
    return;
  }

  timespec_get(ts, TIME_UTC);
  ts->tv_sec += (delayMs / 1000);
  ts->tv_nsec += (delayMs * 1000000);

  return;
}

/// @fn unsigned int raiseUInt(unsigned int x, unsigned int y)
///
/// @brief Raise a non-negative integer to a non-negative exponent.
///
/// @param x The base number to raise.
/// @param y The exponent to raise the base to.
///
/// @param Returns the result of x ** y.
unsigned int raiseUInt(unsigned int x, unsigned int y) {
  unsigned int z = 1;
  unsigned int multiplier = x;

  while (y > 0) {
    if (y & 1) {
      z *= multiplier;
    }

    multiplier *= multiplier;
    y >>= 1;
  }

  return z;
}

/// @fn const char* getUsernameByUserId(UserId userId)
///
/// @brief Get the username for a user given their numeric user ID.
///
/// @param userId The numeric ID of the user.
///
/// @return Returns the username of the user on success, NULL on failure.
const char* getUsernameByUserId(UserId userId) {
  const char *username = "unowned";

  for (int ii = 0; ii < NUM_USERS; ii++) {
    if (users[ii].userId == userId) {
      username = users[ii].username;
      break;
    }
  }

  return username;
}

/// @fn UserId getUserIdByUsername(const char *username)
///
/// @brief Get the numeric ID of a user given their username.
///
/// @param username The username string for the user.
///
/// @return Returns the numeric ID of the user on success, NO_USER_ID on
/// failure.
UserId getUserIdByUsername(const char *username) {
  UserId userId = NO_USER_ID;

  for (int ii = 0; ii < NUM_USERS; ii++) {
    if (strcmp(users[ii].username, username) == 0) {
      userId = users[ii].userId;
      break;
    }
  }

  return userId;
}

/// @def USERNAME_BUFFER_SIZE
///
/// @brief Size to use for the username buffer in the login function.
#define USERNAME_BUFFER_SIZE 16

/// @def PASSWORD_BUFFER_SIZE
///
/// @brief Size to use for the password buffer in the login function.
#define PASSWORD_BUFFER_SIZE 16

/// @fn void login(void)
///
/// @brief Authenticate a user for login.  Sets the owner of the current process
/// to the ID of the authenticated user before returning.
///
/// @param This function returns no value.
void login(void) {
  UserId userId = NO_USER_ID;

  char *username = (char*) malloc(USERNAME_BUFFER_SIZE);
  char *password = (char*) malloc(PASSWORD_BUFFER_SIZE);
  char *newlineAt = NULL;
  size_t usernameLength = 0, passwordLength = 0, ii = 0;

  while (userId == NO_USER_ID) {
    unsigned int checksum = 0;

    fputs("login: ", stdout);
    fgets(username, USERNAME_BUFFER_SIZE, stdin);
    setConsoleEcho(false);
    fputs("Password: ", stdout);
    fgets(password, PASSWORD_BUFFER_SIZE, stdin);
    setConsoleEcho(true);
    fputs("\n\n", stdout);

    newlineAt = strchr(username, '\r');
    if (newlineAt == NULL) {
      newlineAt = strchr(username, '\n');
    }
    if (newlineAt != NULL) {
      // Terminate the string at the newline.
      *newlineAt = '\0';
    }
    usernameLength = strlen(username);
    for (ii = 0; ii < usernameLength; ii++) {
      checksum += (unsigned int) username[ii];
    }

    newlineAt = strchr(password, '\r');
    if (newlineAt == NULL) {
      newlineAt = strchr(password, '\n');
    }
    if (newlineAt != NULL) {
      // Terminate the string at the newline.
      *newlineAt = '\0';
    }
    passwordLength = strlen(password);
    for (ii = 0; ii < passwordLength; ii++) {
      checksum += (unsigned int) password[ii];
    }

    for (int ii = 0; ii < NUM_USERS; ii++) {
      if (strcmp(users[ii].username, username) == 0) {
        if (users[ii].checksum == checksum) {
          userId = users[ii].userId;
        }
        break;
      }
    }

    if (userId == NO_USER_ID) {
      fputs("Login incorrect\n", stderr);
    }
  }

  username = stringDestroy(username);
  password = stringDestroy(password);

  if (schedulerSetProcessUser(userId) != 0) {
    fputs("WARNING:  "
      "Could not set owner of current process to authenticated user.\n",
      stderr);
  }

  return;
}

/// @var users
///
/// @brief The array of user information to simulate a user database.
User users[] = {
  {
    .userId   = 0,
    .username = "root",
    .checksum = 1356, // rootroot
  },
  {
    .userId   = 1000,
    .username = "user1",
    .checksum = 1488, // user1user1
  },
  {
    .userId   = 1001,
    .username = "user2",
    .checksum = 1491, // user2user2
  },
};

/// @var NUM_USERS
///
/// @brief The number of users in the users array.
const int NUM_USERS = sizeof(users) / sizeof(users[0]);
