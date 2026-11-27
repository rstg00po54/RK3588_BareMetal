/* Standalone SRAM image, launched via monitor run (USB halted). */
.section .text.boot,"ax",%progbits
.global _start
_start:
    msr daifset, #0xf
    mrs x0, CurrentEL
    cmp x0, #12
    b.ne 1f
    adr x0, _start
    tst x0, #0xfff
    b.ne 1f
    msr spsel, #1
    adr x0, __stack_top
    mov sp,x0
    adr x0, vectors
    msr vbar_el3,x0
    isb
    bl ddr528_live_main
1:  wfe
    b 1b
.balign 2048
vectors:
.rept 16
    b fatal
    .balign 128
.endr
fatal:
    mrs x0,esr_el3
    mrs x1,elr_el3
    mrs x2,far_el3
    b ddr528_live_fatal
