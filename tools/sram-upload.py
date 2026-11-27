#!/usr/bin/env python3
"""Upload a trusted SRAM shell binary to the inactive slot, then jump.
Close the terminal using the UART before running this tool."""
import argparse
import os
import pathlib
import select
import struct
import termios
import time
import zlib

def send(fd, data):
    deadline = time.monotonic() + 6
    while data:
        if not select.select([], [fd], [], max(0, deadline-time.monotonic()))[1]:
            raise TimeoutError("UART write timeout")
        try:
            n = os.write(fd, data)
        except BlockingIOError:
            continue
        data = data[n:]

def expect(fd, marker, timeout=8):
    deadline = time.monotonic()+timeout
    buf = bytearray()
    while time.monotonic()<deadline:
        if not select.select([fd], [], [], max(0,deadline-time.monotonic()))[0]:
            break
        try:
            b = os.read(fd,1)
        except BlockingIOError:
            continue
        if not b:
            raise OSError("UART disconnected")
        buf.extend(b)
        if b==b"\n":
            line=bytes(buf).strip()
            # Prompt/echo can precede a protocol line; marker is a full line.
            if line.startswith(b"ERR "):
                raise RuntimeError(line.decode(errors="replace"))
            if line==marker:
                return
            print(line.decode(errors="replace"))
            buf.clear()
        if len(buf)>4096:
            raise RuntimeError("unexpected UART stream")
    raise TimeoutError(f"waiting for {marker!r}; partial={bytes(buf)!r}")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("port")
    p.add_argument("image",type=pathlib.Path)
    p.add_argument("--no-run",action="store_true")
    a=p.parse_args()
    b=a.image.read_bytes()
    if not 4<=len(b)<=65536 or len(b)%4:
        p.error("expected a word-padded SRAM image <=64 KiB")
    fd=os.open(a.port,os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
    old=None
    try:
        old=termios.tcgetattr(fd)
        t=termios.tcgetattr(fd)
        t[0]=t[1]=t[3]=0
        t[2]=termios.CLOCAL|termios.CREAD|termios.CS8
        t[4]=t[5]=termios.B1500000
        t[6][termios.VMIN]=0
        t[6][termios.VTIME]=0
        termios.tcsetattr(fd,termios.TCSANOW,t)
        termios.tcflush(fd,termios.TCIFLUSH)
        send(fd,b"\nload\n")
        expect(fd,b"READY")
        send(fd,struct.pack("<II",len(b),zlib.crc32(b)))
        expect(fd,b"GO")
        for i in range(0,len(b),128):
            send(fd,b[i:i+128])
            expect(fd,b"ACK")
        expect(fd,b"OK")
        print(f"SRAM verified: {len(b)} bytes")
        if not a.no_run:
            send(fd,b"run\n")
            expect(fd,b"JUMP")
            expect(fd,b"[SRAM] READY")
            print("New SRAM shell started")
    finally:
        if old is not None:
            termios.tcsetattr(fd,termios.TCSANOW,old)
        os.close(fd)

if __name__=="__main__":
    main()
