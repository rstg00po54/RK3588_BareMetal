/* Independent CODE471 entry probe. No stack, DDR, storage, or ROM return.
 * UART2 M0 pinmux from DDR v1.18 offsets 0x650..0x674; UART setup
 * from 0xfff0..0x1006c. Assumes inherited UART clock, as vendor path does.
 */
.section .text.boot,"ax",%progbits
.global _start
_start:
    b entry
entry:
    msr daifset, #0xf
    movz x1, #0x4000
    movk x1, #0xfd5f, lsl #16
    movz w2, #0x0880
    movk w2, #0x0ff0, lsl #16
    str w2, [x1]
    movz x1, #0x800c
    movk x1, #0xfd5f, lsl #16
    movz w2, #0x0aa0
    movk w2, #0x0ff0, lsl #16
    str w2, [x1]
    dsb sy

    movz x19, #0xfeb5, lsl #16
    mov w0, #0x83
    str w0, [x19, #0xc]
    mov w0, #1
    str w0, [x19]
    str wzr, [x19, #4]
    mov w0, #3
    str w0, [x19, #0xc]
    mov w0, #1
    str w0, [x19, #0x98]
    dsb sy

    adr x0, banner
    bl puts
    adr x0, pcmsg
    bl puts
    adr x0, _start
    bl hex64
    adr x0, elmsg
    bl puts
    mrs x0, CurrentEL
    lsr x0, x0, #2
    add w0, w0, #48
    bl putc
    adr x0, newline
    bl puts

    /* Scratch resides inside our downloaded image; no guessed SRAM address. */
    adr x20, scratch
    mov w21, #0
pattern_loop:
    mov w22, #0
write_loop:
    eor w23, w22, w21
    str w23, [x20, x22, lsl #2]
    add w22, w22, #1
    cmp w22, #64
    b.lo write_loop
    dsb sy
    mov w22, #0
read_loop:
    eor w23, w22, w21
    ldr w24, [x20, x22, lsl #2]
    cmp w23, w24
    b.ne fail
    add w22, w22, #1
    cmp w22, #64
    b.lo read_loop
    cbnz w21, pass
    mov w21, #-1
    b pattern_loop
pass:
    adr x0, passmsg
    bl puts
    b idle
fail:
    adr x0, failmsg
    bl puts
idle:
    wfe
    b idle

/* No stack: puts/hex64 use separate saved LR registers. */
putc:
    mov w2, #0x8000
1:
    ldr w1, [x19, #0x14]
    tbnz w1, #5, 2f
    subs w2, w2, #1
    b.ne 1b
    ret
2:
    str w0, [x19]
    ret
puts:
    mov x9, x30
    mov x10, x0
3:
    ldrb w0, [x10], #1
    cbz w0, 4f
    bl putc
    b 3b
4:
    ret x9
hex64:
    mov x11, x30
    mov x12, x0
    mov w13, #60
    adr x14, digits
5:
    lsr x15, x12, x13
    and x15, x15, #15
    ldrb w0, [x14, x15]
    bl putc
    subs w13, w13, #4
    b.ge 5b
    ret x11
digits: .ascii "0123456789ABCDEF"
banner: .asciz "\r\n[CODE471] independent entry OK\r\n"
pcmsg: .asciz "[CODE471] image=0x"
elmsg: .asciz "\r\n[CODE471] EL="
newline: .asciz "\r\n"
passmsg: .asciz "[CODE471] 256-byte image scratch PASS\r\n[CODE471] intentional halt; no DDR initialized\r\n"
failmsg: .asciz "[CODE471] image scratch FAIL; halt\r\n"
.balign 16
scratch: .space 256, 0
