#include "edge/edge_call.h"
#include "host/keystone.h"
#include <cstring>

using namespace Keystone;

int main(int argc, char** argv)
{
  Enclave enclave;
  Params params;
  params.setFreeMemSize(1024 * 1024);
  params.setUntrustedMem(DEFAULT_UNTRUSTED_PTR, 1024 * 1024);

  if (argc >= 2 && argv[1])
    enclave.init(argv[1], argv[1], params);
  else {
    fprintf(stderr, "Usage: %s <enclave.pkg>\n", argv[0]);
    return 1;
  }

  enclave.registerOcallDispatch(incoming_call_dispatch);
  edge_call_init_internals(
      (uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());
  enclave.run();
  return 0;
}
