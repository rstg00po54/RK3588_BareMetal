# RK3588 DDR v1.18: second pass, post-training GRF and four-channel mapping

Input: original rkbin DDR image SHA1 8d5e55ebf63d94618b448b79757b1f07733fd076.
Offsets are binary offsets. All experiments this round are host-only.

## Second initialization pass

a85c checks type9; LPDDR4X follows a864..a8c0:
- a878 calls 2060(type); a87c loads first word of returned block.
- a88c masks frequency to 12 bits. LPDDR4X block 11658 begins
  38 07 21 00, so unpatched second-pass frequency is 1848.
- a89c calls 6dc4(R,P,T,freq,type,1). The last argument is 1
  rather than first-pass 0, preserving the probed geometry.
- a8b8 calls 89dc(H,R,1). This is a second initialization pass,
  not merely a PLL change. Failure returns into the retry path.
The independent implementation is intentionally targeting fixed
528 MHz; that is a design choice, not the unpatched vendor default.

Actual local experiment /tmp/emu_regen.py: continued the previous
scripted first/second training run, injected hypothetical rank0 MR8
0x10, no second rank, equal geometry in all four channels. This is
NOT data obtained from the board. Called 6dc4 with 528,8,1.
It returned zero after 570 additional interpreted steps (total 4258).
R retained rank count 1, column 10, bank bits 3, row 16, width
category 1, second row 0. For comparison called 5e38 mode0 alone,
returned after another 497 steps (total 4755).
This final call was deliberately NOT the real mode1 89dc path.
P and T still gave TMG14=001600cd and RFSHSET1TMG1=000000c9.
Thus this synthetic second parameter generation is insufficient to
explain the remaining post-init differences (00160098 and 0000004a).
Do not claim the second initialization is fully emulated.

## 0xccb4 / 0xcd78: training-related state save and restore

For each channel:
C=F7010000+ch*1000000, G=FD59C000 for ch<2 or FD59D000 otherwise,
k=0x30*(ch&1), j=4*(0x4c+(ch&1)).
At ccb4:
- R+238+ch*20 = C[180]; C[180] &= fffbfd00.
- Poll C[14]&7 == 1. Vendor has no finite timeout.
- Save G[k+18]&ffff, G[k+4]&1f, G[k+24]&2fff, G[j]&4000
  into R+244,240,248,24c respectively, all with ch*20.
- G[k+4]=001f0000; G[k+24]=2fff0fff; G[j]=40000000.
At cd78:
- call b98c(H,ch,R), store result in H+244+ch*20.
- G[k+18]=ffff0000|result.
- G[k+4]=001f0000|H[240+ch*20].
- G[k+24]=2fff0000|H[248+ch*20].
- G[j]=40000000|H[24c+ch*20].
- C[180]=H[238+ch*20].
Note the actual base register used in ccb4 is x2 (R argument),
whereas cd78 computes H+channel*20. In main the pointer roles
and copying of saved fields must be checked before combining these
fragments. No physical save/restore implementation added this round.

## 0xb98c LPDDR4/4X post-training calculation

Uses active timing bank index from C[8]&3, bank stride 0x100000.
Read TMG2 at B+8+bank offset and DFITMG0 at B+580+bank offset.
Let a=TMG2[22:16], b=TMG2[30:24], c=DFITMG0[21:16].
For tested valid range a>=7, b>=2:
  q = floor(b/2)
  v = floor(c/2)
  u = min(v,floor((b-2)/2))
  extra = max(0,3-q,3-u)
  result = max(0,extra+v-floor((a-7)/2)-4)
This is derived from b98c..bad4, not inferred from a trained snapshot.
Examples: TMG2=060a1817, DFITMG0=07190204 -> 8;
same TMG2 and DFITMG0=07110204 -> 4.
The latter is a prediction for the observed timing words, NOT an
observed GRF value from this board.

Local /tmp/test_mapemu.py reused the restricted disassembly interpreter
to execute actual b98c for 25*22*32=17600 input combinations
(a=7..31,b=2..23,c=0..31). Compared every result with the new native
C implementation, output byte-for-byte identical (cmp PASS).
Files boot/ddr_cold/posttrain.c and test_posttrain.c.
Build:
  cc -std=c11 -Wall -Wextra -Werror -O2 boot/ddr_cold/posttrain.c \
    boot/ddr_cold/test_posttrain.c -o /tmp/posttrain-test
  /tmp/posttrain-test > /tmp/posttrain-values.txt
The test executable prints the 17600 values for differential comparison;
the original-instruction comparison harness is local, not yet bundled.
It also asserts the two examples and invalid argument handling.

## 0xbb34: channel mapping

Actual LPDDR4X caller b4f8 -> b6f8 -> b504..b51c:
header at 115d0; selected block index byte+a=13 -> 11604.
For type!=9, map-table index byte+22=0x3c -> 116c0.
Selected block byte+a=2 -> w3=2.
Table first seven u32 words:
  0,0,800,1000,2000,0,400000 (hex).
Call bb34(H,R,table,2). H[98]=FD586000, H[a0]=FD587000.
bb60..bba0 checks active channels, compares G[18], G[1c], G[38]
against channel0. G[38] is controller address map table index
set by 89dc, not rank count. Mismatch forces fallback branch;
that fallback is not yet covered.

Actual local interpretation of bb34 with four active channels,
equal row0=16, row1=0, synthetic G[38]=0:
184 steps; function returned zero. Second hypothetical case
row0=row1=16 and G[38]=4: 192 steps; return zero.
Both used exact original header table, not zero-filled map input.

Predicted writes in these two cases:
FD586300   00002021 / 00002421
FD586304   00000a00 / 00000a00
FD586308   00001400 / 00001400
FD58630c   00002800 / 00002800
FD586310   00000000 / 00000000
FD586314   00000020 / 00000020
FD586318   00000040 / 00000040
FD58631c   00000000 / 00401000
Then mirrors into FD587240..25c and writes FD59E010=01f00000,
FD59800C=001f0000. UART print helper stubbed.
These are hypothetical equal-geometry paths, not the final board
mapping. Need actual MR8 and actual resulting G[38], plus decode
of fallback/asymmetric branch and capacity boundaries. Do not
replay these values on hardware.

Original instruction block checksums from repository:
b98c..baec FNV d6f473b4; bb34..bcf0 FNV d0acea44;
ccb4..ce24 FNV ee0b8c36.
The local assembler files contain manually transferred code slices,
and the full-blob interpreter remains integration-untested.

## Remaining work

Second-pass 89dc mode1 (including post-probe P/T modifications and
the remaining two timing differences), precise mapping index
selection, additional training after the second pass, full first
cold sequence integration in C, actual DFI/MR/PHY responses, memory
test. No firmware update and no new physical board writes.
