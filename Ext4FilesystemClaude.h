///////////////////////////////////////////////////////////////////////////////
///
/// @file              Ext4Filesystem.h
///
/// @brief             ext4 filesystem driver for NanoOs.
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
///
///////////////////////////////////////////////////////////////////////////////

#ifndef EXT4_FILESYSTEM_H
#define EXT4_FILESYSTEM_H

#include "Filesystem.h"
#include "NanoOs.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Function declarations
long ext4FilesystemFTell(FILE *stream);
#ifdef ftell
#undef ftell
#endif // ftell
#define ftell ext4FilesystemFTell

void* runExt4Filesystem(void *args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // EXT4_FILESYSTEM_H

