/* Multi-enclave vector context switch test:
 * Each enclave: vector compute -> stop -> wait for context switch
 * On resume: vector recompute -> compare with saved
 * If SM doesn't save VS, results differ between runs */
#include <stdint.h>
#include <string.h>
#include "../fconv2d/fconv2d.h"

#define M 112
#define N 112
#define F 7

extern double fconv2d_i[], fconv2d_f[], fconv2d_o[], fconv2d_golden_o[];

static int phase;
static double saved[16];

static void sbi_putchar(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}
static void sbi_puts(const char *s) { while (*s) sbi_putchar(*s++); }

static void sbi_stop(void) {
  __asm__ __volatile__ (
    "li a7, 1104\nli a0, 0\necall\n" : : : "a7", "a0");
}

static void sbi_exit(int code) {
  __asm__ __volatile__ ("li a7, 1101\nmv a0, %0\necall\n" : : "r"((unsigned long)code) : "a7","a0");
}

static void compute(void) {
  memset(fconv2d_o, 0, M * N * 8);
  fconv2d_7x7(fconv2d_o, fconv2d_i, fconv2d_f, M, N, F);
}

void _start(void) {
  __asm__("la gp, __global_pointer$");

  if (phase == 0) {
    sbi_puts("[1] vcompute\n");
    compute();
    memcpy(saved, fconv2d_o, sizeof(saved));
    phase = 1;
    sbi_puts("[1] stop\n");
    sbi_stop();
  }

  sbi_puts("[2] vcompute\n");
  compute();

  int ok = 1;
  for (int i = 0; i < 16; i++) {
    if (saved[i] != fconv2d_o[i]) { ok = 0; break; }
  }
  sbi_puts(ok ? "PASS\n" : "FAIL (VS corrupted?)\n");
  sbi_exit(0);
}
