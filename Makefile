BUILD_DIR ?= build
CROSS_FILE ?= cross/x86_64-none.ini
MESON ?= meson

.PHONY: all setup build qemu qemu-smoke debug disasm symbols size clean \
	check-tools help

all: setup build

setup:
	@if [ -d "$(BUILD_DIR)/meson-private" ]; then \
		$(MESON) setup "$(BUILD_DIR)" --reconfigure --cross-file="$(CROSS_FILE)"; \
	else \
		$(MESON) setup "$(BUILD_DIR)" --cross-file="$(CROSS_FILE)"; \
	fi

build:
	$(MESON) compile -C "$(BUILD_DIR)"

qemu: check-tools all
	$(MESON) compile -C "$(BUILD_DIR)" qemu

qemu-smoke: check-tools all
	sh ./scripts/qemu-smoke.sh "$(BUILD_DIR)"

debug: check-tools all
	$(MESON) compile -C "$(BUILD_DIR)" debug

disasm: setup
	$(MESON) compile -C "$(BUILD_DIR)" kernel.dis

symbols: setup
	$(MESON) compile -C "$(BUILD_DIR)" kernel.sym

size: all
	size "$(BUILD_DIR)/kernel.elf"

clean:
	rm -rf "$(BUILD_DIR)"

check-tools:
	@command -v $(MESON) >/dev/null || (printf '%s\n' "$(MESON) not found" && exit 1)
	@command -v ninja >/dev/null || (printf '%s\n' "ninja not found" && exit 1)
	@command -v gcc >/dev/null || (printf '%s\n' "gcc not found" && exit 1)
	@command -v g++ >/dev/null || (printf '%s\n' "g++ not found" && exit 1)
	@command -v objcopy >/dev/null || (printf '%s\n' "objcopy not found" && exit 1)
	@command -v objdump >/dev/null || (printf '%s\n' "objdump not found" && exit 1)
	@command -v grub-mkimage >/dev/null || (printf '%s\n' "grub-mkimage not found" && exit 1)
	@command -v xorriso >/dev/null || (printf '%s\n' "xorriso not found" && exit 1)
	@command -v qemu-system-x86_64 >/dev/null || (printf '%s\n' "qemu-system-x86_64 not found" && exit 1)
	@command -v timeout >/dev/null || (printf '%s\n' "timeout not found" && exit 1)
	@printf '%s\n' "All required tools found"

help:
	@printf '%s\n' \
		"make all         Configure and build kernel.elf/kernel.bin" \
		"make qemu        Build an ISO and run the serial shell" \
		"make qemu-smoke  Boot QEMU and assert that the shell prompt appears" \
		"make debug       Run QEMU paused with a GDB server on port 1234" \
		"make disasm      Generate build/kernel.dis" \
		"make symbols     Generate build/kernel.sym" \
		"make clean       Remove the selected build directory"
