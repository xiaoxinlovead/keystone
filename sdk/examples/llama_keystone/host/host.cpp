/* llama_keystone host runner */
#include <cstdio>
#include "host/keystone.h"
#include "edge/edge_call.h"

using namespace Keystone;

int main(int argc, char **argv) {
  printf("[host] starting...\n");
  if (argc < 2) {
    printf("[host] Usage: %s <enclave.pkg>\n", argv[0]);
    return 1;
  }

  Enclave enclave;
  Params params;
  params.setFreeMemSize(128 * 1024 * 1024);
  params.setUntrustedMem(0x41000000, 1024 * 1024);

  printf("[host] calling init...\n");
  Error err = enclave.init(argv[1], argv[1], params);
  if (err != Error::Success) {
    printf("[host] enclave init failed: %d\n", (int)err);
    return 1;
  }
  printf("[host] init done\n");

  enclave.registerOcallDispatch(incoming_call_dispatch);
  edge_call_init_internals(
      (uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

  printf("[host] Running enclave...\n");
  Error run_err = enclave.run();
  if (run_err != Error::Success) {
    printf("[host] enclave run failed: %d\n", (int)run_err);
    return 1;
  }

  printf("[host] Enclave done\n");
  return 0;
}
