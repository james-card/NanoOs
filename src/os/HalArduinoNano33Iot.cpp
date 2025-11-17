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

#include <Arduino.h>

#include "HalArduinoNano33Iot.h"

HardwareSerial *serialPorts[2] = {
  &Serial,
  &Serial1,
};

int arduinoNano33IotGetNumSerialPorts(void) {
  return sizeof(serialPorts) / sizeof(serialPorts[0]);
}

int arduinoNano33IotInitializeSerialPort(int port, int baud) {
  serialPorts[port]->begin(baud);
  // wait for serial port to connect.
  while (!(*serialPorts[port]));
  return 0;
}

static Hal arduinoNano33IotHal = {
  // Overlay definitions.
  .overlayMap = (NanoOsOverlayMap*) 0x20001800,
  .overlaySize = 8192,
  
  // Serial port functionality.
  .getNumSerialPorts = arduinoNano33IotGetNumSerialPorts,
  .initializeSerialPort = arduinoNano33IotInitializeSerialPort,
};

const Hal* halArduinoNano33IotInit(void) {
  return &arduinoNano33IotHal;
}
