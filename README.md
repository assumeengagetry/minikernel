# MicroKernel

MicroKernel is a minimal x86_64 freestanding kernel. The current runtime path
is intentionally small:

- GRUB Multiboot2 boot from a BIOS ISO
- 32-bit protected-mode entry and transition to 64-bit long mode
- A 4 KiB bootstrap page table that identity-maps the low 2 MiB
- COM1 serial startup output
- A halted single-CPU idle loop

The kernel does not currently implement memory allocation, scheduling,
interrupts, userspace, system calls, filesystems, networking, or IPC.

## Dependencies

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential binutils grub-common grub-pc-bin \
    xorriso qemu-system-x86
```

## Build And Verify

```bash
make check-tools
make clean
make all
make check
make qemu-smoke
```

Build outputs are written to `bin/` and intermediate objects to `obj/`.
Both directories are ignored by Git.

## Run And Debug

Run QEMU with a serial-only console:

```bash
make qemu
```

Start QEMU paused with a GDB server on TCP port 1234:

```bash
make debug
gdb bin/kernel.elf -ex 'target remote localhost:1234'
```

Generate the bootable ISO without starting QEMU:

```bash
make iso
```

## Structure

- `arch/x86_64/boot.S`: Multiboot2 header, page tables, and long-mode entry
- `arch/x86_64/kernel.ld`: ELF section layout and bootstrap constraints
- `src/kernel/main.c`: serial console and `kernel_main()`
- `grub.cfg`: GRUB Multiboot2 menu entry
- `scripts/make_iso.sh`: BIOS ISO construction
- `scripts/qemu-smoke.sh`: QEMU startup assertion
- `Makefile`: build, validation, ISO, QEMU, and debug targets

The QEMU smoke test succeeds only when serial output reaches
`Kernel initialization complete.`.
