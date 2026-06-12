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

/* Write v0/v4/.../v28 with known values: each gets {R*4, R*4+1, R*4+2, R*4+3}. */
static void fill_vregs(void) {
  unsigned long tmp;
  unsigned int pool[32] __attribute__((aligned(32)));
  unsigned int *p = pool;
  for (int r = 0; r < 8; r++)
    for (int i = 0; i < 4; i++)
      pool[r*4 + i] = r*4 + i;
  __asm__ __volatile__ (
    "vsetvli %0, x0, e32, m1, ta, ma\n"
    "vle32.v v0, (%1)\n"
    "addi %1, %1, 16\n"
    "vle32.v v4, (%1)\n"
    "addi %1, %1, 16\n"
    "vle32.v v8, (%1)\n"
    "addi %1, %1, 16\n"
    "vle32.v v12, (%1)\n"
    "addi %1, %1, 16\n"
    "vle32.v v16, (%1)\n"
    "addi %1, %1, 16\n"
    "vle32.v v20, (%1)\n"
    "addi %1, %1, 16\n"
    "vle32.v v24, (%1)\n"
    "addi %1, %1, 16\n"
    "vle32.v v28, (%1)\n"
    "addi %1, %1, -112\n"
    : "=&r"(tmp), "+r"(p) : : "v0","v4","v8","v12","v16","v20","v24","v28","memory");
}

/* Store v0/v4/.../v28 to mem[0..31] (4 words per reg).
   vtype/vl must still be valid from fill_vregs. */
static void dump_vregs(unsigned int *mem) {
  unsigned long discard;
  __asm__ __volatile__ (
    "vse32.v v0, (%0)\n"
    "addi %0, %0, 16\n"
    "vse32.v v4, (%0)\n"
    "addi %0, %0, 16\n"
    "vse32.v v8, (%0)\n"
    "addi %0, %0, 16\n"
    "vse32.v v12, (%0)\n"
    "addi %0, %0, 16\n"
    "vse32.v v16, (%0)\n"
    "addi %0, %0, 16\n"
    "vse32.v v20, (%0)\n"
    "addi %0, %0, 16\n"
    "vse32.v v24, (%0)\n"
    "addi %0, %0, 16\n"
    "vse32.v v28, (%0)\n"
    "addi %0, %0, -112\n"  /* restore original mem ptr */
    : "=&r"(discard) : "0"(mem) : "memory");
}

int check(unsigned int *got, unsigned int *exp, int n) {
  for (int i = 0; i < n; i++) if (got[i] != exp[i]) return i + 1;
  return 0;
}

void _start(void) {
  __asm__ __volatile__ (
    ".option push\n.option norelax\n"
    "la gp, __global_pointer$\n"
    "la tp, __global_pointer$\n"
    ".option pop\n"
  );

  puts("=== VREGS_SAVE TEST ===\n");

  /* Allocate dump buffers (32 words each) */
  unsigned int before[32] __attribute__((aligned(32)));
  unsigned int after[32]  __attribute__((aligned(32)));
  unsigned int expected[32];
  for (int i = 0; i < 32; i++) expected[i] = i;

  /* ---- Fill vregs and dump ---- */
  fill_vregs();
  dump_vregs(before);
  puts("BEFORE dump:");
  for (int r = 0; r < 8; r++) {
    putchar(' '); puthex(before[r*4]);
    putchar(' '); puthex(before[r*4+1]);
    putchar(' '); puthex(before[r*4+2]);
    putchar(' '); puthex(before[r*4+3]);
    putchar('\n');
  }

  /* Verify BEFORE */
  if (check(before, expected, 32)) {
    puts("FAIL before timer\n"); exit(1);
  }

  puts("NOW busy-wait for timer...\n");
  for (volatile unsigned long i = 0; i < 200000; i++) {
    __asm__ __volatile__ ("nop");
  }
  puts("BACK from timer!\n");

  /* ---- AFTER: dump vregs WITHOUT reloading ---- */
  /* vtype/vl should still be valid from fill_vregs */
  dump_vregs(after);
  puts("AFTER dump:");
  for (int r = 0; r < 8; r++) {
    putchar(' '); puthex(after[r*4]);
    putchar(' '); puthex(after[r*4+1]);
    putchar(' '); puthex(after[r*4+2]);
    putchar(' '); puthex(after[r*4+3]);
    putchar('\n');
  }

  int err = check(after, expected, 32);
  if (err) {
    puts("FAIL at element "); puthex(err-1); putchar('\n');
    exit(1);
  }

  puts("=== VREGS SAVE OK ===\n");
  exit(0);
}
