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

/// @file HalArduinoNano33Iot.cpp
///
/// @brief HAL implementation for an Arduino Nano 33 IoT

// Base Arduino definitions
#include <Arduino.h>

// Basic SPI communication
#include <SPI.h>

// Standard C includes from the compiler
#include <limits.h>

#include "HalArduinoNano33Iot.h"
// Deliberately *NOT* including MemoryManager.h here.  The HAL has to be
// operational prior to the memory manager and really should be completely
// independent of it.
#include "src/kernel/ExFatProcess.h"
#include "src/kernel/NanoOs.h"
#include "src/kernel/Processes.h"
#include "src/kernel/SdCardSpi.h"
#include "src/user/NanoOsErrno.h"
#include "src/user/NanoOsStdio.h"

/// @def SD_CARD_PIN_CHIP_SELECT
///
/// @brief Pin to use for the MicroSD card reader's SPI chip select line.
#define SD_CARD_PIN_CHIP_SELECT 4

// The fact that we've included Arduino.h in this file means that the memory
// management functions from its library are available in this file.  That's a
// problem.  (a) We can't allow dynamic memory at the HAL level and (b) if we
// were to allocate memory from Arduino's memory manager, we'd run the risk
// of corrupting something elsewhere in memory.  Just in case we ever forget
// this and try to use memory management functions in the future, define them
// all to MEMORY_ERROR so that the build will fail.
#define malloc  MEMORY_ERROR
#define calloc  MEMORY_ERROR
#define realloc MEMORY_ERROR
#define freee   MEMORY_ERROR

#if defined(__arm__)

/// @var serialPorts
///
/// @brief Array of serial ports on the system.  Index 0 is the main port,
/// which is the USB serial port.
static HardwareSerial *serialPorts[] = {
  &Serial,
  &Serial1,
};

/// @var _numSerialPorts
///
/// @brief The number of serial ports we support on the Arduino Nano 33 IoT.
static int _numSerialPorts = sizeof(serialPorts) / sizeof(serialPorts[0]);

int arduinoNano33IotGetNumSerialPorts(void) {
  return _numSerialPorts;
}

int arduinoNanoEverySetNumSerialPorts(int numSerialPorts) {
  if (numSerialPorts > (sizeof(serialPorts) / sizeof(serialPorts[0]))) {
    return -ERANGE;
  } else if (numSerialPorts <= -EEND) {
    return -ERANGE;
  }
  
  _numSerialPorts = numSerialPorts;
  
  return 0;
}

int arduinoNano33IotInitializeSerialPort(int port, int baud) {
  int returnValue = -ERANGE;
  
  if ((port >= 0) && (port < _numSerialPorts)) {
    serialPorts[port]->begin(baud);
    // wait for serial port to connect.
    while (!(*serialPorts[port]));
    returnValue = 0;
  }
  
  return returnValue;
}

int arduinoNano33IotPollSerialPort(int port) {
  int serialData = -ERANGE;
  
  if ((port >= 0) && (port < _numSerialPorts)) {
    serialData = serialPorts[port]->read();
  }
  
  return serialData;
}

ssize_t arduinoNano33IotWriteSerialPort(int port,
  const uint8_t *data, ssize_t length
) {
  ssize_t numBytesWritten = -ERANGE;
  
  if ((port >= 0) && (port < _numSerialPorts) && (length >= 0)) {
    numBytesWritten = serialPorts[port]->write(data, length);
  }
  
  return numBytesWritten;
}

int arduinoNano33IotGetNumDios(void) {
  return NUM_DIO_PINS;
}

int arduinoNano33IotConfigureDio(int dio, bool output) {
  int returnValue = -ERANGE;
  
  if ((dio >= DIO_START) && (dio < NUM_DIO_PINS)) {
    uint8_t modes[2] = { INPUT, OUTPUT };
    pinMode(dio, modes[output]);
    
    returnValue = 0;
  }
  
  return returnValue;
}

int arduinoNano33IotWriteDio(int dio, bool high) {
  int returnValue = -ERANGE;
  
  if ((dio >= DIO_START) && (dio < NUM_DIO_PINS)) {
    uint8_t levels[2] = { LOW, HIGH };
    digitalWrite(dio, levels[high]);
    
    returnValue = 0;
  }
  
  return returnValue;
}

/// @var globalSpiConfigured
///
/// @brief Whether or not the Arduino's SPI interface has already been
/// configured.
static bool globalSpiConfigured = false;

/// @var arduinoSpiDevices
///
/// @brief Array of structures that will hold the information about SPI
/// connections.
///
/// @details
/// On the Arduino Nano 33 IoT, 5 DIO pins are reserved:
/// - UART RX
/// - UART TX
/// - SPI SCK
/// - SPI COPI
/// - SPI CIPO
/// So, the maximum number of devcies we can support is the number of DIO pins
/// minus 5.
static struct ArduinoNano33IotSpi {
  bool    configured;         // Will default to false
  uint8_t chipSelect;
  bool    transferInProgress; // Will default to false
} arduinoSpiDevices[NUM_DIO_PINS - 5] = {};

/// @var numArduinoSpis
///
/// @brief The number of devices we support in the arduinoSpiDevices array.
static const int numArduinoSpis
  = sizeof(arduinoSpiDevices) / sizeof(arduinoSpiDevices[0]);

int arduinoNano33IotInitSpiDevice(int spi,
  uint8_t cs, uint8_t sck, uint8_t copi, uint8_t cipo
) {
  if ((spi < 0) || (spi >= numArduinoSpis)) {
    // Outside the limit of the devices we support.
    return -ENODEV;
  } else if ((cs < DIO_START) || (cs >= NUM_DIO_PINS)) {
    // No such DIO pin to configure.
    return -ERANGE;
  } else if (
       (cs   == SPI_SCK_DIO)
    || (cs   == SPI_COPI_DIO)
    || (cs   == SPI_CIPO_DIO)
    || (sck  != SPI_SCK_DIO)
    || (copi != SPI_COPI_DIO)
    || (cipo != SPI_CIPO_DIO)
  ) {
    return -EINVAL;
  } else if (arduinoSpiDevices[spi].configured == true) {
    return -EBUSY;
  }
  
  if (globalSpiConfigured == false) {
    // Set up SPI at the default speed.
    SPI.begin();
    globalSpiConfigured = true;
  }
  
  // Configure the chip select DIO for output.
  arduinoNano33IotConfigureDio(cs, 1);
  // Deselect the chip select pin.
  arduinoNano33IotWriteDio(cs, 1);
  
  // Configure our internal metadata for the device.
  arduinoSpiDevices[spi].chipSelect = cs;
  arduinoSpiDevices[spi].configured = true;
  
  return 0;
}

int arduinoNano33IotStartSpiTransfer(int spi) {
  if ((spi < 0) || (spi >= numArduinoSpis)
    || (arduinoSpiDevices[spi].configured == false)
  ) {
    // Outside the limit of the devices we support.
    return -ENODEV;
  }
  
  // Select the chip select pin.
  arduinoNano33IotWriteDio(arduinoSpiDevices[spi].chipSelect, 0);
  arduinoSpiDevices[spi].transferInProgress = true;
  
  return 0;
}

int arduinoNano33IotEndSpiTransfer(int spi) {
  if ((spi < 0) || (spi >= numArduinoSpis)
    || (arduinoSpiDevices[spi].configured == false)
  ) {
    // Outside the limit of the devices we support.
    return -ENODEV;
  }
  
  // Deselect the chip select pin.
  arduinoNano33IotWriteDio(arduinoSpiDevices[spi].chipSelect, 1);
  for (int ii = 0; ii < 8; ii++) {
    SPI.transfer(0xFF); // 8 clock pulses
  }
  arduinoSpiDevices[spi].transferInProgress = false;
  
  return 0;
}

int arduinoNano33IotSpiTransfer8(int spi, uint8_t data) {
  if ((spi < 0) || (spi >= numArduinoSpis)
    || (arduinoSpiDevices[spi].configured == false)
  ) {
    // Outside the limit of the devices we support.
    return -ENODEV;
  } else if (!arduinoSpiDevices[spi].transferInProgress) {
    // The only error that arduinoNano33IotStartSpiTransfer can return is
    // ENODEV and we've already checked for that, so we don't need to check the
    // return value here.
    arduinoNano33IotStartSpiTransfer(spi);
  }
  
  return (int) SPI.transfer(data);
}

/// @var baseSystemTimeUs
///
/// @brief The time provided by the user or some other process as a baseline
/// time for the system.
static int64_t baseSystemTimeUs = 0;

int arduinoNano33IotSetSystemTime(struct timespec *now) {
  if (now == NULL) {
    return -EINVAL;
  }
  
  baseSystemTimeUs
    = (((int64_t) now->tv_sec) * ((int64_t) 1000000))
    + (((int64_t) now->tv_nsec) / ((int64_t) 1000));
  
  return 0;
}

int64_t arduinoNano33IotGetElapsedMicroseconds(int64_t startTime);

int64_t arduinoNano33IotGetElapsedMilliseconds(int64_t startTime) {
  return arduinoNano33IotGetElapsedMicroseconds(
    startTime * ((int64_t) 1000)) / ((int64_t) 1000);
}

int64_t arduinoNano33IotGetElapsedMicroseconds(int64_t startTime) {
  int64_t now = baseSystemTimeUs + micros();

  if (now < startTime) {
    return -1;
  }

  return now - startTime;
}

int64_t arduinoNano33IotGetElapsedNanoseconds(int64_t startTime) {
  return arduinoNano33IotGetElapsedMicroseconds(
    startTime / ((int64_t) 1000)) * ((int64_t) 1000);
}

int arduinoNano33IotReset(void) {
  NVIC_SystemReset();
  return 0;
}

int arduinoNano33IotShutdown(void) {
  // Configure for standby mode
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  
  // Set standby mode in Power Manager
  PM->SLEEP.reg = PM_SLEEP_IDLE_CPU;
  
  __DSB(); // Data Synchronization Barrier
  __WFI(); // Wait For Interrupt
  return 0;
}

int arduinoNano33IotInitRootStorage(SchedulerState *schedulerState) {
  ProcessDescriptor *allProcesses = schedulerState->allProcesses;
  
  // Create the SD card process.
  SdCardSpiArgs sdCardSpiArgs = {
    .spiCsDio = SD_CARD_PIN_CHIP_SELECT,
    .spiCopiDio = SPI_COPI_DIO,
    .spiCipoDio = SPI_CIPO_DIO,
    .spiSckDio = SPI_SCK_DIO,
  };

  ProcessHandle processHandle = 0;
  if (processCreate(&processHandle, runSdCardSpi, &sdCardSpiArgs)
    != processSuccess
  ) {
    printString("Could not start SD card process.\n");
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
    printString("Could not start filesystem process.\n");
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

typedef struct HardwareTimer {
  Tc *timer;
  void (*callback)(void);
  bool active;
  uint32_t microseconds;
  int64_t startTime;
} HardwareTimer;

static HardwareTimer hardwareTimers[] = {
  {
    .timer = TC4,
    .callback = NULL,
    .active = false,
    .microseconds = 0,
    .startTime = 0,
  },
  {
    .timer = TC5,
    .callback = NULL,
    .active = false,
    .microseconds = 0,
    .startTime = 0,
  },
};

int arduinoNano33IotGetNumHardwareTimers(void) {
  return sizeof(hardwareTimers) / sizeof(hardwareTimers[0]);
}
  
int arduinoNano33IotConfigTimer(int timerId,
    uint32_t microseconds, void (*callback)(void)
) {
  return -ENOTSUP;
}
  
bool arduinoNano33IotIsTimerActive(int timerId) {
  return false;
}
  
int arduinoNano33IotCancelTimer(int timerId) {
  return -ENOTSUP;
}

/// @var arduinoNano33IotHal
///
/// @brief The implementation of the Hal interface for the Arduino Nano 33 Iot.
static Hal arduinoNano33IotHal = {
  // Memory definitions.
  .bottomOfStack = (void*) (0x20001400 + 8192),
  
  // Overlay definitions.
  .overlayMap = (NanoOsOverlayMap*) 0x20001400,
  .overlaySize = 8192,
  
  // Serial port functionality.
  .getNumSerialPorts = arduinoNano33IotGetNumSerialPorts,
  .setNumSerialPorts = arduinoNanoEverySetNumSerialPorts,
  .initializeSerialPort = arduinoNano33IotInitializeSerialPort,
  .pollSerialPort = arduinoNano33IotPollSerialPort,
  .writeSerialPort = arduinoNano33IotWriteSerialPort,
  
  // Digital IO pin functionality.
  .getNumDios = arduinoNano33IotGetNumDios,
  .configureDio = arduinoNano33IotConfigureDio,
  .writeDio = arduinoNano33IotWriteDio,
  
  // SPI functionality.
  .initSpiDevice = arduinoNano33IotInitSpiDevice,
  .startSpiTransfer = arduinoNano33IotStartSpiTransfer,
  .endSpiTransfer = arduinoNano33IotEndSpiTransfer,
  .spiTransfer8 = arduinoNano33IotSpiTransfer8,
  
  // System time functionality.
  .setSystemTime = arduinoNano33IotSetSystemTime,
  .getElapsedMilliseconds = arduinoNano33IotGetElapsedMilliseconds,
  .getElapsedMicroseconds = arduinoNano33IotGetElapsedMicroseconds,
  .getElapsedNanoseconds = arduinoNano33IotGetElapsedNanoseconds,
  
  // Hardware reset and shutdown.
  .reset = arduinoNano33IotReset,
  .shutdown = arduinoNano33IotShutdown,
  
  // Root storage configuration.
  .initRootStorage = arduinoNano33IotInitRootStorage,
  
  // Hardware timers.
  .getNumHardwareTimers = arduinoNano33IotGetNumHardwareTimers,
  .configTimer = arduinoNano33IotConfigTimer,
  .isTimerActive = arduinoNano33IotIsTimerActive,
  .cancelTimer = arduinoNano33IotCancelTimer,
};

const Hal* halArduinoNano33IotInit(void) {
  return &arduinoNano33IotHal;
}

#endif // __arm__

