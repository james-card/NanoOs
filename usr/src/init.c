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
  struct timespec now;
  struct timespec request;
  int64_t futureTime;

  fputs("\n\nStarting init...\n", stdout);

  if (sizeof(uint32_t) == sizeof(uint64_t)) {
    fputs("WARNING:  sizeof(uint32_t) == sizeof(uint64_t)\n", stderr);
  } else if ((sizeof(uint32_t) == 4) && (sizeof(uint64_t) == 8)) {
    fputs("Sizes of uint32_t and uint64_t are correct\n", stdout);
  } else {
    fputs("ERROR:  Unknown configuraiton of uint32_t and uint64_t!\n", stderr);
  }

  for (int ii = 0; ii < 2; ii++) {
    fputs("Hello, world!\n", stdout);
    timespec_get(&now, TIME_UTC);
    futureTime = (now.tv_sec * 1000000000LL) + ((int64_t) now.tv_nsec);
    futureTime += 1000000000LL;
    request.tv_sec = futureTime / 1000000000LL;
    request.tv_nsec = futureTime % 1000000000LL;
    nanosleep(&request, NULL);
    //// fputs("login: ", stdout);
    //// if (fgets(username, sizeof(username), stdin) != username) {
    ////   fputs("Error reading username.\n", stderr);
    ////   continue;
    //// }

    //// fputs("Password: ", stdout);
    //// if (fgets(password, sizeof(password), stdin) != password) {
    ////   fputs("Error reading password.\n", stderr);
    ////   continue;
    //// }

    //// if (strcmp(username, password) == 0) {
    ////   fputs("Login success!\n", stdout);
    //// } else {
    ////   fputs("Login failure!\n", stderr);
    //// }

    //// fputs("\n", stdout);
  }

  fputs("Exiting init.\n", stdout);
  return 1;
}

