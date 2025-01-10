////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2024 James Card                     //
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

// Doxygen marker
/// @file

// Custom includes
#include "Filesystem.h"
#include "SdFat.h"
//// #include "SdCard/SdCardInfo.h"

/// @struct FilesystemState
///
/// @brief State metadata the filesystem process uses to provide access to
/// files.
///
/// @param sdFat An SdFat object as defined by the SdFat library.
typedef struct FilesystemState {
  SdFat sdFat;
} FilesystemState;

/// @def PIN_SD_CS
///
/// @brief Pin to use for the MicroSD card reader's SPI chip select line.
#define PIN_SD_CS 4

const char *sdCardErrors[] = {
  "No error",
  "Card reset failed",
  "SDIO read CID",
  "SDIO publish RCA",
  "Switch card function",
  "SDIO card select",
  "Send and check interface settings",
  "Read CSD data",
  "Read CID data",
  "Stop multiple block transmission",
  "Read card status",
  "Read single block",
  "Read multiple blocks",
  "Write single block",
  "Write multiple blocks",
  "Set first erase block",
  "Set last erase block",
  "Erase selected blocks",
  "Read OCR register",
  "Set CRC mode",
  "Set SDIO bus width",
  "Read extended status",
  "Set pre-erased count",
  "Activate card initialization",
  "Read SCR data",
  "Bad read data token",
  "Read CRC error",
  "SDIO fifo read timeout",
  "Read CID or CSD failed.",
  "Bad readStart argument",
  "Read data timeout",
  "Multiple block stop failed",
  "SDIO transfer complete",
  "Write data not accepted",
  "SDIO fifo write timeout",
  "Bad writeStart argument",
  "Flash programming",
  "Write timeout",
  "DMA transfer failed",
  "Card did not accept erase commands",
  "Card does not support erase",
  "Erase command timeout",
  "Card has not been initialized",
  "Invalid card config",
  "Unsupported SDIO command",
};

void filesystemPrintError(FilesystemState *filesystemState) {
  uint8_t errorCode = filesystemState->sdFat.card()->errorCode();
  uint8_t errorData = filesystemState->sdFat.card()->errorData();

  Serial.print("SD initialization failed. Error Code: 0x");
  Serial.print(errorCode, HEX);
  Serial.print(" Error Data: 0x");
  Serial.println(errorData, HEX);
  Serial.println(sdCardErrors[errorCode]);
  
  return;
}

/// @fn void* runFilesystem(void *args)
///
/// @brief Process entry-point for the filesystem process.  Sets up and
/// configures access to the SD card reader and then enters an infinite loop
/// for processing commands.
///
/// @param args Any arguments to this function, cast to a void*.  Currently
///   ignored by this function.
///
/// @return This function never returns, but would return NULL if it did.
void* runFilesystem(void *args) {
  (void) args;

  FilesystemState filesystemState;
  // Deliberately not checking return value here for now.
  if (filesystemState.sdFat.begin(PIN_SD_CS, SPI_HALF_SPEED)) {
    printString("SdFat library initialized successfully.\n");
  } else {
    printString("ERROR!!!  Could not initialize SdFat library!\n");
    filesystemPrintError(&filesystemState);
  }

  while (1) {
    coroutineYield(NULL);
  }

  return NULL;
}

