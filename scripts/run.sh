#!/bin/sh
mkdir -p logs/
date=$(date '+%d-%m-%Y--%H.%M.%S')
logfile="logs/qemu-serial-$date.log"

if uname -r | grep -qi microsoft; then
    OVMF_PATH="/usr/share/ovmf/OVMF.fd"
else
    OVMF_PATH="/usr/share/OVMF/x64/OVMF.4m.fd"
fi

if [ ! -f "$OVMF_PATH" ]; then
    echo "Error: OVMF firmware not found at $OVMF_PATH"
    echo "Arch: sudo pacman -S edk2-ovmf"
    echo "Ubuntu/WSL: sudo apt install ovmf"
    exit 3
fi

DISPLAY_ARGS="-display gtk"
QEMU="qemu-system-x86_64"
QEMU_ARGS="-cpu qemu64 -m 4G -no-reboot -no-shutdown \
           $DISPLAY_ARGS -serial stdio \
           -d int,guest_errors -D logs/qemu-int-$date.log \
           -bios $OVMF_PATH"

if [ "$#" -lt 2 ]; then
    echo "Usage: ./run.sh <image_type> <image>"
    echo "  image_type: floppy | disk | qcow2 | vmdk"
    exit 1
fi

case "$1" in
    "floppy") QEMU_ARGS="$QEMU_ARGS -drive file=$2,format=raw,if=floppy" ;;
    "disk")   QEMU_ARGS="$QEMU_ARGS -drive file=$2,if=virtio,format=raw" ;;
    "qcow2")  QEMU_ARGS="$QEMU_ARGS -drive file=$2,if=virtio,format=qcow2" ;;
    "vmdk")   QEMU_ARGS="$QEMU_ARGS -drive file=$2,if=virtio,format=vmdk" ;;
    *) echo "Unknown image type '$1'. Valid: floppy | disk | qcow2 | vmdk"; exit 2 ;;
esac

echo "Launching QEMU... (log: $logfile)"
$QEMU $QEMU_ARGS 2>&1 | tee "$logfile"