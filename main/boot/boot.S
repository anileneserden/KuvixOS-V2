.set MULTIBOOT_MAGIC,     0x1BADB002
.set MULTIBOOT_FLAGS,     0x00000003
.set MULTIBOOT_CHECKSUM, -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

.section .multiboot
.align 4
.long MULTIBOOT_MAGIC
.long MULTIBOOT_FLAGS
.long MULTIBOOT_CHECKSUM

.section .text
.global _start
.extern kernel_main
.extern stack_top

_start:
    cli
    mov $stack_top, %esp
    push %eax        # multiboot magic
    push %ebx        # multiboot info pointer
    call kernel_main

.hang:
    hlt
    jmp .hang
