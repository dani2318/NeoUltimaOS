section .text.start
global start

start:
    ; clear direction flag
    cld

    ; set up stack (you need a stack symbol defined somewhere)
    lea rsp, [rel _kernel_stack_top]

    ; rdi already contains BootInfo* from the bootloader call convention
    extern kernel_main
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
_kernel_stack:
    resb 32768          ; 32 KiB stack
_kernel_stack_top: