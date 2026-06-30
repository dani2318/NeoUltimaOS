cmake_minimum_required(VERSION 3.20)

set(CMAKE_C_COMPILER    "clang"       CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER  "clang++"     CACHE STRING "C++ compiler")
set(CMAKE_AR            "llvm-ar"     CACHE STRING "Archiver")
set(CMAKE_RANLIB        "llvm-ranlib" CACHE STRING "Ranlib")
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
set(CMAKE_ASM_NASM_OBJECT_FORMAT "elf64")

project(UltimaOS VERSION 0.1.0 LANGUAGES C CXX ASM_NASM)

set(ARCH       "x86_64" CACHE STRING "Target architecture [x86_64|i686]")
set(IMAGE_SIZE "250M"   CACHE STRING "Disk image size (e.g. 250M)")
set(IMAGE_TYPE "disk"   CACHE STRING "Image type [disk|floppy]")
set(IMAGE_FS   "fat32"  CACHE STRING "Image filesystem [fat32|fat16|fat12]")
set(EDK2_DIR   ""       CACHE PATH   "Path to EDK2 source (optional, for extra kernel headers)")
option(BUILD_TESTS "Compile kernel unit tests into the kernel image" OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type [Debug|Release]" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "ccache found: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER   ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
else()
    message(STATUS "ccache not found — install it with: sudo pacman -S ccache")
endif()

if(ARCH STREQUAL "x86_64")
    set(TOOLCHAIN_TARGET "x86_64-elf")
    set(ARCH_FLAGS       -mno-red-zone)
    set(UEFI_CRT0        "/usr/lib/crt0-efi-x86_64.o")
    set(UEFI_LDS         "/usr/lib/elf_x86_64_efi.lds")
elseif(ARCH STREQUAL "i686")
    set(TOOLCHAIN_TARGET "i686-elf")
    set(ARCH_FLAGS       "")
    set(UEFI_CRT0        "/usr/lib/crt0-efi-ia32.o")
    set(UEFI_LDS         "/usr/lib/elf_ia32_efi.lds")
else()
    message(FATAL_ERROR "Unsupported architecture: ${ARCH}. Choose x86_64 or i686.")
endif()

set(UEFI_INCLUDE "/usr/include/efi")
set(UEFI_LIB     "/usr/lib")

set(ULTIMA_C_FLAGS
    --target=${TOOLCHAIN_TARGET}
    -std=c11
    -ffreestanding
    -nostdlib
    -fno-stack-protector
    -fno-pie
    -fno-pic
    ${ARCH_FLAGS}
    -g
)

set(ULTIMA_CXX_FLAGS
    --target=${TOOLCHAIN_TARGET}
    -std=c++23
    -ffreestanding
    -nostdlib
    -fno-stack-protector
    -fno-exceptions
    -fno-rtti
    -fno-pie
    -fno-pic
    ${ARCH_FLAGS}
    -g
)

set(ULTIMA_LD_FLAGS
    --target=${TOOLCHAIN_TARGET}
    -nostdlib
    -fuse-ld=lld
    -Wl,-no-pie
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND ULTIMA_C_FLAGS   -O3)
    list(APPEND ULTIMA_CXX_FLAGS -O3)
else()
    list(APPEND ULTIMA_C_FLAGS   -O0)
    list(APPEND ULTIMA_CXX_FLAGS -O0)
endif()

# add_subdirectory(src/libs/core)
add_subdirectory(src/boot/)
add_subdirectory(src/kernel)
#add_subdirectory(src/programs/shell)
add_subdirectory(image)

add_custom_target(run
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/run.sh
            ${IMAGE_TYPE} ${CMAKE_BINARY_DIR}/image/os_image.img
    DEPENDS disk_image
    COMMENT "Running UltimaOS in QEMU (raw)"
    USES_TERMINAL
)

add_custom_target(run_qcow2
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/run.sh
            qcow2 ${CMAKE_BINARY_DIR}/image/os_image.qcow2
    DEPENDS disk_image
    COMMENT "Running UltimaOS in QEMU (qcow2)"
    USES_TERMINAL
)

add_custom_target(run_vmdk
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/run.sh
            vmdk ${CMAKE_BINARY_DIR}/image/os_image.vmdk
    DEPENDS disk_image
    COMMENT "Running UltimaOS in QEMU (vmdk)"
    USES_TERMINAL
)

add_custom_target(debug_os
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/debug.sh
            ${IMAGE_TYPE} ${CMAKE_BINARY_DIR}/image/os_image.img
    DEPENDS disk_image
    COMMENT "Running UltimaOS in QEMU with GDB"
    USES_TERMINAL
)

add_custom_target(bochs_run
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/bochs.sh
            ${IMAGE_TYPE} ${CMAKE_BINARY_DIR}/image/os_image.img
    DEPENDS disk_image
    COMMENT "Running UltimaOS in Bochs"
    USES_TERMINAL
)
