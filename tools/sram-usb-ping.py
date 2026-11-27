#!/usr/bin/env python3
"""Read-only EP0 ping for SRAM USB phase 1; no serial or image writes."""
import ctypes as C
import ctypes.util
import sys

def main():
    lib=ctypes.util.find_library("usb-1.0")
    if not lib:
        raise RuntimeError("libusb-1.0 not found")
    u=C.CDLL(lib)
    P=C.c_void_p
    I=C.c_int
    U8=C.c_uint8
    U16=C.c_uint16
    def bind(name,ret,*args):
        f=getattr(u,name)
        f.restype,f.argtypes=ret,args
        return f
    init=bind("libusb_init",I,C.POINTER(P))
    exit_usb=bind("libusb_exit",None,P)
    op=bind("libusb_open_device_with_vid_pid",P,P,U16,U16)
    close=bind("libusb_close",None,P)
    config=bind("libusb_get_configuration",I,P,C.POINTER(I))
    ctrl=bind("libusb_control_transfer",I,P,U8,U8,U16,U16,C.POINTER(U8),U16,C.c_uint)
    error=bind("libusb_error_name",C.c_char_p,I)
    ctx=P()
    rc=init(C.byref(ctx))
    if rc<0:
        raise RuntimeError(error(rc).decode())
    h=None
    try:
        h=op(ctx,0x1209,0x0001)
        if not h:
            raise RuntimeError("1209:0001 not found/openable; check lsusb and permission")
        cfg=I()
        rc=config(h,C.byref(cfg))
        if rc<0 or cfg.value!=1:
            raise RuntimeError("USB device not configured")
        buf=(U8*8)()
        rc=ctrl(h,0xc0,0x51,0,0,buf,len(buf),3000)
        if rc<0:
            raise RuntimeError(error(rc).decode())
        result=bytes(buf[:rc])
        print("RX:",repr(result))
        if result!=b"SRAMUSB1":
            raise RuntimeError("unexpected ping response")
        print("SRAM USB EP0 PING PASS")
        return 0
    finally:
        if h:close(h)
        exit_usb(ctx)

if __name__=="__main__":
    try:
        sys.exit(main())
    except (RuntimeError,OSError) as exc:
        print("ERROR:",exc,file=sys.stderr)
        sys.exit(1)
