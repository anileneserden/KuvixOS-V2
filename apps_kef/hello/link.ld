ENTRY(_start)

SECTIONS {
  . = 0;

  .text : { *(.text .text.*) }
  .rodata : { *(.rodata .rodata.*) }
  .data : { *(.data .data.*) }
  .bss : { *(.bss .bss.* COMMON) }

  /DISCARD/ : {
    *(.comment*)
    *(.note*)
    *(.eh_frame*)
    *(.eh_frame_hdr*)
    *(.gcc_except_table*)
  }
}