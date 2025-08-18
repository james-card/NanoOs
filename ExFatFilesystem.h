///////////////////////////////////////////////////////////////////////////////
///
/// @file              ExFatFilesystem.h
///
/// @brief             exFAT filesystem driver for NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
///
///////////////////////////////////////////////////////////////////////////////

#ifndef EXFAT_FILESYSTEM_H
#define EXFAT_FILESYSTEM_H

#include "Filesystem.h"
#include "NanoOs.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Function declarations
long exFatFilesystemFTell(FILE *stream);
#ifdef ftell
#undef ftell
#endif // ftell
#define ftell exFatFilesystemFTell

void* runExFatFilesystem(void *args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // EXFAT_FILESYSTEM_H

