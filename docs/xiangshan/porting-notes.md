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

## Commands

### Set up build environment

```bash
# On xiangshan-opensbi-linuxkernel
source env.sh

# On keystone
source ./source.sh
```

### Build

```bash
mkdir build64-xs && cd build64-xs
cmake .. -DXIANGSHAN=ON
make -j$(nproc)
```

### Embed DTB and build SM

```bash
make -C sm/opensbi O=build64-xs/sm.build PLATFORM_DIR=sm/plat/generic \
  CROSS_COMPILE=riscv64-unknown-elf- FW_PAYLOAD=y \
  FW_PAYLOAD_PATH=build64-xs/linux.build/arch/riscv/boot/Image \
  FW_FDT_PATH=xiangshan-opensbi-linuxkernel/nemu_board/dts/build/xiangshan.dtb \
  PLATFORM_RISCV_XLEN=64 PLATFORM_RISCV_ISA=rv64imafdc_zifencei PLATFORM_RISCV_ABI=lp64d
```

### Run on NEMU

```bash
/home/yangxin/xs-env/bin/riscv64-nemu-interpreter -b \
  build64-xs/sm.build/platform/generic/firmware/fw_payload.bin
```

### Run hello enclave (after boot)

```bash
insmod /root/keystone/keystone-driver.ko
/root/keystone/hello-runner /root/keystone/hello
```
