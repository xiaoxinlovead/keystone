#include <cstdio>
#include "host/keystone.h"

int main() {
  fprintf(stderr, "before Enclave\n");
  Keystone::Enclave enclave;
  fprintf(stderr, "after Enclave, before Params\n");
  Keystone::Params params;
  fprintf(stderr, "after Params\n");
  return 0;
}
