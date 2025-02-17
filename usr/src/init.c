#include "NanoOsSystemCalls.h"

void _start(void) {
  const char *message = "Hello, world!\n";
  
  // Calculate string length
  int length = 0;
  const char *ptr = message;
  while (*ptr++) {
    length++;
  }
  
  // Write to stdout using syscall
  register int a0 asm("a0") = NANO_OS_STDOUT_FILENO; // stdout file descriptor
  register const char *a1 asm("a1") = message;       // buffer address
  register int a2 asm("a2") = length;                // length
  register int a7 asm("a7") = NANO_OS_SYSCALL_WRITE; // write syscall

  asm volatile(
    "ecall"
    : "+r"(a0)
    : "r"(a1), "r"(a2), "r"(a7)
  );
  
  // Exit syscall
  a0 = 0;                    // exit code 0
  a7 = NANO_OS_SYSCALL_EXIT; // exit syscall
  
  asm volatile(
    "ecall"
    : : "r"(a0), "r"(a7)
  );
}
