# Minimal freestanding kernel build.

CC ?= gcc
LD ?= ld

SRCDIR := src
OBJDIR := obj
BINDIR := bin
ARCHDIR := arch/x86_64

CFLAGS := -ffreestanding -fno-builtin -fno-pie -fno-stack-protector \
	-mno-red-zone -m64 -Iinclude -std=gnu99 -Wall -Wextra -Werror
ASFLAGS := -m64 -c

KERNEL_SOURCES := $(SRCDIR)/kernel/main.c
ARCH_SOURCES := $(ARCHDIR)/boot.S
KERNEL_OBJECTS := $(KERNEL_SOURCES:%.c=$(OBJDIR)/%.o)
ARCH_OBJECTS := $(ARCH_SOURCES:%.S=$(OBJDIR)/%.o)
ALL_OBJECTS := $(KERNEL_OBJECTS) $(ARCH_OBJECTS)
KERNEL_ELF := $(BINDIR)/kernel.elf
KERNEL_ISO := $(BINDIR)/kernel.iso

all: $(KERNEL_ELF)

$(OBJDIR)/%.o: %.c
	mkdir -p "$(@D)"
	$(CC) $(CFLAGS) -c "$<" -o "$@"

$(OBJDIR)/%.o: %.S
	mkdir -p "$(@D)"
	$(CC) $(ASFLAGS) "$<" -o "$@"

$(KERNEL_ELF): $(ALL_OBJECTS) $(ARCHDIR)/kernel.ld
	mkdir -p "$(@D)"
	$(LD) -nostdlib -static -z max-page-size=0x1000 \
		-T "$(ARCHDIR)/kernel.ld" -o "$@" $(ALL_OBJECTS)

clean:
	rm -rf "$(OBJDIR)" "$(BINDIR)"

check-tools:
	@command -v "$(CC)" >/dev/null || (printf '%s\n' "$(CC) not found" && exit 1)
	@command -v "$(LD)" >/dev/null || (printf '%s\n' "$(LD) not found" && exit 1)
	@command -v nm >/dev/null || (printf '%s\n' "nm not found" && exit 1)
	@command -v grub-file >/dev/null || (printf '%s\n' "grub-file not found" && exit 1)
	@command -v grub-mkimage >/dev/null || (printf '%s\n' "grub-mkimage not found" && exit 1)
	@command -v xorriso >/dev/null || (printf '%s\n' "xorriso not found" && exit 1)
	@command -v qemu-system-x86_64 >/dev/null || (printf '%s\n' "qemu-system-x86_64 not found" && exit 1)
	@command -v timeout >/dev/null || (printf '%s\n' "timeout not found" && exit 1)
	@printf '%s\n' "All build tools found"

$(KERNEL_ISO): $(KERNEL_ELF) grub.cfg
	mkdir -p "$(@D)"
	sh scripts/make_iso.sh "$(KERNEL_ELF)" "$@" grub.cfg

check: check-tools all
	grub-file --is-x86-multiboot2 "$(KERNEL_ELF)"
	test -z "$$(nm -u "$(KERNEL_ELF)")"

iso: $(KERNEL_ISO)

qemu: check-tools $(KERNEL_ISO)
	qemu-system-x86_64 -cdrom "$(KERNEL_ISO)" -m 512M \
		-display none -serial stdio -monitor none -no-reboot -no-shutdown

debug: check-tools $(KERNEL_ISO)
	qemu-system-x86_64 -cdrom "$(KERNEL_ISO)" -m 512M \
		-display none -serial stdio -monitor none -s -S -no-reboot -no-shutdown

qemu-smoke: check-tools $(KERNEL_ISO)
	sh scripts/qemu-smoke.sh "$(KERNEL_ISO)"

.PHONY: all clean check check-tools iso qemu debug qemu-smoke
