///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              02.28.2025
///
/// @file              sys/types.h
///
/// @brief             POSIX data types for NanoOs C programs.
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

#ifndef SYS_TYPES_H
#define SYS_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* Integer types */
typedef uint8_t   uchar;
typedef uint16_t  ushort;
typedef uint32_t  uint;
typedef uint64_t  ulong;

/* Process IDs and process group IDs */
typedef int32_t pid_t;     /* Process ID */
typedef int32_t pgrp_t;    /* Process group ID */

/* User IDs and group IDs */
typedef uint32_t uid_t;    /* User ID */
typedef uint32_t gid_t;    /* Group ID */

/* File sizes and offsets */
typedef int64_t off_t;      /* File offset */
typedef int64_t blksize_t;  /* Block size */
typedef int64_t blkcnt_t;   /* File block count */

/* Time-related types */
typedef int64_t time_t;      /* Time in seconds */
typedef int64_t suseconds_t; /* Time in microseconds */
typedef uint64_t useconds_t; /* Unsigned microseconds */
typedef int64_t clock_t;     /* System time units */

/* File system types */
typedef uint64_t ino_t;     /* File serial number (inode) */
typedef uint32_t mode_t;    /* File permissions and type */
typedef uint64_t nlink_t;   /* Number of hard links */
typedef uint64_t dev_t;     /* Device ID */

/* Memory size and difference types */
typedef size_t    size_t;    /* Memory object size (from stdint.h) */
typedef intptr_t  ssize_t;   /* Signed size type */
typedef ptrdiff_t ptrdiff_t; /* Pointer difference (from stddef.h) */

/* Thread and synchronization types */
typedef intptr_t pthread_t;           /* Thread ID */
typedef int32_t  pthread_key_t;       /* Thread-specific key */
typedef int32_t  pthread_once_t;      /* One-time initialization */
typedef struct pthread_mutex_t {      /* Mutex */
  int32_t __dummy;
} pthread_mutex_t;
typedef struct pthread_cond_t {       /* Condition variable */
  int32_t __dummy;
} pthread_cond_t;
typedef struct pthread_attr_t {       /* Thread attributes */
  int32_t __dummy;
} pthread_attr_t;
typedef struct pthread_mutexattr_t {  /* Mutex attributes */
  int32_t __dummy;
} pthread_mutexattr_t;
typedef struct pthread_condattr_t {   /* Condition variable attributes */
  int32_t __dummy;
} pthread_condattr_t;

/* Socket-related types */
typedef uint32_t socklen_t;          /* Socket length */
typedef uint16_t sa_family_t;        /* Socket address family */

/* Additional POSIX types */
typedef int32_t id_t;                /* General identifier */
typedef int32_t key_t;               /* IPC key */
typedef intptr_t timer_t;            /* Timer ID */
typedef int64_t clockid_t;           /* Clock ID type */

#endif // SYS_TYPES_H
