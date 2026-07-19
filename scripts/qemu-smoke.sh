#!/bin/sh
set -eu

build_dir=${1:-build}
timeout_seconds=${QEMU_SMOKE_TIMEOUT:-10}
kernel_iso="$build_dir/kernel.iso"
log_file=$(mktemp)

cleanup() {
    rm -f "$log_file"
}
trap cleanup EXIT

meson compile -C "$build_dir" kernel.iso

status=0
timeout "${timeout_seconds}s" qemu-system-x86_64 \
    -cdrom "$kernel_iso" \
    -m 512M \
    -display none \
    -serial stdio \
    -monitor none \
    -no-reboot \
    -no-shutdown \
    >"$log_file" 2>&1 || status=$?

if ! grep -Fq "Kernel initialization complete." "$log_file" || \
   ! grep -Fq "microkernel> " "$log_file"; then
    cat "$log_file"
    printf 'QEMU smoke test failed with status %s\n' "$status" >&2
    exit 1
fi

printf '%s\n' "QEMU reached the MicroKernel shell prompt"
