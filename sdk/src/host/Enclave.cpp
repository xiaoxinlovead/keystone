//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "Enclave.hpp"
#include <math.h>
#include <sys/mman.h>
#include <sys/stat.h>
extern "C" {
#include "./keystone_user.h"
#include "common/sha3.h"
}
#include "ElfFile.hpp"
#include "FepPackage.hpp"
#include "hash_util.hpp"

namespace Keystone {

Enclave::Enclave() {
  runtimeFile = NULL;
  enclaveFile = NULL;
}

Enclave::~Enclave() {
  if (runtimeFile) delete runtimeFile;
  if (enclaveFile) delete enclaveFile;
  destroy();
}

/* ---- Flat binary loading ---- */
static inline unsigned int
fep_flags_to_mode(uint32_t flags) {
  /* flags: 2=R, 4=W, 8=X, 16=U.
     mode: 0=RT_NOEXEC, 1=USER_NOEXEC, 2=RT_FULL, 3=USER_FULL */
  bool is_user = (flags & 16) != 0;
  if (flags & 8) return is_user ? USER_FULL : RT_FULL;
  else           return is_user ? USER_NOEXEC : RT_NOEXEC;
}

Error
Enclave::loadFlatEnclave(const char* pkgpath) {
  fprintf(stderr, "[FEP] opening %s\n", pkgpath);
  FILE* fp = fopen(pkgpath, "rb");
  if (!fp) {
    ERROR("cannot open flat package: %s", pkgpath);
    return Error::FileInitFailure;
  }

  static char nullpage[FEP_PAGE_SIZE] = {0};

  /* Read header */
  FepHeader hdr;
  if (fread(&hdr, sizeof(hdr), 1, fp) != 1 || !fep_header_valid(&hdr)) {
    ERROR("invalid FEP header");
    fclose(fp);
    return Error::FileInitFailure;
  }

  fprintf(stderr, "[FEP] %u segments, rt=0x%lx user=0x%lx\n",
          hdr.num_segs, hdr.rt_entry, hdr.user_entry);

  /* Read segment table */
  FepSegment* segs = new FepSegment[hdr.num_segs];
  if (fread(segs, sizeof(FepSegment), hdr.num_segs, fp) != (size_t)hdr.num_segs) {
    ERROR("failed to read segment table");
    delete[] segs;
    fclose(fp);
    return Error::FileInitFailure;
  }

  /* Pass 1: allocate VA space for ALL segments (no physical pages yet) */
  fprintf(stderr, "[FEP] pass1: allocating VA space\n");
  for (uint32_t i = 0; i < hdr.num_segs; i++) {
    FepSegment* seg = &segs[i];
    if (pMemory->epmAllocVspace(seg->va_base, seg->va_pages)
        != seg->va_pages) {
      ERROR("vspace allocation failed for VA 0x%lx", seg->va_base);
      delete[] segs;
      fclose(fp);
      return Error::VSpaceAllocationFailure;
    }
  }
  fprintf(stderr, "[FEP] pass2: loading pages\n");

  /* Pass 2: load runtime segments (!U bit) with physical pages */
  for (uint32_t pass = 0; pass < 2; pass++) {
    bool loading_runtime = (pass == 0);

    /* snapshot epmFreeList BEFORE allocating physical pages */
    if (loading_runtime) {
      fprintf(stderr, "[FEP] loading runtime pages\n");
      pMemory->startRuntimeMem();
    } else {
      fprintf(stderr, "[FEP] loading eapp pages\n");
      pMemory->startEappMem();
    }

    for (uint32_t i = 0; i < hdr.num_segs; i++) {
      FepSegment* seg = &segs[i];
      bool is_runtime_seg = !(seg->flags & 16);
      if (is_runtime_seg != loading_runtime) continue;

      unsigned int mode = fep_flags_to_mode(seg->flags);

      fseek(fp, seg->data_offset, SEEK_SET);
      for (uint64_t p = 0; p < seg->data_pages; p++) {
        char page[FEP_PAGE_SIZE] = {0};
        size_t nr = fread(page, 1, FEP_PAGE_SIZE, fp);
        if (nr != FEP_PAGE_SIZE && p + 1 != seg->data_pages) {
          ERROR("short read in segment data");
          delete[] segs;
          fclose(fp);
          return Error::ELFLoadFailure;
        }
        if (!pMemory->allocPage(seg->va_base + p * FEP_PAGE_SIZE,
                                (uintptr_t)page, mode)) {
          ERROR("allocPage failed VA 0x%lx",
                seg->va_base + p * FEP_PAGE_SIZE);
          delete[] segs;
          fclose(fp);
          return Error::PageAllocationFailure;
        }
      }

      for (uint64_t p = seg->data_pages; p < seg->va_pages; p++) {
        if (!pMemory->allocPage(seg->va_base + p * FEP_PAGE_SIZE,
                                (uintptr_t)nullpage, mode)) {
          ERROR("bss allocPage failed VA 0x%lx",
                seg->va_base + p * FEP_PAGE_SIZE);
          delete[] segs;
          fclose(fp);
          return Error::PageAllocationFailure;
        }
      }
    }
  }

  fprintf(stderr, "[FEP] loadFlatEnclave done\n");

  flat_rt_entry   = hdr.rt_entry;
  flat_user_entry = hdr.user_entry;

  delete[] segs;
  fclose(fp);
  return Error::Success;
}

uint64_t
calculate_required_pages(uint64_t eapp_sz, uint64_t rt_sz) {
  uint64_t req_pages = 0;

  req_pages += ceil(eapp_sz / PAGE_SIZE);
  req_pages += ceil(rt_sz / PAGE_SIZE);

  /* FIXME: calculate the required number of pages for the page table.
   * We actually don't know how many page tables the enclave might need,
   * because the SDK never knows how its memory will be aligned.
   * Ideally, this should be managed by the driver.
   * For now, we naively allocate enough pages so that we can temporarily get
   * away from this problem.
   * 15 pages will be more than sufficient to cover several hundreds of
   * megabytes of enclave/runtime. */
  req_pages += 15;
  return req_pages;
}

Error
Enclave::loadUntrusted() {
  uintptr_t va_start = ROUND_DOWN(params.getUntrustedMem(), PAGE_BITS);
  uintptr_t va_end   = ROUND_UP(params.getUntrustedEnd(), PAGE_BITS);

  while (va_start < va_end) {
    if (!pMemory->allocPage(va_start, 0, UTM_FULL)) {
      return Error::PageAllocationFailure;
    }
    va_start += PAGE_SIZE;
  }
  return Error::Success;
}

/* This function will be deprecated when we implement freemem */
bool
Enclave::initStack(uintptr_t start, size_t size, bool is_rt) {
  static char nullpage[PAGE_SIZE] = {
      0,
  };
  uintptr_t high_addr    = ROUND_UP(start, PAGE_BITS);
  uintptr_t va_start_stk = ROUND_DOWN((high_addr - size), PAGE_BITS);
  int stk_pages          = (high_addr - va_start_stk) / PAGE_SIZE;

  for (int i = 0; i < stk_pages; i++) {
    if (!pMemory->allocPage(
            va_start_stk, (uintptr_t)nullpage,
            (is_rt ? RT_NOEXEC : USER_NOEXEC)))
      return false;

    va_start_stk += PAGE_SIZE;
  }

  return true;
}

bool
Enclave::mapElf(ElfFile* elf) {
  uintptr_t va;

  assert(elf);

  size_t num_pages =
      ROUND_DOWN(elf->getTotalMemorySize(), PAGE_BITS) / PAGE_SIZE;
  va = elf->getMinVaddr();

  if (pMemory->epmAllocVspace(va, num_pages) != num_pages) {
    ERROR("failed to allocate vspace\n");
    return false;
  }

  return true;
}

Error
Enclave::loadElf(ElfFile* elf) {
  static char nullpage[PAGE_SIZE] = {
      0,
  };

  unsigned int mode = elf->getPageMode();
  for (unsigned int i = 0; i < elf->getNumProgramHeaders(); i++) {
    if (elf->getProgramHeaderType(i) != PT_LOAD) {
      continue;
    }

    uintptr_t start      = elf->getProgramHeaderVaddr(i);
    uintptr_t file_end   = start + elf->getProgramHeaderFileSize(i);
    uintptr_t memory_end = start + elf->getProgramHeaderMemorySize(i);
    char* src            = reinterpret_cast<char*>(elf->getProgramSegment(i));
    uintptr_t va         = start;

    /* FIXME: This is a temporary fix for loading iozone binary
     * which has a page-misaligned program header. */
    if (!IS_ALIGNED(va, PAGE_SIZE)) {
      size_t offset = va - PAGE_DOWN(va);
      size_t length = PAGE_UP(va) - va;
      char page[PAGE_SIZE];
      memset(page, 0, PAGE_SIZE);
      memcpy(page + offset, (const void*)src, length);
      if (!pMemory->allocPage(PAGE_DOWN(va), (uintptr_t)page, mode))
        return Error::PageAllocationFailure;
      va += length;
      src += length;
    }

    /* first load all pages that do not include .bss segment */
    while (va + PAGE_SIZE <= file_end) {
      if (!pMemory->allocPage(va, (uintptr_t)src, mode))
        return Error::PageAllocationFailure;

      src += PAGE_SIZE;
      va += PAGE_SIZE;
    }

    /* next, load the page that has both initialized and uninitialized segments
     */
    if (va < file_end) {
      char page[PAGE_SIZE];
      memset(page, 0, PAGE_SIZE);
      memcpy(page, (const void*)src, static_cast<size_t>(file_end - va));
      if (!pMemory->allocPage(va, (uintptr_t)page, mode))
        return Error::PageAllocationFailure;
      va += PAGE_SIZE;
    }

    /* finally, load the remaining .bss segments */
    while (va < memory_end) {
      if (!pMemory->allocPage(va, (uintptr_t)nullpage, mode))
        return Error::PageAllocationFailure;
      va += PAGE_SIZE;
    }
  }

  return Error::Success;
}

Error
Enclave::validate_and_hash_enclave(struct runtime_params_t args) {
  hash_ctx_t hash_ctx;
  int ptlevel = RISCV_PGLEVEL_TOP;

  hash_init(&hash_ctx);

  // hash the runtime parameters
  hash_extend(&hash_ctx, &args, sizeof(struct runtime_params_t));

  uintptr_t runtime_max_seen = 0;
  uintptr_t user_max_seen    = 0;

  // hash the epm contents including the virtual addresses
  int valid = pMemory->validateAndHashEpm(
      &hash_ctx, ptlevel, reinterpret_cast<pte*>(pMemory->getRootPageTable()),
      0, 0, &runtime_max_seen, &user_max_seen);

  if (valid == -1) {
    return Error::InvalidEnclave;
  }

  hash_finalize(hash, &hash_ctx);

  return Error::Success;
}

bool
Enclave::initFiles(const char* eapppath, const char* runtimepath) {
  if (runtimeFile || enclaveFile) {
    ERROR("ELF files already initialized");
    return false;
  }

  runtimeFile = new ElfFile(runtimepath);
  enclaveFile = new ElfFile(eapppath);

  if (!runtimeFile->initialize(true)) {
    ERROR("Invalid runtime ELF\n");
    destroy();
    return false;
  }

  if (!enclaveFile->initialize(false)) {
    ERROR("Invalid enclave ELF\n");
    destroy();
    return false;
  }

  if (!runtimeFile->isValid()) {
    ERROR("runtime file is not valid");
    destroy();
    return false;
  }
  if (!enclaveFile->isValid()) {
    ERROR("enclave file is not valid");
    destroy();
    return false;
  }

  return true;
}

bool
Enclave::prepareEnclave(uintptr_t alternatePhysAddr) {
  // FIXME: this will be deprecated with complete freemem support.
  // We just add freemem size for now.
  uint64_t minPages;
  minPages = ROUND_UP(params.getFreeMemSize(), PAGE_BITS) / PAGE_SIZE;
  minPages += calculate_required_pages(
      enclaveFile->getTotalMemorySize(), runtimeFile->getTotalMemorySize());

  if (params.isSimulated()) {
    pMemory->init(0, 0, minPages);
    return true;
  }

  /* Call Enclave Driver */
  if (pDevice->create(minPages) != Error::Success) {
    return false;
  }

  /* We switch out the phys addr as needed */
  uintptr_t physAddr;
  if (alternatePhysAddr) {
    physAddr = alternatePhysAddr;
  } else {
    physAddr = pDevice->getPhysAddr();
  }

  pMemory->init(pDevice, physAddr, minPages);
  return true;
}

Error
Enclave::init(const char* eapppath, const char* runtimepath, Params _params) {
  return this->init(eapppath, runtimepath, _params, (uintptr_t)0);
}

const char*
Enclave::getHash() {
  return this->hash;
}

Error
Enclave::init(
    const char* eapppath, const char* runtimepath, Params _params,
    uintptr_t alternatePhysAddr) {
  params = _params;

  if (params.isSimulated()) {
    pMemory = new SimulatedEnclaveMemory();
    pDevice = new MockKeystoneDevice();
  } else {
    pMemory = new PhysicalEnclaveMemory();
    pDevice = new KeystoneDevice();
  }

  /* Detect flat package (*.pkg) vs traditional ELF pair */
  const char* pkg_ext = strrchr(eapppath, '.');
  bool use_flat = (pkg_ext && strcmp(pkg_ext, ".pkg") == 0);

  if (!use_flat) {
    /* ---- Traditional ELF loading ---- */
    if (!initFiles(eapppath, runtimepath)) {
      return Error::FileInitFailure;
    }
  }

  if (!pDevice->initDevice(params)) {
    destroy();
    return Error::DeviceInitFailure;
  }

  if (use_flat) {
    /* Flat package: calculate required pages from file size */
    struct stat st;
    if (stat(eapppath, &st) != 0) {
      destroy();
      return Error::FileInitFailure;
    }
    uintptr_t minPages = (st.st_size / PAGE_SIZE) + 15
                         + ROUND_UP(params.getFreeMemSize(), PAGE_BITS) / PAGE_SIZE;
    if (pDevice->create(minPages) != Error::Success) {
      destroy();
      return Error::DeviceError;
    }
    uintptr_t physAddr = alternatePhysAddr ? alternatePhysAddr
                                           : pDevice->getPhysAddr();
    pMemory->init(pDevice, physAddr, minPages);
  } else {
    if (!prepareEnclave(alternatePhysAddr)) {
      destroy();
      return Error::DeviceError;
    }
  }

  if (use_flat) {
    /* ---- Flat binary loading ---- */
    if (loadFlatEnclave(eapppath) != Error::Success) {
      destroy();
      return Error::ELFLoadFailure;
    }
  } else {
    /* ---- Traditional ELF loading ---- */
    if (!mapElf(runtimeFile)) {
      destroy();
      return Error::VSpaceAllocationFailure;
    }
    pMemory->startRuntimeMem();
    if (loadElf(runtimeFile) != Error::Success) {
      ERROR("failed to load runtime ELF");
      destroy();
      return Error::ELFLoadFailure;
    }
    if (!mapElf(enclaveFile)) {
      destroy();
      return Error::VSpaceAllocationFailure;
    }
    pMemory->startEappMem();
    if (loadElf(enclaveFile) != Error::Success) {
      ERROR("failed to load enclave ELF");
      destroy();
      return Error::ELFLoadFailure;
    }
  }

  fprintf(stderr, "[init] flat load complete, continuing\n");
  fflush(stderr);

/* initialize stack. If not using freemem */
#ifndef USE_FREEMEM
  fprintf(stderr, "[init] allocating stack at 0x%lx\n", DEFAULT_STACK_START);
  fflush(stderr);
  if (!initStack(DEFAULT_STACK_START, DEFAULT_STACK_SIZE, 0)) {
    ERROR("failed to init static stack");
    destroy();
    return Error::PageAllocationFailure;
  }
#endif /* USE_FREEMEM */

  fprintf(stderr, "[init] allocating UTM\n");
  uintptr_t utm_free;
  utm_free = pMemory->allocUtm(params.getUntrustedSize());

  if (!utm_free) {
    ERROR("failed to init untrusted memory - ioctl() failed");
    destroy();
    return Error::DeviceError;
  }

  if (loadUntrusted() != Error::Success) {
    ERROR("failed to load untrusted");
  }

  struct runtime_params_t runtimeParams;
  if (use_flat) {
    runtimeParams.runtime_entry   = flat_rt_entry;
    runtimeParams.user_entry      = flat_user_entry;
  } else {
    runtimeParams.runtime_entry =
        reinterpret_cast<uintptr_t>(runtimeFile->getEntryPoint());
    runtimeParams.user_entry =
        reinterpret_cast<uintptr_t>(enclaveFile->getEntryPoint());
  }
  runtimeParams.untrusted_ptr =
      reinterpret_cast<uintptr_t>(params.getUntrustedMem());
  runtimeParams.untrusted_size =
      reinterpret_cast<uintptr_t>(params.getUntrustedSize());

  fprintf(stderr, "[init] starting freeMem, finalize\n");
  pMemory->startFreeMem();

  /* TODO: This should be invoked with some other function e.g., measure() */
  if (params.isSimulated()) {
    validate_and_hash_enclave(runtimeParams);
  }

  if (pDevice->finalize(
          pMemory->getRuntimePhysAddr(), pMemory->getEappPhysAddr(),
          pMemory->getFreePhysAddr(), runtimeParams) != Error::Success) {
    destroy();
    return Error::DeviceError;
  }
  if (!mapUntrusted(params.getUntrustedSize())) {
    ERROR(
        "failed to finalize enclave - cannot obtain the untrusted buffer "
        "pointer \n");
    destroy();
    return Error::DeviceMemoryMapError;
  }

  fprintf(stderr, "[init] done, finalize OK\n");
  /* ELF files are no longer needed */
  delete enclaveFile;
  delete runtimeFile;
  enclaveFile = NULL;
  runtimeFile = NULL;
  return Error::Success;
}

bool
Enclave::mapUntrusted(size_t size) {
  if (size == 0) {
    return true;
  }

  shared_buffer = pDevice->map(0, size);

  if (shared_buffer == NULL) {
    return false;
  }

  shared_buffer_size = size;

  return true;
}

Error
Enclave::destroy() {
  if (enclaveFile) {
    delete enclaveFile;
    enclaveFile = NULL;
  }

  if (runtimeFile) {
    delete runtimeFile;
    runtimeFile = NULL;
  }

  return pDevice->destroy();
}

Error
Enclave::resume(uintptr_t* retval) {
  return pDevice->resume(retval);
}

Error
Enclave::run(uintptr_t* retval) {
  if (params.isSimulated()) {
    return Error::Success;
  }

  Error ret = pDevice->run(retval);
  while (ret == Error::EdgeCallHost || ret == Error::EnclaveInterrupted) {
    /* enclave is stopped in the middle. */
    if (ret == Error::EdgeCallHost && oFuncDispatch != NULL) {
      oFuncDispatch(getSharedBuffer());
    }
    ret = pDevice->resume(retval);
  }

  if (ret != Error::Success) {
    ERROR("failed to run enclave - ioctl() failed");
    destroy();
    return Error::DeviceError;
  }

  return Error::Success;
}

void*
Enclave::getSharedBuffer() {
  return shared_buffer;
}

size_t
Enclave::getSharedBufferSize() {
  return shared_buffer_size;
}

Error
Enclave::registerOcallDispatch(OcallFunc func) {
  oFuncDispatch = func;
  return Error::Success;
}

}  // namespace Keystone
