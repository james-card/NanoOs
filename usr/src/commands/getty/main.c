#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;

  char buffer[96];
  char *input = NULL;
  char hostname[HOST_NAME_MAX + 1];
  if (gethostname(hostname, HOST_NAME_MAX) != 0) {
    //// fprintf(stderr, "gethostname failed with status: \"%s\"\n",
    ////   strerror(errno));
    strcpy(hostname, "localhost");
  }
  hostname[HOST_NAME_MAX] = '\0';

  while (input == NULL) {
    fputs("login: ", stdout);
    input = fgets(buffer, sizeof(buffer), stdin);
  }

  return 0;
}

