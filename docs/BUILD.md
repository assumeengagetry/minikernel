# Build And Run

## Build

根目录的 Makefile 是 Meson 的薄封装：

```bash
make check-tools
make all
```

首次配置时可直接使用 Meson：

```bash
meson setup build --cross-file=cross/x86_64-none.ini
meson compile -C build
```

已有构建目录可使用：

```bash
meson setup build --reconfigure --cross-file=cross/x86_64-none.ini
meson compile -C build
```

`make all` 会重新配置已有构建目录。Meson 只在首次配置时读取 cross file，
修改 `cross/x86_64-none.ini` 后需要完全重建：

```bash
make clean
make all
```

## ISO And QEMU

`kernel.iso` 由 Meson 调用 `scripts/make_iso.sh` 按需生成到构建目录；ISO
暂存目录不在源码树中维护。

```bash
make qemu
```

QEMU 通过 COM1 提供 Shell；图形显示和 QEMU monitor 默认关闭。

自动启动检查：

```bash
make qemu-smoke
```

该命令启动 QEMU 最多 10 秒，并要求串口输出包含内核初始化完成消息和
`microkernel>` 提示符。可用 `QEMU_SMOKE_TIMEOUT` 调整秒数。

## Debugging

```bash
make debug
gdb build/kernel.elf -ex 'target remote localhost:1234'
```

## Verification

```bash
make check-tools
make clean
make all
grub-file --is-x86-multiboot build/kernel.elf
test -z "$(nm -u build/kernel.elf)"
make qemu-smoke
```

`nm -u` 应无输出。
