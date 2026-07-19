BUILD_DIR ?= build
CROSS_FILE ?= cross/x86_64-none.ini
MESON ?= meson

.PHONY: all setup qemu qemu-smoke debug clean \
	check-tools help

all: setup
	$(MESON) compile -C "$(BUILD_DIR)"

setup:
	@if [ -d "$(BUILD_DIR)/meson-private" ]; then \
		$(MESON) setup "$(BUILD_DIR)" --reconfigure --cross-file="$(CROSS_FILE)"; \
	else \
		$(MESON) setup "$(BUILD_DIR)" --cross-file="$(CROSS_FILE)"; \
	fi

qemu: check-tools setup
	$(MESON) compile -C "$(BUILD_DIR)" qemu

qemu-smoke: check-tools setup
	sh ./scripts/qemu-smoke.sh "$(BUILD_DIR)"

debug: check-tools setup
	$(MESON) compile -C "$(BUILD_DIR)" debug

clean:
	rm -rf "$(BUILD_DIR)"

check-tools:
	@command -v $(MESON) >/dev/null || (printf '%s\n' "$(MESON) not found" && exit 1)
	@command -v ninja >/dev/null || (printf '%s\n' "ninja not found" && exit 1)
	@command -v gcc >/dev/null || (printf '%s\n' "gcc not found" && exit 1)
	@command -v g++ >/dev/null || (printf '%s\n' "g++ not found" && exit 1)
	@command -v grub-mkimage >/dev/null || (printf '%s\n' "grub-mkimage not found" && exit 1)
	@command -v xorriso >/dev/null || (printf '%s\n' "xorriso not found" && exit 1)
	@command -v qemu-system-x86_64 >/dev/null || (printf '%s\n' "qemu-system-x86_64 not found" && exit 1)
	@command -v timeout >/dev/null || (printf '%s\n' "timeout not found" && exit 1)
	@printf '%s\n' "All required tools found"

help:
	@printf '%s\n' \
		"make all         Configure and build kernel.elf" \
		"make qemu        Build an ISO and run the serial shell" \
		"make qemu-smoke  Boot QEMU and assert that the shell prompt appears" \
		"make debug       Run QEMU paused with a GDB server on port 1234" \
		"make clean       Remove the selected build directory"
