#include <stdio.h>
#include <stdint.h>

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;
  printf("Hello, world!\n");
  
  return 0;
}

void* _start(void *args) {
  MainArgs *mainArgs = (MainArgs*) args;
  return (void*) ((intptr_t) main(mainArgs->argc, mainArgs->argv));
}

// These are the overlay functions exported.
NanoOsOverlayExport exports[] = {
  {"_start", _start},
};

// This needs to be the first thing in the overlay.
__attribute__((section(".overlay_header")))
NanoOsOverlayMap overlayMap = {
  .header = {
    .magic = NANO_OS_OVERLAY_MAGIC,
    .version = (0 << 24) | (0 << 16) | (1 << 8) | (0 << 0),
    .stdCApi = NULL,
    .callOverlayFunction = NULL,
    .numExports = sizeof(exports) / sizeof(exports[0]),
  },
  .exports = exports,
};

