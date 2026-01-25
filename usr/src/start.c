#include "NanoOsUser.h"

int main(int argc, char **argv);

char **environ = NULL;

void* _start(void *args) {
  MainArgs *mainArgs = (MainArgs*) args;
  environ = overlayMap.header.env;
  return (void*) ((intptr_t) main(mainArgs->argc, mainArgs->argv));
}

