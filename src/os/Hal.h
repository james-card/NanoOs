///////////////////////////////////////////////////////////////////////////////
///
/// @file              Hal.h
///
/// @brief             Definitions common to all hardware abstraction layer
///                    (HAL)implementations.
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
/// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///                                James Card
///                         http://www.jamescard.org
///
///////////////////////////////////////////////////////////////////////////////

#ifndef HAL_H
#define HAL_H

#include "NanoOsOverlay.h"


#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Hal {
  // Overlay definitions.
  
  /// @var overlayMap
  ///
  /// @brief Memory address where overlays will be loaded.
  NanoOsOverlayMap *overlayMap;
  
  /// @var overlaySize
  ///
  /// @brief The number of bytes available for the overlay at the address.
  uintptr_t overlaySize;
  
  // Serial port functionality.
  
  /// @fn int getNumSerialPorts(void);
  ///
  /// @brief Get the number of addressable and configurable serial ports on the
  /// system.
  ///
  /// @return Returns the number of serial ports on the system (which may be 0)
  /// on success, -errno on failure.
  int (*getNumSerialPorts)(void);
  
  /// @fn initializeSerialPort(int port, int baud)
  ///
  /// @brief Initialize a hardware serial port.
  ///
  /// @param port The zero-based index of the port to initialize.
  /// @param baud The desired baud rate of the port.
  ///
  /// @return Returns 0 on success, -errno on failure.
  int (*initializeSerialPort)(int port, int baud);
  
  /// @fn int pollSerialPort(int port)
  ///
  /// @brief Poll a serial port for a single byte of data.
  ///
  /// @param port The zero-based index of the port to read from.
  ///
  /// @return Returns the byte read, cast to an int, on success, -errno on
  /// failure.
  int (*pollSerialPort)(int port);
  
  /// @fn ssize_t writeSerialPort(int port, const uint8_t *data, ssize_t length)
  ///
  /// @brief Write data to a serial port.
  ///
  /// @param port The zero-based index of the port to read from.
  /// @param data A pointer to arbitrary bytes of data to write to the serial
  ///   port.
  /// @param length The number of bytes to write to the serial port from the
  ///   data pointer.
  ///
  /// @return Returns the number of bytes written on success, -errno on failure.
  ssize_t (*writeSerialPort)(int port, const uint8_t *data, ssize_t length);
  
  // Digital IO pin functionality.
  
  /// @fn int getNumDios(void)
  ///
  /// @brief Get the number of digial IO pins on the system.
  ///
  /// @return Returns the number of digital IO pins on success, -errno on
  /// failure.
  int (*getNumDios)(void);
} Hal;

extern const Hal *HAL;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HAL_H

