/* hello enclave: write directly to UARTLITE at 0x40600004 */
#define UART_BASE 0x40600000UL
#define UART_TX   0x0004UL

static void uart_putc(char c)
{
  volatile char *tx = (volatile char *)(UART_BASE + UART_TX);
  *tx = c;
}

static void uart_puts(const char *s)
{
  while (*s) {
    if (*s == '\n') uart_putc('\r');
    uart_putc(*s++);
  }
}

void _start(void)
{
  uart_puts("\nhello, world!\n");
  uart_puts("enclave: SUCCESS\n");
  /* exit via illegal instruction to trap to SM */
  __asm__ __volatile__ (".word 0" : : : "memory");
  while (1);
}
