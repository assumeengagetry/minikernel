# Repository Instructions

## Build And Run

- The source of truth is `meson.build`, `meson_options.txt`,
  `cross/x86_64-none.ini`, and the root `Makefile`.
- Check dependencies with `make check-tools`.
- Build with `make all`.
- Run interactively with `make qemu`.
- Verify startup with `make qemu-smoke`; it requires the serial output to reach
  `Kernel initialization complete.` and `microkernel>`.
- Run the paused debugger target with `make debug`, then attach GDB to port
  1234.
- `make clean` removes the ignored `build/` directory. Build outputs must not
  be committed.

## Architecture

- This is an x86_64 freestanding kernel built with GNU C99/C++17 and no hosted
  standard library.
- The active source list is explicit in `meson.build`: the Multiboot1 boot
  assembly, `src/kernel/main.cpp`, `src/kernel/shell.cpp`, and
  `kernel/mm/buddy.cpp`.
- Boot flow is `arch/x86_64/boot/boot.S` -> `kernel_main()` -> memory
  initialization -> the serial Shell.
- The ISO is generated at build time by `scripts/make_iso.sh` using
  `grub-mkimage` and `xorriso`; it does not use `grub-mkrescue` or mtools.
- Scheduler, userspace, syscall, and interrupt implementations are not part
  of the current build. Do not document them as working features.

## Change Rules

- Adding a source file requires adding it explicitly to `meson.build`.
- Keep kernel code freestanding and compatible with the flags in
  `meson.build` and `cross/x86_64-none.ini`.
- Follow `.clang-format`: four spaces, no tabs, 80 columns, Linux brace style.
- New code comments should be in English.
- Before finishing a kernel change, run `make clean`, `make all`,
  `grub-file --is-x86-multiboot build/kernel.elf`,
  `test -z "$(nm -u build/kernel.elf)"`, and `make qemu-smoke`.
- There are no separate unit-test, lint, or typecheck targets in this repo.
