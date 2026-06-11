#include <stdint.h>
#include <string.h>

static int phase;

static void sbi_putchar(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}
static void sbi_puts(const char *s) { while (*s) sbi_putchar(*s++); }

static void sbi_stop(void) {
  __asm__ __volatile__ (
    "li a7, 1104\n"
    "li a0, 0\n"
    "ecall\n"
    : : : "a7", "a0"
  );
}

static void sbi_exit(int code) {
  __asm__ __volatile__ ("li a7, 1101\nmv a0, %0\necall\n" : : "r"((unsigned long)code) : "a7","a0");
}

void _start(void) {
  __asm__("la gp, __global_pointer$");

  if (phase == 0) {
    sbi_puts("[1] stop\n");
    phase = 1;
    sbi_stop();
  }

  sbi_puts("[2] ok\n");
  sbi_exit(0);
}
