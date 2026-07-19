# MicroKernel

MicroKernel 是一个最小的 x86_64 freestanding 内核。目前的可运行范围是：

- GRUB Multiboot1 启动
- 32 位保护模式切换到 64 位长模式
- 串口日志和交互式 Shell

项目当前不包含内存分配器、调度器、用户态、系统调用或中断子系统。

## 依赖

Ubuntu/Debian：

```bash
sudo apt update
sudo apt install build-essential meson ninja-build binutils \
    grub-pc-bin xorriso qemu-system-x86
```

## 构建

```bash
make check-tools
make all
```

构建结果位于 `build/kernel.elf`。运行 QEMU 时会按需生成
`build/kernel.iso`，两个文件均由 `make clean` 删除。

## QEMU

交互运行：

```bash
make qemu
```

自动确认内核到达 Shell 提示符：

```bash
make qemu-smoke
```

调试模式会暂停 CPU，并在 TCP 1234 端口等待 GDB：

```bash
make debug
gdb build/kernel.elf -ex 'target remote localhost:1234'
```

Shell 使用 QEMU 串口输入输出。输入 `help` 查看可用命令。

## 结构

- `arch/x86_64/boot/`：Multiboot 入口、长模式切换和链接脚本
- `src/kernel/main.cpp`：串口控制台和内核入口
- `src/kernel/shell.cpp`：交互式内核 Shell
- `kernel/include/`：当前构建使用的基础头文件
- `scripts/`：ISO 生成和 QEMU 冒烟测试
- `cross/`：Meson bare-metal 配置

更详细的命令见 [`docs/BUILD.md`](docs/BUILD.md)。
