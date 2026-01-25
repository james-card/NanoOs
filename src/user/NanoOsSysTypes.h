///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              01.25.2026
///
/// @file              NanoOsSysTypes.h
///
/// @brief             Definitions that would come from sys/types.h on a POSIX
///                    system
///
/// @copyright
///                   Copyright (c) 2012-2026 James Card
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

#ifndef NANO_OS_SYS_TYPES_H
#define NANO_OS_SYS_TYPES_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint8_t   uid_t;
typedef uint8_t   gid_t;
typedef uint8_t   pid_t;
typedef uintptr_t size_t;
typedef intptr_t  ssize_t;
typedef int64_t   time_t;

#ifdef __cplusplus
}
#endif

#endif // NANO_OS_SYS_TYPES_H

