#include <stdint.h>

static inline void linux_write(int fd, const char *buf, unsigned long count) {
  register long a7 asm("a7") = 64;   // SYS_write
  register long a0 asm("a0") = fd;
  register const char *a1 asm("a1") = buf;
  register unsigned long a2 asm("a2") = count;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
}

void _putchar(char character) {
  linux_write(1, &character, 1);
}