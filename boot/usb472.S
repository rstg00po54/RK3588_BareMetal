/* RK3588(S) CODE472 RAM-only bring-up. UART2 inherited from DDR firmware. */
.section .text.boot,"ax",%progbits
.global _start
.type _start, %function
_start:
    msr daifset, #0xf
    movz x19, #0xfeb5, lsl #16
    adr x0, banner
    bl puts
    adr x0, elmsg
    bl puts
    mrs x0, CurrentEL
    lsr x0, x0, #2
    and x0, x0, #3
    add w0, w0, #48
    bl putc
    adr x0, newline
    bl puts
    adr x20, scratch
    mov w21, #16
    movz x22, #0x55aa
    movk x22, #0xa55a, lsl #16
    mov x23, x20
1:
    str x22, [x23], #8
    subs w21, w21, #1
    b.ne 1b
    dsb sy
    mov w21, #16
    mov x23, x20
2:
    ldr x24, [x23], #8
    cmp x24, x22
    b.ne fail
    subs w21, w21, #1
    b.ne 2b
    adr x0, passmsg
    bl puts
    b idle
fail:
    adr x0, failmsg
    bl puts
idle:
    wfe
    b idle
/* x19 = UART2 base. No stack or clock/pin reconfiguration. */
putc:
    mov w2, #0x8000
3:
    ldr w1, [x19, #0x14]
    tbnz w1, #5, 4f
    subs w2, w2, #1
    b.ne 3b
    ret
4:
    str w0, [x19]
    ret
puts:
    mov x9, x30
    mov x10, x0
5:
    ldrb w0, [x10], #1
    cbz w0, 6f
    bl putc
    b 5b
6:
    ret x9
banner: .asciz "\r\n[RK3588] CODE472 AARCH64 ENTRY OK\r\n"
elmsg: .asciz "[RK3588] CurrentEL=EL"
newline: .asciz "\r\n"
passmsg: .asciz "[RK3588] 128-byte scratch test PASS\r\n[RK3588] RAM-only idle\r\n"
failmsg: .asciz "[RK3588] 128-byte scratch test FAIL\r\n"
.balign 16
scratch: .space 128, 0
