/* llama_keystone: LLM inference inside Keystone enclave */

#include <stdint.h>
#include <string.h>
#include "printf.h"

/* pthread stubs (no system header — avoid type conflicts) */
typedef long pthread_t;
typedef int pthread_mutex_t;
typedef int pthread_cond_t;

int pthread_mutex_lock(pthread_mutex_t *m) { (void)m; return 0; }
int pthread_mutex_unlock(pthread_mutex_t *m) { (void)m; return 0; }
int pthread_mutex_init(pthread_mutex_t *m, const void *a) { (void)m; (void)a; return 0; }
int pthread_mutex_destroy(pthread_mutex_t *m) { (void)m; return 0; }
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { (void)c; (void)m; return 0; }
int pthread_cond_signal(pthread_cond_t *c) { (void)c; return 0; }
int pthread_cond_broadcast(pthread_cond_t *c) { (void)c; return 0; }
int pthread_create(pthread_t *t, const void *a, void *(*f)(void*), void *d) { (void)t; (void)a; (void)f; (void)d; return 0; }
int pthread_join(pthread_t t, void **r) { (void)t; (void)r; return 0; }
int pthread_detach(pthread_t t) { (void)t; return 0; }
pthread_t pthread_self(void) { return 0; }
int pthread_setaffinity_np(pthread_t t, size_t s, const void *m) { (void)t; (void)s; (void)m; return 0; }
int pthread_setschedparam(pthread_t t, int p, const void *m) { (void)t; (void)p; (void)m; return 0; }

/* puts stub (llama library uses puts internally) */
int puts(const char *s) { printf_("%s\n", s); return 0; }

static void sbi_exit(int code) {
  __asm__ __volatile__ ("li a7, 1101\nmv a0, %0\necall\n" : : "r"((unsigned long)code) : "a7", "a0");
}

/* llama/ggml API headers */
#include "llama.h"
#include "ggml.h"

int run_llama(void) {
  printf_("[LLAMA] Enclave started\n");

  /* Initialize llama backend (forces library linking) */
  llama_backend_init();
  printf_("[LLAMA] Backend initialized (ggml_%s)\n", ggml_version());

  printf_("[LLAMA] ggml_ver=%s\n", ggml_version());

  printf_("[LLAMA] Done\n");
  return 0;
}

void _start(void) {
  __asm__ __volatile__ (
    ".option push\n.option norelax\n"
    "la gp, __global_pointer$\n"
    "la tp, __global_pointer$\n"
    ".option pop\n"
  );
  run_llama();
  sbi_exit(0);
  while (1);
}
