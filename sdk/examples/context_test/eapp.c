static void putchar(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}
static void puts(const char *s) { while (*s) putchar(*s++); }
static void puthex(unsigned long x) {
  for (int i = 60; i >= 0; i -= 4)
    putchar("0123456789abcdef"[(x >> i) & 0xf]);
}
static void exit(int code) {
  __asm__ __volatile__ ("li a7, 1101\nmv a0, %0\necall\n" : : "r"((unsigned long)code) : "a7", "a0");
}

void _start(void) {
  __asm__ __volatile__ (
    ".option push\n.option norelax\n"
    "la gp, __global_pointer$\n"
    "la tp, __global_pointer$\n"
    ".option pop\n"
  );

  puts("=== POLLUTE TEST ===\n");

  unsigned long vl;
  long x;

  /* Round 1: set v1=111, wait for timer, read */
  __asm__ __volatile__ (
    "vsetvli %0, x0, e32, m1, ta, ma\n"
    "li t0, 111\n"
    "vmv.v.x v1, t0\n"
    : "=&r"(vl) : : "t0", "v1"
  );
  __asm__ __volatile__ ("vmv.x.s %0, v1" : "=r"(x));
  puts("R1 BEFORE v1="); puthex(x); putchar('\n');

  for (volatile unsigned long i = 0; i < 200000; i++) asm volatile("nop");

  __asm__ __volatile__ ("vmv.x.s %0, v1" : "=r"(x));
  puts("R1 AFTER  v1="); puthex(x);
  if (x == 111) puts(" (SAME-111)\n");
  else          puts(" (CHANGED!)\n");

  /* Round 2: pollute v1=999, wait for timer, read */
  __asm__ __volatile__ (
    "li t0, 999\n"
    "vmv.v.x v1, t0\n"
    : : : "t0", "v1"
  );
  __asm__ __volatile__ ("vmv.x.s %0, v1" : "=r"(x));
  puts("R2 POLLUTED v1="); puthex(x); putchar('\n');

  for (volatile unsigned long i = 0; i < 200000; i++) asm volatile("nop");

  __asm__ __volatile__ ("vmv.x.s %0, v1" : "=r"(x));
  puts("R2 AFTER  v1="); puthex(x);
  if (x == 999) puts(" (STILL-999 - no save/restore)\n");
  else if (x == 111) puts(" (RESTORED-111 - SM saved!)\n");
  else          puts(" (UNKNOWN)\n");

  puts("=== POLLUTE DONE ===\n");
  exit(0);
}
