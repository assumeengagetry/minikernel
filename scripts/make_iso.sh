#!/bin/sh
set -eu

kernel_elf=$1
output_iso=$2
iso_dir=$(mktemp -d)

cleanup() {
    rm -rf "$iso_dir"
}
trap cleanup EXIT

mkdir -p "$iso_dir/boot/grub"
cp "$kernel_elf" "$iso_dir/boot/kernel.elf"

cat > "$iso_dir/boot/grub/grub.cfg" <<'EOF'
set timeout=0
set default=0

menuentry "MicroKernel" {
    multiboot /boot/kernel.elf
    boot
}
EOF

grub-mkrescue -o "$output_iso" "$iso_dir"
