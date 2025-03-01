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

#include <time.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

/// @fn void disableEcho(void)
///
/// @brief Turn off echo of characters typed in the console.
///
/// @return This function returns no value.
void disableEcho(void) {
    struct termios term;
    
    // Get the current terminal settings
    tcgetattr(STDIN_FILENO, &term);
    
    // Turn off echo
    term.c_lflag &= ~ECHO;
    
    // Apply the new settings
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

/// @fn void enableEcho(void)
///
/// @brief Turn on echo of characters typed in the console.
///
/// @return This function returns no value.
void enableEcho(void) {
    struct termios term;
    
    // Get the current terminal settings
    tcgetattr(STDIN_FILENO, &term);
    
    // Turn on echo
    term.c_lflag |= ECHO;
    
    // Apply the new settings
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

/// @fn int main(int argc, char **argv)
///
/// @brief Entry point for the application.
///
/// @param argc Number of command line arguments provided on the command line.
/// @param argv Array of individual arguments from the command line.
///
/// @return Returns 0 on success.  Any other value is failure.
int main(int argc, char **argv) {
  (void) argc;
  (void) argv;

  char username[NANO_OS_MAX_READ_WRITE_LENGTH];
  char password[NANO_OS_MAX_READ_WRITE_LENGTH];

  fputs("\n\nStarting init...\n", stdout);

  while (1) {
    fputs("login: ", stdout);
    if (fgets(username, sizeof(username), stdin) != username) {
      fputs("Error reading username.\n", stderr);
      continue;
    }

    fputs("Password: ", stdout);
    disableEcho();
    if (fgets(password, sizeof(password), stdin) != password) {
      enableEcho();
      fputs("Error reading password.\n", stderr);
      continue;
    }
    enableEcho();

    if (strcmp(username, password) == 0) {
      fputs("Login success!\n", stdout);
    } else {
      fputs("Login failure!\n", stderr);
    }

    fputs("\n", stdout);
  }

  fputs("Exiting init.\n", stdout);
  return 0;
}

