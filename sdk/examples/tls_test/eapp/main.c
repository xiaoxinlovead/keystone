#include <elf.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Bump allocator for __sbrk (TLS + malloc) */
#define HEAP_SIZE (4 * 1024 * 1024)
static char heap_pool[HEAP_SIZE] __attribute__((aligned(4096)));
static char *heap_brk = heap_pool;
extern void *__curbrk;
void *__sbrk(intptr_t incr) {
  char *prev = heap_brk;
  char *next = heap_brk + incr;

  if (next > heap_pool + HEAP_SIZE || next < heap_pool)
    return (void*)-1;

  heap_brk = next;
  __curbrk = heap_brk;
  return prev;
}

void *malloc(size_t size) {
  uintptr_t aligned = (size + 15) & ~(uintptr_t)15;
  void *p = __sbrk((intptr_t)aligned);

  return p == (void *)-1 ? NULL : p;
}

void free(void *ptr) {
  (void)ptr;
}

void *calloc(size_t nmemb, size_t size) {
  if (size != 0 && nmemb > (SIZE_MAX / size))
    return NULL;

  size_t total = nmemb * size;
  void *p = malloc(total);
  if (p)
    memset(p, 0, total);
  return p;
}

void *realloc(void *ptr, size_t size) {
  (void)ptr;
  return malloc(size);
}

void *__libc_malloc(size_t size) { return malloc(size); }
void *__malloc(size_t size) { return malloc(size); }
void __libc_free(void *ptr) { free(ptr); }
void __free(void *ptr) { free(ptr); }
void *__libc_calloc(size_t nmemb, size_t size) { return calloc(nmemb, size); }
void *__calloc(size_t nmemb, size_t size) { return calloc(nmemb, size); }
void *__libc_realloc(void *ptr, size_t size) { return realloc(ptr, size); }
void *__realloc(void *ptr, size_t size) { return realloc(ptr, size); }

/* libc internal: call this to set up TLS, then tp is valid */
extern void _dl_aux_init(Elf64_auxv_t *auxv);
extern void __libc_setup_tls(void);
extern void __libc_init_first(int argc, char **argv, char **envp);

static __thread int tls_counter = 7;

static void sbi_exit(int code);

static void sbi_putchar(int ch) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)ch) : "a7","a0");
}

static void sbi_puts(const char *s) {
  while (*s)
    sbi_putchar(*s++);
}

long __libc_write(int fd, const void *buf, size_t len) {
  const char *p = (const char *)buf;

  if (fd != 1 && fd != 2)
    return -1;

  for (size_t i = 0; i < len; i++)
    sbi_putchar(p[i]);

  return (long)len;
}

long __write(int fd, const void *buf, size_t len) {
  return __libc_write(fd, buf, len);
}

long write(int fd, const void *buf, size_t len) {
  return __libc_write(fd, buf, len);
}

long __write_nocancel(int fd, const void *buf, size_t len) {
  return __libc_write(fd, buf, len);
}

void _exit(int code) {
  sbi_exit(code);
  while (1);
}

void _Exit(int code) {
  _exit(code);
}

static Elf64_auxv_t *find_auxv(uintptr_t *sp) {
  uintptr_t argc = sp[0];
  uintptr_t *p = sp + 1 + argc + 1;

  while (*p)
    p++;
  p++;

  return (Elf64_auxv_t *)p;
}

static void sbi_exit(int code) {
  sbi_puts("[tls_test] exit\n");
  __asm__ __volatile__ ("li a7,93\nmv a0,%0\necall\n" : : "r"((unsigned long)code) : "a7","a0");
  sbi_puts("[tls_test] exit returned\n");
}

int main(void) {
  sbi_puts("[tls_test] main\n");
  tls_counter += 35;
  sbi_puts("[tls_test] before printf\n");
  printf("[tls_test] SUCCESS printf works, tls_counter=%d\n", tls_counter);
  sbi_puts("[tls_test] after printf\n");
  fflush(stdout);
  sbi_puts("[tls_test] after fflush\n");
  return 0;
}

void _start(void) {
  uintptr_t *sp;

  __asm__ __volatile__ (
    ".option push\n.option norelax\n"
    "la gp, __global_pointer$\n"
    ".option pop\n"
  );

  __asm__ __volatile__ ("mv %0, sp" : "=r"(sp));
  __curbrk = heap_brk;
  sbi_puts("[tls_test] aux\n");
  _dl_aux_init(find_auxv(sp));
  sbi_puts("[tls_test] tls\n");
  __libc_setup_tls();
  sbi_puts("[tls_test] call main\n");
  sbi_exit(main());
  while (1);
}
