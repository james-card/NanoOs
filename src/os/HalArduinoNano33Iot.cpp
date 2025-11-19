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

#include "HalArduinoNano33Iot.h"
#include "../unix/NanoOsErrno.h"

/// @def NUM_DIGITAL_IO_PINS
///
/// @brief The number of digital IO pins on the board.  14 on an Arduino Nano.
#define NUM_DIGITAL_IO_PINS 14

/// @var serialPorts
///
/// @brief Array of serial ports on the system.  Index 0 is the main port,
/// which is the USB serial port.
static HardwareSerial *serialPorts[2] = {
  &Serial,
  &Serial1,
};
static const int numSerialPorts = sizeof(serialPorts) / sizeof(serialPorts[0]);

int arduinoNano33IotGetNumSerialPorts(void) {
  return numSerialPorts;
}

int arduinoNano33IotInitializeSerialPort(int port, int baud) {
  serialPorts[port]->begin(baud);
  // wait for serial port to connect.
  while (!(*serialPorts[port]));
  return 0;
}

int arduinoNano33IotPollSerialPort(int port) {
  int serialData = -ERANGE;
  
  if ((port >= 0) && (port < numSerialPorts)) {
    serialData = serialPorts[port]->read();
  }
  
  return serialData;
}

ssize_t arduinoNano33IotWriteSerialPort(int port,
  const uint8_t *data, ssize_t length
) {
  ssize_t numBytesWritten = -ERANGE;
  
  if ((port >= 0) && (port < numSerialPorts) && (length >= 0)) {
    numBytesWritten = serialPorts[port]->write(data, length);
  }
  
  return numBytesWritten;
}

int arduinoNano33IotGetNumDios(void) {
  return NUM_DIGITAL_IO_PINS;
}

int arduinoNano33IotConfigureDio(int dio, bool output) {
  int returnValue = -ERANGE;
  
  if ((dio > 0) && (dio < NUM_DIGITAL_IO_PINS)) {
    uint8_t modes[2] = { INPUT, OUTPUT };
    pinMode(dio, modes[output]);
    
    returnValue = 0;
  }
  
  return returnValue;
}

int arduinoNano33IotWriteDio(int dio, bool high) {
  int returnValue = -ERANGE;
  
  if ((dio > 0) && (dio < NUM_DIGITAL_IO_PINS)) {
    uint8_t levels[2] = { LOW, HIGH };
    digitalWrite(dio, levels[high]);
    
    returnValue = 0;
  }
  
  return returnValue;
}

static bool globalSpiConfigured = false;

static struct ArduinoNano33IotSpi {
  bool    configured;         // Will default to false
  uint8_t chipSelect;
  bool    transferInProgress; // Will default to false
} arduinoSpi[NUM_DIGITAL_IO_PINS] = {};
static const int numArduinoSpis = sizeof(arduinoSpi) / sizeof(arduinoSpi[0]);

int arduinoNano33IotInitSpi(int spi, uint8_t chipSelect) {
  if ((spi < 0) || (spi >= numArduinoSpis)) {
    // Outside the limit of the devices we support.
    return -ENODEV;
  } else if (chipSelect > NUM_DIGITAL_IO_PINS) {
    // No such DIO pin to configure.
    return -ERANGE;
  }
  
  if (globalSpiConfigured == false) {
    // Set up SPI at the default speed.
    SPI.begin();
    globalSpiConfigured = true;
  }
  
  // Configure the chip select DIO for output.
  arduinoNano33IotConfigureDio(chipSelect, 1);
  // Deselect the chip select pin.
  arduinoNano33IotWriteDio(chipSelect, 1);
  
  // Configure our internal metadata for the device.
  arduinoSpi[spi].chipSelect = chipSelect;
  arduinoSpi[spi].configured = true;
  
  return 0;
}

int arduinoNano33IotStartSpiTransfer(int spi) {
  if ((spi < 0) || (spi >= numArduinoSpis) || (!arduinoSpi[spi].configured)) {
    // Outside the limit of the devices we support.
    return -ENODEV;
  }
  
  // Select the chip select pin.
  arduinoNano33IotWriteDio(arduinoSpi[spi].chipSelect, 0);
  arduinoSpi[spi].transferInProgress = true;
  
  return 0;
}

int arduinoNano33IotEndSpiTransfer(int spi) {
  if ((spi < 0) || (spi >= numArduinoSpis) || (!arduinoSpi[spi].configured)) {
    // Outside the limit of the devices we support.
    return -ENODEV;
  }
  
  // Deselect the chip select pin.
  arduinoNano33IotWriteDio(arduinoSpi[spi].chipSelect, 1);
  for (int ii = 0; ii < 8; ii++) {
    SPI.transfer(0xFF); // 8 clock pulses
  }
  arduinoSpi[spi].transferInProgress = false;
  
  return 0;
}

int arduinoNano33IotSpiTransfer8(int spi, uint8_t data) {
  if ((spi < 0) || (spi >= numArduinoSpis) || (!arduinoSpi[spi].configured)) {
    // Outside the limit of the devices we support.
    return -ENODEV;
  } else if (!arduinoSpi[spi].transferInProgress) {
    // The only error that arduinoNano33IotStartSpiTransfer can return is
    // ENODEV and we've already checked for that, so we don't need to check the
    // return value here.
    arduinoNano33IotStartSpiTransfer(spi);
  }
  
  return (int) SPI.transfer(data);
}

static Hal arduinoNano33IotHal = {
  // Overlay definitions.
  .overlayMap = (NanoOsOverlayMap*) 0x20001800,
  .overlaySize = 8192,
  
  // Serial port functionality.
  .getNumSerialPorts = arduinoNano33IotGetNumSerialPorts,
  .initializeSerialPort = arduinoNano33IotInitializeSerialPort,
  .pollSerialPort = arduinoNano33IotPollSerialPort,
  .writeSerialPort = arduinoNano33IotWriteSerialPort,
  
  // Digital IO pin functionality.
  .getNumDios = arduinoNano33IotGetNumDios,
  .configureDio = arduinoNano33IotConfigureDio,
  .writeDio = arduinoNano33IotWriteDio,
  
  // SPI functionality.
  .initSpi = arduinoNano33IotInitSpi,
  .startSpiTransfer = arduinoNano33IotStartSpiTransfer,
  .endSpiTransfer = arduinoNano33IotEndSpiTransfer,
  .spiTransfer8 = arduinoNano33IotSpiTransfer8,
};

const Hal* halArduinoNano33IotInit(void) {
  return &arduinoNano33IotHal;
}
