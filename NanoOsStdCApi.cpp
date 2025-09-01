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

// Doxygen marker
/// @file

#include "NanoOs.h"
#include "NanoOsStdCApi.h"

NanoOsStdCApi nanoOsStdCApi = {
  // File operations:
  .fopen = filesystemFOpen,
  .fclose = filesystemFClose,
  .remove = filesystemRemove,
  .fseek = filesystemFSeek,
  
  // Formatted I/O:
  .vsscanf = vsscanf,
  .sscanf = sscanf,
  .vfscanf = nanoOsVFScanf,
  .fscanf = nanoOsFScanf,
  .scanf = nanoOsScanf,
  .vfprintf = nanoOsVFPrintf,
  .fprintf = nanoOsFPrintf,
  .printf = nanoOsPrintf,
  .vsprintf = vsprintf,
  .vsnprintf = vsnprintf,
  .sprintf = sprintf,
  .snprintf = snprintf,
  
  // Character I/O:
  .fputs = nanoOsFPuts,
  .puts = nanoOsPuts,
  .fgets = nanoOsFGets,
  
  // Direct I/O:
  .fread = filesystemFRead,
  .fwrite = filesystemFWrite,
  
  // Memory management:
  .free = memoryManagerFree,
  .realloc = memoryManagerRealloc,
  .malloc = memoryManagerMalloc,
  .calloc = memoryManagerCalloc,
  
  // Copying functions:
  .memcpy = memcpy,
  .memmove = memmove,
  .strcpy = strcpy,
  .strncpy = strncpy,
  .strcat = strcat,
  .strncat = strncat,
  
  // Comparison functions:
  .memcmp = memcmp,
  .strcmp = strcmp,
  .strncmp = strncmp,
  
  // Miscellaaneous string functions:
  .memset = memset,
  .strerror = nanoOsStrError,
  .strlen = strlen,
};

