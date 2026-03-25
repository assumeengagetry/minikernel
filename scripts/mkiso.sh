#!/usr/bin/env bash
set -euo pipefail

OUT_ISO="$1"
KERNEL_ELF="$2"
GRUB_CFG="$3"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

mkdir -p "$WORKDIR/iso/boot/grub"
cp "$KERNEL_ELF" "$WORKDIR/iso/boot/kernel.elf"
cp "$GRUB_CFG" "$WORKDIR/iso/boot/grub/grub.cfg"

grub-mkrescue -o "$OUT_ISO" "$WORKDIR/iso" >/dev/null
