file(GLOB_RECURSE KERNEL_C_SOURCES   CONFIGURE_DEPENDS "*.c")
file(GLOB_RECURSE KERNEL_CPP_SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB_RECURSE KERNEL_ASM_SOURCES CONFIGURE_DEPENDS "*.asm")

set(EDK2_INCLUDES "")
if(EDK2_DIR)
    list(APPEND EDK2_INCLUDES
        ${EDK2_DIR}/MdePkg/Include
        ${EDK2_DIR}/MdePkg/Include/Uefi
        ${EDK2_DIR}/MdePkg/Include/Library
        ${EDK2_DIR}/MdePkg/Include/Protocol
        ${EDK2_DIR}/MdePkg/Include/X64
    )
    message(STATUS "EDK2 includes: ${EDK2_INCLUDES}")
endif()

add_executable(kernel
    ${KERNEL_C_SOURCES}
    ${KERNEL_CPP_SOURCES}
    ${KERNEL_ASM_SOURCES}
)

target_compile_options(kernel PRIVATE
    $<$<COMPILE_LANGUAGE:C>:${ULTIMA_C_FLAGS} -fshort-wchar>
    $<$<COMPILE_LANGUAGE:CXX>:${ULTIMA_CXX_FLAGS} -fshort-wchar>
    $<$<COMPILE_LANGUAGE:ASM_NASM>:-I${CMAKE_CURRENT_SOURCE_DIR}>
)

target_include_directories(kernel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${EDK2_INCLUDES}
)

target_link_options(kernel PRIVATE
    ${ULTIMA_LD_FLAGS}
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    -Wl,-Map=${CMAKE_BINARY_DIR}/kernel/kernel.map
)

set_target_properties(kernel PROPERTIES
    OUTPUT_NAME              "kernel.elf"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/kernel
    SUFFIX                   ""
)

set(KERNEL_ELF_PATH ${CMAKE_BINARY_DIR}/kernel/kernel.elf CACHE INTERNAL "Path to kernel.elf")

#add_dependencies(kernel shell)