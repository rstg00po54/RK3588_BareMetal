# DDR cold-start correction: v0.5.2 experiment

Independent SRAM cold preflight: OS_REG2..5 zero, PLL A/B CON0=8073,
CON1=1 on four channels. PMU2 idle ACK0=F023FFFF, idle ST0=0023FFFF.

v0.5.2 unconditionally attempted function 0xced0. The first write
FD8D8110=F000F000 completed. ACK0 was already high before the write;
ST0 remained 0023FFFF and the experiment stopped. No subsequent
power-gate writes or PLL/PHY/CTL programming occurred.

Rechecked the exact vendor v1.18 binary (SHA1
8d5e55ebf63d94618b448b79757b1f07733fd076) with AArch64 disassembly:
```
9a2c ldr x0,[x19]
9a30 add x0,x0,#0x10,lsl #12
9a34 ldr w0,[x0,#0x14]
9a38 tst x0,#7
9a3c b.eq 0x9a54
9a40..9a48 print "pd/pu vd_ddr"
9a4c mov x0,x19
9a50 bl 0xced0
9a54 ... configuration preparation
```
The branch tests DDRCTL0 STAT[2:0] at F7010014. The PMU sequence is
conditional, not a universal cold initialization prerequisite. A zero
mode skips it. Do not replay it on a live DDR system.
The old phase1/2 isolated helper is archival, not a valid cold recipe.

Register names verified against ARM Trusted Firmware
plat/rockchip/rk3588/drivers/pmu/pmu.h:
PMU base FD8D0000; PMU2_BUS_IDLE_SFTCON(1)=8110,
BUS_IDLE_ACK(0)=8118, BUS_IDLE_ST(0)=8120, PWR_GATE_SFTCON(2)=8154,
VOL_GATE_CON(2)=8160, PWR_GATE_ST(1)=8184.
BUS_ID_MSCH0..3=28..31; PD_DDR01/23=32/33.

v0.5.3 removes this write experiment from the shell. ddrstage0 first
reads STAT, then samples PMU registers only if low bits are zero.
A nonresponding bus can still hang; the UART address breadcrumb is
printed before each load. The next cold path after 0x9a54 prepares
configuration and calls 0x6dc4 at 0x9c18 with w3=528. Its full
clock/reset and timing prerequisites are not yet reconstructed.
