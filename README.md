<p align="center">
  <h1 align="center">🔬 minikernel OS</h1>
  <p align="center">
    <strong>一个基于微内核架构的操作系统内核</strong>
  </p>
  <p align="center">
    <a href="#特性">特性</a> •
    <a href="#快速开始">快速开始</a> •
    <a href="#架构设计">架构设计</a> •
    <a href="#构建指南">构建指南</a> •
    <a href="#贡献指南">贡献指南</a>
  </p>
</p>

---

## 📖 简介

minikernel OS 是一个从零开始构建的微内核操作系统项目，遵循 Linux 微内核核心文档的设计原则。本项目旨在提供一个清晰、模块化、易于理解的操作系统内核实现，适合操作系统学习者和爱好者深入研究。

### 设计理念

- **最小化内核** - 内核只负责最核心的功能：地址空间管理、线程调度、IPC、低级资源管理、异常/中断处理
- **用户态服务** - 将复杂逻辑（文件系统、网络栈、驱动程序、策略）移至用户空间运行
- **分层清晰** - 代码按职责严格分层：`arch/`、`kernel/`、`lib/`、`user/`、`tools/`、`docs/`
- **接口契约** - 强制 IPC/ABI 接口规范，便于并行开发与多语言互操作

## ✨ 特性

### 已实现

- [x] **基础内核框架** - 完整的内核启动流程和初始化序列
- [x] **多级页表虚拟内存管理** - x86_64 4级页表支持
- [x] **完全公平调度器 (CFS)** - 基于红黑树的 O(log n) 调度算法
- [x] **伙伴系统内存分配** - 高效的物理页面分配器
- [x] **进程管理和上下文切换** - 完整的进程生命周期管理
- [x] **基础系统调用接口** - fork、exec、wait、exit、mmap 等
- [x] **双向链表和红黑树数据结构** - 内核级数据结构实现
- [x] **中断和异常处理框架** - IDT 配置与中断处理

### 开发中

- [ ] 完整的 VFS 实现
- [ ] 网络协议栈
- [ ] 进程间通信机制 (IPC)
- [ ] 用户态驱动支持
- [ ] 内核模块加载
- [ ] 信号机制
- [ ] 文件系统支持
- [ ] 多处理器支持 (SMP)

## 🚀 快速开始

### 环境要求

| 工具 | 最低版本 | 用途 |
|------|----------|------|
| GCC/G++ | 9.0+ | C/C++ 编译器 |
| Meson | 1.0+ | 构建系统 |
| Ninja | 1.10+ | 构建后端 |
| Binutils | 2.34+ | 链接器、objcopy、objdump |
| QEMU | 4.0+ | 虚拟机运行环境 |
| Make | 4.0+ | 构建工具 |
| Conan | 2.0+ | 可选构建工具依赖管理 |

### 安装依赖

**Ubuntu/Debian:**

```bash
sudo apt update
sudo apt install build-essential gcc g++ meson ninja-build binutils qemu-system-x86
```

**Arch Linux:**

```bash
sudo pacman -S base-devel gcc meson ninja qemu-system-x86
```

**Fedora:**

```bash
sudo dnf install gcc gcc-c++ meson ninja-build binutils qemu-system-x86
```

### 构建与运行

```bash
# 克隆项目
git clone https://github.com/assumeengagetry/minikernel.git
cd minikernel

# 检查工具链
make check-tools

# 使用 Meson 构建内核
make all

# 在 QEMU 中运行（需要 grub-mkrescue 和 xorriso）
make qemu
```

### 调试模式

```bash
# 启动 QEMU 并等待 GDB 连接
make debug

# 在另一个终端中连接 GDB
gdb build/kernel.elf -ex 'target remote localhost:1234'
```

## 🏗️ 架构设计

### 项目结构

```
minikernel/
├── .github/                    # GitHub 配置
│   ├── workflows/              # CI/CD 工作流
│   └── ISSUE_TEMPLATE/         # Issue 模板
├── arch/                       # 架构相关代码
│   └── x86_64/
│       ├── boot/               # 引导代码、链接脚本
│       ├── cpu/                # CPU 特定代码
│       └── mm/                 # 架构相关页表实现
├── kernel/                     # 内核核心代码
│   ├── core/                   # 微内核核心模块
│   │   ├── process/            # 进程管理
│   │   ├── thread/             # 线程管理
│   │   ├── sched/              # 调度器
│   │   └── ipc/                # 进程间通信
│   ├── include/                # 内核头文件
│   ├── interrupt/              # 中断处理框架
│   ├── mm/                     # 内存管理（非架构相关）
│   └── drivers/                # 内核态驱动
├── include/                    # 公共头文件
├── lib/                        # 内核与用户态共用库
├── src/                        # 主要源代码
│   ├── kernel/                 # 内核实现
│   └── mm/                     # 内存管理实现
├── user/                       # 用户态组件
│   ├── services/               # 用户态服务
│   │   ├── init/               # 初始化进程
│   │   ├── vfsd/               # VFS 服务
│   │   ├── netd/               # 网络服务
│   │   └── devd/               # 设备管理服务
│   ├── apps/                   # 用户程序示例
│   └── libs/                   # 用户态库
├── tests/                      # 测试代码
│   ├── kernel-unit/            # 内核单元测试
│   └── integration/            # 集成测试
├── tools/                      # 构建和辅助工具
├── scripts/                    # 自动化脚本
├── docs/                       # 文档
│   ├── design/                 # 设计文档
│   ├── dev-setup.md            # 开发环境配置
│   └── roadmap.md              # 项目路线图
├── cross/                      # 交叉编译配置
├── examples/                   # 示例代码
├── build/                      # 构建输出目录
├── Makefile                    # 主构建文件
├── meson.build                 # Meson 构建配置
├── conanfile.py                # Conan 依赖管理
└── README.md                   # 本文件
```

### 内核架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                        用户空间 (User Space)                      │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌───────┐ │
│  │  init   │  │  vfsd   │  │  netd   │  │  devd   │  │ shell │ │
│  │ (PID 1) │  │ (VFS)   │  │ (网络)   │  │ (设备)  │  │       │ │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘  └───┬───┘ │
│       │            │            │            │           │      │
│       └────────────┴────────────┴────────────┴───────────┘      │
│                              │ IPC                              │
├──────────────────────────────┼──────────────────────────────────┤
│                         系统调用接口                              │
├─────────────────────────────────────────────────────────────────┤
│                        内核空间 (Kernel)                         │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                      微内核核心                            │  │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐      │  │
│  │  │ 调度器   │  │ 内存管理 │  │ IPC核心  │  │ 中断处理 │      │  │
│  │  │  (CFS)  │  │ (Buddy) │  │         │  │         │      │  │
│  │  └─────────┘  └─────────┘  └─────────┘  └─────────┘      │  │
│  └───────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                        硬件抽象层 (HAL)                          │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐            │
│  │  x86_64 │  │  页表   │  │  GDT/IDT│  │ 上下文   │            │
│  │  启动    │  │  管理   │  │  管理    │  │  切换   │             │
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘            │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                           硬件                                   │
│     CPU  •  内存  •  磁盘  •  网卡  •  键盘  •  显示器              │
└─────────────────────────────────────────────────────────────────┘
```

### 核心模块说明

#### 调度器 (Scheduler)

基于 Linux CFS (Completely Fair Scheduler) 设计的调度器实现：

- **调度策略**: `SCHED_NORMAL`, `SCHED_FIFO`, `SCHED_RR`, `SCHED_BATCH`, `SCHED_IDLE`
- **优先级**: 支持 nice 值 (-20 到 +19)
- **数据结构**: 红黑树管理可运行任务，保证 O(log n) 时间复杂度
- **负载均衡**: 基于虚拟运行时间 (vruntime) 的公平调度

#### 内存管理 (Memory Management)

- **伙伴系统**: 物理页面分配，最大阶数为 11 (2^11 = 2048 页)
- **虚拟内存**: 4级页表管理，支持按需分页
- **内存区域**: DMA、Normal、HighMem 三个内存区域
- **页面标志**: locked、referenced、dirty、buddy 等状态管理

#### 进程管理 (Process Management)

- **task_struct**: 完整的进程描述符
- **进程状态**: RUNNING, INTERRUPTIBLE, UNINTERRUPTIBLE, STOPPED, ZOMBIE 等
- **Clone 标志**: 支持 CLONE_VM, CLONE_FILES, CLONE_THREAD 等
- **信号处理**: 基础信号框架

## 🛠️ 构建指南

### 使用 Meson (推荐)

```bash
# 配置 bare-metal 交叉构建
meson setup build --cross-file=cross/x86_64-none.ini

# 编译 kernel.elf 和 kernel.bin
meson compile -C build
```

### 使用 Conan + Meson

```bash
# Conan 安装 Meson/Ninja 并调用 Meson cross build
conan build . --output-folder=build-conan
```

### 使用 Makefile 包装命令

```bash
# 完整构建
make all

# 清理构建产物
make clean

# 深度清理
make distclean
```

### 构建选项

| 选项 | 默认值 | 描述 |
|------|--------|------|
| `kernel_debug` | true | 启用调试功能 |
| `serial_debug` | true | 启用串口调试输出 |
| `kernel_optimize` | 2 | 优化级别 (0/1/2/3/s) |
| `max_cpus` | 8 | 最大 CPU 数量 |
| `enable_tests` | false | 构建单元测试 |

### 常用命令

```bash
# 在 QEMU 中运行（需要 grub-mkrescue 和 xorriso）
make qemu

# 调试模式运行
make debug

# 生成反汇编
make disasm

# 生成符号表
make symbols

# 查看内核大小
make size

# 显示配置
make config
```

## 📊 系统调用

当前支持的系统调用：

| 编号 | 名称 | 描述 |
|------|------|------|
| 0 | `read` | 读取文件 |
| 1 | `write` | 写入文件 |
| 2 | `open` | 打开文件 |
| 3 | `close` | 关闭文件 |
| 39 | `getpid` | 获取进程 ID |
| 56 | `clone` | 创建进程/线程 |
| 57 | `fork` | 创建子进程 |
| 58 | `vfork` | 创建子进程 (共享地址空间) |
| 59 | `execve` | 执行程序 |
| 60 | `exit` | 退出进程 |
| 61 | `wait4` | 等待子进程 |
| 62 | `kill` | 发送信号 |
| 63 | `uname` | 获取系统信息 |
| 24 | `sched_yield` | 让出 CPU |
| 12 | `brk` | 调整堆大小 |
| 9 | `mmap` | 内存映射 |
| 11 | `munmap` | 解除内存映射 |
| 99 | `sysinfo` | 获取系统状态 |

## 🧪 测试

```bash
# 运行所有测试
make test

# 运行内核单元测试
cd tests/kernel-unit && make

# 运行集成测试
cd tests/integration && make test
```

## 📚 文档

- [构建指南](/docs/BUILD.md) - 详细的构建说明
- [开发环境配置](docs/dev-setup.md) - 开发环境搭建指南
- [项目路线图](docs/roadmap.md) - 开发计划和进度
- [贡献指南](/docs/CONTRIBUTING.md) - 如何参与贡献
- [行为准则](/docs/CODE_OF_CONDUCT.md) - 社区行为规范

## 🤝 贡献指南

我们欢迎任何形式的贡献！请阅读 [CONTRIBUTING.md](/docs/CONTRIBUTING.md) 了解详细信息。

### 贡献流程

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

### 代码风格

- 遵循 Linux 内核编码风格
- 缩进: 4 空格 (不使用 Tab)
- 行宽: 最大 80 字符
- 使用 `.clang-format` 进行代码格式化

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

```
MIT License

Copyright (c) 2024 minikernel Development Team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction...
```

## 🙏 致谢

- [OSDev Wiki](https://wiki.osdev.org/) - 操作系统开发资源
- [Linux Kernel](https://www.kernel.org/) - 设计参考
- [seL4](https://sel4.systems/) - 微内核设计参考
- 所有贡献者和支持者

## 📮 联系方式

- **Issues**: [GitHub Issues](https://github.com/assumeengagetry/minikernel/issues)
- **Discussions**: [GitHub Discussions](https://github.com/assumeengagetry/minikernel/discussions)

---

<p align="center">
  <sub>用 ❤️ 构建 | Made with ❤️</sub>
</p>
