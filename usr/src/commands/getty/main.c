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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/// @fn int printHostname(void)
///
/// @brief Make the gethostname and return the hostname captured.
///
/// @return Returns 0 on success, -1 on failure.
int printHostname(void) {
  char *hostname = (char*) malloc(HOST_NAME_MAX + 1);
  if (hostname == NULL) {
    fprintf(stderr, "Unable to allocate memory for hostname.\n");
    return -1;
  }
  
  if (gethostname(hostname, HOST_NAME_MAX) != 0) {
    fprintf(stderr, "gethostname failed with status: \"%s\"\n",
      strerror(errno));
    free(hostname);
    return -1;
  }
  hostname[HOST_NAME_MAX] = '\0';
  
  fputs(hostname, stdout);
  
  free(hostname);
  return 0;
}

/// @fn int printEscape(char escapeChar)
///
/// @brief Translate a getty escape character into the corresponding
/// information and display it.
///
/// @param escapeChar The getty escape character.
///
/// @return Returns 0 on success, -1 on failure.
int printEscape(char escapeChar) {
  switch (escapeChar) {
    case 'h': {
      printHostname();
      break;
    }
    
    // We do *NOT* want a default case here.  If nothing matches, we need to
    // silently ignore it.  Printing an error message would mangle the output.
  }
  
  return 0;
}

/// @fn int showIssue(void)
///
/// @brief Display the contents of /etc/issue, replacing any escapes with the
/// appropriate information.
///
/// @return Returns 0 on success, -1 on failure.
int showIssue(void) {
  char *buffer = (char*) malloc(96);
  if (buffer == NULL) {
    fputs("ERROR! Could not allocate space for buffer in showIssue.\n", stderr);
    return -1;
  }
  
  FILE *issueFile = fopen("/etc/issue", "r");
  if (issueFile == NULL) {
    fputs("ERROR! Could not open \"/etc/issue\" in showIssue.\n", stderr);
    return -1;
  }
  
  if (fgets(buffer, 96, issueFile) != buffer) {
    fputs("ERROR! fgets did not read \"/etc/isue\"\n", stderr);
  }
  fclose(issueFile);
  
  char *nextPart = buffer;
  char *backslashAt = strchr(nextPart, '\\');
  while (backslashAt != NULL) {
    *backslashAt = '\0';
    fputs(nextPart, stdout);
    
    // Get to the escape character after the backslash.
    nextPart = backslashAt + 1;
    printEscape(*nextPart);
    
    // Skip over whatever the escape sequence was.
    nextPart = &nextPart[strcspn(nextPart, " \t\n")];
    backslashAt = strchr(nextPart, '\\');
  }
  fputs(nextPart, stdout);
  
  free(buffer);
  return 0;
}

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;
  
  if (showIssue() != 0) {
    // Error message, if any, was already displayed.
    return 1;
  }
  
  char *buffer = (char*) malloc(96);
  if (buffer == NULL) {
    fputs("ERROR! Could not allocate space for buffer in getty.\n", stderr);
    return 1;
  }
  char *input = NULL;
  
  while (input == NULL) {
    fputs("login: ", stdout);
    input = fgets(buffer, sizeof(buffer), stdin);
  }
  
  free(buffer);
  return 0;
}

