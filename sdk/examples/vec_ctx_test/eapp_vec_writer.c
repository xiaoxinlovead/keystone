/* Multi-domain vector context switch test
 *
 * VERIFIES: Keystone SM vector context save/restore on context switch.
 *
 * A writes a pattern to v0, yields. B clobbers v0-v31 (vxor.vv), exits.
 * Host resumes A. If SM saved/restored vector context, v0 holds A's
 * original value. Otherwise v0 would hold B's or the host kernel's value.
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
    sbi_puts("NO_CORRUPTION (SM saves/restores vector context)\n");
  else
    sbi_puts("CORRUPTED (SM lacks vector context switch)\n");

  sbi_exit(0);
}
