#!/usr/bin/env python3
"""Restricted OFFLINE interpreter for the v1.18 first LPDDR4X cold path.

This is a reverse-engineering harness, not a general ARM emulator.
Requires Python 3, clang and llvm-objdump; no Unicorn or board.
Only the exact unpatched image is accepted. All MMIO is a Python dict.
The PHY passes include 32a0 before 6aac/6214, but still omit reset
and address-map steps in 89dc. Commit acknowledgements are scripted.
The target memset is replaced by a byte fill. Unknown instructions
or addresses fail closed. Initial synthetic register values are zero.
Do NOT use output as a hardware write script.
"""
import hashlib
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

if not __debug__:
    raise SystemExit("Do not disable assertions")
if len(sys.argv) != 2:
    raise SystemExit("Usage: python3 tools/ddr-v118-interpret.py exact-v1.18.bin")
blob = Path(sys.argv[1]).read_bytes()
assert len(blob) == 75320
assert hashlib.sha1(blob).hexdigest() == "8d5e55ebf63d94618b448b79757b1f07733fd076"

B=0xff001000; P=0xff081000; T=0xff082000; R=0xff080000; STOP=0xff090000
H=0xff083000; CTL=0xf7000000; PHY=0xfe0c0000
code={}; words={}; executed=set()
# Make LLVM decode actual words directly from the input binary.
# No manually pasted instruction snippets or external disassembly files.
with tempfile.TemporaryDirectory() as d:
    asm=Path(d)/"image.S"; obj=Path(d)/"image.o"
    asm.write_text(".text\n" + "".join(".inst 0x%08x\n"%w for w, in
                   struct.iter_unpack("<I",blob[:0x10530])))
    subprocess.run([os.getenv("CLANG","clang"),"-target","aarch64-none-elf",
                    "-c",str(asm),"-o",str(obj)],check=True)
    out=subprocess.check_output([os.getenv("LLVM_OBJDUMP","llvm-objdump"),
                                 "-d",str(obj)],text=True)
for line in out.splitlines():
    m=re.match(r"\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+([\w.]+)\s*(.*?)\s*(?://.*)?$",line)
    if m and m[3] not in ("udf",".word"):
        addr=int(m[1],16)+B
        code[addr]=(m[3],m[4].split(" <")[0].strip())
        words[addr]=int(m[2],16)

memstart=0xff000000; mem=bytearray(0x100000)
mem[B-memstart:B-memstart+len(blob)]=blob
regs=[0]*32; regs[:6]=[R,P,T,528,8,0]
regs[31]=0xff0f0000; regs[30]=STOP
N=Z=C=V=0
pc=B+0x6dc4
steps=0; phase=0; mmio={}; trace=[]; ctl_trace=[]; electrical_trace=[]

def val(s):
    s=s.strip()
    if s.startswith("#"): return int(s[1:],0)
    if s in ("wzr","xzr"): return 0
    if s=="sp": return regs[31]
    return regs[int(s[1:])] & ((1<<(32 if s[0]=="w" else 64))-1)

def put(r,v):
    if r in ("wzr","xzr"): return
    regs[31 if r=="sp" else int(r[1:])]=v&((1<<(32 if r[0]=="w" else 64))-1)

def rd(a,n):
    if CTL<=a<CTL+0x400000 or PHY<=a<PHY+0x1000:
        assert phase in (1,2,3) and n==4 and a%4==0
        v=mmio.get(a,0)
        if phase==2 and a==CTL+0x10c84:
            # Synthetic commit response, not a model of DDRCTL.
            assert mmio.get(CTL+0x10c80)==1
            v=1
        trace.append(("R",cur-B,a,v)); return v
    assert 0<=a-memstart<=len(mem)-n,hex(a)
    return int.from_bytes(mem[a-memstart:a-memstart+n],"little")

def wr(a,v,n):
    if CTL<=a<CTL+0x400000 or PHY<=a<PHY+0x1000:
        assert phase in (1,2,3) and n==4 and a%4==0
        v&=0xffffffff; trace.append(("W",cur-B,a,v)); mmio[a]=v; return
    assert 0<=a-memstart<=len(mem)-n,hex(a)
    mem[a-memstart:a-memstart+n]=(v&((1<<(8*n))-1)).to_bytes(n,"little")

def flags(a,b,w,sub=True):
    global N,Z,C,V
    mask=(1<<w)-1; a&=mask; b&=mask
    v=(a-b if sub else a+b)&mask
    N=v>>(w-1); Z=int(v==0); C=int(a>=b if sub else a+b>mask)
    V=int(bool(((a^b) if sub else ~(a^b))&(a^v)&(1<<(w-1))))

def cond(c):
    return {"eq":Z,"ne":not Z,"hs":C,"cs":C,"lo":not C,"cc":not C,
            "hi":C and not Z,"ls":not C or Z,"ge":N==V,"lt":N!=V,
            "gt":not Z and N==V,"le":Z or N!=V,"mi":N,"pl":not N,
            "vs":V,"vc":not V,"al":True}[c]

def opval(a,i):
    v=val(a[i])
    if len(a)>i+1:
        typ,num=a[i+1].split(); s=val(num)
        if typ=="lsl": v<<=s
        elif typ=="lsr": v>>=s
        elif typ=="uxtb": v=(v&255)<<s
        elif typ=="uxtw": v=(v&0xffffffff)<<s
        else: raise ValueError(typ)
    return v

def fnv(data):
    h=2166136261
    for x in data: h=((h^x)*16777619)&0xffffffff
    return h

while True:
    if pc==STOP:
        if phase==3: break
        if phase==2:
            assert steps==1574 and len(trace)==32
            assert sum(t[0]=="W" for t in trace)==16
            assert [rd(R+o,4) for o in (0x1c4,0x1c8,0x1cc,0x1d0,0x2c8)]==[7,7,7,6,10]
            assert [mmio[CTL+o] for o in (0x504,0x508,0x50c)]==[0x00310000,0x5c,0x0006005c]
            electrical_trace=trace[:]; trace.clear(); phase=3
            regs[:6]=[H,0,R,0,0,0]; regs[31]=0xff0f0000
            regs[30]=STOP; pc=B+0x6aac
        elif phase==1:
            assert steps==1138 and len(executed)==1072
            assert sum(t[0]=="W" for t in trace)==53 and sum(t[0]=="R" for t in trace)==15
            assert fnv(b"".join(words[x].to_bytes(4,"little") for x in sorted(executed)))==0xb26c2d45
            assert mmio[CTL]==0x19161217 and mmio[CTL+0x10000]==0x03080002
            ctl_trace=trace[:]; trace.clear(); phase=2
            # First electrical config call at 8c90; NOT the full
            # 89dc path. Reset/address-map operations still omitted.
            # Seed ONLY previously measured partial-prefix registers.
            wr(H+8,PHY,8)
            for off,v in ((0,0x44005011),(0xa1c,0x20820),(0xb0,0x2000017e),
                          (0xb4,0x00200100),(0xb8,0x2640),(0xbf4,9)):
                mmio[PHY+off]=v
            regs[:6]=[H,0,R,0,0,0]; regs[31]=0xff0f0000
            regs[30]=STOP; pc=B+0x32a0
        else:
            print("generator steps",steps,"return",regs[0])
            assert regs[0]==0 and steps==641
            assert rd(P+0x28,4)==8 and rd(P+0x2c,4)==528
            assert rd(R+0x228,8)==P and rd(R+0x230,8)==T
            for ch in range(4):
                assert [rd(R+ch*0x6c+i*4,4) for i in range(15)]==[2,11,3,1,1,0,17,17,0,0,0,0,0,0,0]
            # H values are for the exact unpatched binary's first candidate.
            phase=1; wr(H,CTL,8)
            wr(H+0xe8,0,4); wr(H+0xec,13,4)
            wr(H+0xf0,1,4); wr(H+0xf4,1,4); wr(H+0xf8,0,4)
            regs[:6]=[H,0,R,0,0,0]; regs[31]=0xff0f0000
            regs[30]=STOP; pc=B+0x5e38
    steps+=1; assert steps<10000
    if pc==B+0xa0c:
        # A byte fill instead of interpreting the vendor memset body.
        for i in range(regs[2]): wr(regs[0]+i,regs[1],1)
        pc=regs[30]; continue
    m,s=code[pc]; executed.add(pc); cur=pc; pc+=4
    a=re.split(r",\s*(?![^\[]*\])",s) if s else []
    try:
        if m in ("mov","movk"):
            if m=="mov": put(a[0],val(a[1]))
            else:
                shift=val(a[2].split()[1]) if len(a)>2 else 0
                put(a[0],(val(a[0])&~(65535<<shift))|(val(a[1])<<shift))
        elif m=="nop": pass
        elif m=="adrp": put(a[0],B+int(a[1],0))
        elif m in ("add","sub","adds","subs","and","orr","eor","bic","mul","udiv","lsl","lsr","asr"):
            v=val(a[1]); u=opval(a,2); w=32 if a[0][0]=="w" else 64
            if m in ("add","adds"): res=v+u
            elif m in ("sub","subs"): res=v-u
            elif m=="and": res=v&u
            elif m=="orr": res=v|u
            elif m=="eor": res=v^u
            elif m=="bic": res=v&~u
            elif m=="mul": res=v*u
            elif m=="udiv": res=v//u if u else 0
            elif m=="lsl": res=v<<(u%w)
            elif m=="lsr": res=v>>(u%w)
            else: res=((v^(1<<(w-1)))-(1<<(w-1)))>>(u%w)
            if m in ("adds","subs"): flags(v,u,w,m=="subs")
            put(a[0],res)
        elif m=="umull": put(a[0],val(a[1])*val(a[2]))
        elif m in ("madd","msub","umaddl"):
            put(a[0],val(a[3])+val(a[1])*val(a[2]) if m!="msub" else val(a[3])-val(a[1])*val(a[2]))
        elif m in ("ubfx","ubfiz"):
            v=val(a[1]); l=val(a[2]); w=val(a[3])
            put(a[0],(v>>l)&((1<<w)-1) if m=="ubfx" else (v&((1<<w)-1))<<l)
        elif m=="cmp": flags(val(a[0]),opval(a,1),32 if a[0][0]=="w" else 64)
        elif m=="ccmp":
            if cond(a[3]): flags(val(a[0]),val(a[1]),32 if a[0][0]=="w" else 64)
            else:
                v=val(a[2]); N,Z,C,V=[(v>>i)&1 for i in (3,2,1,0)]
        elif m=="tst":
            v=val(a[0])&opval(a,1)
            Z=int(v==0); N=v>>(31 if a[0][0]=="w" else 63); C=V=0
        elif m in ("csel","csinc","csinv","csneg"):
            v=val(a[2]); v={"csel":v,"csinc":v+1,"csinv":~v,"csneg":-v}[m]
            put(a[0],val(a[1]) if cond(a[3]) else v)
        elif m=="cset": put(a[0],int(cond(a[1])))
        elif m in ("b","bl"):
            if m=="bl": regs[30]=pc
            pc=B+int(a[0],0)
        elif m.startswith("b."):
            if cond(m[2:]): pc=B+int(a[0],0)
        elif m in ("cbz","cbnz"):
            if (val(a[0])==0)==(m=="cbz"): pc=B+int(a[1],0)
        elif m in ("tbz","tbnz"):
            if (val(a[0])&(1<<val(a[1]))==0)==(m=="tbz"): pc=B+int(a[2],0)
        elif m=="ret": pc=regs[30]
        elif m in ("ldr","ldrb","ldrh","str","strb","ldp","stp"):
            pair=m in ("ldp","stp"); ix=2 if pair else 1
            ar=a[ix]; base,*off=ar.strip("[]!").split(", ")
            addr=val(base)+(opval(off,0) if off else 0)
            n=1 if m.endswith("b") else 2 if m.endswith("h") else 4 if a[0][0]=="w" else 8
            if ar.endswith("!"): put(base,addr)
            for j in range(2 if pair else 1):
                if m.startswith("ld"): put(a[j],rd(addr+j*n,n))
                else: wr(addr+j*n,val(a[j]),n)
            if len(a)>ix+1: put(base,val(base)+val(a[ix+1]))
        else: raise ValueError(m)
    except Exception:
        print(hex(cur-B),m,s,"regs",[hex(x) for x in regs],file=sys.stderr)
        raise

assert steps==1887 and len(executed)==1703
assert sum(t[0]=="W" for t in trace)==22 and sum(t[0]=="R" for t in trace)==21
assert fnv(b"".join(words[x].to_bytes(4,"little") for x in sorted(executed)))==0x4e98a04d
assert fnv(blob[0x115d0:0x11828])==0xa1b636de
assert fnv(blob[0x6214:0x6aac])==0x1d86a539
assert fnv(blob[0x32a0:0x3ad4])==0x61e6d365
assert fnv(blob[0x11a08:0x11a88])==0x0aea8583
assert fnv(blob[0x125f0:0x12600])==0x4eed1215
assert mmio[PHY+0xb0]==0x2002ef7f and mmio[PHY+0xb8]==0x2210
assert mmio[PHY+0xc]==0x04050300 and mmio[PHY+0xa80]==0x5c000914
print("OFFLINE INTERPRETER PASS",steps,"steps")
print("Synthetic DDRCTL trace (68 accesses)")
for typ,ip,addr,v in ctl_trace:
    print(f"{typ} pc={ip:05x} addr={addr:08x} value={v:08x}")
print("Electrical trace (32 accesses; commit acknowledgements scripted)")
for typ,ip,addr,v in electrical_trace:
    print(f"{typ} pc={ip:05x} addr={addr:08x} value={v:08x}")
print("PHY timing trace (43 accesses; NOT complete cold path)")
for typ,ip,addr,v in trace:
    print(f"{typ} pc={ip:05x} addr={addr:08x} value={v:08x}")
for name,base,size in (("R",R,0x358),("P",P,0x94),("T",T,0x2ac)):
    print(name)
    print(" ".join("%03x:%x"%(i,rd(base+i,4)) for i in range(0,size,4)))
print("No physical register access. Not a hardware write script.")
