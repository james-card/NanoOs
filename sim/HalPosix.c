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
#include <termios.h>

#include "HalPosix.h"

/// @var serialPorts
///
/// @brief Array of serial ports on the system.  Index 0 is the main port,
/// which is the USB serial port.
static FILE **serialPorts[2] = {
  &stdout,
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
    return 0;
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
  //// newFlags.c_cc[VMIN] = 1;
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

#define OVERLAY_SIZE          8192
#define OVERLAY_ADDRESS 0x20001000

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
};

const Hal* halPosixInit(void) {
  int topOfStack = 0;
  fprintf(stderr, "Top of stack        = %p\n", (void*) &topOfStack);
  
  // Simulate having a total of 64 KB available for dynamic memory.
  posixHal.bottomOfStack
    = (void*) (((uintptr_t) &topOfStack) - ((uintptr_t) 65536));
  fprintf(stderr, "Bottom of stack     = %p\n", (void*) posixHal.bottomOfStack);
  
  posixHal.overlayMap = (NanoOsOverlayMap*) mmap((void*) OVERLAY_ADDRESS,
    OVERLAY_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
    -1, 0);
  if (posixHal.overlayMap == MAP_FAILED) {
    fprintf(stderr, "mmap failed with error: %s\n", strerror(errno));
    return NULL;
  }
  
  fprintf(stderr, "posixHal.overlayMap = %p\n", (void*) posixHal.overlayMap);
  fprintf(stderr, "\n");
  
  return &posixHal;
}

