///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              02.28.2025
///
/// @file              termios.h
///
/// @brief             POSIX terminal control interface for NanoOs C programs.
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

#ifndef TERMIOS_H
#define TERMIOS_H

#include <sys/types.h>

/* Type definitions required by POSIX */
typedef unsigned int  tcflag_t;  /* Terminal flags type */
typedef unsigned char cc_t;      /* Control character type */
typedef unsigned int  speed_t;   /* Terminal speed type */

/* Size of the control character array */
#define NCCS 11

/* Terminal control structure as required by POSIX */
struct termios {
  tcflag_t c_iflag;    /* Input modes */
  tcflag_t c_oflag;    /* Output modes */
  tcflag_t c_cflag;    /* Control modes */
  tcflag_t c_lflag;    /* Local modes */
  cc_t     c_cc[NCCS]; /* Special control characters */
};

/* Special control character indices in c_cc[] */
#define VEOF     0  /* End-of-file character (Ctrl-D) */
#define VEOL     1  /* End-of-line character */
#define VERASE   2  /* Erase character (Backspace) */
#define VINTR    3  /* Interrupt character (Ctrl-C) */
#define VKILL    4  /* Kill line character (Ctrl-U) */
#define VMIN     5  /* Minimum number of bytes for read */
#define VQUIT    6  /* Quit character (Ctrl-\) */
#define VSTART   7  /* Start output character (Ctrl-Q) */
#define VSTOP    8  /* Stop output character (Ctrl-S) */
#define VSUSP    9  /* Suspend character (Ctrl-Z) */
#define VTIME   10  /* Time value for read timeout (deciseconds) */

/* Input mode flags for c_iflag */
#define BRKINT  0000001  /* Signal interrupt on break */
#define ICRNL   0000002  /* Translate CR to NL on input */
#define IGNBRK  0000004  /* Ignore break condition */
#define IGNCR   0000010  /* Ignore CR */
#define IGNPAR  0000020  /* Ignore parity errors */
#define INLCR   0000040  /* Translate NL to CR on input */
#define INPCK   0000100  /* Enable input parity check */
#define ISTRIP  0000200  /* Strip 8th bit off characters */
#define IXOFF   0000400  /* Enable software flow control (incoming) */
#define IXON    0001000  /* Enable software flow control (outgoing) */
#define PARMRK  0002000  /* Mark parity errors */

/* Output mode flags for c_oflag */
#define OPOST   0000001  /* Post-process output */
#define ONLCR   0000002  /* Map NL to CR-NL on output */
#define OCRNL   0000004  /* Map CR to NL on output */
#define ONOCR   0000010  /* No CR output at column 0 */
#define ONLRET  0000020  /* NL performs CR function */

/* Control mode flags for c_cflag */
#define B0      0000000  /* Hang up */
#define B50     0000001  /* 50 baud */
#define B75     0000002  /* 75 baud */
#define B110    0000003  /* 110 baud */
#define B134    0000004  /* 134.5 baud */
#define B150    0000005  /* 150 baud */
#define B200    0000006  /* 200 baud */
#define B300    0000007  /* 300 baud */
#define B600    0000010  /* 600 baud */
#define B1200   0000011  /* 1200 baud */
#define B1800   0000012  /* 1800 baud */
#define B2400   0000013  /* 2400 baud */
#define B4800   0000014  /* 4800 baud */
#define B9600   0000015  /* 9600 baud */
#define B19200  0000016  /* 19200 baud */
#define B38400  0000017  /* 38400 baud */

#define CSIZE   0000060  /* Character size mask */
#define CS5     0000000  /* 5 bits */
#define CS6     0000020  /* 6 bits */
#define CS7     0000040  /* 7 bits */
#define CS8     0000060  /* 8 bits */
#define CSTOPB  0000100  /* Set two stop bits, rather than one */
#define CREAD   0000200  /* Enable receiver */
#define PARENB  0000400  /* Parity enable */
#define PARODD  0001000  /* Odd parity, else even */
#define HUPCL   0002000  /* Hang up on last close */
#define CLOCAL  0004000  /* Ignore modem control lines */

/* Local mode flags for c_lflag */
#define ECHO    0000001  /* Enable echo */
#define ECHOE   0000002  /* Echo erase character as BS-SP-BS */
#define ECHOK   0000004  /* Echo NL after kill character */
#define ECHONL  0000010  /* Echo NL even if ECHO is off */
#define ICANON  0000100  /* Canonical input */
#define IEXTEN  0000200  /* Enable extended input processing */
#define ISIG    0000400  /* Enable signals */
#define NOFLSH  0001000  /* Disable flush after interrupt */
#define TOSTOP  0002000  /* Send SIGTTOU for background output */

/* tcflow() and TCXONC use these */
#define TCOOFF  0  /* Suspend output */
#define TCOON   1  /* Restart output */
#define TCIOFF  2  /* Transmit a STOP character */
#define TCION   3  /* Transmit a START character */

/* tcflush() and TCFLSH use these */
#define TCIFLUSH  0  /* Flush pending input */
#define TCOFLUSH  1  /* Flush untransmitted output */
#define TCIOFLUSH 2  /* Flush both pending input and untransmitted output */

/* tcsetattr uses these */
#define TCSANOW   0  /* Change immediately */
#define TCSADRAIN 1  /* Change after transmitting all queued output */
#define TCSAFLUSH 2  /* Change after transmitting all queued output and discarding all pending input */

/* Function prototypes */
speed_t cfgetispeed(const struct termios *termios_p);
speed_t cfgetospeed(const struct termios *termios_p);
int cfsetispeed(struct termios *termios_p, speed_t speed);
int cfsetospeed(struct termios *termios_p, speed_t speed);
int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
int tcsendbreak(int fd, int duration);
int tcdrain(int fd);
int tcflush(int fd, int queue_selector);
int tcflow(int fd, int action);

#endif // TERMIOS_H
