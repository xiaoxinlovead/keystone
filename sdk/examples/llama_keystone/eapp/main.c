#include <elf.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>

/* Bump allocator for TLS, model metadata, ggml buffers, and KV cache. */
#define HEAP_SIZE (1024ull * 1024ull * 1024ull)
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

static void runtime_exit(long code) {
  register unsigned long a0 asm("a0") = (unsigned long) code;
  register unsigned long a7 asm("a7") = SYS_exit_group;
  __asm__ __volatile__("ecall" : "+r"(a0) : "r"(a7) : "memory");
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
int puts(const char *s) {
  sbi_puts(s);
  sbi_putchar('\n');
  return 0;
}

/* llama/ggml API */
#include "llama.h"
#include "ggml.h"

/* Embedded model */
extern const uint8_t _binary__tmp_model_gguf_start[];
extern const uint8_t _binary__tmp_model_gguf_end[];
#define MODEL_SIZE ((size_t)(_binary__tmp_model_gguf_end - _binary__tmp_model_gguf_start))

#define PROMPT_TEXT "你是谁？请用中文简短回答。"
#define N_PREDICT 64

static void probe_file_access(void) {
  char cwd[128] = {0};
  errno = 0;
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("[llama] cwd=%s\n", cwd);
  } else {
    printf("[llama] getcwd failed errno=%d\n", errno);
  }
  fflush(stdout);

  memset(cwd, 0, sizeof(cwd));
  errno = 0;
  long raw_getcwd = syscall(SYS_getcwd, cwd, sizeof(cwd));
  printf("[llama] raw syscall getcwd => %ld errno=%d cwd=%s\n",
         raw_getcwd, errno, raw_getcwd >= 0 ? cwd : "<null>");
  fflush(stdout);

  errno = 0;
  long raw_open = syscall(SYS_openat, AT_FDCWD, "/etc/inittab", O_RDONLY, 0);
  printf("[llama] raw syscall openat(/etc/inittab) => %ld errno=%d\n",
         raw_open, errno);
  fflush(stdout);
  if (raw_open >= 0) {
    syscall(SYS_close, raw_open);
  }

  static const char *paths[] = {
      ".",
      "/",
      "/root",
      "/root/keystone",
      "/etc",
      "/etc/inittab",
      "/proc/version",
  };

  for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
    FILE *f = fopen(paths[i], "rb");
    printf("[llama] probe fopen(%s) => %s errno=%d\n",
           paths[i], f ? "ok" : "fail", f ? 0 : errno);
    fflush(stdout);
    if (f) {
      fclose(f);
    }
  }
}

static struct llama_model *load_embedded_model(void) {
  static const char model_path[] = "./llama-model.gguf";
  probe_file_access();
  FILE *file = fopen(model_path, "wb+");
  if (!file) {
    printf("[llama] fopen(%s) failed errno=%d\n", model_path, errno);
    fflush(stdout);
    return NULL;
  }

  if (fwrite(_binary__tmp_model_gguf_start, 1, MODEL_SIZE, file) != MODEL_SIZE) {
    printf("[llama] write embedded model failed\n");
    fflush(stdout);
    fclose(file);
    return NULL;
  }

  fflush(file);
  fclose(file);

  file = fopen(model_path, "rb");
  if (!file) {
    printf("[llama] reopen(%s) failed errno=%d\n", model_path, errno);
    fflush(stdout);
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    printf("[llama] fseek end failed errno=%d\n", errno);
    fflush(stdout);
    fclose(file);
    return NULL;
  }

  long size = ftell(file);
  if (size < 0) {
    printf("[llama] ftell failed errno=%d\n", errno);
    fflush(stdout);
    fclose(file);
    return NULL;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    printf("[llama] fseek set failed errno=%d\n", errno);
    fflush(stdout);
    fclose(file);
    return NULL;
  }

  unsigned char head[16] = {0};
  size_t nread = fread(head, 1, sizeof(head), file);
  printf("[llama] staged file size=%ld first16=", size);
  for (size_t i = 0; i < nread; ++i) {
    printf("%02x", head[i]);
  }
  printf("\n");
  fflush(stdout);
  fclose(file);

  struct ggml_context *gguf_ctx = NULL;
  struct gguf_init_params gguf_params = {
    .no_alloc = true,
    .ctx = &gguf_ctx,
  };
  struct gguf_context *gguf = gguf_init_from_file(model_path, gguf_params);
  if (!gguf) {
    printf("[llama] gguf_init_from_file failed\n");
    fflush(stdout);
    return NULL;
  }
  printf("[llama] gguf kv=%d tensors=%d version=%u\n",
         gguf_get_n_kv(gguf), gguf_get_n_tensors(gguf), gguf_get_version(gguf));
  fflush(stdout);
  gguf_free(gguf);

  struct llama_model_params model_params = llama_model_default_params();
  model_params.use_mmap = false;
  model_params.use_mlock = false;

  struct llama_model *model = llama_model_load_from_file(model_path, model_params);
  remove(model_path);
  return model;
}

static int tokenize_prompt(const struct llama_vocab *vocab,
                           const char *prompt,
                           llama_token **out_tokens,
                           int *out_count) {
  int count = -llama_tokenize(vocab, prompt, strlen(prompt), NULL, 0, true, true);
  if (count <= 0) {
    return -1;
  }

  llama_token *tokens = (llama_token *) malloc((size_t) count * sizeof(*tokens));
  if (!tokens) {
    return -1;
  }

  if (llama_tokenize(vocab, prompt, strlen(prompt), tokens, count, true, true) < 0) {
    free(tokens);
    return -1;
  }

  *out_tokens = tokens;
  *out_count = count;
  return 0;
}

static char *format_chat_prompt(struct llama_model *model) {
  const char *tmpl = llama_model_chat_template(model, NULL);
  struct llama_chat_message msg = {
    .role = "user",
    .content = PROMPT_TEXT,
  };

  int32_t len = llama_chat_apply_template(tmpl, &msg, 1, true, NULL, 0);
  if (len <= 0) {
    return NULL;
  }

  char *buf = (char *) malloc((size_t) len + 1);
  if (!buf) {
    return NULL;
  }

  if (llama_chat_apply_template(tmpl, &msg, 1, true, buf, len + 1) < 0) {
    free(buf);
    return NULL;
  }

  buf[len] = '\0';
  return buf;
}

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

  struct llama_model *model = load_embedded_model();
  if (!model) {
    printf("[llama] model load failed\n");
    return 1;
  }

  const struct llama_vocab *vocab = llama_model_get_vocab(model);
  char *prompt = format_chat_prompt(model);
  if (!prompt) {
    printf("[llama] chat template failed\n");
    llama_model_free(model);
    return 1;
  }

  llama_token *prompt_tokens = NULL;
  int n_prompt = 0;
  if (tokenize_prompt(vocab, prompt, &prompt_tokens, &n_prompt) != 0) {
    printf("[llama] prompt tokenize failed\n");
    free(prompt);
    llama_model_free(model);
    return 1;
  }

  struct llama_context_params ctx_params = llama_context_default_params();
  ctx_params.n_ctx = (uint32_t) (n_prompt + N_PREDICT);
  ctx_params.n_batch = (uint32_t) n_prompt;
  ctx_params.no_perf = true;

  struct llama_context *ctx = llama_init_from_model(model, ctx_params);
  if (!ctx) {
    printf("[llama] context init failed\n");
    free(prompt_tokens);
    free(prompt);
    llama_model_free(model);
    return 1;
  }

  struct llama_sampler *smpl =
      llama_sampler_chain_init(llama_sampler_chain_default_params());
  llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

  printf("[llama] Prompt: %s\n", PROMPT_TEXT);
  printf("[llama] Answer: ");

  struct llama_batch batch = llama_batch_get_one(prompt_tokens, n_prompt);
  if (llama_decode(ctx, batch) != 0) {
    printf("\n[llama] prompt decode failed\n");
    llama_sampler_free(smpl);
    llama_free(ctx);
    free(prompt_tokens);
    free(prompt);
    llama_model_free(model);
    return 1;
  }

  for (int i = 0; i < N_PREDICT; ++i) {
    llama_token token = llama_sampler_sample(smpl, ctx, -1);
    if (llama_vocab_is_eog(vocab, token)) {
      break;
    }

    char piece[256];
    int n = llama_token_to_piece(vocab, token, piece, sizeof(piece), 0, true);
    if (n < 0) {
      printf("\n[llama] token to piece failed\n");
      break;
    }
    fwrite(piece, 1, (size_t) n, stdout);
    fflush(stdout);

    batch = llama_batch_get_one(&token, 1);
    if (llama_decode(ctx, batch) != 0) {
      printf("\n[llama] decode failed\n");
      break;
    }
  }

  printf("\n");

  llama_sampler_free(smpl);
  llama_free(ctx);
  free(prompt_tokens);
  free(prompt);
  llama_model_free(model);

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
  runtime_exit(ret);
}
