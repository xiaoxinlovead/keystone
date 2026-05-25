# Keystone on Xiangshan (Kunminghu)

Build and deploy the Keystone enclave framework for the Xiangshan RISC-V processor.

## Prerequisites

```bash
source ./source.sh        # sets $RISCV, $PATH, $KEYSTONE_SDK_DIR
```

Requires RISC-V toolchain (riscv64-unknown-linux-gnu- and riscv64-unknown-elf-)
at `$RISCV/bin/`. GCC 15.2.0 tested.

## Build

```bash
mkdir build64-xs && cd build64-xs
cmake .. -DXIANGSHAN=ON
make -j$(nproc)
```

### Build targets

| Target | Description |
|---|---|
| `make linux-config` | Generate Linux kernel .config from xiangshan defconfig |
| `make linux` | Build Linux kernel Image (with initramfs) |
| `make buildroot` | Build root filesystem (rootfs.tar) |
| `make driver` | Build keystone-driver.ko kernel module |
| `make sm` | Build Security Monitor (OpenSBI + keystone SM + kernel payload) |
| `make tests` | Build enclave examples (hello, tests, etc.) |
| `make image` | Assemble final artifacts |

### Key build options

```bash
cmake .. -DXIANGSHAN=ON -DSM_PLATFORM=generic -DCMAKE_BUILD_TYPE=Debug
```

## Output

| File | Path | Description |
|---|---|---|
| `fw_payload.bin` | `sm.build/platform/generic/firmware/` | Bootable firmware for XS |
| `fw_payload.elf` | `sm.build/platform/generic/firmware/` | ELF for debugging |
| `Image` | `linux.build/arch/riscv/boot/` | Linux kernel |
| `keystone-driver.ko` | `linux-keystone-driver.build/` | Kernel module |
| `rootfs.tar` | `buildroot.build/images/` | Root filesystem |
| `hello` | `examples/hello/` | Hello enclave binary |
| `hello-runner` | `examples/hello/` | Hello enclave host runner |

## Deploy on NEMU

```bash
# fw_payload.bin is the bootable firmware for Xiangshan.
# Load it in NEMU via the --bios or --fw option:
nemu/build/riscv64-nemu-interpreter \
    --bios build64-xs/sm.build/platform/generic/firmware/fw_payload.bin \
    ...
```

## Enclave examples

After booting, copy the built examples to the target:

```bash
# hello enclave + host runner
examples/hello/hello          # enclave payload (.ke)
examples/hello/hello-runner   # host-side runner
```

On the Xiangshan Linux, load the driver and run:

```bash
insmod keystone-driver.ko
./hello-runner hello
```

## Key differences from QEMU virt

| Aspect | QEMU virt | Xiangshan |
|---|---|---|
| Kernel defconfig | `linux64-defconfig` | `linux64-xiangshan-defconfig` |
| UART | SiFive (0x10000000) | 8250 DW (0x310B0000) |
| Interrupt controller | PLIC | AIA (APLIC + IMSIC) |
| SM platform | generic | generic + xiangshan_kmh override |
| CMake flag | (none) | `-DXIANGSHAN=ON` |

## Reference

- `/home/yangxin/xiangshan-opensbi-linuxkernel/` — standalone XS Linux + OpenSBI build
- `/home/yangxin/xs-env/` — Xiangshan dev environment (RTL, NEMU)

## Porting notes

See [porting-notes.md](porting-notes.md) for a detailed list of all source code
changes made for the Xiangshan adaptation.
