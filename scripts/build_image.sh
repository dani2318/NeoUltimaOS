#!/bin/bash
set -e

IMAGE=$1
IMAGE_SIZE_SECTORS=$2
END_SECTOR=$3
PARTITION_OFFSET_BYTES=$4
BOOTLOADER=$5
KERNEL=$6
#SHELL_ELF=$7
FONT_FILE=$8

MTOOLS_CONF=$(dirname "$IMAGE")/.mtoolsrc

echo "[1/6] Creating empty disk image..."
dd if=/dev/zero of="$IMAGE" bs=512 count="$IMAGE_SIZE_SECTORS" status=none

echo "[2/6] Creating GPT partition table..."
parted -s "$IMAGE" mklabel gpt
parted -s "$IMAGE" mkpart ESP fat32 2048s ${END_SECTOR}s
parted -s "$IMAGE" set 1 esp on
sync

echo "[3/6] Formatting ESP as FAT32..."
mkfs.fat -F 32 -n "ESP" --offset 2048 -S 512 "$IMAGE"
sync

echo "[4/6] Configuring mtools..."
echo "drive x: file=\"$IMAGE\" offset=$PARTITION_OFFSET_BYTES" > "$MTOOLS_CONF"

echo "[5/6] Copying files..."
MTOOLSRC="$MTOOLS_CONF" mmd -D O x:/EFI          2>/dev/null || true
MTOOLSRC="$MTOOLS_CONF" mmd -D O x:/EFI/BOOT     2>/dev/null || true
MTOOLSRC="$MTOOLS_CONF" mmd -D O x:/Ultima         2>/dev/null || true
MTOOLSRC="$MTOOLS_CONF" mmd -D O x:/Ultima/fonts   2>/dev/null || true
MTOOLSRC="$MTOOLS_CONF" mmd -D O x:/programs      2>/dev/null || true

MTOOLSRC="$MTOOLS_CONF" mcopy -o "$BOOTLOADER" x:/EFI/BOOT/BOOTX64.EFI
MTOOLSRC="$MTOOLS_CONF" mcopy -o "$KERNEL"     x:/Ultima/KERNEL.ELF

#[ -f "$SHELL_ELF" ] && MTOOLSRC="$MTOOLS_CONF" mcopy -o "$SHELL_ELF" x:/programs/SHELL.ELF || true
[ -f "$FONT_FILE" ] && MTOOLSRC="$MTOOLS_CONF" mcopy -o "$FONT_FILE" x:/Ultima/fonts/zap-light16.psf || true

echo "[6/6] Converting to resizable VM formats..."

# QCOW2 — sparse, resizable with: qemu-img resize os_image.qcow2 +1G
qemu-img convert -f raw -O qcow2 \
    -o preallocation=off \
    "$IMAGE" "${IMAGE%.img}.qcow2"

# VMDK — sparse/dynamic, resizable in VMware and with qemu-img resize
qemu-img convert -f raw -O vmdk \
    -o adapter_type=ide,subformat=monolithicSparse \
    "$IMAGE" "${IMAGE%.img}.vmdk"

echo "✓ QCOW2 (resizable): ${IMAGE%.img}.qcow2"
echo "✓ VMDK  (resizable): ${IMAGE%.img}.vmdk"

rm -f "$MTOOLS_CONF"
echo "✓ Image created: $IMAGE"