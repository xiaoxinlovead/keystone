/* TLS test host runner */
#include <cstdio>
#include "host/keystone.h"

using namespace Keystone;

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("[host] Usage: %s <eapp>\n", argv[0]);
    return 1;
  }

  Enclave enclave;
  Params params;

  Error err = enclave.init(argv[1], argv[1], params);
  if (err != Error::Success) {
    printf("[host] enclave init failed: %d\n", (int)err);
    return 1;
  }

  printf("[host] Running enclave...\n");
  Error run_err = enclave.run();
  if (run_err != Error::Success) {
    printf("[host] enclave run failed: %d\n", (int)run_err);
    return 1;
  }

  printf("[host] Enclave done\n");
  return 0;
}
