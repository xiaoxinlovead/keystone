#!/usr/bin/env python3
"""
Package eyrie-rt + eapp ELFs into a flat binary container with a simple header.

Output: FlatEnclavePackage (FEP) format

  Header (24 bytes, all little-endian):
    u32 magic        = 0x504B4745 ("EGPK")
    u32 num_segs     = segment count
    u64 rt_entry     = runtime entry VA
    u64 user_entry   = eapp entry VA

  SegmentTable[num_segs] (32 bytes each):
    u64 va_base      = page-aligned start VA
    u64 va_pages     = total pages (data + bss)
    u64 data_offset  = file offset to raw page data
    u64 data_pages   = pages with actual data (non-bss)
    u32 flags        = R=2, W=4, X=8, U=16 (matches keystone PTE bits)

  Raw page data follows segment table, concatenated.
  bss pages are NOT stored in the file (allocated as zeros at load time).

Build-time usage:
  python3 scripts/pkg-flat.py eyrie-rt hello enclave.pkg
"""

import struct
import sys
import os

MAGIC = 0x504B4745  # "EGPK"
PAGE_SIZE = 4096

PT_LOAD = 1
PF_R = 4
PF_W = 2
PF_X = 1

HEADER_FMT = '<IIQQ'
HEADER_SIZE = struct.calcsize(HEADER_FMT)
SEG_FMT = '<QQQQII'
SEG_SIZE = struct.calcsize(SEG_FMT)


def align_up(n):
    return (n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)


def align_down(n):
    return n & ~(PAGE_SIZE - 1)


def elf_flags_to_pte(pf):
    """Convert ELF PF_ flags to keystone PTE flags."""
    flags = 0
    if pf & PF_R: flags |= 2   # PTE_R
    if pf & PF_W: flags |= 4   # PTE_W
    if pf & PF_X: flags |= 8   # PTE_X
    # U bit is set based on whether VA is in user range or kernel range
    return flags


def parse_elf(path):
    """Parse a 64-bit little-endian ELF, return list of (vaddr, filesz, memsz, pf, data)."""
    with open(path, 'rb') as f:
        data = f.read()

    if data[:4] != b'\x7fELF':
        raise ValueError(f"{path}: not an ELF file")
    if data[4] != 2:
        raise ValueError(f"{path}: not 64-bit")
    if data[5] != 1:
        raise ValueError(f"{path}: not little-endian")

    entry = struct.unpack_from('<Q', data, 24)[0]
    phoff = struct.unpack_from('<Q', data, 32)[0]
    phnum = struct.unpack_from('<H', data, 56)[0]

    segments = []
    for i in range(phnum):
        off = phoff + i * 56
        p_type = struct.unpack_from('<I', data, off)[0]
        p_flags = struct.unpack_from('<I', data, off + 4)[0]
        p_offset = struct.unpack_from('<Q', data, off + 8)[0]
        p_vaddr = struct.unpack_from('<Q', data, off + 16)[0]
        p_filesz = struct.unpack_from('<Q', data, off + 32)[0]
        p_memsz = struct.unpack_from('<Q', data, off + 40)[0]

        if p_type != PT_LOAD:
            continue
        if p_filesz == 0 and p_memsz == 0:
            continue

        segments.append((p_vaddr, p_filesz, p_memsz, p_flags, data[p_offset:p_offset + p_filesz]))

    segments.sort(key=lambda s: s[0])
    return entry, segments


def build_package(rt_path, eapp_path, output_path):
    rt_entry, rt_segs = parse_elf(rt_path)
    user_entry, user_segs = parse_elf(eapp_path)

    # Convert raw segments to aligned page entries
    all_entries = []

    for segs, is_user in [(rt_segs, False), (user_segs, True)]:
        for vaddr, filesz, memsz, pf, raw_data in segs:
            va_base = align_down(vaddr)
            va_end = align_up(vaddr + memsz)
            va_pages = (va_end - va_base) // PAGE_SIZE

            pte_flags = elf_flags_to_pte(pf)
            if is_user:
                pte_flags |= 16  # PTE_U

            # How many pages have actual data (non-bss)?
            file_end = vaddr + filesz
            data_pages = (align_up(file_end) - va_base) // PAGE_SIZE
            if data_pages > va_pages:
                data_pages = va_pages
            # data pages may be zero
            if filesz == 0:
                data_pages = 0

            # The page-aligned va_base may be before the actual vaddr;
            # insert zero padding for the gap so data lands at the correct VA.
            gap = vaddr - va_base
            padded = b'\x00' * gap + raw_data[:filesz]
            pad_len = data_pages * PAGE_SIZE - len(padded)
            if pad_len > 0:
                padded = padded + b'\x00' * pad_len

            all_entries.append({
                'va_base': va_base,
                'va_pages': va_pages,
                'data_pages': data_pages,
                'flags': pte_flags,
                'raw': padded,
            })

    # Compute file offsets
    data_offset = HEADER_SIZE + len(all_entries) * SEG_SIZE
    for e in all_entries:
        e['data_offset'] = data_offset
        data_offset += e['data_pages'] * PAGE_SIZE

    # Write output
    with open(output_path, 'wb') as f:
        f.write(struct.pack(HEADER_FMT, MAGIC, len(all_entries), rt_entry, user_entry))
        for e in all_entries:
            f.write(struct.pack(SEG_FMT,
                                e['va_base'], e['va_pages'],
                                e['data_offset'], e['data_pages'],
                                e['flags'], 0))
        for e in all_entries:
            f.write(e['raw'])

    size = os.path.getsize(output_path)
    pages = size // PAGE_SIZE
    print(f"  Packaged {len(all_entries)} segments into {output_path}")
    print(f"  Size: {size} bytes ({pages} pages)")
    print(f"  Runtime entry: 0x{rt_entry:x}, User entry: 0x{user_entry:x}")
    for e in all_entries:
        print(f"    VA=0x{e['va_base']:016x} pages={e['va_pages']:3d} "
              f"data={e['data_pages']:3d} flags=0x{e['flags']:02x}")


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <eyrie-rt> <eapp> <output.pkg>")
        sys.exit(1)
    build_package(sys.argv[1], sys.argv[2], sys.argv[3])


if __name__ == '__main__':
    main()
