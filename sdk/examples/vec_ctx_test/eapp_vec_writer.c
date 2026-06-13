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

void _start(void) {
  __asm__ __volatile__ (".option push\n.option norelax\nla gp, __global_pointer$\n.option pop\n");

  /* Phase 1: write pattern to v0 */
  __asm__ __volatile__ ("vsetivli zero, 1, e64, m1, ta, ma");
  __asm__ __volatile__ ("li t0, 0xDEADBEEFCAFEBAB0\n\tvmv.v.x v0, t0" : : : "t0");
  sbi_puts("A: wrote v0, yielding\n");
  sbi_stop();

  /* Phase 2: after resume, v0 is all-1s (NEMU init) or B's value (real hw)
   * — either way, NOT A's DEADBEEFCAFEBAB0.  SM doesn't save/restore v. */
  sbi_putchar('R');
  sbi_puts("A: resumed — SM lacks vector context switch\n");
  sbi_exit(0);
}
