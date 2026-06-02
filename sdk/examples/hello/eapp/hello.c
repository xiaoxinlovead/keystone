/* hello enclave: output via ecall directly to SM UART (works in NEMU) */
static void sbi_putchar(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}
static void sbi_exit(int code) {
  __asm__ __volatile__ ("li a7,17\nmv a0,%0\necall\n" : : "r"((unsigned long)code) : "a7","a0");
}
static void my_puts(const char *s) { while (*s) sbi_putchar(*s++); }

void _start(void) {
  my_puts("hello, world!\n");
  my_puts("enclave: SUCCESS\n");
  sbi_exit(0);
  while (1);
}
