string(REGEX REPLACE "[^0-9]" "" IMAGE_SIZE_NUM "${IMAGE_SIZE}")
math(EXPR IMAGE_SIZE_BYTES   "${IMAGE_SIZE_NUM} * 1024 * 1024")
math(EXPR IMAGE_SIZE_SECTORS "${IMAGE_SIZE_BYTES} / 512")
math(EXPR END_SECTOR         "${IMAGE_SIZE_SECTORS} - 34")
math(EXPR PARTITION_OFFSET_BYTES "2048 * 512")

set(IMAGE_FILE  ${CMAKE_CURRENT_BINARY_DIR}/os_image.img)
set(QCOW2_FILE  ${CMAKE_CURRENT_BINARY_DIR}/os_image.qcow2)
set(VMDK_FILE   ${CMAKE_CURRENT_BINARY_DIR}/os_image.vmdk)
set(MTOOLS_CONF ${CMAKE_CURRENT_BINARY_DIR}/.mtoolsrc)
set(FONT_FILE   ${CMAKE_SOURCE_DIR}/src/assets/zap-light16.psf)

# Propagate to parent scope so top-level run targets can reference them
set(QCOW2_FILE ${QCOW2_FILE} PARENT_SCOPE)
set(VMDK_FILE  ${VMDK_FILE}  PARENT_SCOPE)

add_custom_command(
    OUTPUT ${IMAGE_FILE} ${QCOW2_FILE} ${VMDK_FILE}   # <-- all three
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/build_image.sh
        ${IMAGE_FILE}
        ${IMAGE_SIZE_SECTORS}
        ${END_SECTOR}
        ${PARTITION_OFFSET_BYTES}
        ${BOOTLOADER_EFI_PATH}
        ${KERNEL_ELF_PATH}
        ${SHELL_ELF_PATH}
        ${FONT_FILE}
    DEPENDS bootloader kernel # shell
    COMMENT "Creating UEFI disk image + qcow2 + vmdk (${IMAGE_SIZE})"
)

add_custom_target(disk_image ALL DEPENDS ${IMAGE_FILE} ${QCOW2_FILE} ${VMDK_FILE})

# ---- mount / umount helpers ----
set(MOUNT_DIR     ${CMAKE_CURRENT_BINARY_DIR}/mount)
set(LOOPBACK_FILE ${CMAKE_CURRENT_BINARY_DIR}/.loopback)

add_custom_target(image_mount
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/mount_image.sh
        ${IMAGE_FILE} ${MOUNT_DIR} ${LOOPBACK_FILE}
    COMMENT "Mounting disk image"
    USES_TERMINAL
)
add_custom_target(image_umount
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/umount_image.sh
        ${LOOPBACK_FILE} ${MOUNT_DIR}
    COMMENT "Unmounting disk image"
    USES_TERMINAL
)
add_custom_target(image_list
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/list_image.sh
        ${IMAGE_FILE} ${PARTITION_OFFSET_BYTES}
    COMMENT "Listing disk image contents"
    USES_TERMINAL
)