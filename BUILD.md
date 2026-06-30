# UltimaOS — CMake + Ninja Build System

## Prerequisites

### Build prerequisites

#### Arch
```sh
sudo pacman -S cmake ninja ccache dosfstools mtools parted lld binutils clang clangd llvm nasm uuid-dev 
```

#### Debian/Ubuntu:
```sh
   sudo apt install build-essential cmake ninja-build ccache dosfstools mtools parted lld binutils clang clangd llvm nasm uuid-dev gnu-efi
```

### Setup EDK2
```sh
    git clone https://github.com/tianocore/edk2.git
    cd ~/edk2
    git submodule update --init
    make -C BaseTools
    export EDK2_DIR="$(pwd)/toolchain/edk2"
    export WORKSPACE=$EDK2_DIR/Build
    export EDK_TOOLS_PATH=$EDK2_DIR/BaseTools
    source $EDK2_DIR/edksetup.sh

    mkdir ~/edk2-output
    find $EDK2_DIR/Build -name "*.efi" -o -name "*.fd" | xargs -I{} cp {} ~/edk2-output/
    rm -rf ~/edk2
    mv ~/edk2-output ~/edk2
```

## First-time setup

```bash
cmake -B build -G Ninja -DEDK2_DIR=/path/to/edk2 ((optional if debug) -DCMAKE_BUILD_TYPE=<Release/Debug> )
```

## Building

```bash
ninja -C build              # build everything (libcore → bootloader → kernel → shell → image)
ninja -C build kernel       # build only the kernel
ninja -C build bootloader   # build only the bootloader
ninja -C build disk_image   # build only the disk images
```

## Running

```bash
ninja -C build run        # QEMU
ninja -C build run_qcow2  # QEMU boot from QCOW2 file
ninja -C build run_vmdk   # QEMU boot from VMDK file
ninja -C build debug_os   # QEMU + GDB
ninja -C build bochs_run  # Bochs
```

## Utilities

```bash
ninja -C build image_list       # list contents of the disk image
ninja -C build image_mount      # mount the image (requires sudo)
ninja -C build image_umount     # unmount
```

## CMake options

| Option           | Default   | Description                         |
|------------------|-----------|-------------------------------------|
| `CMAKE_BUILD_TYPE` | `Debug` | `Debug` or `Release`                |
| `ARCH`           | `x86_64`  | `x86_64` or `i686`                  |
| `IMAGE_SIZE`     | `250M`    | Disk image size                     |
| `IMAGE_TYPE`     | `disk`    | `disk` or `floppy`                  |
| `IMAGE_FS`       | `fat32`   | `fat32`, `fat16`, `fat12`           |
| `EDK2_DIR`       | *(empty)* | Optional path to EDK2 source        |

Override any option at configure time:
```bash
cmake -B build -G Ninja -DARCH=i686 -DIMAGE_SIZE=512M
```

## Output locations

| Artifact            | Path                              |
|---------------------|-----------------------------------|
| `libcore.a`         | `build/libcore/libcore.a`         |
| `BOOTX64.EFI`       | `build/src/boot/uefi_boot/BOOTX64.EFI` |
| `kernel.elf`        | `build/kernel/kernel.elf`         |
| `kernel.map`        | `build/kernel/kernel.map`         |
| `shell.elf`         | `build/programs/shell.elf`        |
| `os_image.img`      | `build/image/os_image.img`        |
| `compile_commands.json` | `build/compile_commands.json` |

## clangd / IDE integration

`compile_commands.json` is generated automatically. Point your editor at it:

```bash
ln -sf build/compile_commands.json compile_commands.json
```

