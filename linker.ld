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

    /* YENİ BÖLÜM: Komut tablosunu burada oluşturuyoruz */
    .cmd_section : ALIGN(4K)
    {
        _cmd_start = .;          /* Başlangıç adresini işaretle */
        KEEP(*(.cmd_section))    /* Tüm dosyalardaki bu bölümü topla */
        _cmd_end = .;            /* Bitiş adresini işaretle */
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

    end = .;
    _end = .;
}