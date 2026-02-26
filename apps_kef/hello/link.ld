ENTRY(_start)

SECTIONS {
  . = 0;

  .text : {
    *(.text._start)     /* ✅ önce _start */
    *(.text .text.*)    /* sonra kalan kod */
  }

  .rodata : { *(.rodata .rodata.*) }
  .data   : { *(.data .data.*) }
  .bss    : { *(.bss .bss.* COMMON) }
}