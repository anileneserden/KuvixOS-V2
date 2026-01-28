ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot ALIGN(4K) : {
        *(.multiboot)
    }

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(COMMON)
        *(.bss*)
    }

    .stack (NOLOAD) : {
        . = ALIGN(16);
        stack_bottom = .;
        . += 16K;
        stack_top = .;
    }
}
