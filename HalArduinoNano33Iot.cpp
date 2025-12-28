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

int arduinoNano33IotSetNumSerialPorts(int numSerialPorts) {
  if (numSerialPorts > (sizeof(serialPorts) / sizeof(serialPorts[0]))) {
    return -ERANGE;
  } else if (numSerialPorts < -ELAST) {
    return -EINVAL;
  }
  
  _numSerialPorts = numSerialPorts;
  
  return 0;
}

int arduinoNano33IotInitSerialPort(int port, int baud) {
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
  Tc *tc;
  IRQn_Type irqType;
  bool initialized;
  void (*callback)(void);
  bool active;
  uint32_t microseconds;
  int64_t startTime;
} HardwareTimer;

static HardwareTimer hardwareTimers[] = {
  {
    .tc = TC4,
    .irqType = TC4_IRQn,
    .initialized = false,
    .callback = NULL,
    .active = false,
    .microseconds = 0,
    .startTime = 0,
  },
  {
    .tc = TC5,
    .irqType = TC5_IRQn,
    .initialized = false,
    .callback = NULL,
    .active = false,
    .microseconds = 0,
    .startTime = 0,
  },
};

static int _numTimers
  = sizeof(hardwareTimers) / sizeof(hardwareTimers[0]);

int arduinoNano33IotGetNumTimers(void) {
  return _numTimers;
}

int arduinoNano33IotSetNumTimers(int numTimers) {
  if (numTimers > (sizeof(hardwareTimers) / sizeof(hardwareTimers[0]))) {
    return -ERANGE;
  } else if (numTimers < -ELAST) {
    return -EINVAL;
  }
  
  _numTimers = numTimers;
  
  return 0;
}

int arduinoNano33IotInitTimer(int timer) {
  if (timer >= _numTimers) {
    return -ERANGE;
  }
  
  HardwareTimer *hwTimer = &hardwareTimers[timer];
  
  // Enable GCLK for the TC timer (48MHz)
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN |
                      GCLK_CLKCTRL_GEN_GCLK0 |
                      GCLK_CLKCTRL_ID_TC4_TC5;
  while (GCLK->STATUS.bit.SYNCBUSY);

  // Reset the TC timer
  hwTimer->tc->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
  while (hwTimer->tc->COUNT16.STATUS.bit.SYNCBUSY);

  // Configure the TC timer in one-shot mode
  hwTimer->tc->COUNT16.CTRLA.reg
    = TC_CTRLA_MODE_COUNT16        // 16-bit counter
    | TC_CTRLA_WAVEGEN_MFRQ        // Match frequency mode
    | TC_CTRLA_PRESCALER_DIV1;     // No prescaling (48MHz)
  
  while (hwTimer->tc->COUNT16.STATUS.bit.SYNCBUSY);

  // Enable one-shot mode via CTRLBSET
  hwTimer->tc->COUNT16.CTRLBSET.reg = TC_CTRLBSET_ONESHOT;
  while (hwTimer->tc->COUNT16.STATUS.bit.SYNCBUSY);

  // Enable compare match interrupt
  hwTimer->tc->COUNT16.INTENSET.reg = TC_INTENSET_MC0;
  
  // Enable the TC timer interrupt in NVIC
  NVIC_SetPriority(hwTimer->irqType, 0);
  NVIC_EnableIRQ(hwTimer->irqType);
  
  return 0;
}

int arduinoNano33IotConfigTimer(int timer,
    uint32_t microseconds, void (*callback)(void)
) {
  if (timer >= _numTimers) {
    return -ERANGE;
  }
  
  // Cancel any existing timer
  int arduinoNano33IotCancelTimer(int);
  arduinoNano33IotCancelTimer(timer);
  
  // Make sure we don't overflow
  if (microseconds > 89478485) {
    microseconds = 89478485; // 0xffffffff / 48
  }
  
  // Calculate ticks (48 ticks per microsecond)
  uint32_t ticks = microseconds * 48;
  
  // Check if we need prescaling for longer delays
  uint16_t prescaler = TC_CTRLA_PRESCALER_DIV1;
  uint8_t divider = 1;
  
  if (ticks > 65535) {
    // Use DIV8 for up to ~10.9ms
    prescaler = TC_CTRLA_PRESCALER_DIV8;
    divider = 8;
    ticks = (microseconds * 48) / 8;
    
    if (ticks > 65535) {
      // Use DIV64 for up to ~87ms
      prescaler = TC_CTRLA_PRESCALER_DIV64;
      divider = 64;
      ticks = (microseconds * 48) / 64;
      
      if (ticks > 65535) {
        // Use DIV256 for up to ~349ms
        prescaler = TC_CTRLA_PRESCALER_DIV256;
        divider = 256;
        ticks = (microseconds * 48) / 256;
        
        if (ticks > 65535) {
          ticks = 65535; // Clamp to max
        }
      }
    }
  }
  
  HardwareTimer *hwTimer = &hardwareTimers[timer];
  hwTimer->callback = callback;
  hwTimer->active = true;
  
  // Disable timer
  hwTimer->tc->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;
  while (hwTimer->tc->COUNT16.STATUS.bit.SYNCBUSY);
  
  // Update prescaler
  hwTimer->tc->COUNT16.CTRLA.bit.PRESCALER = prescaler;
  while (hwTimer->tc->COUNT16.STATUS.bit.SYNCBUSY);
  
  // Set counter value (counts down to 0)
  hwTimer->tc->COUNT16.COUNT.reg = ticks;
  while (hwTimer->tc->COUNT16.STATUS.bit.SYNCBUSY);
  
  // Clear any pending interrupts
  hwTimer->tc->COUNT16.INTFLAG.reg = TC_INTFLAG_OVF;
  
  // Enable timer
  hwTimer->tc->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
  while (hwTimer->tc->COUNT16.STATUS.bit.SYNCBUSY);
  
  return 0;
}

bool arduinoNano33IotIsTimerActive(int timer) {
  if (timer >= _numTimers) {
    return false;
  }
  
  return hardwareTimers[timer].active;
}

int arduinoNano33IotCancelTimer(int timer) {
  if (timer >= _numTimers) {
    return -ERANGE;
  }
  
  HardwareTimer *hwTimer = &hardwareTimers[timer];
  if (!hwTimer->active) {
    // Not an error but nothing to do.
    return 0;
  }
  
  // Disable timer
  hwTimer->tc->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;
  while (hwTimer->tc->COUNT16.STATUS.bit.SYNCBUSY);
  
  // Clear interrupt flag
  hwTimer->tc->COUNT16.INTFLAG.reg = TC_INTFLAG_OVF;
  
  hwTimer->active = false;
  hwTimer->callback = nullptr;
  
  return 0;
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
  .setNumSerialPorts = arduinoNano33IotSetNumSerialPorts,
  .initSerialPort = arduinoNano33IotInitSerialPort,
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
  .getNumTimers = arduinoNano33IotGetNumTimers,
  .setNumTimers = arduinoNano33IotSetNumTimers,
  .initTimer = arduinoNano33IotInitTimer,
  .configTimer = arduinoNano33IotConfigTimer,
  .isTimerActive = arduinoNano33IotIsTimerActive,
  .cancelTimer = arduinoNano33IotCancelTimer,
};

const Hal* halArduinoNano33IotInit(void) {
  return &arduinoNano33IotHal;
}

#endif // __arm__

