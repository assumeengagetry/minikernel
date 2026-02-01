ARCH := x86_64

CC := gcc
LD := ld
AS := nasm
OBJCOPY := objcopy
OBJDUMP := objdump

CFLAGS := -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector
CFLAGS += -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-sse3 -mno-3dnow
CFLAGS += -m64 -mcmodel=kernel -fno-pic -fno-pie
CFLAGS += -Wall -Wextra -Werror -O2 -g
CFLAGS += -Ikernel/include -std=gnu99

LDFLAGS := -nostdlib -static -Wl,--build-id=none
LDFLAGS += -Wl,-z,max-page-size=0x1000 -Wl,-z,common-page-size=0x1000

ASFLAGS := -f elf64

SRCDIR := src
INCDIR := kernel/include
OBJDIR := obj
BINDIR := bin
ARCHDIR := arch/$(ARCH)

KERNEL_SOURCES := $(SRCDIR)/kernel/main.c
KERNEL_SOURCES += $(SRCDIR)/kernel/sched.c
KERNEL_SOURCES += $(SRCDIR)/kernel/sched_fair.c
KERNEL_SOURCES += $(SRCDIR)/mm/buddy.c

ARCH_SOURCES := $(ARCHDIR)/boot.S
ARCH_SOURCES += $(ARCHDIR)/entry.S
ARCH_SOURCES += $(ARCHDIR)/switch.S
ARCH_SOURCES += $(ARCHDIR)/interrupt.S

KERNEL_OBJECTS := $(KERNEL_SOURCES:%.c=$(OBJDIR)/%.o)
ARCH_OBJECTS := $(ARCH_SOURCES:%.S=$(OBJDIR)/%.o)
ALL_OBJECTS := $(KERNEL_OBJECTS) $(ARCH_OBJECTS)

KERNEL_ELF := $(BINDIR)/kernel.elf
KERNEL_BIN := $(BINDIR)/kernel.bin
KERNEL_ISO := $(BINDIR)/kernel.iso

all: $(KERNEL_ELF) $(KERNEL_BIN)

$(OBJDIR):
	mkdir -p $(OBJDIR)/$(SRCDIR)/kernel
	mkdir -p $(OBJDIR)/$(SRCDIR)/mm
	mkdir -p $(OBJDIR)/$(SRCDIR)/fs
	mkdir -p $(OBJDIR)/$(SRCDIR)/net
	mkdir -p $(OBJDIR)/$(SRCDIR)/ipc
	mkdir -p $(OBJDIR)/$(SRCDIR)/drivers
	mkdir -p $(OBJDIR)/$(ARCHDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.S | $(OBJDIR)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL_ELF): $(ALL_OBJECTS) $(ARCHDIR)/kernel.ld | $(BINDIR)
	$(LD) $(LDFLAGS) -T $(ARCHDIR)/kernel.ld -o $@ $(ALL_OBJECTS)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

$(KERNEL_ISO): $(KERNEL_BIN)
	mkdir -p iso/boot/grub
	cp $(KERNEL_BIN) iso/boot/kernel.bin
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo '' >> iso/boot/grub/grub.cfg
	echo 'menuentry "Micro Kernel" {' >> iso/boot/grub/grub.cfg
	echo '    multiboot2 /boot/kernel.bin' >> iso/boot/grub/grub.cfg
	echo '    boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ iso/

qemu: $(KERNEL_BIN)
	qemu-system-x86_64 -kernel $(KERNEL_BIN) -m 512M -serial stdio

debug: $(KERNEL_ELF)
	qemu-system-x86_64 -kernel $(KERNEL_ELF) -m 512M -serial stdio -s -S

disasm: $(KERNEL_ELF)
	$(OBJDUMP) -d $< > $(BINDIR)/kernel.dis

symbols: $(KERNEL_ELF)
	$(OBJDUMP) -t $< > $(BINDIR)/kernel.sym

size: $(KERNEL_ELF)
	size $<

clean:
	rm -rf $(OBJDIR) $(BINDIR) iso/

distclean: clean
	rm -f *~

help:
	@echo "Available targets:"
	@echo "  all       - Build kernel ELF and binary"
	@echo "  qemu      - Run kernel in QEMU"
	@echo "  debug     - Run kernel in QEMU with GDB support"
	@echo "  disasm    - Generate disassembly"
	@echo "  symbols   - Generate symbol table"
	@echo "  size      - Show kernel size"
	@echo "  clean     - Clean build files"
	@echo "  distclean - Deep clean"
	@echo "  help      - Show this help"

check-tools:
	@which $(CC) > /dev/null || (echo "$(CC) not found" && exit 1)
	@which $(LD) > /dev/null || (echo "$(LD) not found" && exit 1)
	@which $(AS) > /dev/null || (echo "$(AS) not found" && exit 1)
	@which $(OBJCOPY) > /dev/null || (echo "$(OBJCOPY) not found" && exit 1)
	@which $(OBJDUMP) > /dev/null || (echo "$(OBJDUMP) not found" && exit 1)
	@echo "All tools found"

stats:
	@echo "Lines of code:"
	@find $(SRCDIR) $(INCDIR) -name "*.c" -o -name "*.h" | xargs wc -l | tail -1
	@echo "Assembly lines:"
	@find $(ARCHDIR) -name "*.S" | xargs wc -l | tail -1



release: $(KERNEL_BIN) $(KERNEL_ISO)
	@mkdir -p release
	@cp $(KERNEL_BIN) release/
	@cp $(KERNEL_ISO) release/
	@cp README.md release/
	@tar -czf release/microkernel-$(shell date +%Y%m%d).tar.gz release/
	@echo "Release package created in release/"

depends:
	@echo "Checking dependencies..."
	@echo "Required packages:"
	@echo "  - gcc (cross-compiler for x86_64)"
	@echo "  - nasm (assembler)"
	@echo "  - binutils (linker, objcopy, objdump)"
	@echo "  - qemu-system-x86_64 (emulator)"
	@echo "  - grub-mkrescue (for ISO creation)"
	@echo "  - xorriso (ISO creation dependency)"

config:
	@echo "Kernel configuration:"
	@echo "  Architecture: $(ARCH)"
	@echo "  Compiler: $(CC)"
	@echo "  CFLAGS: $(CFLAGS)"
	@echo "  LDFLAGS: $(LDFLAGS)"
