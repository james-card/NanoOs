#include "VirtualMemory.h"

int wasiMain(int argc, char **argv) {
  int returnValue = 0;
  VirtualMemoryState codeSegment = {}, linearMemory = {}, globalStack = {},
    callStack = {}, globalStorage = {}, tableSpace = {};
  char tempFilename[13];

  if (virtualMemoryInit(&codeSegment, argv[0]) != 0) {
    returnValue = -1;
    goto exit;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&linearMemory, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupCodeSegment;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&globalStack, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupLinearMemory;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&callStack, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupGlobalStack;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&globalStorage, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupCallStack;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&tableSpace, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupGlobalStorage;
  }

cleanupTableSpace:
  virtualMemoryCleanup(&tableSpace, true);

cleanupGlobalStorage:
  virtualMemoryCleanup(&globalStorage, true);

cleanupCallStack:
  virtualMemoryCleanup(&callStack, true);

cleanupGlobalStack:
  virtualMemoryCleanup(&globalStack, true);

cleanupLinearMemory:
  virtualMemoryCleanup(&linearMemory, true);

cleanupCodeSegment:
  virtualMemoryCleanup(&codeSegment, false);

exit:
  return returnValue;
}
