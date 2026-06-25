#include "edge/edge_call.h"
#include "host/keystone.h"
#include <cstdio>

using namespace Keystone;

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <enclave.pkg>\n", argv[0]);
    return 1;
  }

  Enclave enclave;
  Params params;
  params.setFreeMemSize(256 * 1024);
  params.setUntrustedMem(0x8f000000, 4096);

  Error err = enclave.init(argv[1], argv[1], params);
  if (err != Error::Success) {
    fprintf(stderr, "[host] enclave init failed: %d\n", (int)err);
    return 1;
  }

  enclave.registerOcallDispatch(incoming_call_dispatch);
  edge_call_init_internals(
      (uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

  Error run_err = enclave.run();
  if (run_err != Error::Success) {
    fprintf(stderr, "[host] enclave run failed: %d\n", (int)run_err);
    return 1;
  }

  return 0;
}
