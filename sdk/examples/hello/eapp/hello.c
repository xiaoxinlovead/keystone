/* Entry point _start calls main, bypassing CRT initialization */
int main(void);

void _start(void)
{
  main();
  /* exit via ecall */
  __asm__ __volatile__ ("li a7, 17\nli a0, 0\necall\n");
  while (1);
}

int main(void)
{
  const char *s = "hello, world!\n";
  while (*s) {
    __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)*s) : "a7","a0");
    s++;
  }
  return 0;
}
