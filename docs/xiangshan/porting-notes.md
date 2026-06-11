# Xiangshan (XS) porting notes

## 2026-06-09: FEP data loading bug fix & multi-enclave test

### Summary

Fixed the FlatEnclavePackage (FEP) loader bug where data segment pages
were loaded as zeros despite correct data in the `.pkg` file. Also fixed
a secondary bug in intermediate page table PPN format introduced during
earlier UTM PTE_U changes.

### Fix 1: Intermediate PTE PPN format (`sdk/src/host/Memory.cpp:115`)

**Problem**: `__ept_continue_walk_create` was incorrectly using
`(free_ppn << RISCV_PGSHIFT)` instead of `ptd_create(free_ppn)`.
`RISCV_PGSHIFT` is 12, but the RISC-V PTE format requires PPN at bits
[53:10], i.e. `PPN << PTE_PPN_SHIFT` (10). This 2-bit shift error
caused intermediate page tables to point to physical addresses 4× the
intended value. The SM's `validate_and_hash_epm` rejected the enclave
with `SBI_ERR_SM_ENCLAVE_ILLEGAL_PTE (100015)`.

**Root cause**: A previous change added `PTE_U` to intermediate PTEs
and inadvertently changed `ptd_create(free_ppn)` (which uses `<< 10`)
to `(free_ppn << RISCV_PGSHIFT)` (which uses `<< 12`).

**Diff** (before):
```c
*pte = ptd_create(free_ppn);
```
(after):
```c
*ptePtr = ptd_create(free_ppn);
```

**Note**: Intermediate PTEs must NOT have PTE_U (reserved per RISC-V
privileged spec). `ptd_create` creates PTEs with `(ppn << 10) | PTE_V`,
which is correct.

### Fix 2: USER_NOEXEC missing writeMem (`sdk/src/host/Memory.cpp:80`)

**Problem**: The `USER_NOEXEC` and `RT_NOEXEC` cases in `allocPage()`
created the leaf PTE but never called `writeMem()` to copy data from
the FEP source buffer to the physical page. Data pages (read-only
executable) were left zeroed despite correct data in the `.pkg` file.

**Diff** (before):
```c
case USER_NOEXEC: {
    *pte = pte_create(page_addr, ...);
    break;  // missing writeMem!
}
```
(after):
```c
case USER_NOEXEC: {
    *pte = pte_create(page_addr, ...);
    writeMem(src, ...);  // now copies data
    break;
}
```

Applied identically to `RT_NOEXEC`.

### Verification

After both fixes, the enclave's data arrays read correct non-zero
values:
```
i[0]=3fb62038a4c232d8 i[1]=3fc7ccf201a67a78
f[0]=3feb200ba7ea9f90 f[48]=3fe2b20c3dc3a7a2
```
(These are IEEE 754 hex dumps of expected float values.)

### Multi-enclave switching test (WIP)

The committed test (`sdk/examples/ara_test_context/`) runs 4 enclaves
sequentially and compares results via UTM (untrusted memory at
`0x41000000`). This test revealed a UTM page fault issue:
```
[runtime] page fault at 0x10318 on 0x41000000 (scause: 0xd)
```

**Root cause**: The runtime switches to a new page table during boot
(`csr_write(satp, ...)`). `copy_root_page_table()` should copy the
UTM mapping (VPN[2]=1) from the old root table, but the runtime's new
page table does not have the UTM entry. This causes a store page fault
when the runtime accesses `0x41000000`.

**Status**: BLOCKED. Fix requires either:
1. Fixing UTM page table mapping in the runtime's `copy_root_page_table`
2. Adding UTM fault handling in the SM
3. Using a different communication mechanism (SBI calls instead of UTM)

### Files changed

| File | Change |
|---|---|
| `sdk/src/host/Memory.cpp` | Fix intermediate PTE format, add writeMem to USER_NOEXEC/RT_NOEXEC |
| `sdk/src/host/Enclave.hpp` | Add `resume()` method declaration |
| `sdk/src/host/Enclave.cpp` | Add `resume()` method implementation |

## 2026-06-10: UTM page fault root cause & fix

### Problem
Multi-enclave test failed with `[runtime] page fault at 0x10318 on 0xFFFFFFFF80000000` (UTM VA). The enclave could not access untrusted shared memory from U-mode.

### Root cause chain (three bugs)

**Bug 1: `medeleg` not cleared** (`sm/src/enclave.c:52`)

The SM cleared `mideleg` (interrupt delegation) but NOT `medeleg` (exception
delegation). The Linux kernel delegates page faults to S-mode via `medeleg`.
When the enclave ran, page faults went directly to S-mode (`stvec`), bypassing
the SM's M-mode trap handler (`trap_vector_enclave`). All SM-level page fault
handling code was dead code.

Fix: `csr_write(medeleg, 0)` in `context_switch_to_enclave`.

**Bug 2: NEMU store buffer** (`runtime/sys/boot.c`)

The `copy_root_page_table` loop wrote `root_page_table[510] = old_root[510]`,
but the subsequent read-back (`root_page_table[510]`) returned a DIFFERENT
value. NEMU's store buffer cached the write; the page table walker read the
stale physical memory. UART marker `C` (copy done) followed by `X` (read-back
mismatch) confirmed this.

**Bug 3: SM page fault handler not force-overwriting** (`sm/src/sbi_trap_hack.c`)

The SM fixup checked `!(new_root[l2_idx] & PTE_V)` before overwriting. Since
Bug 2 left a stale-but-valid entry, the condition was false and the fixup
skipped. The stale entry pointed to a wrong L1 page, causing the page table
walk to fail at the L0 level.

Fix: always overwrite `new_root[l2_idx] = old_root[l2_idx]` when the old value
is valid, regardless of the new value's state.

### Files changed (UTM fix)

| File | Change |
|---|---|
| `sm/src/enclave.c` | Add `csr_write(medeleg, 0)`; add `get_enclave_dram_base()` |
| `sm/src/enclave.h` | Declare `get_enclave_dram_base()` |
| `sm/src/sbi_trap_hack.c` | Add `cpu.h`/`page.h` includes; UTM fixup in `default` case |

### Status
- FEP data loading: ✅
- UTM page fault: ✅ (all 5 enclaves run without page fault)
- Multi-enclave verification: ⏳ (computes, needs more NEMU time to complete)
