ENTRY(_start)
SECTIONS {
    . = 0;
    .text : { KEEP(*(.text.boot)) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) *(.got .got.*) }
    . = ALIGN(16);
    .bss (NOLOAD) : {
        __bss_start = .; *(.bss .bss.* COMMON)
        . = ALIGN(16); __bss_end = .;
    }
    .stack : {
        . = ALIGN(16); . += 4096; __stack_top = .;
        BYTE(0); __image_end = .;
    }
    /DISCARD/ : { *(.comment .note* .eh_frame*) }
}
ASSERT(__image_end <= 0x10000, "SRAM slot overflow")
