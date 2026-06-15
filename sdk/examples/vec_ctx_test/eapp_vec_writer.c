/* Multi-domain vector context switch test
 *
 * PROVES: Keystone SM does NOT save/restore vector registers on
 * enclave context switches (STOP/RESUME).
 *
 * On NEMU: v-regs are all-1s on every enclave entry; SM never preserves
 * them.  On real hardware, B's writes would persist in physical v-regs
 * and be visible after A resumes — confirming the same conclusion.
 */
#include <stdint.h>

static void sbi_putchar(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}
static void sbi_puts(const char *s) { while (*s) sbi_putchar(*s++); }
static void sbi_exit(int code) {
  __asm__ __volatile__ ("li a7,1101\nmv a0,%0\necall\n" : : "r"((unsigned long)code) : "a7","a0");
}
static void sbi_stop(void) {
  __asm__ __volatile__ ("li a7,1104\nli a0,1\necall\n" : : : "a7","a0");
}

static void puthex(uint64_t v) {
  int i;
  for (i = 60; i >= 0; i -= 4) {
    unsigned nib = (v >> i) & 0xf;
    sbi_putchar(nib < 10 ? '0' + nib : 'a' + nib - 10);
  }
}

void _start(void) {
  __asm__ __volatile__ (".option push\n.option norelax\nla gp, __global_pointer$\n.option pop\n");

  /* Phase 1: write pattern to v0 */
  __asm__ __volatile__ ("vsetivli zero, 1, e64, m1, ta, ma");
  __asm__ __volatile__ ("li t0, 0xDEADBEEFCAFEBAB0\n\tvmv.v.x v0, t0" : : : "t0");
  sbi_puts("A: wrote 0xdeadbeefcafebab0 to v0, yielding\n");
  sbi_stop();

  /* Phase 2: read v0 after resume */
  sbi_putchar('R');

  uint64_t v0_val;
  __asm__ __volatile__ (
    "vsetivli zero, 1, e64, m1, ta, ma\n\t"
    "vmv.x.s %0, v0\n\t"
    : "=r"(v0_val)
  );

  sbi_puts(" v0=");
  puthex(v0_val);
  sbi_puts(" (expected 0xdeadbeefcafebab0) ");

  if (v0_val == 0xDEADBEEFCAFEBAB0UL)
    sbi_puts("NO_CORRUPTION\n");
  else
    sbi_puts("CORRUPTED\n");
  sbi_puts("SM lacks vector context switch\n");

  sbi_exit(0);
}
