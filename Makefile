ARCH ?= x86_64
BUILD_DIR ?= build
CROSS_FILE ?= cross/x86_64-none.ini

MESON ?= meson
CONAN ?= conan
QEMU ?= qemu-system-x86_64

.PHONY: all setup conan build qemu debug disasm symbols size clean distclean check-tools config help

all: setup build

setup:
	@if [ -d "$(BUILD_DIR)" ]; then \
		$(MESON) setup "$(BUILD_DIR)" --reconfigure --cross-file="$(CROSS_FILE)"; \
	else \
		$(MESON) setup "$(BUILD_DIR)" --cross-file="$(CROSS_FILE)"; \
	fi

conan:
	$(CONAN) install . --output-folder="$(BUILD_DIR)" --build=missing
	$(CONAN) build . --output-folder="$(BUILD_DIR)"

build:
	$(MESON) compile -C "$(BUILD_DIR)"

qemu: all
	$(MESON) compile -C "$(BUILD_DIR)" qemu

debug: setup
	$(MESON) compile -C "$(BUILD_DIR)" debug

disasm: setup
	$(MESON) compile -C "$(BUILD_DIR)" kernel.dis

symbols: setup
	$(MESON) compile -C "$(BUILD_DIR)" kernel.sym

size: all
	size "$(BUILD_DIR)/kernel.elf"

clean:
	rm -rf "$(BUILD_DIR)"

distclean: clean
	rm -f *~

check-tools:
	@command -v $(MESON) >/dev/null || (echo "$(MESON) not found" && exit 1)
	@command -v ninja >/dev/null || (echo "ninja not found" && exit 1)
	@command -v gcc >/dev/null || (echo "gcc not found" && exit 1)
	@command -v g++ >/dev/null || (echo "g++ not found" && exit 1)
	@command -v objcopy >/dev/null || (echo "objcopy not found" && exit 1)
	@command -v objdump >/dev/null || (echo "objdump not found" && exit 1)
	@echo "All required tools found"

config:
	@echo "Kernel configuration:"
	@echo "  Architecture: $(ARCH)"
	@echo "  Build dir:    $(BUILD_DIR)"
	@echo "  Cross file:   $(CROSS_FILE)"

help:
	@echo "Available targets:"
	@echo "  all       - Configure and build with Meson"
	@echo "  setup     - Configure or reconfigure Meson"
	@echo "  conan     - Install Conan tool requirements and build"
	@echo "  build     - Build kernel ELF and binary"
	@echo "  qemu      - Run kernel in QEMU"
	@echo "  debug     - Run QEMU with GDB support"
	@echo "  disasm    - Generate disassembly"
	@echo "  symbols   - Generate symbol table"
	@echo "  size      - Show kernel size"
	@echo "  clean     - Clean Meson build directory"
	@echo "  distclean - Deep clean"
	@echo "  config    - Show build configuration"
	@echo "  help      - Show this help"
