# Xiangshan Porting Notes

Detailed record of all source code changes made to adapt Keystone for the
Xiangshan (Kunminghu) RISC-V processor.

## FEP packaging bug fix

### File: `scripts/pkg-flat.py` — segment data alignment

**Bug:** The `build_package` function set `va_base = align_down(vaddr)` but wrote the ELF segment data directly to the file without inserting zero padding for the gap between the page-aligned `va_base` and the actual ELF segment `vaddr`. This caused the entire data segment (including GOT, data, bss) to be shifted earlier in the virtual address space by `(vaddr - va_base)` bytes.

**Effect:** Code that accessed global data via the GOT (such as `__libc_setup_tls`) read from the wrong virtual address, getting zeros instead of the expected pointer, leading to a NULL pointer dereference at `runtime/util/rt_util.c:62`.

Before:
```python
padded = raw_data[:filesz]
pad_len = data_pages * PAGE_SIZE - len(padded)
if pad_len > 0:
    padded = padded + b'\x00' * pad_len
```

After:
```python
gap = vaddr - va_base
padded = b'\x00' * gap + raw_data[:filesz]
pad_len = data_pages * PAGE_SIZE - len(padded)
if pad_len > 0:
    padded = padded + b'\x00' * pad_len
```

Root cause: the FEP writer padded only to the end of the page, but not the beginning. Since most RISC-V ELF segments have page-aligned VAs this went unnoticed; it manifests when a data segment VA (e.g., `0x782f8`) is not page-aligned.

## Enclave libc startup fix — bypass glibc TLS init

### Files: `sdk/examples/hello/eapp/hello.c`, `sdk/examples/hello/CMakeLists.txt`

**Problem:** Even with the FEP alignment fix, the hello enclave crashed at `__libc_setup_tls` (user code VA `0x10688`). The statically-linked glibc's TLS initialization (`_dl_ns`, `_dl_tls_*`) expects runtime dynamic linker data structures that are not initialized in the bare-metal enclave environment.

**Fix:** Skip glibc's CRT startup entirely. Use `_start` as entry point (via `-nostartfiles -Wl,-e,_start`), call `main()` manually from `_start`. Output via inline ecall a7=1 (forwarded by runtime to SM UART).

```c
void _start(void) { main(); /* exit ecall */ }
int main(void) {
  /* ecall a7=1 output */
  while (*s) __asm__("li a7,1\nmv a0,%0\necall\n" : : "r"(*s++));
}
```

**glibc function compatibility with CRT bypassed:**

| Works | Broken |
|---|---|
| string.h: strlen, memcpy, memset, strcmp | printf, puts, fprintf |
| stdlib.h: atoi, abs, rand (no-lock) | malloc, calloc, free |
| ctype.h: isdigit, isalpha | fopen, fread, fwrite |
| math.h: sin, cos, sqrt, pow | Any function using TLS or FILE* |

The runtime (`runtime/call/syscall.c`) was also patched to forward legacy SBI putchar ecalls (`a7=1`) from U-mode to M-mode via `sbi_putchar(arg0)`.

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

### 5. Console fallback for NEMU UARTLITE — `sm/plat/nemu_xiangshan/platform.c`

`fdt_serial_init()` returns `SBI_ENODEV` on NEMU-2026.03.r3 even though the
FDT correctly describes `xlnx,xps-uartlite-1.00.a` at MMIO 0x40600000. The
root cause is that OpenSBI's `fdt_get_address()` returns the relocated FDT
at 0x82200000, but the actual FDT data at that location appears invalid to
the serial driver probing code.

**Fix:** In `generic_early_init()`, if `fdt_serial_init()` fails, register a
fallback console device directly using the known UARTLITE MMIO address:

```c
static void early_putc(char c)
{
	volatile char *uart = (volatile char *)0x40600004;
	*uart = c;
}

/* In generic_early_init(): */
rc = fdt_serial_init(fdt);
if (rc) {
	static struct sbi_console_device xs_console = {
		.name = "xs-uartlite"
	};
	xs_console.console_putc = (void (*)(char))early_putc;
	sbi_console_set_device(&xs_console);
	rc = 0;
}
```

**Note:** The only added header is `<sbi/sbi_console.h>` for
`sbi_console_set_device()`. The UARTLITE TX FIFO is at MMIO offset 0x4 from
the base 0x40600000 (per Xilinx UARTLITE spec). NEMU outputs UARTLITE data
to stderr.

## Enclave runtime: `eyrie_boot` stub / stale initramfs

### Root cause (two issues)

**Issue 1:** The runtime's `eyrie_boot` function in the build tree
(`build64-xs/examples/hello/runtime/src/eyrie-hello-eyrie/sys/boot.c`)
had been replaced with a stub that just returned immediately:
```c
/* Minimal test: just return, no sbi_putchar, no init */
return;
```
This was left over from an earlier debugging session. The source file at
`runtime/sys/boot.c` still had the full initialization code, but the build
tree copy was stale. The runtime binary (`eyrie-rt`) was compiled with the
stub, so the enclave entered and immediately returned without making any
SBI ecalls or producing output.

**Fix:** Rebuild the enclave runtime from scratch by deleting the
ExternalProject stamp and source:
```bash
rm -rf build64-xs/examples/hello/runtime/src/eyrie-hello-eyrie*
```
Or manually recopy boot.c and force the enclave package rebuild.

**Issue 2:** When manually rebuilding the kernel Image, `CONFIG_INITRAMFS_ROOT_UID`
must be set to the build user's UID (not 0) for the uid→root mapping to work
correctly. The cmake build uses `id -u` and `id -g` for this:
```cmake
execute_process(COMMAND id -u OUTPUT_VARIABLE uid)
CONFIG_INITRAMFS_ROOT_UID=${uid}
```

### Verification
After the fix, the SM prints the correct runtime entry instructions:
```
[SM] runtime PA=0x81005000: insns 0x00008117 0xd6813103 0x14001073 0x49c000ef
```
Note: `0xd6813103` (ld sp, -664) and `0x49c000ef` (jal eyrie_boot) differ
from the old stub binary. The runtime's `sbi_putchar('R')` output appears
on the console, confirming end-to-end ecall-and-console functionality.

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
| 20 | `sm/plat/nemu_xiangshan/platform.c` | mod | Console fallback when `fdt_serial_init()` fails on NEMU |
| 21 | `sm/src/trap.S` | mod | Add `.align 2` before `trap_vector_enclave` |
| 22 | `sm/src/enclave.c` | mod | Preserve `MSTATUS_FS | MSTATUS_VS` in `regs->mstatus` |
| 23 | `CMakeLists.txt` | mod | Add `FW_PAYLOAD_FDT_OFFSET=0x2600000` — relocate FDT out of PMP M-mode region AND after kernel image (initially 0x2200000 but FDT overlapped with 33.9MB kernel) |
| 24 | `sm/plat/nemu_xiangshan/configs/defconfig` | mod | Add `CONFIG_FDT_SERIAL_XILINX_UARTLITE=y` — enable proper UARTLITE driver |
| 25 | `conf/linux64-xiangshan-defconfig` | mod | Add `# CONFIG_EFI is not set` + fix duplicate CMA entries — EFI auto-enabled via EFIVAR_FS caused non-EFI emu to hang |
| 26 | `conf/linux64-xiangshan-defconfig` | mod | Add `# CONFIG_RISCV_ISA_ZICBOZ is not set` — emu doesn't support `cbo.zero`; `clear_page` crashes with Illegal Instruction |
| 27 | `sdk/examples/hello/eapp/hello.c` | mod | Add `int main(void);` forward declaration — GCC 15 `-Werror=implicit-function-declaration` |
| 28 | `sdk/examples/CMakeLists.txt` | mod | Add `add_subdirectory(hello-vector)` — was missing from examples list |
| 29 | `CMakeLists.txt` image-deps | mod | Add hello-vector files (enclave.pkg, vector-runner, vector, vector-rt) to initramfs overlay |

## 调试与修复记录

### 20. Console fallback — `sm/plat/nemu_xiangshan/platform.c`

**问题：** `fdt_serial_init()` 在 NEMU-2026.03.r3 上返回 `SBI_ENODEV`，导致 OpenSBI 无控制台输出。

**原因：** OpenSBI 的 FDT 重定位后，`fdt_get_address()` 返回的地址处 FDT 数据对串口驱动探测无效。

**修复：** 在 `generic_early_init()` 中注册回退控制台设备：

```c
static void early_putc(char c) {
    volatile char *uart = (volatile char *)0x40600004;
    *uart = c;
}

/* 在 generic_early_init() 中： */
rc = fdt_serial_init(fdt);
if (rc) {
    static struct sbi_console_device xs_console = { .name = "xs-uartlite" };
    xs_console.console_putc = (void (*)(char))early_putc;
    sbi_console_set_device(&xs_console);
    rc = 0;
}
```

### 21. `trap_vector_enclave` 未对齐 — `sm/src/trap.S`

**问题：** SM 进入 enclave 后，runtime 的 `sbi_putchar('R')` ecall 引发的异常无法到达 SM 的 trap handler。

**根因：** `trap_vector_enclave` 标签的 VMA 为 `0x1e5a6`（低 2 位 = 0b10），不满足 RISC-V `mtvec` 的 4 字节对齐要求。`csr_write(mtvec, &trap_vector_enclave)` 时低 2 位被 NEMU 清除变为 `0x1e5a4`，指向错误地址。

**症状：** NEMU 日志显示无休止的 `cause=1 prev_pc=0x8001e5a4`（Instruction Access Fault）。

**修复：** 在标签前加 `.align 2`：

```asm
  .globl trap_vector_enclave
  .align 2
trap_vector_enclave:
    csrrw    tp, CSR_MSCRATCH, tp
```

### 22. `mstatus` 清除了 `VS/FS` 字段 — `sm/src/enclave.c`

**问题：** runtime 的 `sbi_putchar('R')` 执行后，后续初始化遇到向量指令（`vsetivli` 等）时触发 Illegal Instruction 异常。

**根因：** `context_switch_to_enclave()` 设置 `regs->mstatus = (1 << MSTATUS_MPP_SHIFT)`（即 `0x800`）时，**仅设置了 MPP 字段**，`VS`（位 [10:9]，控制 V 扩展）和 `FS`（位 [14:13]，控制浮点）被清零为 `0b00 = Off`。runtime 和 hello 二进制均启用 V 扩展，执行第一条向量指令即崩溃。

**修复：** 设置 mstatus 时保留 FS 和 VS：

```c
regs->mstatus = (1 << MSTATUS_MPP_SHIFT) | MSTATUS_FS | MSTATUS_VS;
// = 0x800 | 0x6000 | 0x600 = 0x6E00
//   MPP=1(S-mode)  FS=0b11(Dirty)  VS=0b11(Dirty)
```

### 23. Initramfs UID 映射 — 手动构建 kernel 注意事项

**问题：** 手动运行 `make -C linux.build` 时指定 `CONFIG_INITRAMFS_ROOT_UID=0` 而非构建用户的实际 UID，导致 initramfs 文件保留 uid=1000，`init` 进程以非 root 运行，`insmod` 静默失败。

**修复：** 必须使用 `CONFIG_INITRAMFS_ROOT_UID=$(id -u) CONFIG_INITRAMFS_ROOT_GID=$(id -g)`：

```bash
make -C linux.build \
  CONFIG_INITRAMFS_ROOT_UID=1000 CONFIG_INITRAMFS_ROOT_GID=1000 \
  ...
```

### 24. FEP 段对齐导致 GOT 读零 — `scripts/pkg-flat.py`

**问题：** `build_package()` 将 VA 向下对齐到页边界（`va_base = align_down(vaddr)`），但直接写入 ELF 段数据，未在开头插入 gap 填充。用户数据段 VA 为 `0x782f8`，页对齐后为 `0x78000`，gap 为 `0x2F8` 字节。缺少 gap 导致整个数据段在 VA 空间中偏移 `0x2F8` 字节。

**表现：** GOT 条目读到地址偏移后对应的值是全零（原 GOT 条目 `0x0007d408` 位于偏移 `0x3CF0`，但代码读到的是偏移 `0x3FE8` 处的值 `0x0000000000000000`）。`__libc_setup_tls` 通过 GOT 加载 `_dl_ns` 指针时得到 0，执行 `ld s0, 0(a5)` 触发 NULL 指针解引用。

**修复：** 在段数据开头插入 gap 填充：
```python
gap = vaddr - va_base
padded = b'\x00' * gap + raw_data[:filesz]
```

### 25. glibc TLS 初始化不兼容裸机 enclave

**问题：** 即使 GOT 条目值正确，`__libc_setup_tls` 仍会崩溃。静态链接的 glibc 中 `__libc_start_main` 调用顺序为 `__libc_setup_tls` → `__libc_init_first`，其中 `__libc_setup_tls` 需要 `_dl_ns` 等由 `__libc_init_first` 初始化的数据结构。此外 `__libc_init_first` 需要内核 execve 时传递的 auxv 向量（AT_PHDR、AT_PAGESZ 等），enclave 环境无法完整提供。

**修复：** 跳过 glibc CRT 启动，使用 `_start` 直接调用 `main()`，链接选项 `-nostartfiles -Wl,-e,_start`。输出用 inline ecall a7=1（运行时 `handle_syscall` 捕获后转发到 SM UART）。

### 26. 运行时缺少 legacy putchar ecall 转发 — `runtime/call/syscall.c`

**问题：** 用户态通过 `ecall a7=1`（SBI_EXT_0_1_CONSOLE_PUTCHAR）输出字符时，ecall 从 U-mode 委派到 S-mode 运行时。但运行时的 `handle_syscall` 未处理 `a7=1`，静默忽略请求。

**修复：** 在 `handle_syscall` 开头增加转发逻辑：
```c
if (n == 1) {
    sbi_putchar(arg0);
    ctx->regs.a0 = 0;
    return;
}
```

### 27. 运行时 `io_syscall_write` 回声未生效

**问题：** 在 runtime/call/io_wrap.c 中对 `fd==1||fd==2` 添加了 sbi_putchar 回声，但因 `copy_from_user` 在某些执行上下文中访问用户内存失败，字符未被输出。

**结论：** 该方案因运行时构建系统缓存问题未能成功生效。当前改用 ecall a7=1 直出方案。

### 28. 用户态 exit ecall 号错误

**问题：** 用户代码使用 `a7=17`（legacy SBI shutdown）退出 enclave，但运行时不处理该 ecall 号。用户代码陷入 `while(1)` 死循环，enclave 不结束。

**修复：** 改为 `a7=1101`（`RUNTIME_SYSCALL_EXIT`，运行时定义的退出 ecall 号）。

### 29. 内核配置问题

| 问题 | 症状 | 修复 |
|---|---|---|
| `CONFIG_INITRAMFS_SOURCE=""` | Kernel panic - VFS: Unable to mount root fs | 在 `.config` 中设置正确的 initramfs 路径 |
| `CONFIG_STACKPROTECTOR=y` | Kernel panic - stack protector | 在 defconfig 中添加 `# CONFIG_STACKPROTECTOR is not set` |
| 手动 `make -C linux` 时 kernel config 变更 | Linux 内核无法启动或缺少驱动 | 始终用 cmake `make linux` 或提供完整 `CONFIG_*` 参数 |
| CMake `make linux` 不重新编译 | Initramfs 未更新 | `rm -f linux.build/arch/riscv/boot/Image` + `touch initramfs-sysroot/.extracted` |

### 30. 运行时 debug 标记 'R'

**文件：** `runtime/sys/boot.c:121`

**问题：** `sbi_putchar('R')` 在每次 runtime 启动时输出 'R' 字符，污染用户输出。

**修复：** 注释掉该行。

### 31. NEMU `mnstatus.nmie` 诊断信息

**表现：** enlave 正常退出后 NEMU 打印：
```
HIT CRITICAL ERROR: trap when mnstatus.nmie close
```
**原因：** SM 在 `exit_enclave` 时设置 `mideleg=0`，在中断关闭状态下执行 `mret`。NEMU 的 RISC-V 规范检查认为这是异常情况。不影响功能，`GOOD TRAP` 和 `halt ret: 0` 确认正确退出。

### 最终 hello enclave 方案

```
void _start(void) { main(); sbi_exit(1101); }
int main(void) {
    while (*s) __asm__("li a7,1\nmv a0,%0\necall\n" : : "r"(*s++));
}
```

链接：`-static -nostartfiles -Wl,-e,_start`

**可用 glibc 函数：** string.h（strlen, memcpy, memset）、stdlib.h（atoi, abs）、ctype.h、math.h
**不可用：** printf、malloc、fopen 等依赖 TLS/stdio 初始化的函数

**测试命令：**
```bash
timeout 120 /home/yangxin/xs-env/NEMU-2026.03.r3/build/riscv64-nemu-interpreter \
  -b -I 10000000000 \
  build64-xs/sm.build/platform/nemu_xiangshan/firmware/fw_payload.bin \
  2>&1 | grep -E "hello|page fault|\[SM\].*enclave|exit_enclave"
```

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

### 32. FDT 地址在固件 PMP 区域内导致 Linux 启动卡死

**表现：** OpenSBI/SM 初始化完成，显示 "Boot HART Debug Triggers: 4 triggers" 后无输出，系统卡死。

**根因：** FDT 在 `fw_payload.bin` 中内嵌于 OpenSBI `.rodata` 段，地址为 `0x80024020`。该地址位于 OpenSBI 固件 PMP 区域（`0x80000000-0x8003ffff`），被配置为 M-mode only。当 OpenSBI 跳转到 Linux（S-mode）时，传入 `a1=0x80024020`，Linux 尝试读取 FDT 时触发 PMP 访问违例。

**与 QEMU 的对照：** QEMU virt 平台的 `sm/opensbi/platform/generic/objects.mk:34` 定义了 `FW_PAYLOAD_FDT_OFFSET=0x2200000`，`fw_next_arg1` 返回 `_fw_start + 0x2200000 = 0x82200000`，不在固件 PMP 区域内。`nemu_xiangshan` 平台缺乏此定义，走 pass-through 路径。

**初次修复：** `CMakeLists.txt:284` — 在 sm 构建命令中添加 `FW_PAYLOAD_FDT_OFFSET=0x2200000`。

**问题：** `FW_PAYLOAD_FDT_OFFSET=0x2200000` 让 FDT 位于 `0x82200000`。但内核 Image 为 33.9MB（加载于 `0x80200000`），结束于 `0x8225AAB0`。FDT 在 `0x82200000` 覆盖了内核的代码/数据，导致内核早期启动时崩溃。

**最终修复：** 使用 `FW_PAYLOAD_FDT_OFFSET=0x2600000`，FDT 位于 `0x82600000`（内核结束之后）：
```cmake
FW_PAYLOAD_FDT_OFFSET=0x2600000
```

### 33. 缺少 Xilinx UARTLITE 串口驱动 — `sm/plat/nemu_xiangshan/configs/defconfig`

**表现：** `fdt_serial_init()` 无法识别 DTB 中的 `xlnx,xps-uartlite-1.00.a` 节点，回退到 `early_putc()` 裸写（无 TXFULL 状态检查）。

**根因：** `sm/plat/nemu_xiangshan/configs/defconfig` 缺少 `CONFIG_FDT_SERIAL_XILINX_UARTLITE=y`，导致 carray 中不包含该驱动，`fdt_serial_init()` 扫描全部 DT 节点时无法匹配。

**修复：** 添加 `CONFIG_FDT_SERIAL_XILINX_UARTLITE=y`，并强制删除 `sm.build` 目录以重新生成 carray：
```diff
 CONFIG_FDT_SERIAL_UART8250=y
+CONFIG_FDT_SERIAL_XILINX_UARTLITE=y
 CONFIG_SERIAL_SEMIHOSTING=y
```

### 34. emu 不支持 Z 扩展 — 内核 Illegal Instruction 崩溃

**表现：** Linux 内核启动初期在 `clear_page` 中 Illegal Instruction 崩溃。

**根因：** emu（Verilog转C模拟器）不支持 `zicboz`、`zicbom`、`zba`、`zbb`、`zbc` 等 Z 扩展，但这些扩展在 Linux 内核中均为 `default y`，通过 alternatives patching 在运行时检测 DTB 中的 ISA 声明后自动启用。

emu 不支持的 Z 扩展列表：
```
zba zbb zbc zbs zfh zfhmin zic64b zicbom zicbop zicboz
zicntr zihpm zicsr zifencei zkn zknd zkne zknh zksed zksh zkt
zbkb zbkc zbkx zvbb zvfh zvfhmin zvkt zvl128b zvl64b
```

**修复：** 在 `conf/linux64-xiangshan-defconfig` 中禁用 emu 不支持的 Z 扩展：
```diff
+# CONFIG_RISCV_ISA_ZICBOZ is not set
+# CONFIG_RISCV_ISA_ZICBOM is not set
+# CONFIG_RISCV_ISA_ZBA is not set
+# CONFIG_RISCV_ISA_ZBB is not set
+# CONFIG_RISCV_ISA_ZBC is not set
```

### 35. 内核 `CONFIG_EFI=y` 导致非 EFI 平台启动卡死

**表现：** Linux 内核启动初期在 `clear_page` 函数中崩溃，`cause=2 (Illegal instruction)`。

**根因：** `CONFIG_RISCV_ISA_ZICBOZ` 的 `default y` 使内核在 `clear_page` 中使用了 `cbo.zero` 指令。Xiangshan NEMU 支持该指令，但 Verilog-to-C 的 emu 不支持 `cbo.zero`。

**修复：** 在 `conf/linux64-xiangshan-defconfig` 中添加 `# CONFIG_RISCV_ISA_ZICBOZ is not set`，使 `clear_page` 退化为 `__memset`。

### 35. 内核 `CONFIG_EFI=y` 导致非 EFI 平台启动卡死

**表现：** Linux 内核无任何输出，即使 FDT 地址和 UART 驱动都已正确。

**根因：** `CONFIG_EFI=y` 及其依赖（`EFI_STUB`、`EFI_EARLYCON`、`EFI_PARAMS_FROM_FDT`）在非 EFI 的 NEMU 平台上被自动选中（通过 `CONFIG_EFIVAR_FS=y`），内核在 EFI 初始化时卡死。

**修复：** 在 `conf/linux64-xiangshan-defconfig` 中添加 `# CONFIG_EFI is not set`。
同时清理了重复 3 次的 `CONFIG_CMA` / `CONFIG_DMA_CMA` 配置项。

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

### Run on NEMU

```bash
/home/yangxin/xs-env/bin/riscv64-nemu-interpreter -b \
  build64-xs/sm.build/platform/generic/firmware/fw_payload.bin
```

### Run hello enclave (after boot)

**Flat binary mode (merge_runtime branch):**

```bash
insmod /root/keystone/keystone-driver.ko
/root/keystone/hello-runner /root/keystone/enclave.pkg
```

**Legacy ELF mode (previous branches):**

```bash
insmod /root/keystone/keystone-driver.ko
/root/keystone/hello-runner /root/keystone/hello /root/keystone/eyrie-rt
```

## Flat Binary Packaging (merge_runtime)

Combines eyrie-rt + hello ELFs into a single `.pkg` file at build time,
eliminating libelf parsing and mmap-based page copying in the SDK.

**Files changed:**

| File | Change |
|---|---|
| `scripts/pkg-flat.py` | NEW: build-time ELF→flat package converter |
| `sdk/include/host/FepPackage.hpp` | NEW: flat package format header |
| `sdk/src/host/Enclave.cpp` | ADD `loadFlatEnclave()`, `init()` detects `.pkg` |
| `sdk/include/host/Enclave.hpp` | ADD `loadFlatEnclave`, flat entry fields |
| `sdk/examples/hello/CMakeLists.txt` | ADD `.pkg` build target |
| `sdk/examples/hello/host/host.cpp` | Support single-arg `.pkg` input |
| `CMakeLists.txt` | Copy `enclave.pkg` into initramfs rootfs |

**FEP format (all little-endian, 64-bit):**
- 24-byte header: magic=0x504B4745, num_segs, rt_entry, user_entry
- 32-byte SegEntry[num_segs]: va_base, va_pages, data_offset, data_pages, flags
- Raw page-aligned segment data follows

**Rationale:** Eliminates libelf dependency and fragile mmap+memcpy page
loading in the SDK. The runtime and enclave app are packaged at build time
into a single flat binary with a simple header.
