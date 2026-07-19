# MicroKernel Repository Instructions

## Active Scope

- This is a minimal x86_64 freestanding GNU C99 kernel.
- The active runtime path is `arch/x86_64/boot.S` -> `kernel_main()`.
- GRUB loads `bin/kernel.elf` through the Multiboot2 protocol.
- The bootstrap identity-maps the low 2 MiB with 4 KiB pages before entering
  long mode.
- `src/kernel/main.c` initializes COM1, prints the startup marker, and idles.
- Do not describe schedulers, allocators, interrupts, userspace, system calls,
  filesystems, networking, or IPC as implemented.

## Build And Verification

- Check dependencies with `make check-tools`.
- Build the ELF with `make all`.
- Validate Multiboot2 and undefined symbols with `make check`.
- Generate the GRUB BIOS ISO with `make iso`.
- Verify startup with `make qemu-smoke`.
- Run interactively with `make qemu`.
- Run the paused GDB target with `make debug`.
- Remove generated `obj/` and `bin/` directories with `make clean`.

Before committing kernel changes, run:

```bash
make clean
make check
make qemu-smoke
```

## Change Rules

- Keep kernel code freestanding and compatible with the flags in `Makefile`.
- Add active source files explicitly to `KERNEL_SOURCES` or `ARCH_SOURCES`.
- Keep the Multiboot2 header in the first ELF load section.
- Do not commit files from `obj/`, `bin/`, or personal `.vscode/` settings.
- Prefer deleting nonfunctional subsystem scaffolding over documenting it as a
  future implementation.
