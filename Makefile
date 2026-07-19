# 简化版 Makefile

# 目标架构
ARCH := x86_64

# 工具链
CC := gcc
LD := ld
AS := $(CC)

# 编译选项
CFLAGS := -ffreestanding -nostdlib -m64 -Iinclude -std=gnu99
ASFLAGS := -m64 -c

# 目录
SRCDIR := src
OBJDIR := obj
BINDIR := bin
ARCHDIR := arch/$(ARCH)

# 源文件
KERNEL_SOURCES := $(SRCDIR)/kernel/main.c $(SRCDIR)/kernel/sched.c
ARCH_SOURCES := $(ARCHDIR)/boot.S

# 对象文件
KERNEL_OBJECTS := $(KERNEL_SOURCES:%.c=$(OBJDIR)/%.o)
ARCH_OBJECTS := $(ARCH_SOURCES:%.S=$(OBJDIR)/%.o)
ALL_OBJECTS := $(KERNEL_OBJECTS) $(ARCH_OBJECTS)

# 目标文件
KERNEL_ELF := $(BINDIR)/kernel.elf

# 默认目标
all: $(KERNEL_ELF)

# 创建必要的目录
$(OBJDIR):
	mkdir -p $(OBJDIR)/$(SRCDIR)/kernel
	mkdir -p $(OBJDIR)/$(ARCHDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# 编译C源文件
$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 编译汇编文件
$(OBJDIR)/%.o: %.S | $(OBJDIR)
	$(AS) $(ASFLAGS) $< -o $@

# 链接内核
$(KERNEL_ELF): $(ALL_OBJECTS) $(ARCHDIR)/kernel.ld | $(BINDIR)
	$(LD) -nostdlib -static -T $(ARCHDIR)/kernel.ld -o $@ $(ALL_OBJECTS)

# 清理
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# 伪目标
.PHONY: all clean check-tools

check-tools:
	@command -v $(CC) >/dev/null || (echo "$(CC) not found" && exit 1)
	@command -v $(LD) >/dev/null || (echo "$(LD) not found" && exit 1)
	@echo "All build tools found"
