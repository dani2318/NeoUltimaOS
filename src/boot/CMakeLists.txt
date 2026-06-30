execute_process(
    COMMAND sh -c "objcopy --info 2>/dev/null | grep -c efi-app-x86_64"
    OUTPUT_VARIABLE EFI_OBJCOPY_SUPPORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

file(GLOB_RECURSE BOOT_C_SOURCES CONFIGURE_DEPENDS "*.c")

add_library(bootloader_objs OBJECT ${BOOT_C_SOURCES})

target_compile_options(bootloader_objs PRIVATE
    --target=${TOOLCHAIN_TARGET}
    -std=c11
    -ffreestanding
    -fno-stack-protector
    -fshort-wchar
    -mno-red-zone
    -fPIC
    -fno-stack-check
    "-DEFIAPI=__attribute__((ms_abi))"
    -g
)

target_include_directories(bootloader_objs PRIVATE
    ${UEFI_INCLUDE}
    ${UEFI_INCLUDE}/x86_64
    ${UEFI_INCLUDE}/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

set(BOOT_SO  ${CMAKE_CURRENT_BINARY_DIR}/BOOTX64.so)
set(BOOT_EFI ${CMAKE_CURRENT_BINARY_DIR}/BOOTX64.EFI)

add_custom_command(
    OUTPUT ${BOOT_SO}
    COMMAND ld
        -nostdlib
        -znocombreloc
        -T ${UEFI_LDS}
        -shared
        -Bsymbolic
        -L ${UEFI_LIB}
        ${UEFI_CRT0}
        $<TARGET_OBJECTS:bootloader_objs>
        -o ${BOOT_SO}
        -lefi -lgnuefi
    DEPENDS bootloader_objs
    COMMENT "[LD]      BOOTX64.so"
)

if(EFI_OBJCOPY_SUPPORT GREATER 0)
    add_custom_command(
        OUTPUT ${BOOT_EFI}
        COMMAND objcopy
            -j .text -j .rodata -j .sdata -j .data
            -j .dynamic -j .dynsym
            -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc
            --input-target=elf64-x86-64
            --output-target=efi-app-x86_64
            ${BOOT_SO} ${BOOT_EFI}
        DEPENDS ${BOOT_SO}
        COMMENT "[OBJCOPY] BOOTX64.EFI"
    )
else()
    message(STATUS "objcopy has no EFI support — using scripts/elf_to_efi.py (Arch path)")
    add_custom_command(
        OUTPUT ${BOOT_EFI}
        COMMAND python3
            ${CMAKE_SOURCE_DIR}/scripts/elf_to_efi.py
            ${BOOT_SO} ${BOOT_EFI}
        DEPENDS ${BOOT_SO}
        COMMENT "[PY]      BOOTX64.EFI"
    )
endif()

add_custom_target(bootloader ALL DEPENDS ${BOOT_EFI})

set(BOOTLOADER_EFI_PATH ${BOOT_EFI} CACHE INTERNAL "Path to BOOTX64.EFI")