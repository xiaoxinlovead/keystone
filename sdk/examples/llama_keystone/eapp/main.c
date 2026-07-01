#include <elf.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Bump allocator for __sbrk (TLS + malloc) */
#define HEAP_SIZE (64 * 1024 * 1024)
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

static void *aligned_alloc_from_heap(size_t alignment, size_t size) {
  if (alignment < sizeof(void *)) {
    alignment = sizeof(void *);
  }
  if ((alignment & (alignment - 1)) != 0) {
    return NULL;
  }

  uintptr_t current = (uintptr_t) heap_brk;
  uintptr_t aligned = (current + alignment - 1) & ~(uintptr_t) (alignment - 1);
  uintptr_t next = aligned + size;
  if (next > (uintptr_t) (heap_pool + HEAP_SIZE) || next < aligned) {
    return NULL;
  }

  heap_brk = (char *) next;
  __curbrk = heap_brk;
  return (void *) aligned;
}

void *malloc(size_t size) {
  uintptr_t aligned = (size + 15) & ~(uintptr_t)15;
  void *p = __sbrk((intptr_t)aligned);
  return p == (void *)-1 ? NULL : p;
}
void free(void *ptr) { (void)ptr; }
void *calloc(size_t nmemb, size_t size) {
  if (size != 0 && nmemb > (SIZE_MAX / size)) return NULL;
  size_t total = nmemb * size;
  void *p = malloc(total);
  if (p) memset(p, 0, total);
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
int posix_memalign(void **memptr, size_t alignment, size_t size) {
  void *ptr = aligned_alloc_from_heap(alignment, size);
  if (ptr == NULL) {
    *memptr = NULL;
    return 12; /* ENOMEM */
  }
  *memptr = ptr;
  return 0;
}
int __posix_memalign(void **memptr, size_t alignment, size_t size) {
  return posix_memalign(memptr, alignment, size);
}
void *aligned_alloc(size_t alignment, size_t size) {
  return aligned_alloc_from_heap(alignment, size);
}
void *__libc_memalign(size_t alignment, size_t size) {
  return aligned_alloc_from_heap(alignment, size);
}
void *__memalign(size_t alignment, size_t size) {
  return aligned_alloc_from_heap(alignment, size);
}
void *memalign(size_t alignment, size_t size) {
  return aligned_alloc_from_heap(alignment, size);
}
void *__libc_valloc(size_t size) {
  return aligned_alloc_from_heap(4096, size);
}
void *__valloc(size_t size) {
  return aligned_alloc_from_heap(4096, size);
}
void *valloc(size_t size) {
  return aligned_alloc_from_heap(4096, size);
}
void *__libc_pvalloc(size_t size) {
  size_t rounded = (size + 4095) & ~(size_t)4095;
  return aligned_alloc_from_heap(4096, rounded);
}
void *__pvalloc(size_t size) {
  return __libc_pvalloc(size);
}
void *pvalloc(size_t size) {
  return __libc_pvalloc(size);
}

/* glibc TLS + libc init */
extern void _dl_aux_init(Elf64_auxv_t *auxv);
extern void __libc_setup_tls(void);

/* console output */
static void sbi_putchar(int ch) {
  __asm__ __volatile__ ("li a7,1\nmv a0,%0\necall\n" : : "r"((unsigned long)ch) : "a7","a0");
}
static void sbi_puts(const char *s) {
  while (*s) sbi_putchar(*s++);
}

static void sbi_exit_enclave(long code) {
  register unsigned long a0 asm("a0") = (unsigned long)code;
  register unsigned long a6 asm("a6") = 3006;
  register unsigned long a7 asm("a7") = 0x08424b45;
  __asm__ __volatile__ ("ecall"
                        : "+r"(a0)
                        : "r"(a6), "r"(a7)
                        : "memory");
  while (1) { }
}

long __libc_write(int fd, const void *buf, size_t len) {
  const char *p = (const char *)buf;
  if (fd != 1 && fd != 2) return -1;
  for (size_t i = 0; i < len; i++) sbi_putchar(p[i]);
  return (long)len;
}
long __write(int fd, const void *buf, size_t len) { return __libc_write(fd, buf, len); }
long write(int fd, const void *buf, size_t len) { return __libc_write(fd, buf, len); }
long __write_nocancel(int fd, const void *buf, size_t len) { return __libc_write(fd, buf, len); }

void _exit(int code) { while (1); }
void _Exit(int code) { _exit(code); }

/* pthread stubs */
#include <pthread.h>
int pthread_mutex_lock(pthread_mutex_t *m) { (void)m; return 0; }
int pthread_mutex_unlock(pthread_mutex_t *m) { (void)m; return 0; }
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) { (void)m; (void)a; return 0; }
int pthread_mutex_destroy(pthread_mutex_t *m) { (void)m; return 0; }
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { (void)c; (void)m; return 0; }
int pthread_cond_signal(pthread_cond_t *c) { (void)c; return 0; }
int pthread_cond_broadcast(pthread_cond_t *c) { (void)c; return 0; }
int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void*), void *d) { (void)t;(void)a;(void)f;(void)d;return 0; }
int pthread_join(pthread_t t, void **r) { (void)t;(void)r;return 0; }
int pthread_detach(pthread_t t) { (void)t;return 0; }
pthread_t pthread_self(void) { return 0; }
int pthread_setaffinity_np(pthread_t t, size_t s, const cpu_set_t *m) { (void)t;(void)s;(void)m;return 0; }
int pthread_setschedparam(pthread_t t, int p, const struct sched_param *m) { (void)t;(void)p;(void)m;return 0; }
int puts(const char *s) { printf("%s\n", s); return 0; }

/* llama/ggml API */
#include "llama.h"
#include "ggml.h"

/* Embedded model */
extern const uint8_t _binary_model_gguf_start[];
extern const uint8_t _binary_model_gguf_end[];
#define MODEL_SIZE ((size_t)(_binary_model_gguf_end - _binary_model_gguf_start))

static Elf64_auxv_t *find_auxv(uintptr_t *sp) {
  uintptr_t argc = sp[0];
  uintptr_t *p = sp + 1 + argc + 1;
  while (*p) p++;
  p++;
  return (Elf64_auxv_t *)p;
}

int main(void) {
  sbi_puts("[llama] Enclave started\n");
  sbi_puts("[llama] Model built-in\n");

  llama_backend_init();
  sbi_puts("[llama] Backend init done\n");

  printf("[llama] ggml version: %s\n", ggml_version());

  sbi_puts("[llama] Done\n");
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
  _dl_aux_init(find_auxv(sp));
  __libc_setup_tls();

  int ret = main();
  sbi_exit_enclave(ret);
}
