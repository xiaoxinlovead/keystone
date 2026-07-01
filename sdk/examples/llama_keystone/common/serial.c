#include <stdint.h>

/* Use ecall a7=1 (SBI legacy putchar) for enclave output to SM UART */
void _putchar(char character) {
  __asm__ __volatile__ ("li a7, 1\nmv a0, %0\necall\n" : : "r"((unsigned long)character) : "a7", "a0");
}
