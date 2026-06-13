/* Multi-domain vector context switch test */
#include <stdint.h>
#define UTM_BASE ((volatile uint64_t *)0x41000000)

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
  __asm__ __volatile__ (
    "vsetivli zero, 1, e64, m8, ta, ma\n\t"
    "vse64.v v0, (%0)\n\t"
    :
    : "r"(UTM_BASE)
    : "memory"
  );

  UTM_BASE[64] = 0xCAFE; /* marker: wrote v0 */
  sbi_puts("A: wrote v0=DEADBEEFCAFEBAB0, yielding\n");
  sbi_stop();

  /* Phase 2: after resume */
  sbi_putchar('R');
  sbi_puts("A: resumed, exiting\n");
  sbi_exit(0);
}
