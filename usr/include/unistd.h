///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              02.28.2025
///
/// @file              unistd.h
///
/// @brief             POSIX standard symbolic constants and types for NanoOs C
///                    programs.
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

#ifndef UNISTD_H
#define UNISTD_H

#include <sys/types.h>

/* Standard file descriptors */
#define STDIN_FILENO  0  /* Standard input */
#define STDOUT_FILENO 1  /* Standard output */
#define STDERR_FILENO 2  /* Standard error output */

/* Values for the second argument to access */
#define F_OK 0  /* Test for existence */
#define R_OK 4  /* Test for read permission */
#define W_OK 2  /* Test for write permission */
#define X_OK 1  /* Test for execute permission */

/* Values for the whence argument to lseek */
#define SEEK_SET 0  /* Seek from beginning of file */
#define SEEK_CUR 1  /* Seek from current position */
#define SEEK_END 2  /* Seek from end of file */

/* Pathconf variables */
#define _PC_LINK_MAX          0
#define _PC_MAX_CANON         1
#define _PC_MAX_INPUT         2
#define _PC_NAME_MAX          3
#define _PC_PATH_MAX          4
#define _PC_PIPE_BUF          5
#define _PC_CHOWN_RESTRICTED  6
#define _PC_NO_TRUNC          7
#define _PC_VDISABLE          8

/* Sysconf variables */
#define _SC_ARG_MAX           0
#define _SC_CHILD_MAX         1
#define _SC_CLK_TCK           2
#define _SC_NGROUPS_MAX       3
#define _SC_OPEN_MAX          4
#define _SC_STREAM_MAX        5
#define _SC_TZNAME_MAX        6
#define _SC_JOB_CONTROL       7
#define _SC_SAVED_IDS         8
#define _SC_VERSION           9
#define _SC_PAGESIZE         10

/* Version test macros */
#define _POSIX_VERSION 200809L
#define _POSIX_C_SOURCE 200809L

/* Feature test macros */
#define _POSIX_CHOWN_RESTRICTED 1
#define _POSIX_NO_TRUNC         1
#define _POSIX_JOB_CONTROL      1
#define _POSIX_SAVED_IDS        1
#define _POSIX_VDISABLE         0xff

/* NULL pointer constant */
#ifndef NULL
#define NULL ((void *)0)
#endif

/* Function prototypes */

/* Process primitives */
pid_t   fork(void);
pid_t   getpid(void);
pid_t   getppid(void);
uid_t   getuid(void);
uid_t   geteuid(void);
gid_t   getgid(void);
gid_t   getegid(void);
int     setuid(uid_t uid);
int     setgid(gid_t gid);
int     getgroups(int gidsetsize, gid_t grouplist[]);
int     setpgid(pid_t pid, pid_t pgid);
pid_t   setsid(void);
int     execl(const char *path, const char *arg, ...);
int     execv(const char *path, char *const argv[]);
int     execle(const char *path, const char *arg, ...);
int     execve(const char *path, char *const argv[], char *const envp[]);
int     execlp(const char *file, const char *arg, ...);
int     execvp(const char *file, char *const argv[]);
void    _exit(int status);
unsigned sleep(unsigned seconds);
int     pause(void);
unsigned alarm(unsigned seconds);

/* Environment */
char   *getlogin(void);
char   *getcwd(char *buf, size_t size);
int     chdir(const char *path);
int     chown(const char *path, uid_t owner, gid_t group);
int     rmdir(const char *path);
int     access(const char *path, int amode);
long    pathconf(const char *path, int name);
long    fpathconf(int fd, int name);
long    sysconf(int name);

/* I/O primitives */
int     pipe(int fildes[2]);
int     dup(int fildes);
int     dup2(int fildes, int fildes2);
int     close(int fildes);
ssize_t read(int fildes, void *buf, size_t nbyte);
ssize_t write(int fildes, const void *buf, size_t nbyte);
off_t   lseek(int fildes, off_t offset, int whence);
int     unlink(const char *path);
int     link(const char *path1, const char *path2);
int     symlink(const char *path1, const char *path2);
ssize_t readlink(const char *path, char *buf, size_t bufsize);
int     fsync(int fildes);
int     fdatasync(int fildes);

/* Terminal I/O */
int     isatty(int fildes);
char   *ttyname(int fildes);
int     tcgetpgrp(int fildes);
int     tcsetpgrp(int fildes, pid_t pgid_id);

#endif // UNISTD_H
