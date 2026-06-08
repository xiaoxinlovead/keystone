//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "edge/edge_call.h"
#include "host/keystone.h"

#include <cstring>

using namespace Keystone;

int
main(int argc, char** argv) {
  Enclave enclave;
  Params params;

  params.setFreeMemSize(256 * 1024);  /* reduced to 256KB for large enclaves */
  params.setUntrustedMem(DEFAULT_UNTRUSTED_PTR, 1024 * 1024);

  /* Support both traditional (2 ELF args) and flat (.pkg) modes */
  Error err;
  if (argc >= 3 && argv[1] && argv[2]) {
    err = enclave.init(argv[1], argv[2], params);
  } else if (argc >= 2 && argv[1]) {
    err = enclave.init(argv[1], argv[1], params);  /* second arg unused for .pkg */
  } else {
    fprintf(stderr, "Usage: %s <enclave.pkg> | <eapp> <eyrie-rt>\n", argv[0]);
    return 1;
  }

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
