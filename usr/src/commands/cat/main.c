#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  char buffer[96];
  if (argc > 1) {
    const char *filename = argv[1];
    FILE *inputFile = fopen(filename, "r");
    if (inputFile == NULL) {
      fprintf(stderr, "ERROR: Could not open file \"%s\"\n", filename);
      return 1;
    }
    while (
      fread(buffer, 1, sizeof(buffer) - 1, inputFile) == sizeof(buffer) - 1
    ) {
      fputs(buffer, stdout);
    }
    fputs("\n", stdout);
  } else {
    // Read from stdin and echo the input back to the user until "EOF\n" is
    // received.
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

