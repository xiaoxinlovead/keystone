# Xiangshan Porting Notes

Detailed record of all source code changes made to adapt Keystone for the
Xiangshan (Kunminghu) RISC-V processor.

## Platform integration

### 1. Linux kernel defconfig — `conf/linux64-xiangshan-defconfig`

**New file.** 59-line standalone defconfig seeded from
`xiangshan-opensbi-linuxkernel/nemu_board/configs/xiangshan_defconfig`
(52-line fragment), expanded via `make olddefconfig` and augmented with:

- `CONFIG_SYSFS=y` — required for keystone driver loading
- `CONFIG_MODULES=y` — required for kernel modules
- `CONFIG_BLOCK=y` — re-enabled (fragment disabled it)
- `CONFIG_BINFMT_SCRIPT=y` — required for init scripts
- `CONFIG_BINFMT_ELF=y`, `CONFIG_BINFMT_MISC=y`
- `CONFIG_DEVTMPFS=y`, `CONFIG_DEVTMPFS_MOUNT=y`
- `CONFIG_NET=y`, `CONFIG_INET=y`, `CONFIG_UNIX=y`
- `CONFIG_CGROUPS=y`, `CONFIG_NAMESPACES=y`
- `CONFIG_BPF_SYSCALL=y`
- `CONFIG_IKCONFIG=y`, `CONFIG_IKCONFIG_PROC=y`
- `CONFIG_TMPFS=y`, `CONFIG_SHMEM=y`
- `CONFIG_EPOLL=y`, `CONFIG_SIGNALFD=y`, `CONFIG_TIMERFD=y`
- `CONFIG_POSIX_MQUEUE=y`

Preserved xiangshan-specific options: `8250_DW`, `UARTLITE`, `HVC_RISCV_SBI`,
`RISCV_SBI_V01`, `NONPORTABLE`, `# CONFIG_VIRTIO_MENU is not set`,
`# CONFIG_FTRACE is not set`, `CMDLINE="earlycon=sbi"`.

### 2. CMakeLists.txt — Xiangshan build branch

**Modified.** Added xiangshan platform branch following existing
firesim/sifive/cva6 pattern:

```cmake
# Line 48-50: new CACHE variable
set(XIANGSHAN FALSE CACHE BOOL "Build for Xiangshan platform")

# Lines 100-104: new elseif branch
elseif(xiangshan)
  message(STATUS "Xiangshan configs. Forcing initramfs=y")
  set(linux_defconfig ${confdir}/linux64-xiangshan-defconfig)
  set(initramfs true)
```

Activate with: `cmake .. -DXIANGSHAN=ON`

### 3. SM platform override — `sm/plat/generic/xiangshan_kmh.c`

**New file.** OpenSBI platform override module matching xiangshan device trees
via FDT compatible strings:

- `"xiangshan,nemu-board"` — NEMU simulator
- `"xiangshan,kunminghu"` — Xiangshan Kunminghu hardware/FPGA

Provides `fdt_fixup` callback calling `fdt_reserved_memory_nomap_fixup()`
to mark reserved memory regions as no-map for the kernel.

### 4. SM platform objects — `sm/plat/generic/objects.mk`

**Modified.** Registered xiangshan_kmh in the platform override carray:

```makefile
carray-platform_override_modules-y += xiangshan_kmh
platform-objs-y += xiangshan_kmh.o
```

## GCC 15 / RISC-V toolchain fixes

The RISC-V toolchain (GCC 15.2.0) defaults to `-std=gnu23` and uses modern
RISC-V ISA definitions, requiring several fixes.

### 5. CSR rename: `sbadaddr` → `stval`

**Modified files:**

- `runtime/sys/entry.S:147` — `csrr t0, sbadaddr` → `csrr t0, stval`
- `sm/src/thread.h:60` — struct field `sbadaddr` → `stval`
- `sm/src/thread.c:75` — `LOCAL_SWAP_CSR(sbadaddr)` → `LOCAL_SWAP_CSR(stval)`
- `sm/src/thread.c:117` — `state->prev_csrs.sbadaddr` → `state->prev_csrs.stval`

**Reason:** RISC-V privileged spec v1.10 renamed supervisor bad address CSR from
`sbadaddr` to `stval`. GCC 15 no longer recognizes the old name.

### 6. `bool` typedef conflict in OpenSBI

**Modified:** `sm/opensbi/include/sbi/sbi_types.h:47`

```c
// Before:
typedef int   bool;

// After:
#if __STDC_VERSION__ < 202311L
typedef int   bool;
#endif
```

**Reason:** C23 / GCC 15 makes `bool` a built-in keyword. The OpenSBI typedef
conflicts with the compiler built-in, producing `-Werror` failures.

### 7. ISA string: add `zifencei`

**Modified:** `CMakeLists.txt:64`

```cmake
# Before:
set(ISA rv${BITS}imafdc)

# After:
set(ISA rv${BITS}imafdc_zifencei)
```

**Reason:** GCC 15 separates `fence.i` into the `zifencei` extension.
OpenSBI uses `fence.i` in `lib/sbi/sbi_tlb.c` and requires the extension
to be enabled in the ISA string.

## Linux 6.12.27 kernel compatibility

### 8. `MAX_ORDER` → `MAX_PAGE_ORDER`

**Modified:** `linux-keystone-driver/keystone-page.c:43`

```c
// Before:
if (order < MAX_ORDER)

// After:
if (order < MAX_PAGE_ORDER)
```

**Reason:** Linux 6.0+ renamed the compile-time constant `MAX_ORDER` to
`MAX_PAGE_ORDER` in `include/linux/mmzone.h`. The driver must use the new
name to compile against Linux 6.12.27.

### 9. image-deps — enclave binaries in initramfs

**Modified:** `CMakeLists.txt` (image-deps target, lines 283-287)

Added `"tests"` dependency and copy commands to place enclave binaries
into the buildroot overlay, so they end up in the initramfs:

```cmake
# Before:
add_custom_target("image-deps" DEPENDS "driver" ${overlay_root}
  COMMAND find ${driver_wrkdir} -name "*.ko" -exec cp {} ${overlay_root} \\\\;
)

# After:
add_custom_target("image-deps" DEPENDS "driver" "tests" ${overlay_root}
  COMMAND mkdir -p ${overlay_root}/keystone
  COMMAND find ${driver_wrkdir} -name "*.ko" -exec cp {} ${overlay_root}/keystone \\\\;
  COMMAND cp ${CMAKE_BINARY_DIR}/examples/hello/hello ${overlay_root}/keystone/ ...
  COMMAND cp ${CMAKE_BINARY_DIR}/examples/hello/hello-runner ${overlay_root}/keystone/ ...
)
```

Files end up at `/root/keystone/` in the target rootfs:
- `keystone-driver.ko` — kernel module
- `hello` — enclave payload (eyrie-rt + hello)
- `hello-runner` — host-side runner

### 10. CMake initramfs tar extraction fix

**Modified:** `CMakeLists.txt` (initramfs preparation, lines 213-219)

Fixed a bug where `tar -xpf rootfs.tar` was bundled as a COMMAND inside
the linux kernel build step. CMake did not reliably re-execute it when
only `rootfs.tar` changed.

Split extraction into a separate `add_custom_command` with a stamp file:

```cmake
# Extraction step (runs when rootfs.tar changes)
add_custom_command(OUTPUT ${initramfs_sysroot}/.extracted
  DEPENDS ${initramfs_sysroot} ${buildroot_wrkdir}/images/rootfs.tar
  COMMAND tar -xpf ${buildroot_wrkdir}/images/rootfs.tar -C ${initramfs_sysroot} ...
  COMMAND touch ${initramfs_sysroot}/.extracted
)

# Kernel build depends on stamp file
add_custom_command(OUTPUT ${linux_image} DEPENDS ${initramfs_sysroot}/.extracted ...
  COMMAND $(MAKE) -C ${linux_srcdir} O=${linux_wrkdir} ...
)
```

### 11. DTB embedding in SM firmware

**Procedure** (not yet wired into CMakeLists.txt):

Xiangshan NEMU requires the DTB to be embedded in `fw_payload.bin` via
OpenSBI's `FW_FDT_PATH`:

```bash
make -C sm/opensbi O=build64-xs/sm.build FW_PAYLOAD=y \
  FW_PAYLOAD_PATH=build64-xs/linux.build/arch/riscv/boot/Image \
  FW_FDT_PATH=xiangshan-opensbi-linuxkernel/nemu_board/dts/build/xiangshan.dtb \
  ...
```

DTB source: `xiangshan-opensbi-linuxkernel/nemu_board/dts/DTSGen.py`.
Pre-built DTB at `nemu_board/dts/build/xiangshan.dtb` (1 hart, 128MB DRAM).

## Summary

| # | File | Change | Reason |
|---|---|---|---|
| 1 | `conf/linux64-xiangshan-defconfig` | new | Platform kernel config |
| 2 | `CMakeLists.txt` | mod | Xiangshan cmake branch |
| 3 | `sm/plat/generic/xiangshan_kmh.c` | new | SM FDT override |
| 4 | `sm/plat/generic/objects.mk` | mod | Register override |
| 5 | `runtime/sys/entry.S`, `sm/src/thread.*` | mod | CSR `sbadaddr`→`stval` |
| 6 | `sm/opensbi/include/sbi/sbi_types.h` | mod | `bool` typedef guard |
| 7 | `CMakeLists.txt` ISA | mod | Add `_zifencei` |
| 8 | `linux-keystone-driver/keystone-page.c` | mod | `MAX_ORDER`→`MAX_PAGE_ORDER` |
| 9 | `CMakeLists.txt` image-deps | mod | Enclave binaries in initramfs |
| 10 | `CMakeLists.txt` initramfs | mod | Fix tar extraction (stamp file) |
| 11 | SM build procedure | proc | DTB embedding via `FW_FDT_PATH` |
| 12 | `sm/opensbi/` | repl | OpenSBI v1→v3 (xiangshan opensbi submodule) |
| 13 | `sm/src/sm-sbi.h`, `sm-sbi.c`, `sm-sbi-opensbi.c` | mod | v3 ecall handler API adaptation |
| 14 | `sm/plat/generic/platform.c` | mod | v3 platform.c + sm_init hook |
| 15 | `sm/plat/generic/objects.mk` | mod | v3 compile flags, ecall carray reg |
| 16 | `sm/src/stubs.c` | new | stubs for disabled SM functions |
| 17 | `sm/plat/generic/Kconfig` | new | v3 platform Kconfig |
| 18 | `sm/plat/generic/configs/defconfig` | new | v3 platform feature defconfig |
| 19 | `sm/plat/generic/config.mk` | mod | cppflags for v3 |

## OpenSBI v1 → v3 replacement

### 12. Replace `sm/opensbi/` with xiangshan OpenSBI v3

**Removed:** 284 files of OpenSBI v1.x tracked directly in parent repo.

**Added:** Git submodule pointing to `git@github.com:xiaoxinlovead/opensbi.git`,
branch `hellovector` (OpenSBI v3 with NEMU board support).

```bash
git rm -r --cached sm/opensbi/
# commit removal
git submodule add -b hellovector git@github.com:xiaoxinlovead/opensbi.git sm/opensbi
```

Added to `.gitmodules`:
```
[submodule "opensbi"]
    path = sm/opensbi
    url = git@github.com:xiaoxinlovead/opensbi.git
    branch = hellovector
```

**Reason:** keystone's OpenSBI v1.x cannot boot on NEMU/Xiangshan.
The xiangshan-opensbi-linuxkernel project validated OpenSBI v3 with NEMU.
v3 requires `-fPIE` linker support, so the toolchain was switched from
`riscv64-unknown-elf-` to `riscv64-unknown-linux-gnu-` for the SM build.

### 13. SBI ecall handler API adaptation (v1 → v3)

**Modified:** `sm/src/sm-sbi.h`, `sm/src/sm-sbi.c`, `sm/src/sm-sbi-opensbi.c`

v1 handler signature:
```c
int (* handle)(unsigned long extid, unsigned long funcid,
               const struct sbi_trap_regs *regs,
               unsigned long *out_val,
               struct sbi_trap_info *out_trap);
```

v3 handler signature:
```c
int (* handle)(unsigned long extid, unsigned long funcid,
               struct sbi_trap_regs *regs,
               struct sbi_ecall_return *out);
```

Changes made:
- `sbi_sm_run/resume/exit/stop_enclave`: replaced `struct sbi_trap_info *out_trap`
  parameter with `struct sbi_ecall_return *out`
- `sbi_trap_redirect(regs, out_trap)` → `out->skip_regs_update = true`
  (v3 ecall framework handles trap redirection by checking `skip_regs_update`)
- `*out_val = x` → `out->value = x`
- Removed `const` from `regs` parameter (v3 handler passes non-const)
- Registered extension via `carray-sbi_ecall_exts-y` in objects.mk

### 14. platform.c — v3 base with SM hook

**Modified:** `sm/plat/generic/platform.c`

Replaced keystone-modified v1 platform.c with OpenSBI v3 default platform.c,
adding only `#include "sm.h"` and `sm_init(cold_boot)` call in `generic_final_init()`.

The v3 platform.c uses `struct fdt_driver` (not `struct platform_override`)
for platform override modules — this change is pending (#2.5).

### 15. objects.mk — v3 build system compatibility

**Modified:** `sm/plat/generic/objects.mk`

- Moved `-I../src` from config.mk to objects.mk (v3 ignores config.mk)
- Added `carray-sbi_ecall_exts-y += ecall_keystone_enclave` for ecall registration
- Added `platform-objs-y += platform.o` (v3 platform)
- Added `platform-objs-y += ../../src/stubs.o` for disabled function stubs
- Temporarily disabled: `sbi_trap_hack.c`, `ipi.c`, platform override modules

### 16. stubs.c — stubs for temporarily disabled SM functions

**New file:** `sm/src/stubs.c`

Provides dummy implementations and data symbols:
- `send_and_sync_pmp_ipi()` — PMP IPI synchronization (from ipi.c)
- `sbi_trap_handler_keystone_enclave()` — enclave trap handler (from sbi_trap_hack.c)
- `sanctum_sm_hash/signature/public_key/secret_key`, `sanctum_dev_public_key` —
  secure boot key symbols (from the disabled secure boot patch)
- `platform_override_modules[]` — empty platform override array

### 17-18. v3 platform Kconfig and defconfig

**New files:**
- `sm/plat/generic/Kconfig` — copied from opensbi v3 platform/generic/Kconfig
- `sm/plat/generic/configs/defconfig` — minimal defconfig enabling FDT serial,
  IRQ chip (PLIC, APLIC, IMSIC), IPI, and timer support

### 19. config.mk — include path fix

**Modified:** `sm/plat/generic/config.mk`

Added `platform-cppflags-y = -I$(src_dir)/include -I$(src)/../src` as backup
include path. Note: v3 ignores config.mk; the effective include path is set
in objects.mk via `platform-cflags-y`.

## Commands

### Set up build environment

```bash
source ./source.sh                 # sets $RISCV, $KEYSTONE_SDK_DIR
```

### Build

```bash
mkdir build64-xs && cd build64-xs
cmake .. -DXIANGSHAN=ON
make -j$(nproc)
```

### 16. Fix: Remove redundant SBI ecall registration in `sm.c`

**File**: `sm/src/sm.c` line 134

**Before**:
```c
sbi_ecall_register_extension(&ecall_keystone_enclave);
```

**After**: Removed

**Reason**: OpenSBI v3 framework automatically registers all extensions in
`sbi_ecall_exts[]` via `sbi_ecall_init()` (called from `init_coldboot()`).
The `ecall_keystone_enclave` extension's `register_extensions` callback
(`sm/src/sm-sbi-opensbi.c:87`) already calls `sbi_ecall_register_extension()`.
The direct call in `sm_init()` was a leftover from the OpenSBI v1.x era that
caused a double registration, returning `SBI_EINVAL (-3)`:

```
sbi_ecall_init: [0] name=keyston failed ret=-3
init_coldboot: ecall init failed (error -3)
```

### 17. Revert: rename `.name` from `"keyston"` back to `"keystone"`

**File**: `sm/src/sm-sbi-opensbi.c` line 92

**Before**:
```c
.name = "keyston",
```

**After**:
```c
.name = "keystone",
```

**Reason**: OpenSBI `struct sbi_ecall_extension` has `char name[8]`.
`"keystone"` is exactly 8 characters, which fits (without `\0`).
Debug printing uses `%.8s` format specifier, so no overflow.
The truncation to `"keyston"` (7 chars + `\0`) was unnecessary.

### 18. Xiangshan buildroot config + console setup

**New file**: `conf/riscv64_xiangshan_defconfig`
**Modified**: `CMakeLists.txt` lines 103-111, lines 224-227

Before this change, xiangshan reused the QEMU buildroot config
(`qemu_riscv64_virt_defconfig`), which had no getty configured.
The system booted to a login prompt requiring `root` / `sifive`.

**Changes**:

1. Copied `conf/qemu_riscv64_virt_defconfig` → `conf/riscv64_xiangshan_defconfig`
2. Getty DISABLED — shell spawned directly on `/dev/console` via `::respawn:-/bin/sh`
   in inittab. This avoids hvc0 getty input issues while the DTS specifies
   `console=hvc0` but `/dev/console` handles input correctly.
3. Keystone driver auto-load + hello enclave auto-run added to inittab
   (see section 20).
4. Set `buildroot_config` in CMakeLists.txt xiangshan branch:
   ```cmake
   set(buildroot_config ${confdir}/riscv64_xiangshan_defconfig)
   ```

### 20. Fix: /dev/console interactive shell + auto-run keystone hello

**File**: `CMakeLists.txt` lines 224-227 (initramfs build section)

getty on hvc0 did not accept keyboard input. Fixed by:
- Disabling buildroot getty (removes `hvc0::respawn:/sbin/getty ...` from inittab)
- Adding `::respawn:-/bin/sh` to use `/dev/console` directly
- Adding `::sysinit:` entries to auto-load keystone driver and run hello

**After** (inittab generation):
```make
COMMAND echo "::sysinit:/bin/mount -t devtmpfs devtmpfs /dev" >> .../inittab
COMMAND echo "::sysinit:/sbin/insmod /root/keystone/keystone-driver.ko" >> .../inittab
COMMAND echo "::sysinit:/root/keystone/hello-runner /root/keystone/hello" >> .../inittab
COMMAND echo "::respawn:/sbin/getty -L -n -l /bin/sh /dev/console 0 vt100" >> .../inittab
```

**Execution order at boot**:
1. `::sysinit:` mount devtmpfs
2. `::sysinit:` insert keystone-driver.ko
3. `::sysinit:` run hello-runner (enclave test)
4. `::respawn:` interactive shell on /dev/console (via getty, with terminal echo)

**Rationale**: `::respawn:-/bin/sh` gave a working shell but without terminal echo
(typed characters invisible). getty on `/dev/console` properly initializes the
terminal, including echo. Using `/dev/console` (not hvc0/ttyS0) is correct
because the kernel console subsystem routes I/O through the actual console
driver (hvc0 in this case via `console=hvc0` DTS chosen node).

### 21. Fix: toolchain mismatch via full static linking

**File**: `sdk/examples/CMakeLists.txt` line 33

**Error** (progressive — three stages):
```
# Stage 1: GLIBCXX mismatch
hello-runner: /usr/lib/libstdc++.so.6: version `GLIBCXX_3.4.32' not found

# Stage 2: glibc mismatch (after static libstdc++)
hello-runner: /lib64/libc.so.6: version `GLIBC_2.38' not found
hello-runner: /lib64/libc.so.6: version `GLIBC_2.35' not found

# Stage 3: glibc replacement broke dropbear
dropbear: /lib/libcrypt.so.1: undefined symbol: __snprintf, version GLIBC_PRIVATE
```

**Root cause**: buildroot (Bootlin GCC 10, glibc 2.34) and SDK (/opt/riscv
GCC 15, glibc 2.42) use different toolchains. hello-runner links against
GLIBCXX_3.4.32 (libstdc++) and GLIBC_2.35/2.36/2.38 symbols
(`_dl_find_object`, `arc4random`, `__isoc23_strtoul`) from GCC 15 runtime.
Replacing glibc wholesale broke buildroot binaries (dropbear).

**Fix**: Full static linking — zero dynamic library dependencies:
```cmake
# sdk/examples/CMakeLists.txt
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
```

**Verification**: `readelf -d hello-runner` → "There is no dynamic section in
this file." Binary is 2.4MB, fully self-contained. Rootfs remains untouched
(glibc 2.34, dropbear works).

### 19. Fix: fw_jump.bin symlink for test package dependencies

**File**: `CMakeLists.txt` lines 271-278 (sm target)

**Before**:
```cmake
add_custom_target("sm" ALL DEPENDS "linux" ${sm_wrkdir_exists}
  ...
  COMMENT "Building sm"
)
```

**After** (added two COMMAND lines):
```cmake
  COMMAND ln -sf fw_payload.bin platform/generic/firmware/fw_jump.bin
  COMMAND ln -sf fw_payload.elf platform/generic/firmware/fw_jump.elf
```

**Reason**: The SM build always produces `fw_payload.bin` (via `FW_PAYLOAD=y`),
but `${fw_bin}` and `${fw_elf}` variables point to `fw_jump.bin` / `fw_jump.elf`.
Test packages (attestation, etc.) use `${fw_bin}` as a file dependency for
packaging, which caused `No rule to make target 'fw_jump.bin'` errors.
Symlinks fix this post-build.

### Run on NEMU

```bash
/home/yangxin/xs-env/bin/riscv64-nemu-interpreter -b \
  build64-xs/sm.build/platform/generic/firmware/fw_payload.bin
```

### Run hello enclave (after boot)

```bash
insmod /root/keystone/keystone-driver.ko
/root/keystone/hello-runner /root/keystone/hello /root/keystone/eyrie-rt
```

## Initramfs visibility & SM trap handler fixes

### 7. Initramfs inittab — add echo visibility — `CMakeLists.txt:223-231`

**Before:**
```cmake
COMMAND echo "::sysinit:/bin/mount -t devtmpfs devtmpfs /dev" >> .../inittab || true
COMMAND echo "::sysinit:/sbin/insmod /root/keystone/keystone-driver.ko" >> .../inittab || true
COMMAND echo "::sysinit:/root/keystone/hello-runner ..." >> .../inittab || true
COMMAND echo "::respawn:/sbin/getty -L -n -l /bin/sh /dev/console 0 vt100" >> .../inittab || true
```

**After:** Added `::once:/bin/echo === ... ===` lines before each sysinit step:
```cmake
COMMAND echo "::once:/bin/echo === Keystone init: mounting devtmpfs ===" >> .../inittab || true
COMMAND echo "::sysinit:/bin/mount -t devtmpfs devtmpfs /dev" >> .../inittab || true
COMMAND echo "::once:/bin/echo === Keystone init: loading keystone driver ===" >> .../inittab || true
COMMAND echo "::sysinit:/sbin/insmod /root/keystone/keystone-driver.ko" >> .../inittab || true
COMMAND echo "::once:/bin/echo === Keystone init: running hello enclave ===" >> .../inittab || true
COMMAND echo "::sysinit:/root/keystone/hello-runner ..." >> .../inittab || true
COMMAND echo "::once:/bin/echo === Keystone init: spawning shell ===" >> .../inittab || true
COMMAND echo "::respawn:/sbin/getty -L -n -l /bin/sh /dev/console 0 vt100" >> .../inittab || true
```

**Reason**: Previously the console showed no indication of which init step was running.
If the `hello-runner` (a `::sysinit` command) hangs, BusyBox init never reaches the
`getty` shell line. Echo markers let us see exactly where init stops.

---

### 8. Enable enclave trap handler — `sm/plat/generic/objects.mk:21-23`

**Before:**
```makefile
#platform-objs-y += ../../src/sbi_trap_hack.o  # TODO: port to v3
platform-objs-y += ../../src/trap.o
#platform-objs-y += ../../src/ipi.o  # TODO: port to v3 (sbi_tlb_info API changed)
```

**After:**
```makefile
platform-objs-y += ../../src/sbi_trap_hack.o
platform-objs-y += ../../src/trap.o
platform-objs-y += ../../src/ipi.o
```

**Reason**: The SM's enclave trap handler (`sbi_trap_handler_keystone_enclave`) was
previously an empty stub in `stubs.c` that just returned without processing any trap.
This meant timer interrupts during enclave execution were silently ignored, causing
the `KEYSTONE_IOC_RUN_ENCLAVE` ioctl to block indefinitely in the kernel.
Re-enabling the real trap handler allows the SM to properly stop the enclave
on timer interrupts (setting `SBI_ERR_SM_ENCLAVE_INTERRUPTED`) and resume it.

Also enabled `ipi.c` for cross-hart PMP synchronization.

---

### 9. Fix sbi_trap_hack.c for OpenSBI v3 API — `sm/src/sbi_trap_hack.c`

**Changes:**
- Replaced `#include <sbi/sbi_misaligned_ldst.h>` with `#include <sbi/sbi_trap_ldst.h>`
  (file was renamed in OpenSBI v3)
- Added `#include <sbi/sbi_string.h>` for `sbi_memcpy`
- Created a local `struct sbi_trap_context tcntx` on the stack to wrap `regs`
  into the v3 `sbi_trap_context` format (which includes trap info alongside regs)
- Updated calls to OpenSBI handlers that changed signature to take
  `struct sbi_trap_context *` instead of `struct sbi_trap_regs *`:

| Old API (v1.x) | New API (v3) |
|---|---|
| `sbi_illegal_insn_handler(mtval, regs)` | `sbi_illegal_insn_handler(&tcntx)` |
| `sbi_misaligned_load_handler(mtval, mtval2, mtinst, regs)` | `sbi_misaligned_load_handler(&tcntx)` |
| `sbi_misaligned_store_handler(mtval, mtval2, mtinst, regs)` | `sbi_misaligned_store_handler(&tcntx)` |
| `sbi_ecall_handler(regs)` | `sbi_ecall_handler(&tcntx)` |
| `trap.epc = regs->mepc` (before `sbi_trap_redirect`) | Removed — `sbi_trap_redirect` now reads `mepc` from `regs` directly |

- After each handler call, `sbi_memcpy(regs, &tcntx.regs, ...)` copies back any
  register modifications (e.g., ecall return values in a0/a1)

**Reason**: OpenSBI v3 changed the signature of all trap-handling functions from
taking raw `struct sbi_trap_regs *` to taking `struct sbi_trap_context *` which
embeds regs plus trap cause/value info. Without these fixes, the re-enabled
`sbi_trap_hack.c` would fail to compile.

---

### 10. Fix ipi.c for OpenSBI v3 API — `sm/src/ipi.c` (DISABLED — see note)

**Note: ipi.c remains disabled in `objects.mk`** because OpenSBI v3 removed
the `local_fn` callback field from `struct sbi_tlb_info`. The old PMP IPI
mechanism abused `sbi_tlb_request()` with a custom callback, which is no
longer supported. For single-hart Xiangshan setups, the empty stub in
`stubs.c` is sufficient. Porting to `sbi_ipi_event_create()` is TODO.

The code was partially updated for v3 but not enabled:

**Before:**
```c
ulong mask = 0;
sbi_hsm_hart_interruptible_mask(sbi_domain_thishart_ptr(), &mask);
...
sbi_tlb_request(mask, 0, &tlb_info);
```

**After:**
```c
struct sbi_hartmask mask;
SBI_HARTMASK_INIT(&mask);
sbi_hsm_hart_interruptible_mask(sbi_domain_thishart_ptr(), &mask);
...
sbi_tlb_request(sbi_hartmask_bits(&mask)[0], 0, &tlb_info);
```

Also added `#include <sbi/sbi_hartmask.h>`.

**Reason**: `sbi_hsm_hart_interruptible_mask()` changed its second parameter from
`ulong *` to `struct sbi_hartmask *` (a multi-word bitmap). The hartmask must be
converted back to a `ulong` via `sbi_hartmask_bits(&mask)[0]` for `sbi_tlb_request()`
which still expects a single-word bitmask.

---

### 11. Update stubs.c — remove replaced stubs — `sm/src/stubs.c:7-9`

**Before:**
```c
void send_and_sync_pmp_ipi(void) { }
void sbi_trap_handler_keystone_enclave(struct sbi_trap_regs *regs) { }
```

**After:**
```c
void send_and_sync_pmp_ipi(int region_idx, int type, uint8_t perm) { }
```

**Reason**: The real `sbi_trap_handler_keystone_enclave` in `sbi_trap_hack.c` is
now linked, so its empty stub causes a duplicate symbol error. Removed. Also
fixed `send_and_sync_pmp_ipi` stub signature to match the declaration in `ipi.h`
(`void send_and_sync_pmp_ipi(int, int, uint8_t)` instead of `void`).

---

### 12. Add debug output to hello-runner — `sdk/examples/hello/host/host.cpp`

**Before:** No output at all — silently does `enclave.init()` → `enclave.run()`.

**After:** Added `fprintf(stderr, ...)` at each step:
```cpp
fprintf(stderr, "[hello-runner] Starting...\n");
fprintf(stderr, "[hello-runner] Initializing enclave (eapp=%s, rt=%s)...\n", argv[1], argv[2]);
fprintf(stderr, "[hello-runner] Registering ocall dispatch...\n");
fprintf(stderr, "[hello-runner] Running enclave...\n");
fprintf(stderr, "[hello-runner] Enclave completed successfully!\n");
```

Added `#include <cstdio>`.

**Reason**: The hello-runner had zero user-visible output. If it hung during init
or run, there was no way to tell which step failed. stderr output goes to the console
(via the getty shell's tty).

---

### 13. Fix NULL `out` dereference in trap handler — `sm/src/sbi_trap_hack.c`

**Before:**
```c
sbi_sm_stop_enclave(regs, STOP_TIMER_INTERRUPT, NULL);
sbi_sm_exit_enclave(regs, rc, NULL);
```

**After:**
```c
struct sbi_ecall_return out;
sbi_sm_stop_enclave(regs, STOP_TIMER_INTERRUPT, &out);
sbi_sm_exit_enclave(regs, rc, &out);
```

Also updated `sbi_trap_error()` signature to accept `struct sbi_ecall_return *`.

**Reason**: `sbi_sm_exit_enclave()` and `sbi_sm_stop_enclave()` in `sm-sbi.c:64,73`
directly dereference `out->skip_regs_update = true` without NULL check. Passing
NULL causes M-mode crash → kernel ioctl hangs forever.

---

### 14. Add `-march=rv64gc -mabi=lp64d` to enclave/SDK compilation

**Files modified:**
- `sdk/macros.cmake:34` — `CFLAGS` now includes `-march=rv64gc -mabi=lp64d`
- `runtime/CMakeLists.txt:57` — added `-march=rv64gc -mabi=lp64d`
- `sdk/examples/CMakeLists.txt:35` — added `add_compile_options(-march=rv64gc -mabi=lp64d)`

**Before:** No `-march`/`-mabi` flags set anywhere in the SDK or runtime build
chain. All components inherited the toolchain default ISA which includes V-extension
and B-extension instructions.

**After:** All enclave components (eapp, eyrie-rt, libkeystone-eapp.a) compiled
with `-march=rv64gc`. The actual `.o` files are clean of V/B instructions.

**Caveat:** The statically-linked eapp binary still shows V/B in its ELF ISA
attribute due to the toolchain's glibc (libc.a) which was pre-compiled with
V/B support. The eapp's `__libc_setup_tls` startup path contains V instructions
from glibc. For full V-free enclave, a custom C library compiled without V is needed.

---

### 15. Fix inittab colon separator — `CMakeLists.txt:226-233`

**Before:**
```
::once:/bin/echo === Keystone init: mounting devtmpfs ===
```

**After:**
```
::once:/bin/echo === Keystone init -- mounting devtmpfs ===
```

**Reason**: BusyBox inittab uses `:` as the field delimiter
(`id:runlevel:action:process`). The 4th `:` in the echo message caused the
process field to be truncated (e.g., `mounting devtmpfs ===` was treated as
part of the process command, not the echo argument). This produced `sh: /:
Permission denied` errors and the echo lines appeared out of order.

---

### 16. Fix `XIANGSHAN` cache corruption — cmake rebuild with DTB

**Symptom:** SM built without `FW_FDT_PATH`, kernel had no device tree → no boot.

**Root cause:** During cmake cache manipulation (fixing corrupted `CMAKE_C_FLAGS`),
the `XIANGSHAN:BOOL=FALSE` entry was restored from a stale cache. Without
`XIANGSHAN=y`, the SM was built without the Xiangshan DTB, and the
`CONFIG_EFIVAR_FS=m` kernel module symvers issue resurfaced.

**Fix:** Reconfigured with `-DXIANGSHAN=y`, disabled `EFIVAR_FS` in kernel config,
rebuilt kernel, driver, buildroot, and SM. Verified `FW_FDT_PATH` points to
`xiangshan-opensbi-linuxkernel/nemu_board/dts/build/xiangshan.dtb`.

---

### 20. Fix: Enclave trap handling + `__builtin_unreachable()` removal (branch: `double_trap`)

**Critical design insight**: In Keystone, traps inside the enclave are handled
by the **eyrie-rt runtime (S-mode)** via `encl_trap_handler`, NOT by the SM's
`trap_vector_enclave`. The SM's `sbi_trap_handler_keystone_enclave` only handles
**M-mode interrupts** (timer, IPI) to pause/resume the enclave. All S/U-mode
exceptions (ecalls, page faults) are delegated to S-mode via `medeleg`
and processed by eyrie-rt.

This design is preserved by not clearing `medeleg` in `context_switch_to_enclave`.
The original Keystone code (commit `ecb663854`) only clears `mideleg` (to capture
timer interrupts in M-mode). Adding `csr_write(medeleg, 0)` in the xiangshan port
(commit `db00a0450`) would force ALL exceptions to M-mode, bypassing eyrie-rt.

**Three fixes applied:**

#### 20a. Remove `csr_write(medeleg, 0)` — `sm/src/enclave.c`

```diff
 uintptr_t interrupts = 0;
 csr_write(mideleg, interrupts);
-csr_write(medeleg, interrupts);
```

**Diagnostic confirmed**: `medeleg=0xf4b509` preserved (bit 9 = 0 → S-mode ecall
to M-mode, bit 8 = 1 → U-mode ecall to S-mode).

#### 20b. Remove `__builtin_unreachable()` — `sm/src/sm-sbi-opensbi.c`

Removed from all 4 cases: `SBI_SM_RUN_ENCLAVE`, `SBI_SM_RESUME_ENCLAVE`,
`SBI_SM_STOP_ENCLAVE`, `SBI_SM_EXIT_ENCLAVE`.

**Reason**: In OpenSBI v1.x these functions performed `mret` directly and never
returned. In v3 they return so `sbi_ecall_handler` can apply `skip_regs_update`.
`__builtin_unreachable()` caused the compiler to eliminate the function epilogue,
preventing `sbi_ecall_handler` from properly skipping the `mepc += 4` update.

**Diagnostic confirmed**: `sbi_ecall_handler` shows `skip=1, mepc=0xffffffffc0000000`
(correct enclave entry).

#### 20c. Add `CAUSE_USER_ECALL` handling — `sm/src/sbi_trap_hack.c`

```diff
+case CAUSE_USER_ECALL:
 case CAUSE_SUPERVISOR_ECALL:
 case CAUSE_MACHINE_ECALL:
```

**Reason**: Enclave app runs in U-mode, ecall from U-mode causes mcause=8
which was unhandled. Now forwarded to `sbi_ecall_handler`.

### 21. Add `stvec=0` safety check — `sm/src/sbi_trap_hack.c`

```diff
 default:
+    if (!csr_read(CSR_STVEC)) {
+        msg = "enclave trap with no S-mode handler (stvec=0)";
+        goto trap_error;
+    }
```

**Reason**: `clean_state()` initializes `stvec=0`. If a fault occurs before
eyrie-rt sets up its `stvec`, `sbi_trap_redirect` would jump to VA 0 → infinite
fault cascade. This fix exits the enclave cleanly instead.

### 22. Diagnosed but NOT applied: `sfence.vma` after satp write

Added then reverted. PMP_SET macros already contain `sfence.vma` which covers
TLB coherency. No evidence this was a problem.

### 23. Diagnosed but NOT applied: `mscratch` in `switch_vector_enclave()`

**Hypothesis**: `_trap_handler` saves but never restores `mscratch` after each
trap. `trap_vector_enclave` expects `mscratch` to point to SBI scratch area.
If corrupted, `csrrw tp, mscratch, tp` loads wrong address → `sd t0, 96(tp)`
writes to random physical address → bus error → NEMU double trap.

**Test**: Added `csr_write(CSR_MSCRATCH, sbi_scratch_thishart_ptr())` to
`switch_vector_enclave()`. Result: no change, 'T' still absent.

### Open issue: double trap at PC 0xffffffffc000546c

After applying fixes 20a-20c and 21, the enclave executes ~780M instructions
but no M-mode trap ever reaches `trap_vector_enclave` (confirmed by assembly-level
UART writes of 'T' and 'M' that never appear). The system eventually halts with
NEMU reporting "CRITICAL ERROR: trap when mnstatus.nmie close" at runtime PC
`0xffffffffc000546c` (inside `rt_util_getrandom`).

Key observations:
- `medeleg=0xf4b509` (bit 9=0) means S-mode ecalls SHOULD go to M-mode
- `mideleg` is properly cleared (timer → M-mode)
- `mtvec` points to `trap_vector_enclave` (verified in `[SM] entering enclave` print)
- Eyrie-rt disassembly confirms `sbi_random` executes `ecall` instruction
- Yet `trap_vector_enclave` assembly's very first instruction (UART write) never executes

This suggests the S-mode `ecall` instruction is either being delegated to S-mode
despite `medeleg=0`, or the M-mode trap entry sequence itself fails on the
Xiangshan NEMU simulator.
