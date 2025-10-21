#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;

  char buffer[96];
  char *input = NULL;

  while (input == NULL) {
    fputs("login: ", stdout);
    input = fgets(buffer, sizeof(buffer), stdin);
  }

  return 0;
}

