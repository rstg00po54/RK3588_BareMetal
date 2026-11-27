.section .text.boot,"ax",%progbits
.global _start
_start:
    .long 0x31504153
    .long 1
    .long 16
    .long 0
    b app_main
