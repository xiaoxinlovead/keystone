extern int run_fconv2d(void);
extern int run_fmatmul(void);

static void sbi_putchar(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}
static void sbi_puts(const char *s) { while (*s) sbi_putchar(*s++); }
static void puthex(unsigned long x) {
  for (int i = 60; i >= 0; i -= 4) {
    int nibble = (int)(x >> i) & 0xf;
    sbi_putchar(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
  }
}
static void sbi_exit(int code) {
  __asm__ __volatile__ ("li a7, 1101\nmv a0, %0\necall\n" : : "r"((unsigned long)code) : "a7", "a0");
}

/* Read and print vector CSRs */
static void dump_vec_csr(const char *tag) {
  unsigned long vl, vtype, vlenb;
  __asm__ __volatile__ ("csrr %0, vl"     : "=r"(vl));
  __asm__ __volatile__ ("csrr %0, vtype"   : "=r"(vtype));
  __asm__ __volatile__ ("csrr %0, vlenb"   : "=r"(vlenb));
  sbi_puts(tag); sbi_puts(" vl="); puthex(vl);
  sbi_puts(" vtype="); puthex(vtype);
  sbi_puts(" vlenb="); puthex(vlenb);
  sbi_putchar('\n');
}

void _start(void) {
  __asm__ __volatile__ (
    ".option push\n.option norelax\n"
    "la gp, __global_pointer$\n"
    "la tp, __global_pointer$\n"
    ".option pop\n"
  );

  sbi_putchar('S');
  sbi_putchar('T');
  dump_vec_csr("before_fconv2d");
  sbi_putchar('D');

  run_fconv2d();
  sbi_putchar('A'); /* after run_fconv2d */

  dump_vec_csr("after_fconv2d");
  sbi_puts("fconv2d done\n");

  run_fmatmul();

  dump_vec_csr("after_fmatmul");
  sbi_puts("fmatmul done\n");

  sbi_puts("=[DONE]=\n");
  sbi_exit(0);
  while (1);
}
