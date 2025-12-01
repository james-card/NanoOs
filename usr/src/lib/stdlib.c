#include <stdlib.h>
#include <string.h>

char* getenv(const char *name) {
  char **env = overlayMap.header.env;
  if ((name == NULL) || (*name == '\0') || (env == NULL)) {
    return NULL;
  }

  size_t nameLen = strlen(name);
  char *value = NULL;
  for (int ii = 0; env[ii] != NULL; ii++) {
    if ((strncmp(env[ii], name, nameLen) == 0) && env[ii][nameLen] == '=') {
      value = &env[ii][nameLen + 1];
      break;
    }
  }

  return value;
}

