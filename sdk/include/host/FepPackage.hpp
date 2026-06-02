//******************************************************************************
// Flat Enclave Package (FEP) format definitions
//
// Format (all little-endian):
//   FepHeader (24 bytes):
//     u32 magic        = 0x504B4745 ("EGPK")
//     u32 num_segs
//     u64 rt_entry     = runtime entry VA
//     u64 user_entry   = enclave app entry VA
//
//   FepSegment[num_segs] (32 bytes each):
//     u64 va_base      = page-aligned VA
//     u64 va_pages     = total pages (data + bss)
//     u64 data_offset  = file offset to raw page data
//     u64 data_pages   = pages with file data (non-bss)
//     u32 flags        = PTE flags (R=2,X=8 → 10)
//******************************************************************************
#pragma once

#include <stdint.h>
#include <cstdio>
#include <cstring>

#define FEP_MAGIC  0x504B4745
#define FEP_PAGE_SIZE 4096

struct FepHeader {
  uint32_t magic;
  uint32_t num_segs;
  uint64_t rt_entry;
  uint64_t user_entry;
};

struct FepSegment {
  uint64_t va_base;
  uint64_t va_pages;
  uint64_t data_offset;
  uint64_t data_pages;
  uint32_t flags;
  uint32_t reserved;
};

static inline bool fep_header_valid(const FepHeader* h) {
  return h->magic == FEP_MAGIC && h->num_segs > 0 && h->num_segs < 64;
}
