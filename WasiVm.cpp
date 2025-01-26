#include "VirtualMemory.h"

typedef struct WasiVm {
  VirtualMemoryState codeSegment;
  VirtualMemoryState linearMemory;
  VirtualMemoryState globalStack;
  VirtualMemoryState callStack;
  VirtualMemoryState globalStorage;
  VirtualMemoryState tableSpace;
} WasiVm;

int wasiMain(int argc, char **argv) {
  int returnValue = 0;
  WasiVm wasiVm = {};
  char tempFilename[13];

  if (virtualMemoryInit(&wasiVm.codeSegment, argv[0]) != 0) {
    returnValue = -1;
    goto exit;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm.linearMemory, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupCodeSegment;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm.globalStack, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupLinearMemory;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm.callStack, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupGlobalStack;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm.globalStorage, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupCallStack;
  }

  sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
  if (virtualMemoryInit(&wasiVm.tableSpace, tempFilename) != 0) {
    returnValue = -1;
    goto cleanupGlobalStorage;
  }

cleanupTableSpace:
  virtualMemoryCleanup(&wasiVm.tableSpace, true);

cleanupGlobalStorage:
  virtualMemoryCleanup(&wasiVm.globalStorage, true);

cleanupCallStack:
  virtualMemoryCleanup(&wasiVm.callStack, true);

cleanupGlobalStack:
  virtualMemoryCleanup(&wasiVm.globalStack, true);

cleanupLinearMemory:
  virtualMemoryCleanup(&wasiVm.linearMemory, true);

cleanupCodeSegment:
  virtualMemoryCleanup(&wasiVm.codeSegment, false);

exit:
  return returnValue;
}
