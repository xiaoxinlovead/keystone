# Keystone repo guide for agents

## Project

RISC-V secure enclave framework — **fork** adapted for Xiangshan (Kunminghu) RISC-V processor. Monorepo with git submodules.

## Architecture

| Directory | Role |
|---|---|
| `sm/` | Security Monitor (OpenSBI v3-based, M-mode firmware) |
| `sm/plat/nemu_xiangshan/` | Xiangshan SM platform (OpenSBI Kconfig + platform.c + xiangshan_kmh FDT override) |
| `sdk/` | C/C++ library for writing enclaves (cross-compiled for RISC-V) |
| `runtime/` | Eyrie runtime (enclave payload, modular cmake options) |
| `linux-keystone-driver/` | Loadable kernel module for enclave ioctl |
| `linux/` | Linux 6.12.27 submodule (xiaoxinlovead fork) |
| `sm/opensbi/` | OpenSBI v3 submodule (xiaoxinlovead fork) |
| `buildroot/` | Buildroot 2022.08 (direct-tracked, NOT a submodule) |
| `qemu/` | QEMU 7.0.0 (direct-tracked, NOT a submodule; unused by Xiangshan) |
| `bootrom/` | BootROM binary |
| `patches/` | Per-submodule patches (QEMU patch only active; Linux patches commented out) |
| `conf/` | Defconfigs for Linux, buildroot, initramfs |

Entrypoints:
- Root `CMakeLists.txt` — full-system build orchestration via `add_custom_target`
- `sdk/CMakeLists.txt` — standalone SDK build (cross-compiled for RISC-V)
- `runtime/CMakeLists.txt` — standalone runtime build (requires SDK installed)

## Required env vars

- `$RISCV` — RISC-V toolchain install prefix (fatal error if unset)
- `$KEYSTONE_SDK_DIR` — installed SDK path (runtime build errors if unset)

Both set by `source ./source.sh` (currently hardcoded to `/opt/riscv` and `sdk/build64`).

## Setup

```bash
source ./source.sh        # sets RISCV, PATH, KEYSTONE_SDK_DIR
```

Toolchain expected at `/opt/riscv/bin/{riscv64-unknown-linux-gnu-,riscv64-unknown-elf-}*`.
`newfast-setup.sh` builds the SDK and generates `source.sh` (does NOT download toolchain, does NOT auto-clone linux/qemu).

## Build

### Standard (QEMU virt)
```bash
source ./source.sh
mkdir build64 && cd build64
cmake ..
make -j$(nproc)
```

### Xiangshan (NEMU)
```bash
source ./source.sh
mkdir build64-xs && cd build64-xs
cmake .. -DXIANGSHAN=ON
make -j$(nproc)           # builds: linux, buildroot, driver, sm → fw_payload.bin
```

Individual targets: `make linux`, `make buildroot`, `make driver`, `make sm`, `make bootrom`, `make tests`.

### Build dependency chain

```
run-tests → tests + tools + image + fw_jump.bin
image     → buildroot + sm
sm        → linux + buildroot + opensbi (FW_PAYLOAD_PATH=${linux_image})
linux     → buildroot (initramfs mode)
tests     → examples (sdk subdirectory)
quick-test → fw_jump.bin (runs NEMU directly, no SSH)
```

### Build variants (pass to `cmake`)

- RV32: `-DRISCV32=y`
- SiFive FU540: `-DLINUX_SIFIVE=y -DSM_PLATFORM=sifive/fu540`
- CVA6: `-DSM_PLATFORM=cva6`
- Xiangshan: **`-DXIANGSHAN=ON`** (forces `SM_PLATFORM=nemu_xiangshan`, `linux64-xiangshan-defconfig`, `riscv64_xiangshan_defconfig`, initramfs=true)
- Debug: `-DCMAKE_BUILD_TYPE=Debug` or `RelWithDebInfo`
- XS FDT path: `-DXS_FDT_PATH=/path/to/xiangshan.dtb` (defaults to `/home/yangxin/xiangshan-opensbi-linuxkernel/nemu_board/dts/build/xiangshan.dtb`)

## Build system quirks

**Top-level cmake uses `add_custom_target`/`add_custom_command`** (NOT `add_subdirectory`) — it orchestrates external builds, not native cmake targets.

**Dual toolchain**: SM and bootrom use `riscv{BITS}-unknown-elf-` (bare-metal). SDK, Linux, buildroot use `riscv{BITS}-unknown-linux-gnu-` (Linux userspace). Exception: Xiangshan SM build uses `linux-gnu` toolchain (CMakeLists.txt passes `FW_PAYLOAD_PATH` etc. via `-C`).

**Submodules**: Only `linux` (xiaoxinlovead/linux-6.12.27) and `sm/opensbi` (xiaoxinlovead/opensbi, v3) remain as submodules. `buildroot` and `qemu` are direct-tracked files.

**SM platform**: `nemu_xiangshan` is a standalone OpenSBI platform dir with its own `Kconfig`, `config.mk`, `configs/defconfig`, `objects.mk`, and `platform_override_modules.carray`. The `xiangshan_kmh.c` override matches FDT compatibles `"xiangshan,nemu-board"` and `"xiangshan,kunminghu"`.

**Linux defconfig**: `conf/linux64-xiangshan-defconfig` is a standalone (NOT fragment) defconfig — unlike `conf/xiangshan_defconfig` (52-line fragment that requires merging with base).

**Patches in `patches/<submodule>/`** auto-apply via `add_patch()` macro. Linux patches in `patches/linux/` are **commented out** in CMakeLists.txt (lines 210-216) — not auto-applied.

**QEMU** builds both `riscv{BITS}-softmmu` and `riscv{BITS}-linux-user` (needed for SM unit tests). Unused by Xiangshan (uses NEMU instead).

Build artifacts go to the cmake binary dir (`build64/` or `build64-xs/`). Fully out-of-source.

## Runtime build options

Pass to runtime cmake: `-DFREEMEM=on -DPAGING=on -DPAGE_CRYPTO=on -DPAGE_HASH=on -DLINUX_SYSCALL=on -DIO_SYSCALL=on -DNET_SYSCALL=on`

These set `-DUSE_<NAME>` compile flags. The `options_log` target logs which are enabled.

CI tests: Default, FREEMEM, LINUX_SYSCALL+FREEMEM, PAGING, PAGE_CRYPTO, PAGE_HASH, PAGE_CRYPTO+PAGE_HASH.

## Testing

| Scope | Command | Notes |
|---|---|---|
| QEMU integration | `make run-tests` (from build dir) | boots QEMU, SSH in, runs `.ke` packages |
| NEMU enclave | `make quick-test` (from build dir) | runs NEMU directly, checks for hello/SUCCESS, no page faults |
| SDK unit tests | `make check` (from sdk/build dir) | runs on **host** (x86 gcc); requires GoogleTest |
| Runtime tests | `make test` (from runtime/build dir) | cmocka-based |
| SM tests | `cmake .. && make test` (from `sm/tests/build`) | cmocka, cross-compiled, runs in `qemu-riscv{32,64}` user-mode |

**NEMU test script**: `scripts/test-nemu.sh` runs `fw_payload.bin` through NEMU and checks:
- `hello, world` / `SUCCESS` printed (PASS)
- zero `page fault` and `non-handlable` messages
- `exit_enclave` seen (informational)

Env: `NEMU=/home/yangxin/xs-env/NEMU-2026.03.r3/build/riscv64-nemu-interpreter`

QEMU SSH port auto-assigned (3000-5999). On OpenSSH >= 9, passes `-O` flag to `scp`.

## Xiangshan (XS) adaptation status

**Goal**: Boot Keystone on Xiangshan (Kunminghu) via NEMU simulator.

**Current state**: Full stack builds (fw_payload.bin). SM successfully enters enclave. Eyrie runtime silent exit during boot — root cause under investigation (diagnostic prints added in SM trap handler).

| Layer | Status | Details |
|---|---|---|
| CMake config | ✅ | `-DXIANGSHAN=ON` branch (CMakeLists.txt:103-116) |
| Linux defconfig | ✅ | `conf/linux64-xiangshan-defconfig` (standalone, 71 lines) |
| Buildroot config | ✅ | `conf/riscv64_xiangshan_defconfig` |
| SM platform | ✅ | `sm/plat/nemu_xiangshan/` — FDT-based, AIA (APLIC+IMSIC), 8250 DW UART, UARTLITE fallback |
| Hello enclave builds | ✅ | `make tests` produces hello/hello-runner/eyrie-rt and ara_test packages |
| NEMU test harness | ✅ | `scripts/test-nemu.sh`, `make quick-test` |
| Enclave verification | ⏳ | SM enters enclave; runtime hangs before output |

**SM platform details** (`sm/plat/nemu_xiangshan/`):
- Uses UART address `0x40600004` (early_putc fallback), 8250 DW at `0x310B0000` (FDT)
- AIA interrupt controller (APLIC + IMSIC), no PLIC
- DRAM at `0x80000000`, FDT at `0x8000 + 0x2A00000` (42 MB offset to avoid kernel overlap)
- `FW_PAYLOAD_FDT_OFFSET=0x2A00000` in CMakeLists.txt, `FW_JUMP_FDT_ADDR` in config.mk must match

**vec_ctx_test**: New test in `sdk/examples/vec_ctx_test/` proves SM lacks vector context switch:
- Enclave A writes unique patterns to v0-v31, saves to UTM, yields via `sbi_stop`
- Enclave B zeros all v registers, exits
- Enclave A resumes and compares — all 32 registers will show corruption
- Host runner: `vec_ctx_test-runner vec_writer.pkg vec_trash.pkg`
- Currently blocked by runtime silent exit (same root cause as all enclaves)

**External reference**: `/home/yangxin/xiangshan-opensbi-linuxkernel/` has standalone XS Linux + OpenSBI build. Use ONLY for hardware-specific config values — do NOT replace keystone's build pipeline.

**Documentation requirement**: Every source code modification must be recorded with `scripts/record-change.py`:
```bash
python3 scripts/record-change.py --file path/to/file.c --title "Fix X" --reason "Because Y"
```
Or manually in `docs/xiangshan/porting-notes.md` with file path, line number, before/after diff, and reason.

## Known build issues

### Linux kernel: `rseq_execve` / `rseq_syscall` redefinition
Both `include/linux/sched.h` and `include/linux/rseq.h` define the same `static inline` stub. Fix: remove duplicates from `sched.h`.

### Linux kernel: modpost unresolved symbols with fragment configs
`linux-symvers` target runs `make modules` before final vmlinux+initramfs (CMakeLists.txt:254-257). If a fragment disables subsystems (BLOCK, SYSFS) but base defconfig has `CONFIG_FOO=m`, modpost fails. Fix: add `# CONFIG_FOO is not set` to fragment.

### SDK: missing `stdint.h` / `cstdint` with newer toolchains
Files using `uintptr_t` / `uint8_t` may lack includes. Add `#include <stdint.h>` (C) or `#include <cstdint>` (C++) to:
- `sdk/include/host/keystone_user.h`
- `sdk/src/verifier/json11.cpp`

### FDT offset overlaps kernel `.data` / `init_thread_union`
The `FW_PAYLOAD_FDT_OFFSET` in `CMakeLists.txt` must place the FDT **past the kernel load end**. With `FW_TEXT_START=0x80000000`, kernel at `0x80200000`, Image ~36.6 MB → kernel end at `~0x82695000`. Old offset `0x2600000` put FDT at `0x82600000` (inside kernel `.data`), causing `"No DTB passed to the kernel"`. Fix: use `FW_PAYLOAD_FDT_OFFSET=0x2A00000` (FDT at `0x82A00000`, 3.4 MB past kernel end). Keep `FW_JUMP_FDT_ADDR` in `config.mk` in sync.

### medeleg not cleared — enclave ecalls delegated to S-mode instead of M-mode
`sm/src/enclave.c` `context_switch_to_enclave` clears `mideleg` but did NOT clear `medeleg`. With Linux having `CAUSE_USER_ECALL` (bit 8) set in medeleg, every ecall from the enclave (U-mode) was delegated to S-mode (the runtime), where the runtime's `handle_syscall` didn't recognize keystone extension calls (STOP=3004, EXIT=3006). Fix: add `csr_write(medeleg, 0)` alongside the existing `csr_write(mideleg, 0)`.

### SBI ecall convention: a7 = extension_id, a6 = func_id
The SBI v0.2+ convention requires `a7` = extension ID and `a6` = function ID. The original sbi_stop/sbi_exit stubs used `li a7, 1104` (legacy style) or `li a7, 3004` (wrong — puts funcid in a7). Correct keystone SBI call:
```c
li a7, 0x08424b45   // extension: SBI_EXT_EXPERIMENTAL_KEYSTONE_ENCLAVE
li a6, 3004         // func: SBI_SM_STOP_ENCLAVE
li a0, 1            // request: STOP_EDGE_CALL_HOST
ecall
```

### NEMU store buffer: page table writes may not be visible
Write to a page table entry, then read it back — NEMU may return stale data. Fix: insert a barrier or force re-read from physical memory. Seen in runtime's `copy_root_page_table`.

## Code format

SDK and runtime each have `.clang-format` and `make format` target (SDK also runs cpplint).

## Build-test cycle for enclave changes

The cmake `sm` target depends on `attestor-package` which fails on Xiangshan (expects `generic` platform). Workaround: build firmware manually.

```bash
source ./source.sh

# Build enclave pkg + runner
cmake --build build64-xs --target <name>-pkg

# Copy to initramfs directly (not overlay — cmake buildroot fails)
cp build64-xs/examples/<name>/<name>-runner build64-xs/initramfs-sysroot/root/keystone/
cp build64-xs/examples/<name>/enclave.pkg build64-xs/initramfs-sysroot/root/keystone/

# Rebuild linux kernel (embeds initramfs)
make -C linux O=build64-xs/linux.build \
  CONFIG_INITRAMFS_SOURCE="conf/initramfs.txt build64-xs/initramfs-sysroot" \
  CONFIG_INITRAMFS_ROOT_UID="squash" CONFIG_INITRAMFS_ROOT_GID="squash" \
  CONFIG_DEVTMPFS=y CONFIG_DEVTMPFS_MOUNT=y \
  CROSS_COMPILE=riscv64-unknown-linux-gnu- ARCH=riscv

# Rebuild OpenSBI firmware
make -C sm/opensbi O=build64-xs/sm.build \
  PLATFORM_DIR=sm/plat/nemu_xiangshan \
  CROSS_COMPILE=riscv64-unknown-linux-gnu- \
  FW_PAYLOAD_PATH=build64-xs/linux.build/arch/riscv/boot/Image \
  FW_PAYLOAD=y PLATFORM_RISCV_XLEN=64 \
  PLATFORM_RISCV_ISA=rv64imafdc_zifencei PLATFORM_RISCV_ABI=lp64d \
  FW_FDT_PATH=/home/yangxin/xiangshan-opensbi-linuxkernel/nemu_board/dts/build/xiangshan.dtb \
  FW_PAYLOAD_ALIGN=0x200000 FW_PAYLOAD_FDT_OFFSET=0x3000000

# Test
timeout 180 /home/yangxin/xs-env/NEMU-2026.03.r3/build/riscv64-nemu-interpreter \
  -b -I 5000000000 \
  build64-xs/sm.build/platform/nemu_xiangshan/firmware/fw_payload.bin 2>&1
```

**Key gotchas for creating new enclaves:**
- **Enclave runtime page fault? First check for `#include <stdio.h>`.** libc's `printf` accesses uninitialized FILE structs (stdout/stderr) in enclave context (no C runtime init with `-nostartfiles`). This causes NULL deref → page fault delegated to S-mode via medeleg → runtime's `rt_page_fault` calls `sbi_exit_enclave` → SM doesn't handle a7=1101 properly → runtime accesses `shared_buffer` → secondary fault. Fix: replace `<stdio.h>` with `"printf.h"` from `common/` (which provides a bare-metal `printf` via `#define printf printf_`), and include `serial.c` for `_putchar` (direct ecall a7=1).
- Host runner MUST use `params.setUntrustedMem(0x41000000, ...)`. Default `DEFAULT_UNTRUSTED_PTR` (`0xffffffff80000000`) is in the kernel's direct-map region which the enclave page table can't access.
- `enclave.pkg` is a SEPARATE cmake target. Always run `<name>-pkg`.
- For new examples: add `add_subdirectory(<name>)` in `sdk/examples/CMakeLists.txt` and copy commands in top-level `CMakeLists.txt` `image-deps` target.
- After changing initramfs files, must rebuild BOTH linux AND opensbi.
