# MiniKernel OS 文档中心

欢迎来到 MiniKernel OS 的技术文档中心！本文档详细介绍了内核的设计原理、实现细节和 API 参考。

---

## 📚 文档导航

### 架构设计

| 文档 | 描述 |
|------|------|
| [架构总览](architecture/00-overview.md) | 系统整体架构、设计理念、目录结构 |
| [启动流程详解](architecture/01-boot-process.md) | 从 BIOS 到内核初始化的完整启动过程 |

### 子系统详解

| 文档 | 描述 |
|------|------|
| [内存管理](subsystems/memory-management.md) | 伙伴系统、页表管理、虚拟内存 |
| [进程调度](subsystems/scheduler.md) | CFS 调度器、任务状态、上下文切换 |
| [中断处理](subsystems/interrupts.md) | IDT、异常处理、IRQ、系统调用入口 |

### API 参考

| 文档 | 描述 |
|------|------|
| [系统调用](api/syscalls.md) | 完整的系统调用 API 参考 |

### 开发指南

| 文档 | 描述 |
|------|------|
| [构建指南](BUILD.md) | 编译、运行、调试内核的方法 |
| [开发环境配置](dev-setup.md) | 搭建开发环境的步骤 |
| [贡献指南](CONTRIBUTING.md) | 如何参与项目贡献 |
| [行为准则](CODE_OF_CONDUCT.md) | 社区行为规范 |
| [项目路线图](roadmap.md) | 开发计划和进度 |

---

## 🗂️ 文档结构

```
docs/
├── README.md                      # 本文件 - 文档索引
├── BUILD.md                       # 构建指南
├── CODE_OF_CONDUCT.md             # 行为准则
├── CONTRIBUTING.md                # 贡献指南
├── dev-setup.md                   # 开发环境配置
├── roadmap.md                     # 项目路线图
│
├── architecture/                  # 架构文档
│   ├── 00-overview.md             # 架构总览
│   └── 01-boot-process.md         # 启动流程详解
│
├── subsystems/                    # 子系统文档
│   ├── memory-management.md       # 内存管理详解
│   ├── scheduler.md               # 进程调度详解
│   └── interrupts.md              # 中断处理详解
│
├── api/                           # API 文档
│   └── syscalls.md                # 系统调用参考
│
└── design/                        # 设计文档（待补充）
```

---

## 🎯 快速入门

### 新手推荐阅读顺序

1. **[架构总览](architecture/00-overview.md)** - 了解系统整体设计
2. **[启动流程详解](architecture/01-boot-process.md)** - 理解内核如何启动
3. **[内存管理](subsystems/memory-management.md)** - 学习伙伴系统分配器
4. **[进程调度](subsystems/scheduler.md)** - 理解 CFS 调度算法
5. **[系统调用](api/syscalls.md)** - 查阅 API 参考

### 想要贡献代码？

1. 阅读 [开发环境配置](dev-setup.md) 搭建环境
2. 阅读 [构建指南](BUILD.md) 学习编译方法
3. 阅读 [贡献指南](CONTRIBUTING.md) 了解贡献流程
4. 查看 [项目路线图](roadmap.md) 选择感兴趣的任务

---

## 📖 核心概念速查

### 内存管理

| 概念 | 说明 |
|------|------|
| 伙伴系统 | 物理页面分配算法，O(log n) 时间复杂度 |
| 阶 (Order) | 分配块大小，order=n 表示 2^n 个页面 |
| GFP 标志 | 内存分配行为标志，如 GFP_KERNEL、GFP_ATOMIC |
| 页帧号 (PFN) | 物理页面的编号，PFN = 物理地址 >> 12 |

### 进程调度

| 概念 | 说明 |
|------|------|
| CFS | 完全公平调度器，基于虚拟运行时间 |
| vruntime | 虚拟运行时间，决定调度顺序 |
| 红黑树 | 存储可运行任务的数据结构 |
| 时间片 | 任务一次获得的 CPU 时间 |

### 中断处理

| 概念 | 说明 |
|------|------|
| IDT | 中断描述符表，存储中断处理程序地址 |
| IRQ | 硬件中断请求，如时钟、键盘 |
| 异常 | CPU 内部产生的同步中断，如页错误 |
| syscall | 系统调用指令，用户态进入内核态 |

---

## 🔧 常用命令

```bash
# 构建内核
make all
# 或使用 Meson
meson compile -C build

# 在 QEMU 中运行
make qemu

# 调试模式
make debug
# 在另一终端连接 GDB
gdb build/kernel.elf -ex 'target remote localhost:1234'

# 生成反汇编
make disasm

# 清理构建
make clean
```

---

## 📮 获取帮助

- **Issues**: [GitHub Issues](https://github.com/assumeengagetry/minikernel/issues)
- **Discussions**: [GitHub Discussions](https://github.com/assumeengagetry/minikernel/discussions)
- **参考资料**: [OSDev Wiki](https://wiki.osdev.org/)

---

## 📝 文档贡献

发现文档错误或想补充内容？欢迎提交 Pull Request！

文档编写规范：
- 使用中文编写
- 代码示例需包含注释
- 复杂概念配以图表说明
- 保持格式统一

---

*最后更新：2024年*