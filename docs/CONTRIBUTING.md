# Contributing

## Style

- 使用根目录 `.clang-format`：4 空格、不使用 Tab、80 列、Linux brace style。
- 新增代码注释使用英文。
- 内核是 freestanding C++17/GNU C99 环境，不得依赖宿主标准库。
- 新源文件必须显式加入 `meson.build`。

## Verification

提交前按顺序运行：

```bash
make check-tools
make clean
make all
grub-file --is-x86-multiboot build/kernel.elf
test -z "$(nm -u build/kernel.elf)"
make qemu-smoke
```

仓库当前没有独立单元测试、lint 或 typecheck 目标；QEMU 冒烟测试是运行时
验收入口。
