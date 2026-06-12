#include <cstdio>
#include <cstdint>
#include "host/keystone.h"

using namespace Keystone;

static Enclave *g_enclaveB = nullptr;

static void ocall_handler(void *) {
  fprintf(stderr, "[host] A yielded, running B...\n");
  if (g_enclaveB) {
    Error err = g_enclaveB->run();
    if (err != Error::Success)
      fprintf(stderr, "[host] B run failed: %d\n", (int)err);
  }
  fprintf(stderr, "[host] B done, resume A...\n");
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <vec_writer.pkg> <vec_trash.pkg>\n", argv[0]);
    return 1;
  }

  Params params;
  params.setUntrustedMem(0x41000000, 4096);
  params.setFreeMemSize(1024 * 1024);

  Enclave enclaveA;
  fprintf(stderr, "[host] initializing A (%s)...\n", argv[1]);
  Error err = enclaveA.init(argv[1], argv[1], params);
  if (err != Error::Success) { fprintf(stderr, "[host] A init failed\n"); return 1; }
  enclaveA.registerOcallDispatch(ocall_handler);

  Enclave enclaveB;
  fprintf(stderr, "[host] initializing B (%s)...\n", argv[2]);
  err = enclaveB.init(argv[2], argv[2], params);
  if (err != Error::Success) { fprintf(stderr, "[host] B init failed\n"); return 1; }
  g_enclaveB = &enclaveB;

  fprintf(stderr, "[host] running A...\n");
  err = enclaveA.run();
  if (err != Error::Success) {
    fprintf(stderr, "[host] A run failed: %d\n", (int)err);
    return 1;
  }

  volatile uint64_t *utm = (volatile uint64_t *)enclaveA.getSharedBuffer();
  int result = (int)utm[64];
  int nfail = 0;
  for (int i = 0; i < 32; i++)
    if (utm[i] != utm[32 + i]) nfail++;

  fprintf(stderr, "\n=== Vector Context Switch Test ===\n");
  fprintf(stderr, "Saved vs current: %d/%d mismatched\n", nfail, 32);
  for (int i = 0; i < 32; i += 4)
    fprintf(stderr, "  v%02d-v%02d: 0x%016lx 0x%016lx 0x%016lx 0x%016lx\n",
            i, i + 3,
            (unsigned long)utm[i], (unsigned long)utm[i + 1],
            (unsigned long)utm[i + 2], (unsigned long)utm[i + 3]);
  fprintf(stderr, "\n=== RESULT: %s ===\n",
          nfail > 0 ? "FAIL — SM does NOT save/restore vector context"
                    : "PASS — vector context preserved (unexpected)");
  return 0;
}
