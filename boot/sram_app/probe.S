/* ABI 1 smoke tests. No external code or static pointer relocations. */
.section .text,"ax",%progbits
.global app_main
app_main:
#if APP_PROBE == 0
    /* No stack, no API dereference, only return to the monitor. */
    movz w0, #0x5678
    movk w0, #0x1234, lsl #16
    ret
#elif APP_PROBE == 1
    /* Validate the dedicated app stack and the ABI header. */
    stp x19, x30, [sp, #-16]!
    mov x19, x0
    mov w0, #1
    cbz x19, 1f
    ldr w1, [x19]
    cmp w1, #1
    b.ne 1f
    ldr w1, [x19, #4]
    cmp w1, #24
    b.lo 1f
    movz w0, #0x5679
    movk w0, #0x1234, lsl #16
1:
    ldp x19, x30, [sp], #16
    ret
#elif APP_PROBE == 2
    /* Test the monitor callback explicitly, with ADR not a GOT entry. */
    stp x19, x30, [sp, #-16]!
    mov x19, x0
    ldr x9, [x19, #8]
    adr x0, message
    blr x9
    movz w0, #0x567a
    movk w0, #0x1234, lsl #16
    ldp x19, x30, [sp], #16
    ret
message: .asciz "[APP] Assembly callback PASS\n"
#else
#error unknown APP_PROBE
#endif
