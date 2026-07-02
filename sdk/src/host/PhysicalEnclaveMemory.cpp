//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "Memory.hpp"
#include <cstring>

namespace Keystone {

PhysicalEnclaveMemory::~PhysicalEnclaveMemory() {
  if (pDevice && epmMappedBase && epmSize)
    pDevice->unmap(reinterpret_cast<void*>(epmMappedBase), epmSize);
}

void
PhysicalEnclaveMemory::init(
    KeystoneDevice* dev, uintptr_t phys_addr, size_t min_pages) {
  pDevice = dev;
  epmSize       = PAGE_SIZE * min_pages;
  epmMappedBase = reinterpret_cast<uintptr_t>(pDevice->map(0, epmSize));
  assert(epmMappedBase);
  rootPageTable = epmMappedBase;
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
  (void)size;
  assert(epmMappedBase);
  return epmMappedBase;
}

uintptr_t
PhysicalEnclaveMemory::readMem(uintptr_t src, size_t size) {
  (void)size;
  assert(epmMappedBase);
  assert(src >= startAddr);
  assert(size <= epmSize);
  assert(src - startAddr <= epmSize - size);
  return epmMappedBase + (src - startAddr);
}

void
PhysicalEnclaveMemory::writeMem(uintptr_t src, uintptr_t dst, size_t size) {
  assert(epmMappedBase);
  assert(dst >= startAddr);
  assert(size <= epmSize);
  assert(dst - startAddr <= epmSize - size);
  void* va_dst = reinterpret_cast<void*>(epmMappedBase + (dst - startAddr));
  memcpy(va_dst, reinterpret_cast<void*>(src), size);
}

}  // namespace Keystone
