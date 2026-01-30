ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        KEEP(*(.multiboot))
        *(.text*)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata*)
    }

    /* TEK VE TEMİZ KOMUT BÖLÜMÜ */
    .cmd_section BLOCK(4K) : ALIGN(4K)
    {
        _cmd_start = .;
        KEEP(*(.cmd_section))
        KEEP(*(.commands))
        _cmd_end = .;
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

    .stack (NOLOAD) : {
        . = ALIGN(16);
        stack_bottom = .;
        . += 16K;
        stack_top = .;
    }

    end = .;
    _end = .;
}