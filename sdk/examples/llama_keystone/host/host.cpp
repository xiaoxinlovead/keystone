/* llama_keystone host runner:
 * Loads GGUF model file, passes to enclave via UTM. */
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "host/keystone.h"
#include "edge/edge_call.h"

using namespace Keystone;

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <enclave.pkg> <model.gguf>\n", argv[0]);
    return 1;
  }

  /* Open model file */
  int fd = open(argv[2], O_RDONLY);
  if (fd < 0) { perror("open model"); return 1; }

  struct stat st;
  fstat(fd, &st);
  size_t model_size = st.st_size;

  /* mmap model for fast access */
  uint8_t *model_mmap = (uint8_t *)mmap(NULL, model_size, PROT_READ,
                                          MAP_PRIVATE, fd, 0);
  if (!model_mmap) { perror("mmap model"); close(fd); return 1; }
  close(fd);

  fprintf(stderr, "[host] Model: %s (%zu bytes)\n", argv[2], model_size);

  /* Create enclave */
  Enclave enclave;
  Params params;
  params.setFreeMemSize(1024 * 768);   /* 768 KB for runtime */
  params.setUntrustedMem(DEFAULT_UNTRUSTED_PTR, 1024 * 1024);  /* 1 MB UTM */

  Error err = enclave.init(argv[1], argv[1], params);
  if (err != Error::Success) {
    fprintf(stderr, "[host] enclave init failed: %d\n", (int)err);
    return 1;
  }

  enclave.registerOcallDispatch(incoming_call_dispatch);
  edge_call_init_internals(
      (uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

  /* Copy model data to enclave UTM */
  uintptr_t utm_base = (uintptr_t)enclave.getSharedBuffer();
  size_t copy_size = (model_size < 1024 * 1024) ? model_size : (1024 * 1024 - 4096);
  memcpy((void *)utm_base, model_mmap, copy_size);
  fprintf(stderr, "[host] Copied %zu bytes of model to UTM at 0x%lx\n",
          copy_size, utm_base);

  /* Run enclave */
  Error run_err = enclave.run();
  if (run_err != Error::Success) {
    fprintf(stderr, "[host] enclave run failed: %d\n", (int)run_err);
    return 1;
  }

  munmap(model_mmap, model_size);
  fprintf(stderr, "[host] Enclave done\n");
  return 0;
}
