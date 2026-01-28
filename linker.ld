ENTRY(_start)

SECTIONS
{
    /* Kernel 1MB noktasından başlayacak */
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        /* Multiboot header'ı dosyanın tam başında tutar */
        KEEP(*(.multiboot))
        *(.text*)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data*)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }

    /* Yığın (Stack) alanı */
    .stack (NOLOAD) : {
        . = ALIGN(16);
        stack_bottom = .;
        . += 16K;
        stack_top = .;
    }
}