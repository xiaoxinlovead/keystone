# Keystone 项目分支演进分析

## 一、上游基础版本 `08a241b5f1`

本项目基于 [keystone-enclave/keystone](https://github.com/keystone-enclave/keystone) 上游 commit `08a241b5f1762938d1a3ec7986ed152a0425a490`。

该 commit 的 **git submodule 清单**：

| Submodule | Commit | 版本 | 日期 | 上游来源 |
|---|---|---|---|---|
| `riscv-gnu-toolchain` | `407cdc0ceb` | (预编译工具链) | — | riscv/riscv-gnu-toolchain |
| `buildroot` | `0003fdbed3` | **2022.08** | 2022-09 | buildroot/buildroot |
| `linux` | `3d7cb6b04c` | **5.19** | 2022-07 | torvalds/linux |
| `qemu` | `823a3f11fb` | **7.0.0** | 2022-04 | qemu/qemu |
| `sm/opensbi` | `4489876e93` | **v1.1** | 2022-06 | riscv/opensbi |
| `runtime/test/cmocka` | (另行指定) | (cmocka 测试框架) | — | cmocka/cmocka |

整个仓库使用 git submodule 机制管理，除项目源码（`sm/src/`、`sdk/`、`runtime/`、`linux-keystone-driver/`、`bootrom/`）外，所有依赖项均为 submodule。

---

## 二、初始 Commit `ecb663854` —— "Initial commit"

该 commit 对上游进行了以下操作，**破坏了 submodule 结构**：

### 2.1 各 Submodule 处理方式

| Submodule | 初始 Commit 中的处理 | 版本变化 | 原因 |
|---|---|---|---|
| `sm/opensbi` | **扁平化**为直接跟踪目录 | **不变** (v1.1) | 保留了上游的 OpenSBI v1.1，但丢失了 submodule 元数据 |
| `buildroot` | **扁平化**为直接跟踪目录 | **不变** (2022.08) | 树 hash 匹配上游 submodule，仅改变管理方式 |
| `qemu` | **扁平化**为直接跟踪目录 | **不变** (7.0.0) | 同上 |
| `linux` | **扁平化**且**替换版本** | **5.19 → 6.12.27** | 需匹配 Xiangshan/NEMU 的新内核版本 |
| `riscv-gnu-toolchain` | **移除** | — | 使用外部预装工具链 (`/opt/riscv`)，不纳入仓库管理 |
| `runtime/test/cmocka` | **移除** | — | cmocka 测试框架暂时不需要 |

### 2.2 额外操作

- 新增 `build64/` 目录（预构建产物，上游无此目录）
- 新增 `AGENTS.md`（项目指南）
- 丢失了上游的 `.github/` 目录（CI 配置）

**总结：** 初始 commit 本质上是一次 **"flatten + replace"** 操作——将上游 submodule 扁平化，同时替换了 Linux 内核版本，为后续 Xiangshan 适配做准备。

---

## 三、`jiagedaqi` 分支相对于初始 Commit 的修改

`jiagedaqi` 分支包含 **7 个 commit**（初始 commit 之后），分为五个阶段：

### 3.1 构建产物更新 (`692d922e3`)

```
692d922e3 update keystone: CMakeLists, config, and build artifacts
```

更新 `build64/` 中的 CMake 缓存和构建系统文件，反映本地构建环境。

### 3.2 重新 Submodule 化

**这步操作的本质：将初始 commit 中扁平化的 `linux` 和 `sm/opensbi` 重新转为 submodule，但指向的是 fork 仓库而非上游。**

| Commit | 操作 | 新 Submodule 配置 |
|---|---|---|
| `b4cca4a47` | 删除 `linux/` 86713 个直接跟踪文件 | — |
| `08af96306` | 添加 `linux` submodule | `xiaoxinlovead/linux-6.12.27` (master 分支) |
| `6d603f3a4` | 删除 `sm/opensbi/` 284 个直接跟踪文件 | — |
| `c4f432fbc` | 添加 `sm/opensbi` submodule | `xiaoxinlovead/opensbi` (hellovector 分支) |

**版本对比：**

| 组件 | 上游 `08a241b5` | 初始 Commit | `jiagedaqi` |
|---|---|---|---|
| Linux | 5.19 (torvalds/linux) | 6.12.27 (直接跟踪) | 6.12.27 (fork submodule) |
| OpenSBI | v1.1 (riscv/opensbi) | v1.1 (直接跟踪) | **v3.0** (fork submodule) |

**为什么替换为 fork 仓库而非直接用上游？**

- Linux 6.12.27 需应用于 Xiangshan 的 defconfig、驱动修改等定制
- OpenSBI v3 是 Xiangshan/NEMU 的需求（v1.1 不支持 AIA 中断控制器等新特性）
- Fork 仓库便于管理这些定制修改

**注意：** `buildroot` 和 `qemu` 在 jiagedaqi 中仍维持初始 commit 的直接跟踪状态，未转为 submodule。Xiangshan 构建使用外部预装的 buildroot 和 QEMU/NEMU。

### 3.3 Xiangshan 平台适配 (`9dd73179a`)

```
9dd73179a Xiangshan adaptation: defconfig, CMake branch, SM override, GCC 15 fixes
```

**新增文件：**

| 文件 | 用途 |
|---|---|
| `conf/linux64-xiangshan-defconfig` | Xiangshan 全量内核 defconfig（基于 `nemu_board/configs/xiangshan_defconfig` 52 行 fragment 扩展） |
| `sm/plat/generic/xiangshan_kmh.c` | 平台 override 模块，FDT 匹配 `"xiangshan,nemu-board"` 和 `"xiangshan,kunminghu"` |
| `docs/xiangshan/README.md` | Xiangshan 端口说明文档 |
| `docs/xiangshan/porting-notes.md` | 详细移植笔记 |

**修改文件：**

| 文件 | 变更 | 原因 |
|---|---|---|
| `CMakeLists.txt` | 添加 `-DXIANGSHAN=ON` 构建分支、ISA 添加 `_zifencei`、initramfs 路径修复、image-deps 拷贝 enclave 到 rootfs | Xiangshan 编译链支持 |
| `sm/plat/generic/objects.mk` | 注册 `xiangshan_kmh` override 模块 | 平台代码编译 |
| `linux-keystone-driver/keystone-page.c` | `MAX_ORDER` → `MAX_PAGE_ORDER` | **Linux 6.12 内核 API 变更** |
| `runtime/sys/entry.S` | `csrr sbadaddr` → `csrr stval` | **RISC-V 规范**：`sbadaddr` 在较新 toolchain 中已废弃 |
| `sm/src/thread.c` | `sbadaddr` → `stval`（两处 CSR 交换） | 同上 |
| `sm/src/thread.h` | `struct csrs` 中 `sbadaddr` → `stval` | 同上 |

### 3.4 OpenSBI v1.1 → v3 移植 (`6c3781e06`，工作量最大)

```
6c3781e06 Phase 2: Port keystone SM to OpenSBI v3 + documentation
```

这是技术难度最高的 commit。OpenSBI v1.1 与 v3.0 的 ecall handler API **完全不兼容**。

**API 变更对照：**

| 接口 | OpenSBI v1.1 | OpenSBI v3.0 |
|---|---|---|
| Ecall handler 签名 | `int fn(extid, funcid, const regs*, out_val*, out_trap*)` | `int fn(extid, funcid, regs*, sbi_ecall_return*)` |
| 跳过 mepc 自增 | `sbi_trap_exit(regs)` | `out->skip_regs_update = true` |
| 返回值传递 | `*out_val = value` | `out->value = value` |
| Ecall 寄存器上下文 | 分离的 `struct sbi_trap_regs *` | 统一的 `struct sbi_trap_context` |
| Misaligned handler | `sbi_misaligned_load_handler(mtval, mtval2, mtinst, regs)` | `sbi_misaligned_load_handler(&tcntx)` |
| Illegal instruction handler | `sbi_illegal_insn_handler(mtval, regs)` | `sbi_illegal_insn_handler(&tcntx)` |
| Ecall handler 调用 | `sbi_ecall_handler(regs)` | `sbi_ecall_handler(&tcntx)` |
| 结果回写 | (由 handler 内部完成) | `sbi_memcpy(regs, &tcntx.regs, sizeof(*regs))` |
| Extension 注册 | 隐式（`extid_start`/`extid_end`） | 显式（`register_extensions` 回调 + `.head` 链表 + `.name`） |
| Platform override | `struct platform_override *[]` + `platform_override_modules_size` | `struct fdt_driver *const []` (终止项为 NULL) |
| C array 生成 | `scripts/carray.sh` | 移除（v3 采用不同的 carray 机制） |

**修改文件：**

| 文件 | 变更 | 原因 |
|---|---|---|
| `sm/src/sm-sbi-opensbi.c` | 函数签名更新、`out` 参数、`register_extensions` 注册、`.name`、`.head` 链表 | v3 ecall extension API |
| `sm/src/sm-sbi.c` | 所有操作函数增加 `out` 参数，`sbi_trap_exit` → `skip_regs_update` | v3 ecall return 机制 |
| `sm/src/sm-sbi.h` | 所有函数声明同步更新 | 接口一致性 |
| `sm/plat/generic/platform.c` | 完全重写为 v3 基类，增加 `sm_init` hook、heap 计算、fdt_driver 驱动模型 | v3 平台初始化流程 |
| `sm/plat/generic/objects.mk` | v3 编译标志、ecall carray 注册方式更改 | v3 构建系统 |
| `sm/plat/generic/Kconfig` | 新建 | v3 配置系统 |
| `sm/plat/generic/configs/defconfig` | 新建（FDT 串口/中断/定时器等配置） | v3 默认配置 |
| `sm/src/stubs.c` | 新建桩文件 | **暂时禁用** trap handler、IPI、平台 override 模块，保证编译通过 |
| `CMakeLists.txt` | SM 构建使用 `linux-gnu` 工具链（替代 `elf`）、增加 `FW_FDT_PATH`、`FW_PAYLOAD_ALIGN`、移除 carray.sh 和 opensbi patch | v3 构建需求 |

**`stubs.c` 桩函数的作用：**

```c
// 三个核心功能暂时空实现，让编译通过
void send_and_sync_pmp_ipi(void) { }           // IPI → 空函数
void sbi_trap_handler_keystone_enclave(
    struct sbi_trap_regs *regs) { }            // trap handler → 空函数
const struct fdt_driver *const
    platform_override_modules[] = { NULL };    // override → NULL 终止
```

至此，**jiagedaqi 分支达到"编译通过"状态，但 enclave 运行功能被 stubs 覆盖，实际不可用。**

---

## 四、`xiangshanv3` 分支相对于 `jiagedaqi` 的新增修改

在 `jiagedaqi` 的最后一个 commit (`6c3781e06`) 之上，`xiangshanv3` 增加了 **3 个 commit**：

```
db00a0450 hello-world: [hello-runner] Running enclave...
763b1c721 Fix: remove __builtin_unreachable() and csr_write(medeleg,0), add CAUSE_USER_ECALL
27ca308a9 Update porting-notes: document all fixes, diagnostics, and open issues
```

### 4.1 Hello-world Enclave 功能启用 (`db00a0450`)

**核心任务：用真正的实现替换 `stubs.c` 中的空桩函数。**

| 组件 | 变更 | 说明 |
|---|---|---|
| `sbi_trap_hack.c` | 完全重写为 v3 兼容版本（使用 `sbi_trap_context`、`tcntx`、`out` 参数） | 取代 stub 中的空 `sbi_trap_handler_keystone_enclave` |
| `ipi.c` | 实现 `send_and_sync_pmp_ipi()`，修正 `SBI_IPI_EVENT_SM` 值 | 取代 stub 中的空函数 |
| `platform_override_modules.c` | v3 `fdt_driver` 格式，注册 `xiangshan_kmh` | 取代 stub 中的 NULL 数组 |

**其他变更：**

| 文件 | 变更 | 目的 |
|---|---|---|
| `sm/src/sm-sbi-opensbi.c` | 补充 `.head` 链表初始化、`register_extensions` 回调 | v3 extension 注册完整性 |
| `sm/src/sm.c` | 删除两行 sbi_printf | 清理调试输出 |
| `sm/plat/generic/configs/defconfig` | 增加多项 CONFIG | v3 平台配置完善 |
| `CMakeLists.txt` | 构建物拷贝（hello-runner/hello → rootfs）、sm 去掉 patch 和 carray.sh | Hello enclave 打包进 rootfs |
| `conf/riscv64_xiangshan_defconfig` | 新建 buildroot 配置 | Xiangshan initramfs 构建 |
| `sdk/examples/hello/host/host.cpp` | hello-runner 添加 `fprintf(stderr, ...)` 调试输出 | 确认启动流程 |
| `runtime/CMakeLists.txt`、`sdk/macros.cmake` | 编译选项微调 | 交叉编译兼容 |

### 4.2 三个关键 Bug 修复 (`763b1c721`)

这些是**阻塞 enclave 实际运行**的 bug：

| # | 问题 | 文件 | 修复 | 症状 |
|---|---|---|---|---|
| 1 | `__builtin_unreachable()` | `sm/src/sm-sbi-opensbi.c` | 删除 RUN/RESUME/STOP/EXIT 后的 `__builtin_unreachable()` | OpenSBI v3 要求 ecall handler **正常返回**以执行 `skip_regs_update`。编译器优化提示阻止返回 → mepc 设置错误 → enclave 入口崩溃 |
| 2 | `csr_write(medeleg, 0)` | `sm/src/enclave.c` | 删除此代码 | xiangshan 移植时误加。medeleg=0 强制**所有异常**去 M-mode，阻止 eyrie runtime 的 S-mode 异常处理（stvec delegate） |
| 3 | 缺少 `CAUSE_USER_ECALL` | `sm/src/sbi_trap_hack.c` | 添加 `case CAUSE_USER_ECALL:` + `stvec==0` 安全检查 | enclave app (U-mode) 的 ecall (putchar) 未被处理 → 落入 default 重定向 → stvec=0 → **死循环** |

### 4.3 文档更新 (`27ca308a9`)

更新 `docs/xiangshan/porting-notes.md`，整理所有修复、诊断和待解决问题。

---

## 五、当前未提交修改（诊断阶段）

在 commit `27ca308a9` 之上的工作区修改：

| 文件 | 变更 | 目的 |
|---|---|---|
| `sm/src/enclave.c` | 删除大量 debug print + PT walk 诊断代码；添加 `[SM] entering enclave` 打印 + runtime PA 验证 + trap 入口诊断 | 减少指令开销（用于性能测量）；定位 enclave 无输出的根本原因 |
| `sm/src/sbi_trap_hack.c` | 缩进规范化；trap 入口添加 `[SM] enclave trap` 诊断打印 | 代码风格 + 诊断 |
| `build64/` (build64-xs/) | CMakeFiles 和构建物更新 | cmake 配置变化后的自动更新 |

**当前状态：** SM 已成功进入 enclave（`[SM] entering enclave: eid=0 mepc=0xffffffffbffffffc`），但 eyrie runtime 无输出（debug 宏在 `USE_DEBUG` 未定义时为 no-op），hello app 也无输出。正在通过 trap 入口打印定位 runtime silient crash 的根本原因。

---

## 六、演进总结

### 6.1 Submodule 版本演进总表

| 组件 | 上游 `08a241b5` | 初始 Commit | `jiagedaqi` | `xiangshanv3` |
|---|---|---|---|---|
| **Linux** | 5.19 (torvalds/linux, submodule) | **6.12.27** (直接跟踪) | 6.12.27 (xiaoxinlovead fork, submodule) | 同左 |
| **OpenSBI** | v1.1 (riscv/opensbi, submodule) | v1.1 (直接跟踪) | **v3.0** (xiaoxinlovead fork, submodule) | 同左 |
| **Buildroot** | 2022.08 (submodule) | 2022.08 (直接跟踪) | 2022.08 (直接跟踪) | 同左 |
| **QEMU** | 7.0.0 (submodule) | 7.0.0 (直接跟踪) | 7.0.0 (直接跟踪) | 同左 (xiangshan 不使用) |
| **riscv-gnu-toolchain** | (submodule) | **移除** (外部安装) | 外部安装 | 同左 |
| **cmocka** | (submodule) | **移除** | 移除 | 同左 |

### 6.2 项目管理方式变化

```
上游 08a241b5:   [submodule × 6] + 项目源码
         ↓ (flatten + replace linux)
初始 commit:     [直接跟踪 × 4] + 项目源码 + build64/
         ↓ (remove → add as fork submodule, upgrade opensbi)
jiagedaqi:       [submodule × 2 (fork)] + [直接跟踪 × 2] + 项目源码 + Xiangshan 适配
         ↓ (re-enable stubs, bug fixes, diagnostics)
xiangshanv3:     同 jiagedaqi + enclave 功能启用 + 调试代码
```

### 6.3 各分支功能状态

| 状态 | 初始 Commit | `jiagedaqi` | `xiangshanv3` |
|---|---|---|---|
| 编译通过 | ✓ | ✓ | ✓ |
| SM 初始化 | ✓ (v1.1) | ✓ (v3.0, 但 trap stub) | ✓ |
| Enclave 创建 | ✓ | ✓ | ✓ |
| Enclave 运行 | ✗ (QEMU 目标，无 xiangshan 支持) | ✗ (trap handler 为 stub) | **部分** (SM 入口成功，runtime 输出挂死诊断中) |
