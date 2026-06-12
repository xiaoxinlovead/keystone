/* Multi-enclave vector context switch test:
   enclave A: compute -> stop
   enclave B: compute -> exit (corrupts VS via different vector computation)
   enclave A: resume -> compute -> compare -> exit
   If results differ -> VS was corrupted by enclave B */
#include <cstdio>
#include "host/keystone.h"
#include "edge/edge_call.h"

using namespace Keystone;

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <enclave.pkg>\n", argv[0]);
    return 1;
  }
  Params params;
  params.setFreeMemSize(1024 * 1024);
  params.setUntrustedMem(0x41000000, 4096);

  fprintf(stderr, "[host] Creating enclave A\n");
  Enclave enclave_a;
  if (enclave_a.init(argv[1], argv[1], params) != Error::Success) {
    fprintf(stderr, "enclave_a init failed\n"); return 1;
  }
  enclave_a.registerOcallDispatch(incoming_call_dispatch);
  edge_call_init_internals((uintptr_t)NULL, 0);

  fprintf(stderr, "[host] Running enclave A (phase 0: compute + stop)\n");
  Error err = enclave_a.run();
  /* SDK run() handles stop internally and returns Success after completion */
  if (err != Error::Success) {
    fprintf(stderr, "enclave_a run failed: %d\n", (int)err);
    return 1;
  }
  fprintf(stderr, "[host] Enclave A done\n");
  return 0;
}
