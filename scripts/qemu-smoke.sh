#!/bin/sh
set -eu

kernel_iso=${1:?kernel ISO path is required}
timeout_seconds=${QEMU_SMOKE_TIMEOUT:-10}
log_file=$(mktemp)

cleanup() {
    rm -f "$log_file"
}
trap cleanup EXIT

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

if [ "$status" -ne 124 ] || \
   ! grep -Fq "Kernel initialization complete." "$log_file"; then
    cat "$log_file"
    printf 'QEMU smoke test failed with status %s\n' "$status" >&2
    exit 1
fi

printf '%s\n' "QEMU reached kernel_main"
