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

/// @file HalPosix.cpp
///
/// @brief HAL implementation for a Posix simulator.

// Standard C includes from the compiler
#undef errno
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <sys/mman.h>
#include </usr/include/time.h>
#include <termios.h>
#include <unistd.h>

#include "HalPosix.h"
#include "SdCardPosix.h"
#include "kernel/ExFatProcess.h"
#include "kernel/NanoOs.h"
#include "kernel/Processes.h"

/// @def OVERLAY_BASE_ADDRESS
///
/// @brief This is the base address that we will use in our mmap call.  The
/// address has to be page aligned on Linux.  The address below should work
/// fine unless the host is using 1 GB pages.
#define OVERLAY_BASE_ADDRESS 0x20000000

/// @def OVERLAY_OFFSET
///
/// @brief This is the offset into the allocated and mapped memory that the
/// overlays will actually be loaded into.
#define OVERLAY_OFFSET           0x1400

/// @def OVERLAY_SIZE
///
/// @brief This is the size of the overlay that's permitted by the real
/// hardware.
#define OVERLAY_SIZE               16384

/// @var serialPorts
///
/// @brief Array of serial ports on the system.  Index 0 is the main port,
/// which is the USB serial port.
static FILE **serialPorts[] = {
  &stderr,
};

/// @var numSerialPorts
///
/// @brief The number of serial ports we support on the Arduino Nano 33 IoT.
static const int numSerialPorts = sizeof(serialPorts) / sizeof(serialPorts[0]);

int posixGetNumSerialPorts(void) {
  return numSerialPorts;
}

int posixInitializeSerialPort(int port, int baud) {
  (void) baud;
  
  if (port != 0) {
    return -ERANGE;
  }
  
  // We don't actually need to do anything to stdout or stderr, but we do need
  // to configure stdin to be non-blocking.
  if (fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK) != 0) {
    return -errno;
  }
  
  // We manage all the prints to screen ourselves, so disable stdin echoing
  // as well.
  struct termios oldFlags = {0};
  struct termios newFlags = {0};
  
  // Get the current console flags.
  int stdinFileno = fileno(stdin);
  if (tcgetattr(stdinFileno, &oldFlags) != 0) {
    fprintf(stderr, "Could not get current attributes for console.\n");
    return -errno;
  }

  // Disable echo to the console.
  newFlags = oldFlags;
  newFlags.c_lflag |= ECHONL;
  newFlags.c_lflag &= ~(ECHO | ICANON);
  if (tcsetattr(stdinFileno, TCSANOW, &newFlags) != 0) {
    fprintf(stderr, "Could not set new attributes for console.\n");
    return -errno;
  }
  
  return 0;
}

int posixPollSerialPort(int port) {
  int serialData = -1;
  
  // While we'll support two outputs, we will only support one input to keep
  // things simple in the simulator.
  if (port == 0) {
    serialData = getchar();
    if (serialData == EOF) {
      serialData = -1;
    }
  }
  
  return serialData;
}

ssize_t posixWriteSerialPort(int port,
  const uint8_t *data, ssize_t length
) {
  ssize_t numBytesWritten = -ERANGE;
  
  if ((port >= 0) && (port < numSerialPorts) && (length >= 0)) {
    numBytesWritten = fwrite(data, 1, length, *serialPorts[port]);
  }
  
  return numBytesWritten;
}

int posixGetNumDios(void) {
  return -ENOSYS;
}

int posixConfigureDio(int dio, bool output) {
  (void) dio;
  (void) output;
  
  return -ENOSYS;
}

int posixWriteDio(int dio, bool high) {
  (void) dio;
  (void) high;
  
  return -ENOSYS;
}

int posixInitSpiDevice(int spi,
  uint8_t cs, uint8_t sck, uint8_t copi, uint8_t cipo
) {
  (void) spi;
  (void) cs;
  (void) sck;
  (void) copi;
  (void) cipo;
  
  return -ENOSYS;
}

int posixStartSpiTransfer(int spi) {
  (void) spi;
  
  return -ENOSYS;
}

int posixEndSpiTransfer(int spi) {
  (void) spi;
  
  return -ENOSYS;
}

int posixSpiTransfer8(int spi, uint8_t data) {
  (void) spi;
  (void) data;
  
  return -ENOSYS;
}

int posixSetSystemTime(struct timespec *now) {
  (void) now;
  
  return 0;
}

// posixGetElapsedNanoseconds is used as the base implementation, so declare
// its prototype here.
int64_t posixGetElapsedNanoseconds(int64_t startTime);

int64_t posixGetElapsedMilliseconds(int64_t startTime) {
  return posixGetElapsedNanoseconds(
    startTime * ((int64_t) 1000000)) / ((int64_t) 1000000);
}

int64_t posixGetElapsedMicroseconds(int64_t startTime) {
  return posixGetElapsedNanoseconds(
    startTime * ((int64_t) 1000)) / ((int64_t) 1000);
}

int64_t posixGetElapsedNanoseconds(int64_t startTime) {
  #include <time.h>
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  return ((((int64_t) spec.tv_sec) * ((int64_t) 1000000000))
    + ((int64_t) spec.tv_nsec)) - startTime;
}

static jmp_buf _resetBuffer;

int posixReset(void) {
  // Unmap the overlay so that we can map it again when we reset.
  long pageSize = sysconf(_SC_PAGESIZE);
  size_t overlayBaseSize
    = ((size_t) (OVERLAY_OFFSET + OVERLAY_SIZE + (pageSize - 1)))
    & ~((size_t) (pageSize - 1));
  
  if (munmap((void*) OVERLAY_BASE_ADDRESS, overlayBaseSize) < 0) {
    fprintf(stderr, "ERROR: munmap returned: %s\n", strerror(errno));
    fprintf(stderr, "Exiting.\n");
    exit(1);
  }
  longjmp(_resetBuffer, 1);
  return 0;
}

int posixShutdown(void) {
  exit(0);
  return 0;
}

/// @var _sdCardDevicePath
///
/// @brief Path to the device node to connect to for the SdCardSim process.
static const char *_sdCardDevicePath = NULL;

int posixInitRootStorage(SchedulerState *schedulerState) {
  ProcessDescriptor *allProcesses = schedulerState->allProcesses;
  
  // Create the SD card process.
  ProcessHandle processHandle = 0;
  if (processCreate(&processHandle, runSdCardPosix, (void*) _sdCardDevicePath)
    != processSuccess
  ) {
    fputs("Could not start SD card process.\n", stderr);
  }
  printDebugString("Started SD card process.\n");
  processSetId(processHandle, NANO_OS_SD_CARD_PROCESS_ID);
  allProcesses[NANO_OS_SD_CARD_PROCESS_ID].processId
    = NANO_OS_SD_CARD_PROCESS_ID;
  allProcesses[NANO_OS_SD_CARD_PROCESS_ID].processHandle = processHandle;
  allProcesses[NANO_OS_SD_CARD_PROCESS_ID].name = "SD card";
  allProcesses[NANO_OS_SD_CARD_PROCESS_ID].userId = ROOT_USER_ID;
  BlockStorageDevice *sdDevice = (BlockStorageDevice*) coroutineResume(
    allProcesses[NANO_OS_SD_CARD_PROCESS_ID].processHandle, NULL);
  sdDevice->partitionNumber = 1;
  printDebugString("Configured SD card process.\n");
  
  // Create the filesystem process.
  processHandle = 0;
  if (processCreate(&processHandle, runExFatFilesystem, sdDevice)
    != processSuccess
  ) {
    fputs("Could not start filesystem process.\n", stderr);
  }
  processSetId(processHandle, NANO_OS_FILESYSTEM_PROCESS_ID);
  allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID].processId
    = NANO_OS_FILESYSTEM_PROCESS_ID;
  allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID].processHandle = processHandle;
  allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID].name = "filesystem";
  allProcesses[NANO_OS_FILESYSTEM_PROCESS_ID].userId = ROOT_USER_ID;
  printDebugString("Created filesystem process.\n");
  
  return 0;
}

/// @var posixHal
///
/// @brief The implementation of the Hal interface for the Arduino Nano 33 Iot.
static Hal posixHal = {
  // Memory definitions.
  .bottomOfStack = 0,
  
  // Overlay definitions.
  .overlayMap = NULL,
  .overlaySize = OVERLAY_SIZE,
  
  // Serial port functionality.
  .getNumSerialPorts = posixGetNumSerialPorts,
  .initializeSerialPort = posixInitializeSerialPort,
  .pollSerialPort = posixPollSerialPort,
  .writeSerialPort = posixWriteSerialPort,
  
  // Digital IO pin functionality.
  .getNumDios = posixGetNumDios,
  .configureDio = posixConfigureDio,
  .writeDio = posixWriteDio,
  
  // SPI functionality.
  .initSpiDevice = posixInitSpiDevice,
  .startSpiTransfer = posixStartSpiTransfer,
  .endSpiTransfer = posixEndSpiTransfer,
  .spiTransfer8 = posixSpiTransfer8,
  
  // System time functionality.
  .setSystemTime = posixSetSystemTime,
  .getElapsedMilliseconds = posixGetElapsedMilliseconds,
  .getElapsedMicroseconds = posixGetElapsedMicroseconds,
  .getElapsedNanoseconds = posixGetElapsedNanoseconds,
  
  // Hardware reset and shutdown.
  .reset = posixReset,
  .shutdown = posixShutdown,
  
  // Root storage configuration.
  .initRootStorage = posixInitRootStorage,
};

const Hal* halPosixInit(jmp_buf resetBuffer, const char *sdCardDevicePath) {
  fprintf(stdout, "Setting _sdCardDevicePath.\n");
  fflush(stdout);
  _sdCardDevicePath = sdCardDevicePath;
  fprintf(stdout, "_sdCardDevicePath set.\n");
  fflush(stdout);

  // Saver our reset context for later.
  memcpy(_resetBuffer, resetBuffer, sizeof(jmp_buf));
  fprintf(stdout, "resetBuffer copied.\n");
  fflush(stdout);
  
  int topOfStack = 0;
  fprintf(stderr, "Top of stack        = %p\n", (void*) &topOfStack);
  
  // Simulate having a total of 64 KB available for dynamic memory.
  posixHal.bottomOfStack
    = (void*) (((uintptr_t) &topOfStack) - ((uintptr_t) 65536));
  fprintf(stderr, "Bottom of stack     = %p\n", (void*) posixHal.bottomOfStack);
  
  // The size used in the mmap call has to be large enough to accommodate the
  // size used for the overlay, plus the offset into the overlay.  It also has
  // to be page aligned.  Do the appropriate math to get us what we need
  // without wasting too much space.
  long pageSize = sysconf(_SC_PAGESIZE);
  size_t overlayBaseSize
    = ((size_t) (OVERLAY_OFFSET + OVERLAY_SIZE + (pageSize - 1)))
    & ~((size_t) (pageSize - 1));
  
  posixHal.overlayMap = (NanoOsOverlayMap*) mmap((void*) OVERLAY_BASE_ADDRESS,
    overlayBaseSize, PROT_READ | PROT_WRITE | PROT_EXEC,
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
    -1, 0);
  if (posixHal.overlayMap == MAP_FAILED) {
    fprintf(stderr, "mmap failed with error: %s\n", strerror(errno));
    return NULL;
  }
  
  // The address that the code is built around for both the Cortex-M0 and the
  // simulation code is OVERLAY_OFFSET bytes into the map we just made.
  posixHal.overlayMap = (void*) (OVERLAY_BASE_ADDRESS + OVERLAY_OFFSET);
  
  fprintf(stderr, "posixHal.overlayMap = %p\n", (void*) posixHal.overlayMap);
  fprintf(stderr, "\n");
  
  return &posixHal;
}

