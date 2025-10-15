///////////////////////////////////////////////////////////////////////////////
///
/// @file              ExFatProcess.h
///
/// @brief             exFAT process for NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
///
///////////////////////////////////////////////////////////////////////////////

#ifndef EXFAT_PROCESS_H
#define EXFAT_PROCESS_H

#include "Filesystem.h"
#include "NanoOs.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Function declarations
long exFatProcessFTell(FILE *stream);
#ifdef ftell
#undef ftell
#endif // ftell
#define ftell exFatProcessFTell

void* runExFatFilesystem(void *args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // EXFAT_PROCESS_H

