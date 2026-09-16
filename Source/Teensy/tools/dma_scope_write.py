#!/usr/bin/env python3
"""Repeat single-byte DMA writes to one C64/C128 address from a TeensyROM+ over USB serial, to put a
DMA write under a scope.

Each write is its own DMA session, and the readback after it is a DMA read, which never pulls R/W low.
So a scope triggering on R/W falling while /DMA is low fires exactly once per write.  The script stops at
the first write that reads back wrong (unless --keep-going), which leaves a scope in Normal trigger mode
showing that write.

  python dma_scope_write.py                         the only Teensy found, $C0EF, current timing
  python dma_scope_write.py COM5 --te 405           set te405 first (build with Dbg_SerTimChg)
  python dma_scope_write.py COM5 --te 405 --addr C0FF    another address, to compare captures

The C64 is reset to the menu first: the errors only show while the C64 program is writing RAM, and a quiet
loop hides them.  Needs pyserial (pip install pyserial).  Close the TeensyROM UI first, it holds the port."""
import argparse, re, sys, time
import serial
from serial.tools import list_ports

ACK = b"\xcc\x64"
FW_FULL, FW_MINIMAL = b"\xe2\x64", b"\xe1\x64"  #FWFullToken/FWMinimalToken, low byte first


def find_port():
    ports = [p.device for p in list_ports.comports() if p.vid == 0x16C0]  #PJRC
    if len(ports) != 1:
        raise serial.SerialException(f"found {len(ports)} Teensy ports {ports}, name the port on the command line")
    return ports[0]


def open_port(name):
    ser = serial.Serial(name or find_port(), 115200, timeout=0.5)
    time.sleep(0.5)
    drain(ser, 0.3)
    return ser


def read_exact(ser, n, timeout=5):
    data, start = b"", time.time()
    while len(data) < n and time.time() - start < timeout:
        data += ser.read(n - len(data))
    return data


def drain(ser, idle):
    """read until the port has been quiet for idle seconds, return it as text"""
    data, last = b"", time.time()
    while time.time() - last < idle:
        if ser.in_waiting:
            data += ser.read(ser.in_waiting)
            last = time.time()
        else:
            time.sleep(0.02)
    return data.decode(errors="replace")


def expect_ack(ser, what):
    got = read_exact(ser, 2)
    if got != ACK:
        raise RuntimeError(f"{what}: expected ack cc64, got {got.hex() or 'nothing'} {drain(ser, 1)!r}")


def firmware(ser):
    ser.write(b"\x64\xe0")  #FWCheckToken
    return read_exact(ser, 2, timeout=2)


def dma_setup(ser):
    """nS_DMASetup from the 't' listing, or None on a build without Dbg_SerTimChg"""
    ser.write(b"t")
    listing = drain(ser, 0.5)
    found = re.search(r"nS_DMASetup\s+(\d+)", listing)
    return int(found.group(1)) if found else None


def dma_cmd(ser, token, payload, what, probe=False):
    if probe:
        #token alone first: a build without the DMA tokens answers it at once, and the payload must not follow,
        #   since its bytes would run as top-level serial commands ('e' resets the EEPROM)
        ser.write(token)
        time.sleep(0.15)  #well inside the firmware's 500mS wait for the payload
        if ser.in_waiting:
            raise RuntimeError(f"firmware rejected the DMA {what} token, is this a TR+ build? {drain(ser, 0.5)!r}")
        ser.write(payload)
    else:
        ser.write(token + payload)
    expect_ack(ser, what)


def dma_write(ser, addr, val):
    dma_cmd(ser, b"\x64\xfb", bytes([addr >> 8, addr & 0xFF, 0, 1, val]), "write")  #WriteC64MemToken


def dma_read(ser, addr, probe=False):
    dma_cmd(ser, b"\x64\xfd", bytes([addr >> 8, addr & 0xFF, 0, 1]), "read", probe)  #ReadC64MemToken
    return read_exact(ser, 1)


def reset_from_minimal(port):
    """in the minimal firmware the reset token jumps to the main image, which re-enumerates USB"""
    ser = open_port(port)
    ser.write(b"\x64\xee")
    ser.close()
    time.sleep(3)
    for _ in range(30):
        try:
            return open_port(port)  #the port name can change with the image when auto-detecting
        except serial.SerialException:
            time.sleep(1)
    sys.exit("the TR+ didn't come back on USB after the reset")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", nargs="?", help="serial port, e.g. COM5 (default: the only Teensy found)")
    ap.add_argument("--addr", default="C0EF", help="hex address to write (default C0EF)")
    ap.add_argument("--te", type=int, help="set nS_DMASetup (0-820) first, with tw=840-te and ty=820-te; needs "
                                           "a build with Dbg_SerTimChg, and the detected defaults are restored on exit")
    ap.add_argument("--count", type=int, default=0, help="writes to do, 0 = until Ctrl+C")
    ap.add_argument("--interval", type=float, default=0, help="seconds between writes (default 0)")
    ap.add_argument("--keep-going", action="store_true", help="count bad writes instead of stopping at the first")
    ap.add_argument("--no-reset", action="store_true", help="don't reset the C64 to the menu first")
    args = ap.parse_args()
    addr = int(args.addr, 16)
    if not 0 <= addr <= 0xFFFF:
        ap.error("--addr must be 0000-FFFF")
    if args.te is not None and not 0 <= args.te <= 820:
        ap.error("--te must be 0-820, so tw and ty stay 3-digit and positive")

    try:
        ser = open_port(args.port)
    except serial.SerialException as err:
        sys.exit(str(err))
    fw = firmware(ser)
    if fw == FW_MINIMAL:
        if args.no_reset:
            sys.exit("the TR+ is in its minimal firmware (a cart is running), drop --no-reset to go to the menu")
        print("minimal firmware running, resetting to the main firmware and menu...")
        ser.close()
        ser = reset_from_minimal(args.port)
        drain(ser, 6)
        fw = firmware(ser)
    elif fw == FW_FULL and not args.no_reset:
        print("resetting the C64 to the menu...")
        ser.write(b"\x64\xee")
        drain(ser, 6)  #the menu reports the machine type, which re-selects the detected timing
    if fw != FW_FULL:
        sys.exit(f"no TeensyROM firmware answering on this port (firmware check got {fw.hex() or 'nothing'})")

    writes = bad = 0
    timing_changed = False
    try:
        setup = dma_setup(ser)
        if args.te is not None:
            #check before sending anything else: without Dbg_SerTimChg the letters after 't' run as their own
            #   top-level commands, and 'e' resets the EEPROM to defaults
            if setup is None:
                sys.exit("--te needs a firmware built with #define Dbg_SerTimChg in TeensyROM.h, nothing was sent")
            timing_changed = True
            for cmd in (f"te{args.te:03d}", f"tw{840 - args.te:03d}", f"ty{820 - args.te:03d}"):
                ser.write(cmd.encode())
                drain(ser, 0.4)
            setup = dma_setup(ser)
            if setup != args.te:
                sys.exit(f"te didn't take, DMA setup reads {setup}")
        print(f"DMA setup {setup if setup is not None else '(not reported by this build)'}, writing ${addr:04X} "
              f"alternating $00/$FF, Ctrl+C to stop", flush=True)

        prev = dma_read(ser, addr, probe=True)  #the first DMA request, so it carries the probe
        while not args.count or writes < args.count:
            val = 0xFF if writes & 1 else 0x00
            dma_write(ser, addr, val)
            got = dma_read(ser, addr)
            writes += 1
            if got != bytes([val]):
                bad += 1
                kind = "no reply" if not got else "unchanged" if got == prev else "partial"
                print(f"write {writes}: wrote ${val:02X}, read back "
                      f"{'$%02X' % got[0] if got else 'nothing'} ({kind})", flush=True)
                if not args.keep_going:
                    print("stopped: the last write the scope triggered on is the bad one")
                    break
            elif writes % 1000 == 0:
                print(f"{writes} writes, {bad} bad", flush=True)
            prev = got
            if args.interval:
                time.sleep(args.interval)
    except KeyboardInterrupt:
        pass
    finally:
        print(f"{writes} writes to ${addr:04X}, {bad} bad")
        if timing_changed:
            drain(ser, 0.5)
            ser.write(b"td")
            reply, start = "", time.time()
            while "Defaults set" not in reply and time.time() - start < 3:  #a quiet-port wait missed it once after Ctrl+C
                reply += drain(ser, 0.2)
            restored = [line for line in reply.splitlines() if "Defaults set" in line]
            print(restored[0].strip() if restored else "td sent, no confirmation seen")
        ser.close()


if __name__ == "__main__":
    main()
