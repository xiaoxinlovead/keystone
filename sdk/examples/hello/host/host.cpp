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

  params.setFreeMemSize(1024 * 1024);
  params.setUntrustedMem(DEFAULT_UNTRUSTED_PTR, 1024 * 1024);

  /* Support both traditional (2 ELF args) and flat (.pkg) modes */
  if (argc >= 3 && argv[1] && argv[2]) {
    enclave.init(argv[1], argv[2], params);
  } else if (argc >= 2 && argv[1]) {
    enclave.init(argv[1], argv[1], params);  /* second arg unused for .pkg */
  } else {
    fprintf(stderr, "Usage: %s <enclave.pkg> | <eapp> <eyrie-rt>\n", argv[0]);
    return 1;
  }

  enclave.registerOcallDispatch(incoming_call_dispatch);
  edge_call_init_internals(
      (uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

  enclave.run();

  return 0;
}
