#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc > 1) {
    // The usual case.  Print the arguments separated by one (1) space.
    for (int ii = 1; ii < argc; ii++) {
      fputs(argv[ii], stdout);
      if (ii < (argc - 1)) {
        fputs(" ", stdout);
      }
    }
    fputs("\n", stdout);
  } else {
    // Read from stdin and echo the input back to the user until "EOF\n" is
    // received.
    char buffer[96];
    while (fgets(buffer, sizeof(buffer), stdin)) {
      if (strcmp(buffer, "EOF\n") != 0) {
        fputs(buffer, stdout);
      } else {
        break;
      }
    }
  }

  return 0;
}

