#!/usr/bin/env python3
"""Send only raw CODE471 to RK3588 MaskROM via libusb; never touch UART/storage."""
import argparse
import binascii
import ctypes as C
import ctypes.util
import pathlib
import sys

class Descriptor(C.Structure):
    _fields_ = [
        ("bLength", C.c_uint8), ("bDescriptorType", C.c_uint8),
        ("bcdUSB", C.c_uint16), ("bDeviceClass", C.c_uint8),
        ("bDeviceSubClass", C.c_uint8), ("bDeviceProtocol", C.c_uint8),
        ("bMaxPacketSize0", C.c_uint8), ("idVendor", C.c_uint16),
        ("idProduct", C.c_uint16), ("bcdDevice", C.c_uint16),
        ("iManufacturer", C.c_uint8), ("iProduct", C.c_uint8),
        ("iSerialNumber", C.c_uint8), ("bNumConfigurations", C.c_uint8),
    ]

def framed(raw):
    if not 4 <= len(raw) <= 65536 or len(raw) % 4:
        raise ValueError("expected raw word-aligned CODE471, 4..65536 bytes")
    # Word alignment excludes the vendor protocol's special 4094/4095 cases.
    return raw + binascii.crc_hqx(raw, 0xffff).to_bytes(2, "big")

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("image", type=pathlib.Path)
    a = p.parse_args()
    raw = a.image.read_bytes()
    data = framed(raw)
    name = ctypes.util.find_library("usb-1.0")
    if not name:
        raise RuntimeError("libusb-1.0 not found")
    u = C.CDLL(name)
    def bind(n, restype, *args):
        f = getattr(u, n)
        f.restype, f.argtypes = restype, args
        return f
    P = C.c_void_p
    I = C.c_int
    U8 = C.c_uint8
    U16 = C.c_uint16
    init = bind("libusb_init", I, C.POINTER(P))
    exit_usb = bind("libusb_exit", None, P)
    devlist = bind("libusb_get_device_list", C.c_ssize_t, P, C.POINTER(C.POINTER(P)))
    free = bind("libusb_free_device_list", None, C.POINTER(P), I)
    desc = bind("libusb_get_device_descriptor", I, P, C.POINTER(Descriptor))
    op = bind("libusb_open", I, P, C.POINTER(P))
    close = bind("libusb_close", None, P)
    ctrl = bind("libusb_control_transfer", I, P, U8, U8, U16, U16,
                C.POINTER(U8), U16, C.c_uint)
    error = bind("libusb_error_name", C.c_char_p, I)
    ctx, handle = P(), P()
    rc = init(C.byref(ctx))
    if rc < 0:
        raise RuntimeError(error(rc).decode())
    try:
        devices = C.POINTER(P)()
        n = devlist(ctx, C.byref(devices))
        if n < 0:
            raise RuntimeError(error(n).decode())
        try:
            found = []
            for i in range(n):
                d = Descriptor()
                if desc(devices[i], C.byref(d)) == 0 and (d.idVendor,d.idProduct)==(0x2207,0x350b):
                    print(f"RK3588 USB bcdUSB=0x{d.bcdUSB:04x}", flush=True)
                    if not (d.bcdUSB & 1):
                        found.append(i)
            if len(found)!=1:
                raise RuntimeError(f"need exactly one MaskROM device; found {len(found)}")
            rc = op(devices[found[0]], C.byref(handle))
            if rc < 0:
                raise RuntimeError(error(rc).decode())
        finally:
            free(devices,1)
        print(f"CODE471 raw={len(raw)} framed={len(data)} CRC=0x{data[-2:].hex()}",flush=True)
        for off in range(0,len(data),4096):
            part=data[off:off+4096]
            final=off+len(part)==len(data)
            buf=(U8*len(part)).from_buffer_copy(part)
            print(f"TX 0x471 offset=0x{off:05x} bytes={len(part)} final={final}",flush=True)
            rc=ctrl(handle,0x40,0x0c,0,0x0471,buf,len(part),3000)
            if rc!=len(part):
                detail=error(rc).decode() if rc<0 else f"short transfer: {rc}"
                print(f"USB transfer not acknowledged: {detail}",file=sys.stderr,flush=True)
                if final:
                    print("Final request may already have executed CODE471; check UART manually. No retry.",flush=True)
                return 2
            print("ACK",flush=True)
        print("CODE471 transfer acknowledged. Check UART manually; no CODE472 sent.",flush=True)
        return 0
    finally:
        if handle:
            close(handle)
        exit_usb(ctx)

if __name__=="__main__":
    try:
        sys.exit(main())
    except (RuntimeError,ValueError,OSError) as exc:
        print(f"ERROR: {exc}",file=sys.stderr)
        sys.exit(1)
