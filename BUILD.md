# MicroKernel 构建指南

本文档介绍如何使用 Meson 和 Conan 构建 MicroKernel 项目。

## 目录

- [前置要求](#前置要求)
- [快速开始](#快速开始)
- [构建方法](#构建方法)
  - [方法一：使用构建脚本（推荐）](#方法一使用构建脚本推荐)
  - [方法二：使用 Conan + Meson](#方法二使用-conan--meson)
  - [方法三：仅使用 Meson](#方法三仅使用-meson)
- [构建选项](#构建选项)
- [运行内核](#运行内核)
- [调试](#调试)
- [项目结构](#项目结构)
- [常见问题](#常见问题)

---

## 前置要求

### 必需工具

| 工具 | 最低版本 | 用途 |
|------|----------|------|
| GCC | 9.0+ | C 编译器 |
| NASM | 2.14+ | 汇编器 |
| Binutils | 2.34+ | 链接器、objcopy、objdump |
| Meson | 1.0.0+ | 构建系统 |
| Ninja | 1.10+ | 构建后端 |

### 可选工具

| 工具 | 用途 |
|------|------|
| Conan | 依赖管理（2.0+） |
| QEMU | 运行/测试内核 |
| GDB | 调试 |
| grub-mkrescue | 生成 ISO 镜像 |

### 安装依赖

**Ubuntu/Debian:**
```bash
# 必需工具
sudo apt update
sudo apt install build-essential gcc nasm binutils

# Meson 和 Ninja
sudo apt install meson ninja-build

# 可选：QEMU
sudo apt install qemu-system-x86

# 可选：Conan
pip install conan
```

**Arch Linux:**
```bash
sudo pacman -S base-devel gcc nasm meson ninja qemu-system-x86
pip install conan
```

**Fedora:**
```bash
sudo dnf install gcc nasm binutils meson ninja-build qemu-system-x86
pip install conan
```

---

## 快速开始

最快的构建方式：

```bash
# 克隆项目后进入目录
cd Kernal

# 使用构建脚本
./scripts/build.sh all

# 运行内核
./scripts/build.sh qemu
```

---

## 构建方法

### 方法一：使用构建脚本（推荐）

构建脚本封装了所有构建步骤，是最简单的方式。

```bash
# 完整构建（检查依赖 + 配置 + 编译）
./scripts/build.sh all

# 仅构建
./scripts/build.sh build

# 清理并重新构建
./scripts/build.sh rebuild

# 清理构建目录
./scripts/build.sh clean

# 查看所有可用命令
./scripts/build.sh help
```

### 方法二：使用 Conan + Meson

如果你需要管理复杂的依赖关系，推荐使用 Conan。

```bash
# 1. 安装依赖并生成构建文件
conan install . --output-folder=build --build=missing

# 2. 配置 Meson（使用 Conan 生成的工具链）
cd build
meson setup .. --cross-file=../cross/x86_64-none.ini

# 3. 编译
meson compile

# 或者使用 conanfile.py 一键构建
conan build .
```

### 方法三：仅使用 Meson

如果不需要 Conan，可以直接使用 Meson。

```bash
# 1. 配置（使用交叉编译文件）
meson setup build --cross-file=cross/x86_64-none.ini

# 2. 编译
meson compile -C build

# 或者
cd build && ninja
```

---

## 构建选项

### Meson 选项

在 `meson_options.txt` 中定义了以下选项：

| 选项 | 类型 | 默认值 | 描述 |
|------|------|--------|------|
| `arch` | combo | x86_64 | 目标架构 |
| `kernel_debug` | boolean | true | 启用调试功能 |
| `serial_debug` | boolean | true | 启用串口调试输出 |
| `kernel_optimize` | combo | 2 | 优化级别 (0/1/2/3/s) |
| `max_cpus` | integer | 8 | 最大 CPU 数量 |
| `enable_tests` | boolean | false | 构建单元测试 |
| `enable_userspace` | boolean | false | 构建用户空间程序 |
| `log_level` | combo | info | 日志级别 |

### 使用选项

```bash
# 配置时设置选项
meson setup build --cross-file=cross/x86_64-none.ini \
    -Dkernel_debug=true \
    -Dmax_cpus=16 \
    -Dlog_level=debug

# 修改已有配置的选项
meson configure build -Dkernel_optimize=3
```

### Conan 选项

在 `conanfile.py` 中可以设置以下选项：

```bash
conan install . -o arch=x86_64 -o kernel_debug=True -o max_cpus=16
```

---

## 运行内核

### 使用 QEMU

```bash
# 使用构建脚本
./scripts/build.sh qemu

# 或直接运行
qemu-system-x86_64 -kernel build/kernel.bin -m 512M -serial stdio
```

### 调试模式

```bash
# 启动 QEMU 并等待 GDB 连接
./scripts/build.sh debug

# 在另一个终端连接 GDB
gdb build/kernel.elf -ex 'target remote localhost:1234'
```

### 生成 ISO（需要 grub-mkrescue）

```bash
# 生成可启动 ISO
meson compile -C build kernel.iso
```

---

## 调试

### 生成反汇编

```bash
./scripts/build.sh disasm
# 输出：build/kernel.dis
```

### 查看符号表

```bash
objdump -t build/kernel.elf > kernel.sym
```

### GDB 调试命令

```gdb
# 连接到 QEMU
target remote localhost:1234

# 设置断点
break kernel_main
break *0x100000

# 继续执行
continue

# 单步执行
step
next

# 查看寄存器
info registers

# 查看内存
x/10x 0x100000
```

---

## 项目结构

```
Kernal/
├── arch/                   # 架构相关代码
│   └── x86_64/
│       ├── boot/          # 引导代码和链接脚本
│       ├── cpu/           # CPU 相关代码
│       └── mm/            # 内存管理
├── cross/                  # 交叉编译配置文件
│   └── x86_64-none.ini
├── include/                # 公共头文件
├── kernel/                 # 内核核心代码
│   ├── core/              # 核心模块
│   │   └── sched/         # 调度器
│   ├── include/           # 内核头文件
│   ├── interrupt/         # 中断处理
│   └── mm/                # 内存管理
├── scripts/                # 构建和辅助脚本
│   └── build.sh
├── src/                    # 主要源代码
│   └── kernel/
│       └── main.c         # 内核入口
├── tests/                  # 测试代码
├── user/                   # 用户空间程序
├── conanfile.py           # Conan 配置
├── conanfile.txt          # Conan 简化配置
├── meson.build            # Meson 主构建文件
├── meson_options.txt      # Meson 选项定义
├── Makefile               # 传统 Makefile（兼容）
└── BUILD.md               # 本文档
```

---

## 常见问题

### Q: 构建失败，提示找不到 nasm

确保 NASM 已安装：
```bash
sudo apt install nasm  # Debian/Ubuntu
sudo pacman -S nasm    # Arch
sudo dnf install nasm  # Fedora
```

### Q: 链接错误，找不到符号

检查链接脚本路径是否正确，确保 `arch/x86_64/boot/kernel.ld` 存在。

### Q: QEMU 启动后没有输出

确保使用了 `-serial stdio` 参数，内核通过串口输出调试信息。

### Q: 如何切换回传统 Makefile 构建？

原有的 Makefile 仍然保留，可以直接使用：
```bash
make clean
make all
make qemu
```

### Q: Conan 2.x 和 1.x 有什么区别？

本项目使用 Conan 2.x API。如果你使用 Conan 1.x，需要升级：
```bash
pip install --upgrade conan
```

### Q: 如何添加新的源文件？

编辑 `meson.build`，在 `kernel_sources` 或 `arch_asm_sources` 中添加新文件：
```meson
kernel_sources = files(
    'src/kernel/main.c',
    'src/kernel/new_file.c',  # 新增
    ...
)
```

---

## 相关链接

- [Meson 官方文档](https://mesonbuild.com/)
- [Conan 官方文档](https://docs.conan.io/)
- [QEMU 文档](https://www.qemu.org/documentation/)
- [OSDev Wiki](https://wiki.osdev.org/)

---

## 贡献

欢迎提交 Issue 和 Pull Request！请参阅 [CONTRIBUTING.md](CONTRIBUTING.md) 了解贡献指南。