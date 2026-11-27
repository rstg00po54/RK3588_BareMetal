ENTRY(_start)
SECTIONS {
    . = 0;
    .text : { KEEP(*(.text.boot)) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) *(.got .got.*) }
    .bss : { *(.bss .bss.* COMMON) }
    . = ALIGN(4);
    .end : { LONG(0); }
    /DISCARD/ : { *(.comment .note* .eh_frame*) }
}
ASSERT(SIZEOF(.bss) == 0, "App ABI 1 has no automatic BSS initialization")
ASSERT(. <= 0xf000, "App exceeds inactive slot payload")
