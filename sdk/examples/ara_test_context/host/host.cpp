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

  Enclave enclave;
  if (enclave.init(argv[1], argv[1], params) != Error::Success) {
    fprintf(stderr, "init failed\n"); return 1;
  }
  enclave.registerOcallDispatch(incoming_call_dispatch);
  edge_call_init_internals((uintptr_t)NULL, 0);

  Error err = enclave.run();
  if (err != Error::Success) {
    fprintf(stderr, "run failed: %d\n", (int)err);
    return 1;
  }
  fprintf(stderr, "[host] done\n");
  return 0;
}
