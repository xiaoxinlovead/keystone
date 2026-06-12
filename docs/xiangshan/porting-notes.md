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

### Fix FDT overlap with kernel stack

**Files:** `CMakeLists.txt`
**Date:** 2026-06-11 20:43

**Reason:** FW_PAYLOAD_FDT_OFFSET=0x2600000 placed FDT at 0x82600000, which overlaps with kernel init_thread_union (.data section). Kernel stack grows from 0x82604000 downward, quickly corrupting the FDT and causing 'No DTB passed' at boot. Changed offset to 0x2A00000 (FDT at 0x82A00000, past kernel end at ~0x82694E10).

```diff
diff --git a/CMakeLists.txt b/CMakeLists.txt
index e228fc916..21dbbfa12 100644
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -280,7 +280,7 @@ add_custom_target("driver" ALL DEPENDS ${driver_srcdir} ${linux_srcdir} "linux-s
 add_custom_target("sm" ALL DEPENDS "linux" "buildroot" ${sm_wrkdir_exists} WORKING_DIRECTORY ${sm_wrkdir}
   COMMAND $(MAKE) -C ${sm_srcdir}/opensbi O=${sm_wrkdir} PLATFORM_DIR=${sm_srcdir}/plat/${platform}
   CROSS_COMPILE=${cross_compile} FW_PAYLOAD_PATH=${linux_image} FW_PAYLOAD=y PLATFORM_RISCV_XLEN=${BITS}
-  PLATFORM_RISCV_ISA=${ISA} PLATFORM_RISCV_ABI=${ABI} FW_FDT_PATH=${xs_fdt} FW_PAYLOAD_ALIGN=0x200000 FW_PAYLOAD_FDT_OFFSET=0x2600000
+   PLATFORM_RISCV_ISA=${ISA} PLATFORM_RISCV_ABI=${ABI} FW_FDT_PATH=${xs_fdt} FW_PAYLOAD_ALIGN=0x200000 FW_PAYLOAD_FDT_OFFSET=0x2A00000
   COMMAND ln -sf fw_payload.bin platform/${platform}/firmware/fw_jump.bin
   COMMAND ln -sf fw_payload.elf platform/${platform}/firmware/fw_jump.elf
   COMMENT "Building sm"
@@ -319,6 +319,9 @@ add_custom_target("image-deps" DEPENDS "driver" "tests" ${overlay_root}
   COMMAND cp ${CMAKE_BINARY_DIR}/examples/ara_test_context/ara_test_both.pkg ${overlay_root}/keystone/ 2>/dev/null || true
   COMMAND cp ${CMAKE_BINARY_DIR}/examples/ara_test_context/ara_test_check.pkg ${overlay_root}/keystone/ 2>/dev/null || true
   COMMAND cp ${CMAKE_BINARY_DIR}/examples/ara_test_context/ara_test_context-runner ${overlay_root}/keystone/ 2>/dev/null || true
+  COMMAND cp ${CMAKE_BINARY_DIR}/examples/vec_ctx_test/vec_writer.pkg ${overlay_root}/keystone/ 2>/dev/null || true
+  COMMAND cp ${CMAKE_BINARY_DIR}/examples/vec_ctx_test/vec_trash.pkg ${overlay_root}/keystone/ 2>/dev/null || true
+  COMMAND cp ${CMAKE_BINARY_DIR}/examples/vec_ctx_test/vec_ctx_test-runner ${overlay_root}/keystone/ 2>/dev/null || true
 )
 add_custom_target("image" DEPENDS "buildroot" "sm"
   COMMENT "Generating image"

```

### Fix FW_JUMP_FDT_ADDR to match new offset

**Files:** `sm/plat/nemu_xiangshan/config.mk`
**Date:** 2026-06-11 20:43

**Reason:** Update FW_JUMP_FDT_ADDR from 0x2200000 to 0x2A00000 to be consistent with CMakeLists.txt FW_PAYLOAD_FDT_OFFSET change. The old 0x82200000 was also within kernel range (kernel ends at ~0x82694E10).

```diff
diff --git a/sm/plat/nemu_xiangshan/config.mk b/sm/plat/nemu_xiangshan/config.mk
index 3323f2574..a64ccd27b 100644
--- a/sm/plat/nemu_xiangshan/config.mk
+++ b/sm/plat/nemu_xiangshan/config.mk
@@ -19,7 +19,7 @@ ifeq ($(PLATFORM_RISCV_XLEN), 32)
 else
   FW_JUMP_ADDR=$(shell printf "0x%X" $$(($(FW_TEXT_START) + 0x200000)))
 endif
-FW_JUMP_FDT_ADDR=$(shell printf "0x%X" $$(($(FW_TEXT_START) + 0x2200000)))
+FW_JUMP_FDT_ADDR=$(shell printf "0x%X" $$(($(FW_TEXT_START) + 0x2A00000)))
 FW_PAYLOAD=y
 ifeq ($(PLATFORM_RISCV_XLEN), 32)
   FW_PAYLOAD_OFFSET=0x400000

```

### Multi-enclave vector context switch test

**Files:** `sdk/examples/vec_ctx_test/eapp_vec_writer.c`, `sdk/examples/vec_ctx_test/eapp_vec_trash.c`, `sdk/examples/vec_ctx_test/host/host.cpp`, `sdk/examples/vec_ctx_test/CMakeLists.txt`
**Date:** 2026-06-11 20:45

**Reason:** Prove SM lacks vector context save/restore via empirical test: Enclave A writes patterns to v0-v31, yields, Enclave B trashes all vector regs to zero, Enclave A resumes and detects corruption

```diff

```

### Fix FDT offset to avoid kernel overlap

**Files:** `CMakeLists.txt`
**Date:** 2026-06-11 20:46

**Reason:** FDT at offset 0x2600000 (addr 0x82600000) overlapped with kernel's init_thread_union, causing 'No DTB passed to kernel' crash. Moved to 0x2A00000 (addr 0x82A00000, 3.4MB past kernel end)

```diff
diff --git a/CMakeLists.txt b/CMakeLists.txt
index e228fc916..21dbbfa12 100644
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -280,7 +280,7 @@ add_custom_target("driver" ALL DEPENDS ${driver_srcdir} ${linux_srcdir} "linux-s
 add_custom_target("sm" ALL DEPENDS "linux" "buildroot" ${sm_wrkdir_exists} WORKING_DIRECTORY ${sm_wrkdir}
   COMMAND $(MAKE) -C ${sm_srcdir}/opensbi O=${sm_wrkdir} PLATFORM_DIR=${sm_srcdir}/plat/${platform}
   CROSS_COMPILE=${cross_compile} FW_PAYLOAD_PATH=${linux_image} FW_PAYLOAD=y PLATFORM_RISCV_XLEN=${BITS}
-  PLATFORM_RISCV_ISA=${ISA} PLATFORM_RISCV_ABI=${ABI} FW_FDT_PATH=${xs_fdt} FW_PAYLOAD_ALIGN=0x200000 FW_PAYLOAD_FDT_OFFSET=0x2600000
+   PLATFORM_RISCV_ISA=${ISA} PLATFORM_RISCV_ABI=${ABI} FW_FDT_PATH=${xs_fdt} FW_PAYLOAD_ALIGN=0x200000 FW_PAYLOAD_FDT_OFFSET=0x2A00000
   COMMAND ln -sf fw_payload.bin platform/${platform}/firmware/fw_jump.bin
   COMMAND ln -sf fw_payload.elf platform/${platform}/firmware/fw_jump.elf
   COMMENT "Building sm"
@@ -319,6 +319,9 @@ add_custom_target("image-deps" DEPENDS "driver" "tests" ${overlay_root}
   COMMAND cp ${CMAKE_BINARY_DIR}/examples/ara_test_context/ara_test_both.pkg ${overlay_root}/keystone/ 2>/dev/null || true
   COMMAND cp ${CMAKE_BINARY_DIR}/examples/ara_test_context/ara_test_check.pkg ${overlay_root}/keystone/ 2>/dev/null || true
   COMMAND cp ${CMAKE_BINARY_DIR}/examples/ara_test_context/ara_test_context-runner ${overlay_root}/keystone/ 2>/dev/null || true
+  COMMAND cp ${CMAKE_BINARY_DIR}/examples/vec_ctx_test/vec_writer.pkg ${overlay_root}/keystone/ 2>/dev/null || true
+  COMMAND cp ${CMAKE_BINARY_DIR}/examples/vec_ctx_test/vec_trash.pkg ${overlay_root}/keystone/ 2>/dev/null || true
+  COMMAND cp ${CMAKE_BINARY_DIR}/examples/vec_ctx_test/vec_ctx_test-runner ${overlay_root}/keystone/ 2>/dev/null || true
 )
 add_custom_target("image" DEPENDS "buildroot" "sm"
   COMMENT "Generating image"

```

### Update JUMP_FDT_ADDR to match CMake FDT offset

**Files:** `sm/plat/nemu_xiangshan/config.mk`
**Date:** 2026-06-11 20:46

**Reason:** Consistency: config.mk FW_JUMP_FDT_ADDR must match CMakeLists.txt FW_PAYLOAD_FDT_OFFSET to keep FDT location in sync when FW_PAYLOAD_FDT_ADDR is used

```diff
diff --git a/sm/plat/nemu_xiangshan/config.mk b/sm/plat/nemu_xiangshan/config.mk
index 3323f2574..a64ccd27b 100644
--- a/sm/plat/nemu_xiangshan/config.mk
+++ b/sm/plat/nemu_xiangshan/config.mk
@@ -19,7 +19,7 @@ ifeq ($(PLATFORM_RISCV_XLEN), 32)
 else
   FW_JUMP_ADDR=$(shell printf "0x%X" $$(($(FW_TEXT_START) + 0x200000)))
 endif
-FW_JUMP_FDT_ADDR=$(shell printf "0x%X" $$(($(FW_TEXT_START) + 0x2200000)))
+FW_JUMP_FDT_ADDR=$(shell printf "0x%X" $$(($(FW_TEXT_START) + 0x2A00000)))
 FW_PAYLOAD=y
 ifeq ($(PLATFORM_RISCV_XLEN), 32)
   FW_PAYLOAD_OFFSET=0x400000

```

### Add vec_ctx_test to build and boot

**Files:** `CMakeLists.txt`, `sdk/examples/CMakeLists.txt`, `conf/S90keystone`
**Date:** 2026-06-11 20:46

**Reason:** Register vec_ctx_test example, copy artifacts to initramfs, and run on boot via S90keystone

```diff
diff --git a/CMakeLists.txt b/CMakeLists.txt
index e228fc916..21dbbfa12 100644
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -280,7 +280,7 @@ add_custom_target("driver" ALL DEPENDS ${driver_srcdir} ${linux_srcdir} "linux-s
 add_custom_target("sm" ALL DEPENDS "linux" "buildroot" ${sm_wrkdir_exists} WORKING_DIRECTORY ${sm_wrkdir}
   COMMAND $(MAKE) -C ${sm_srcdir}/opensbi O=${sm_wrkdir} PLATFORM_DIR=${sm_srcdir}/plat/${platform}
   CROSS_COMPILE=${cross_compile} FW_PAYLOAD_PATH=${linux_image} FW_PAYLOAD=y PLATFORM_RISCV_XLEN=${BITS}
-  PLATFORM_RISCV_ISA=${ISA} PLATFORM_RISCV_ABI=${ABI} FW_FDT_PATH=${xs_fdt} FW_PAYLOAD_ALIGN=0x200000 FW_PAYLOAD_FDT_OFFSET=0x2600000
+   PLATFORM_RISCV_ISA=${ISA} PLATFORM_RISCV_ABI=${ABI} FW_FDT_PATH=${xs_fdt} FW_PAYLOAD_ALIGN=0x200000 FW_PAYLOAD_FDT_OFFSET=0x2A00000
   COMMAND ln -sf fw_payload.bin platform/${platform}/firmware/fw_jump.bin
   COMMAND ln -sf fw_payload.elf platform/${platform}/firmware/fw_jump.elf
   COMMENT "Building sm"
@@ -319,6 +319,9 @@ add_custom_target("image-deps" DEPENDS "driver" "tests" ${overlay_root}
   COMMAND cp ${CMAKE_BINARY_DIR}/examples/ara_test_context/ara_test_both.pkg ${overlay_root}/keystone/ 2>/dev/null || true
   COMMAND cp ${CMAKE_BINARY_DIR}/examples/ara_test_context/ara_test_check.pkg ${overlay_root}/keystone/ 2>/dev/null || true
   COMMAND cp ${CMAKE_BINARY_DIR}/examples/ara_test_context/ara_test_context-runner ${overlay_root}/keystone/ 2>/dev/null || true
+  COMMAND cp ${CMAKE_BINARY_DIR}/examples/vec_ctx_test/vec_writer.pkg ${overlay_root}/keystone/ 2>/dev/null || true
+  COMMAND cp ${CMAKE_BINARY_DIR}/examples/vec_ctx_test/vec_trash.pkg ${overlay_root}/keystone/ 2>/dev/null || true
+  COMMAND cp ${CMAKE_BINARY_DIR}/examples/vec_ctx_test/vec_ctx_test-runner ${overlay_root}/keystone/ 2>/dev/null || true
 )
 add_custom_target("image" DEPENDS "buildroot" "sm"
   COMMENT "Generating image"
diff --git a/conf/S90keystone b/conf/S90keystone
index c90f018f6..49408b48a 100755
--- a/conf/S90keystone
+++ b/conf/S90keystone
@@ -1,5 +1,5 @@
 #!/bin/sh
-echo "Starting keystone ara_test_context..."
+echo "Starting keystone vec_ctx_test..."
 mount -t proc proc /proc 2>&1
 mount -t sysfs sysfs /sys 2>&1
 modprobe keystone_driver 2>&1 || insmod /root/keystone/keystone-driver.ko 2>&1
@@ -7,4 +7,5 @@ MINOR=$(grep keystone_enclave /proc/misc 2>/dev/null | awk '{print $1}')
 if [ -n "$MINOR" ]; then
   mknod /dev/keystone_enclave c 10 $MINOR 2>&1
 fi
-/root/keystone/ara_test_context-runner /root/keystone/ara_test_both.pkg
+/root/keystone/vec_ctx_test-runner /root/keystone/vec_writer.pkg /root/keystone/vec_trash.pkg
+echo "=== vec_ctx_test done ==="
diff --git a/sdk/examples/CMakeLists.txt b/sdk/examples/CMakeLists.txt
index b7ca67c0f..b7205499f 100644
--- a/sdk/examples/CMakeLists.txt
+++ b/sdk/examples/CMakeLists.txt
@@ -46,3 +46,4 @@ add_subdirectory(tests)
 add_subdirectory(ara_test_keystone)
 add_subdirectory(ara_test_context)
 add_subdirectory(hello-vector)
+add_subdirectory(vec_ctx_test)

```

### Clear medeleg in context_switch_to_enclave

**Files:** `sm/src/enclave.c`
**Date:** 2026-06-12 16:10

**Reason:** medeleg had CAUSE_USER_ECALL (bit 8) set from Linux, causing enace ecalls to be delegated to S-mode (runtime) instead of M-mode (SM). This prevented all keystone SBI calls (STOP, EXIT) from reaching the SM's handler. The enclave stack trace showed 'illegal instruction' at runtime entry+0x10 because the runtime was trying to handle ecalls it didn't understand.

```diff
diff --git a/sm/src/enclave.c b/sm/src/enclave.c
index 302630e49..3495a508b 100644
--- a/sm/src/enclave.c
+++ b/sm/src/enclave.c
@@ -41,14 +41,19 @@ extern byte dev_public_key[PUBLIC_KEY_SIZE];
 static inline void context_switch_to_enclave(struct sbi_trap_regs* regs,
                                                 enclave_id eid,
                                                 int load_parameters){
+  sbi_printf("[SM] ctx->enclave: eid=%d load=%d host_mepc_pre=0x%lx thread_mepc=0x%lx\n",
+             eid, load_parameters, regs->mepc, enclaves[eid].threads[0].prev_mepc);
   /* save host context */
   swap_prev_state(&enclaves[eid].threads[0], regs, 1);
   swap_prev_mepc(&enclaves[eid].threads[0], regs, regs->mepc);
   swap_prev_mstatus(&enclaves[eid].threads[0], regs, regs->mstatus);
+  sbi_printf("[SM] ctx->enclave: after_swap thread_mepc=0x%lx regs_mepc=0x%lx\n",
+             enclaves[eid].threads[0].prev_mepc, regs->mepc);
 
   uintptr_t interrupts = 0;
   sbi_printf("[SM] medeleg(before)=0x%lx, mideleg(before)=0x%lx\n",
              csr_read(medeleg), csr_read(mideleg));
+  csr_write(medeleg, 0);  /* no exception delegation — enclave ecalls must go to M-mode */
   csr_write(mideleg, interrupts);
 
   if(load_parameters) {
@@ -168,6 +173,9 @@ static inline void context_switch_to_host(struct sbi_trap_regs *regs,
     enclave_id eid,
     int return_on_resume){
 
+  sbi_printf("[SM] ctx->host: eid=%d resume=%d regs_mepc=0x%lx thread_mepc=0x%lx\n",
+             eid, return_on_resume, regs->mepc, enclaves[eid].threads[0].prev_mepc);
+
   // set PMP
   int memid;
   for(memid=0; memid < ENCLAVE_REGIONS_MAX; memid++) {
@@ -184,6 +192,8 @@ static inline void context_switch_to_host(struct sbi_trap_regs *regs,
   swap_prev_state(&enclaves[eid].threads[0], regs, return_on_resume);
   swap_prev_mepc(&enclaves[eid].threads[0], regs, regs->mepc);
   swap_prev_mstatus(&enclaves[eid].threads[0], regs, regs->mstatus);
+  sbi_printf("[SM] ctx->host: after_swap regs_mepc=0x%lx thread_mepc=0x%lx\n",
+             regs->mepc, enclaves[eid].threads[0].prev_mepc);
 
   switch_vector_host();
 

```

### Revert medeleg clear — ecalls must go to S-mode runtime

**Files:** `sm/src/enclave.c`
**Date:** 2026-06-12 17:12

**Reason:** U-mode ecalls are delegated to S-mode (runtime) via medeleg. The runtime processes syscalls and forwards keystone SBI calls to SM via S-mode ecalls. Clearing medeleg would break this architecture.

```diff

```

### Fix SBI calls: use runtime syscall numbers

**Files:** `sdk/examples/vec_ctx_test/eapp_vec_writer.c`, `sdk/examples/vec_ctx_test/eapp_vec_trash.c`, `sdk/examples/vec_ctx_test/host/host.cpp`
**Date:** 2026-06-12 17:12

**Reason:** Enclave apps must use runtime syscall numbers (a7=1104/1101) not modern SBI extension convention (a7=0x08424B45)

```diff

```
