.section .text.boot,"ax",%progbits
.global _start
_start:
    msr daifset, #0xf
    mrs x0, CurrentEL
    cmp x0, #12
    b.ne early_fail
    adr x0, _start
    tst x0, #0xfff
    b.ne early_fail
    msr spsel, #1
    adr x0, __stack_top
    mov sp, x0
    adr x0, vectors
    msr vbar_el3, x0
    isb
    adr x0, __bss_start
    adr x1, __bss_end
1:  cmp x0, x1
    b.hs 2f
    str xzr, [x0], #8
    b 1b
2:  bl boot_main
3:  wfe
    b 3b
early_fail:
    movz x1, #0xfeb5, lsl #16
    mov w0, #33
    mov w3, #0x8000
4:  ldr w2, [x1, #0x14]
    tbnz w2, #5, 5f
    subs w3, w3, #1
    b.ne 4b
    b 3b
5:  str w0, [x1]
    b 3b

/* Return 1 on success; recover only a synchronous data abort at this load. */
.global probe32
probe32:
probe_load:
    ldr w2, [x0]
    str w2, [x1]
    mov w0, #1
    ret
probe_fail:
    mov w0, #0
    ret
sync_handler:
    mrs x3, esr_el3
    lsr x3, x3, #26
    cmp x3, #0x25
    b.ne fatal
    mrs x3, elr_el3
    adr x4, probe_load
    cmp x3, x4
    b.ne fatal
    adr x3, probe_fail
    msr elr_el3, x3
    eret
fatal:
    mrs x0, esr_el3
    mrs x1, elr_el3
    mrs x2, far_el3
    b boot_fatal

.balign 2048
vectors:
.macro slot target
    b \target
    .balign 128
.endm
    slot sync_handler
    slot fatal
    slot fatal
    slot fatal
    slot sync_handler
    slot fatal
    slot fatal
    slot fatal
    slot fatal
    slot fatal
    slot fatal
    slot fatal
    slot fatal
    slot fatal
    slot fatal
    slot fatal
