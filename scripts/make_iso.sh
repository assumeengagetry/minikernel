#!/bin/sh
set -eu

kernel_elf=$1
output_iso=$2
config_file=${3:-grub.cfg}
iso_dir=$(mktemp -d)
grub_dir=${GRUB_MODULE_DIR:-/usr/lib/grub/i386-pc}

cleanup() {
    rm -rf "$iso_dir"
}
trap cleanup EXIT

command -v grub-mkimage >/dev/null 2>&1
command -v xorriso >/dev/null 2>&1
[ -d "$grub_dir" ]
[ -f "$grub_dir/cdboot.img" ]

mkdir -p "$iso_dir/boot/grub/i386-pc"
cp "$kernel_elf" "$iso_dir/boot/kernel.elf"
cp "$config_file" "$iso_dir/boot/grub/grub.cfg"

grub-mkimage \
    -O i386-pc \
    -d "$grub_dir" \
    -p /boot/grub \
    -o "$iso_dir/boot/grub/i386-pc/core.img" \
    biosdisk iso9660 normal multiboot2 configfile

cat "$grub_dir/cdboot.img" \
    "$iso_dir/boot/grub/i386-pc/core.img" \
    > "$iso_dir/boot/grub/i386-pc/eltorito.img"

xorriso -as mkisofs \
    -R \
    -o "$output_iso" \
    -b boot/grub/i386-pc/eltorito.img \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    "$iso_dir"
