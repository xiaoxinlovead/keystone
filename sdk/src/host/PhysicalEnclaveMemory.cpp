//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "Memory.hpp"
#include <cstring>

namespace Keystone {

void
PhysicalEnclaveMemory::init(
    KeystoneDevice* dev, uintptr_t phys_addr, size_t min_pages) {
  pDevice = dev;
  epmSize       = PAGE_SIZE * min_pages;
  rootPageTable = reinterpret_cast<uintptr_t>(pDevice->map(0, PAGE_SIZE));
  epmFreeList   = phys_addr + PAGE_SIZE;
  startAddr     = phys_addr;
}

uintptr_t
PhysicalEnclaveMemory::allocUtm(size_t size) {
  uintptr_t ret = pDevice->initUTM(size);
  utmFreeList   = ret;
  untrustedSize = size;
  utmPhysAddr   = ret;
  return ret;
}

uintptr_t
PhysicalEnclaveMemory::allocMem(size_t size) {
  assert(pDevice);
  return reinterpret_cast<uintptr_t>(pDevice->map(0, PAGE_SIZE));
}

uintptr_t
PhysicalEnclaveMemory::readMem(uintptr_t src, size_t size) {
  assert(pDevice);
  /* Map the page, read it, unmap immediately.
   * vm.max_map_count must be increased enough for large enclaves. */
  uintptr_t ret = reinterpret_cast<uintptr_t>(
      pDevice->map(src - startAddr, size));
  return ret;
}

void
PhysicalEnclaveMemory::writeMem(uintptr_t src, uintptr_t dst, size_t size) {
  assert(pDevice);
  void* va_dst = pDevice->map(dst - startAddr, size);
  memcpy(va_dst, reinterpret_cast<void*>(src), size);
  pDevice->unmap(va_dst, size);
}

}  // namespace Keystone