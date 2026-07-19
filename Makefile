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
	rm -rf "$(OBJDIR)" "$(BINDIR)" "iso"

check-tools:
	@command -v "$(CC)" >/dev/null || (printf '%s\n' "$(CC) not found" && exit 1)
	@command -v "$(LD)" >/dev/null || (printf '%s\n' "$(LD) not found" && exit 1)
	@command -v nm >/dev/null || (printf '%s\n' "nm not found" && exit 1)
	@printf '%s\n' "All build tools found"

.PHONY: all clean check-tools
