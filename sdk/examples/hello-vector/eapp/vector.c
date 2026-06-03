#include <stdint.h>

static void sbi_putchar(char c) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)c) : "a7","a0");
}
static void sbi_exit(int code) {
  __asm__ __volatile__ ("li a7,1101\nmv a0,%0\necall\n" : : "r"((unsigned long)code) : "a7","a0");
}
static void my_puts(const char *s) { while (*s) sbi_putchar(*s++); }
static void puthex(unsigned long x) {
  sbi_putchar('0'); sbi_putchar('x');
  for (int i = 60; i >= 0; i -= 4) {
    int nibble = (int)(x >> i) & 0xf;
    sbi_putchar(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
  }
}

int main(void);
void _start(void) { main(); sbi_exit(0); while (1); }

int main(void)
{
  my_puts("vector test: ");

  /* Try vector directly — misa is inaccessible from U-mode */
  unsigned long vdata[4] __attribute__((aligned(16))) = {1, 2, 3, 4};
  unsigned long vresult[4] __attribute__((aligned(16))) = {0};

  my_puts("executing vadd.vi...\n");

  __asm__ __volatile__ (
    "vsetvli t0, x0, e32, m1, ta, ma\n"
    "vle32.v v2, (%[src])\n"
    "vadd.vi v2, v2, 10\n"
    "vse32.v v2, (%[dst])\n"
    :
    : [src] "r"(vdata), [dst] "r"(vresult)
    : "memory", "t0"
  );

  my_puts("vadd.vi result: ");
  puthex(vresult[0]); sbi_putchar(' ');
  puthex(vresult[1]); sbi_putchar(' ');
  puthex(vresult[2]); sbi_putchar(' ');
  puthex(vresult[3]); sbi_putchar('\n');
  my_puts("PASS\n");
  sbi_exit(0);
  while (1);
}
