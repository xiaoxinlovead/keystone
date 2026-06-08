/* hello enclave: ecall-based output (bypass CRT TLS init) */
static void sbi_putchar(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}
static void sbi_exit(int code) {
  __asm__ __volatile__ ("li a7,1101\nmv a0,%0\necall\n" : : "r"((unsigned long)code) : "a7","a0");
}
static void my_puts(const char *s) { while (*s) sbi_putchar(*s++); }

int main(void);
void _start(void) { main(); sbi_exit(0); while (1); }

int main(void) {
  my_puts("hello, world!\n");
  return 0;
}
