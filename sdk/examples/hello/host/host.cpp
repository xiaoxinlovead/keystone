//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "edge/edge_call.h"
#include "host/keystone.h"

#include <cstdio>

using namespace Keystone;

int
main(int argc, char** argv) {
  fprintf(stderr, "[hello-runner] Starting...\n");

  Enclave enclave;
  Params params;

  params.setFreeMemSize(1024 * 1024);
  params.setUntrustedMem(DEFAULT_UNTRUSTED_PTR, 1024 * 1024);

  fprintf(stderr, "[hello-runner] Initializing enclave (eapp=%s, rt=%s)...\n",
          argv[1], argv[2]);
  enclave.init(argv[1], argv[2], params);

  fprintf(stderr, "[hello-runner] Registering ocall dispatch...\n");
  enclave.registerOcallDispatch(incoming_call_dispatch);
  edge_call_init_internals(
      (uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

  fprintf(stderr, "[hello-runner] Running enclave...\n");
  enclave.run();

  fprintf(stderr, "[hello-runner] Enclave completed successfully!\n");
  return 0;
}
