#include <cstdio>
#include <cstdint>
#include "host/keystone.h"

using namespace Keystone;

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <vec_writer.pkg>\n", argv[0]);
    return 1;
  }

  Params params;
  params.setUntrustedMem(0x41000000, 4096);
  params.setFreeMemSize(1024 * 1024);

  Enclave enclave;
  fprintf(stderr, "[host] initializing (%s)...\n", argv[1]);
  Error err = enclave.init(argv[1], argv[1], params);
  if (err != Error::Success) {
    fprintf(stderr, "[host] init failed: %d\n", (int)err);
    return 1;
  }

  fprintf(stderr, "[host] running...\n");
  err = enclave.run();
  if (err != Error::Success) {
    fprintf(stderr, "[host] run failed: %d\n", (int)err);
    return 1;
  }

  volatile uint64_t *utm = (volatile uint64_t *)enclave.getSharedBuffer();
  int nfail = 0;
  for (int i = 0; i < 32; i++) {
    uint64_t expected = 0xDEAD000000000000ULL + i;
    if (utm[i] == expected) {
      fprintf(stderr, "  v%02d: 0x%016lx PASS\n", i, (unsigned long)utm[i]);
    } else if (utm[32 + i] == expected) {
      fprintf(stderr, "  v%02d: 0x%016lx (verify pass) PASS\n", i, (unsigned long)utm[32 + i]);
    } else {
      fprintf(stderr, "  v%02d: expected 0x%016lx, got 0x%016lx FAIL\n",
              i, (unsigned long)expected,
              (unsigned long)(utm[i] ? utm[i] : utm[32 + i]));
      nfail++;
    }
  }

  if (nfail == 0)
    fprintf(stderr, "\nRESULT: All 32 vector registers work correctly on NEMU\n");
  else
    fprintf(stderr, "\nRESULT: %d failures\n", nfail);
  return 0;
}
