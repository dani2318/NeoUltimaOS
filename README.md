# Neo-UltimaOS

Neo-UltimaOS is an operating system project designed for educational and experimental purposes. This repository contains everything needed to build, run, and explore the OS.

---

## Prerequisites

Make sure the following tools are installed on your system before getting started:

| Tool | Purpose |
|------|---------|
| `python3` | Required to run the toolchain setup script |
| `clang` | C compiler for building the OS |
| `llvm` |  for 'llvm-ar', 'llvm-ranlib' and 'llvm-strip' |
| `nasm` | Assembler for the assembly components |
| `qemu` | Emulator for running the OS image |

> ⚠️ **Warning:** Additional tools may be required depending on your system configuration or as the project evolves. Always check the repository for the most up-to-date requirements check [build.md](BUILD.md).

### External Libraries

| Library | Purpose |
|---------|---------|
| [OVMF](https://github.com/tianocore/tianocore.github.io/wiki/OVMF) | UEFI firmware for use with QEMU — enables UEFI boot emulation |
| [EDK2](https://github.com/tianocore/edk2) | Open-source UEFI firmware development kit used as the UEFI implementation base |

> **Windows users:** It is strongly recommended to use [WSL2](https://learn.microsoft.com/en-us/windows/wsl/install) to build the toolchain and project.

---

## Getting Started
For instructions on how to set up the toolchain, build, and run the project, see [build.md](BUILD.md).
