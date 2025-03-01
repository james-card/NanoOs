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

// Standard C includes
#include <termios.h>
#include <errno.h>

// NanoOs includes
#include "NanoOsSystemCalls.h"

/// @var terminalIos
///
/// @brief Array of termios structures for the three basic file descriptors
/// (stdin, stdout, and stderr).
static struct termios terminalIos[] = {
  {
    // 0:  STDIN_FILENO
    .c_iflag = IGNCR,
    .c_oflag = ONLRET,
    .c_cflag = CSIZE,
    .c_lflag = ECHO | ECHOE | ECHOK | ECHONL | ISIG,
    .c_cc = {
      0x1c,  // ASCII file separator   VEOF,   End-of-file character (Ctrl-D)
      0x0a,  // ASCII line feed        VEOL,   End-of-line character
      0x08,  // ASCII backspace        VERASE, Erase character (Backspace)
      0x18,  // ASCII cancel           VINTR,  Interrupt character (Ctrl-C)
      0x03,  // ASCII end of text      VKILL,  Kill line character (Ctrl-U)
      0x01,  //                        VMIN,   Minimum number of bytes for read
      0x07,  // ASCII bell, alert      VQUIT,  Quit character (Ctrl-\)
      0x01,  // ASCII start of heading VSTART, Start output character (Ctrl-Q)
      0x04,  // ASCII end of tx        VSTOP,  Stop output character (Ctrl-S)
      0x1b,  // ASCII escape           VSUSP,  Suspend character (Ctrl-Z)
      255,   //                        VTIME,  Deciseconds for read timeout
    }
  },
  {
    // 1:  STDOUT_FILENO
    .c_iflag = IGNCR,
    .c_oflag = ONLRET,
    .c_cflag = CSIZE,
    .c_lflag = ECHO | ECHOE | ECHOK | ECHONL | ISIG,
    .c_cc = {
      0x1c,  // ASCII file separator   VEOF,   End-of-file character (Ctrl-D)
      0x0a,  // ASCII line feed        VEOL,   End-of-line character
      0x08,  // ASCII backspace        VERASE, Erase character (Backspace)
      0x18,  // ASCII cancel           VINTR,  Interrupt character (Ctrl-C)
      0x03,  // ASCII end of text      VKILL,  Kill line character (Ctrl-U)
      0x01,  //                        VMIN,   Minimum number of bytes for read
      0x07,  // ASCII bell, alert      VQUIT,  Quit character (Ctrl-\)
      0x01,  // ASCII start of heading VSTART, Start output character (Ctrl-Q)
      0x04,  // ASCII end of tx        VSTOP,  Stop output character (Ctrl-S)
      0x1b,  // ASCII escape           VSUSP,  Suspend character (Ctrl-Z)
      255,   //                        VTIME,  Deciseconds for read timeout
    }
  },
  {
    // 2:  STDERR_FILENO
    .c_iflag = IGNCR,
    .c_oflag = ONLRET,
    .c_cflag = CSIZE,
    .c_lflag = ECHO | ECHOE | ECHOK | ECHONL | ISIG,
    .c_cc = {
      0x1c,  // ASCII file separator   VEOF,   End-of-file character (Ctrl-D)
      0x0a,  // ASCII line feed        VEOL,   End-of-line character
      0x08,  // ASCII backspace        VERASE, Erase character (Backspace)
      0x18,  // ASCII cancel           VINTR,  Interrupt character (Ctrl-C)
      0x03,  // ASCII end of text      VKILL,  Kill line character (Ctrl-U)
      0x01,  //                        VMIN,   Minimum number of bytes for read
      0x07,  // ASCII bell, alert      VQUIT,  Quit character (Ctrl-\)
      0x01,  // ASCII start of heading VSTART, Start output character (Ctrl-Q)
      0x04,  // ASCII end of tx        VSTOP,  Stop output character (Ctrl-S)
      0x1b,  // ASCII escape           VSUSP,  Suspend character (Ctrl-Z)
      255,   //                        VTIME,  Deciseconds for read timeout
    }
  },
};

/// @var NUM_TERMINAL_IOS
///
/// @brief Number of elements in the terminalIos array.
const int NUM_TERMINAL_IOS = sizeof(terminalIos) / sizeof(terminalIos[0]);

/// @fn int tcgetattr(int fd, struct termios *termios_p)
///
/// @brief Get the termios parameters associated with the provided file
/// descriptor.
///
/// @param fd Standard POSIX file descriptor for stdin, stdout, or stderr.
/// @param termios_p A pointer to the termios structure to populate.
///
/// @return Returns 0 on success, sets errno and returns -1 on failure.
int tcgetattr(int fd, struct termios *termios_p) {
  if (fd >= NUM_TERMINAL_IOS) {
    errno = ERANGE;
    return -1;
  } else if (termios_p == NULL) {
    errno = EINVAL;
    return -1;
  }

  *termios_p = terminalIos[fd];
  return 0;
}

/// @fn int tcsetattr(
///   int fd, int optional_actions, const struct termios *termios_p)
///
/// @brief Set the termios parameters associated with the provided file
/// descriptor.
///
/// @param fd Standard POSIX file descriptor for stdin, stdout, or stderr.
/// @param optional_actions specifies when the changes take effect.
/// @param termios_p A pointer to the termios structure to populate.
///
/// @return Returns 0 on success, sets errno and returns -1 on failure.
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
  if (fd >= NUM_TERMINAL_IOS) {
    errno = ERANGE;
    return -1;
  } else if (termios_p == NULL) {
    errno = EINVAL;
    return -1;
  }

  terminalIos[fd] = *termios_p;

  // desiredEchoState is a Boolean value.
  register int a0 asm("a0") = ((termios_p->c_lflag & ECHO) != 0);
  register int a7 asm("a7") = NANO_OS_SYSCALL_SET_ECHO; // syscall number

  asm volatile(
    "ecall"
    : "+r"(a0)
    : "r"(a7)
  );

  return a0;
}

