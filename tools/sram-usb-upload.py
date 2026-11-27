#!/usr/bin/env python3
"""Stage a raw position-independent SRAM image via EP0. Never run it remotely."""
import argparse
import binascii
import ctypes as C
import ctypes.util
import pathlib
import struct
import sys

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("image",type=pathlib.Path)
    args=p.parse_args()
    data=args.image.read_bytes()
    if not 4<=len(data)<=65536 or len(data)%4:
        p.error("raw SRAM image must be 4..65536 bytes, word aligned")
    if data[:4] in (b"BOOT",b"LDR ",b"RKNS",b"\x7fELF"):
        p.error("use raw sram.bin, not loader container or ELF")
    crc=binascii.crc32(data)&0xffffffff
    print(f"image={args.image} size={len(data)} CRC32={crc:08x}",flush=True)
    lib=ctypes.util.find_library("usb-1.0")
    if not lib:raise RuntimeError("libusb-1.0 not found")
    u=C.CDLL(lib)
    P=C.c_void_p;I=C.c_int;U8=C.c_uint8;U16=C.c_uint16
    def bind(name,ret,*types):
        f=getattr(u,name);f.restype,f.argtypes=ret,types;return f
    init=bind("libusb_init",I,C.POINTER(P))
    exit_usb=bind("libusb_exit",None,P)
    op=bind("libusb_open_device_with_vid_pid",P,P,U16,U16)
    close=bind("libusb_close",None,P)
    config=bind("libusb_get_configuration",I,P,C.POINTER(I))
    ctrl=bind("libusb_control_transfer",I,P,U8,U8,U16,U16,C.POINTER(U8),U16,C.c_uint)
    err=bind("libusb_error_name",C.c_char_p,I)
    ctx=P();rc=init(C.byref(ctx))
    if rc<0:raise RuntimeError(err(rc).decode())
    h=None
    try:
        h=op(ctx,0x1209,0x0001)
        if not h:raise RuntimeError("1209:0001 not found or permission denied")
        cfg=I()
        if config(h,C.byref(cfg))<0 or cfg.value!=1:
            raise RuntimeError("device not configured")
        def request(bm,req,value=0,payload=b"",length=0):
            n=length if bm&0x80 else len(payload)
            buf=(U8*max(n,1))()
            if payload:C.memmove(buf,payload,len(payload))
            rc=ctrl(h,bm,req,value,0,buf,n,3000)
            if rc<0:raise RuntimeError(f"request 0x{req:02x} at {value}: {err(rc).decode()}")
            if rc!=n:raise RuntimeError(f"request 0x{req:02x}: short transfer {rc}/{n}")
            return bytes(buf[:n]) if bm&0x80 else None
        if request(0xc0,0x51,length=8)!=b"SRAMUSB1":
            raise RuntimeError("not SRAM USB device")
        status=struct.unpack("<IIII",request(0xc0,0x55,length=16))
        print("initial status:",status,flush=True)
        request(0x40,0x52,payload=struct.pack("<II",len(data),crc))
        for off in range(0,len(data),64):
            request(0x40,0x53,value=off,payload=data[off:off+64])
            if off%4096==0 or off+64>=len(data):
                print(f"staged {min(off+64,len(data))}/{len(data)}",flush=True)
        request(0x40,0x54)
        status=struct.unpack("<IIII",request(0xc0,0x55,length=16))
        if status!=(2,len(data),len(data),crc):
            raise RuntimeError(f"unexpected final status {status}")
        print("SRAM USB UPLOAD PASS; CRC32 verified on target.")
        print("Use serial terminal: loadstat, then run. No remote execution.")
        return 0
    finally:
        if h:close(h)
        exit_usb(ctx)

if __name__=="__main__":
    try:sys.exit(main())
    except (RuntimeError,OSError,struct.error) as exc:
        print("ERROR:",exc,file=sys.stderr)
        print("No automatic retry. Re-arm with usbload in UART before retrying.",file=sys.stderr)
        sys.exit(1)
