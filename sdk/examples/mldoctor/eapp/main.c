extern int run_mldoctor(void);

static void sbi_exit(int code) {
  __asm__ __volatile__ ("li a7, 1101\nmv a0, %0\necall\n" : : "r"((unsigned long)code) : "a7", "a0");
}

void _start(void) {
  __asm__ __volatile__ (
    ".option push\n.option norelax\n"
    "la gp, __global_pointer$\n"
    "la tp, __global_pointer$\n"
    ".option pop\n"
  );
  run_mldoctor();
  sbi_exit(0);
  while (1);
}
