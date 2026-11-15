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
  python dma_scope_write.py --tw 350                sweep nS_DMADataHold alone, te/ty left at their current value
  python dma_scope_write.py --te 445 --tw 350       te445 and the ty it gets, but tw forced to 350

--te moves nS_DMADataHold (tw) and nS_DMADataSetup (ty) the other way, since both count from the point nS_DMASetup
sets: te+tw (the write's data release) and te+ty (the read's latch) stay at what the board's 't' listing shows
before the change, which after the reset to the menu is the machine's own PAL, NTSC or NTSC C128 set.

The C64 is reset to the menu first: the errors only show while the C64 program is writing RAM, and a quiet
loop hides them.  Needs pyserial (pip install pyserial).  Stop Trom API first (`trom api stop`), it holds the port."""
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


def parse_dma_timing(listing):
    """(nS_DMASetup, nS_DMADataHold, nS_DMADataSetup) from a 't' listing, or None if it doesn't show all three"""
    found = [re.search(rf"(\d+)\s+\({cmd}###\)", listing) for cmd in ("te", "tw", "ty")]
    return tuple(int(m.group(1)) for m in found) if all(found) else None


def dma_timing(ser):
    """the board's (te, tw, ty) now, or None on a build without Dbg_SerTimChg (it ignores a bare 't')"""
    ser.write(b"t")
    return parse_dma_timing(drain(ser, 0.5))


def te_commands(timing, te):
    """the te/tw/ty commands that set nS_DMASetup to te and keep te+tw and te+ty at timing's sums, ValueError if
    any of the three would leave 0-999 (each command takes exactly 3 digits)"""
    sum_w, sum_y = timing[0] + timing[1], timing[0] + timing[2]
    low, top = max(0, sum_w - 999, sum_y - 999), min(sum_w, sum_y, 999)
    if not low <= te <= top:
        raise ValueError(f"--te must be {low}-{top} on this board (te+tw {sum_w}, te+ty {sum_y}), "
                         f"so te, tw and ty stay 0-999")
    return f"te{te:03d}", f"tw{sum_w - te:03d}", f"ty{sum_y - te:03d}"


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
    ap.add_argument("--te", type=int, help="set nS_DMASetup first, with tw and ty moved the other way so te+tw and "
                                           "te+ty stay at the board's values (0 up to the smaller sum); needs a build "
                                           "with Dbg_SerTimChg, and the detected defaults are restored on exit")
    ap.add_argument("--ta", type=int, help="set nS_DMAAssert (0-999) directly, independent of --te")
    ap.add_argument("--tb", type=int, help="set nS_DMABAWait (0-999) directly, independent of --te")
    ap.add_argument("--tw", type=int, help="set nS_DMADataHold (0-999) directly, independent of --te -- applied "
                                           "after --te, so it overrides the tw --te would otherwise set")
    ap.add_argument("--ty", type=int, help="set nS_DMADataSetup (0-999) directly, independent of --te -- applied "
                                           "after --te, so it overrides the ty --te would otherwise set")
    ap.add_argument("--count", type=int, default=0, help="writes to do, 0 = until Ctrl+C")
    ap.add_argument("--interval", type=float, default=0, help="seconds between writes (default 0)")
    ap.add_argument("--keep-going", action="store_true", help="count bad writes instead of stopping at the first")
    ap.add_argument("--no-reset", action="store_true", help="don't reset the C64 to the menu first")
    ap.add_argument("--blank-screen", action="store_true", help="DMA-write $D011=$00 (DEN, display off) and "
                                     "$D015=$00 (sprites off) before the loop, to rule out badline/VIC cycle "
                                     "stealing as a contributor -- undone by the next reset-to-menu")
    args = ap.parse_args()
    addr = int(args.addr, 16)
    if not 0 <= addr <= 0xFFFF:
        ap.error("--addr must be 0000-FFFF")
    overrides = {"ta": args.ta, "tb": args.tb, "tw": args.tw, "ty": args.ty}
    for name, val in {"te": args.te, **overrides}.items():
        if val is not None and not 0 <= val <= 999:
            ap.error(f"--{name} must be 0-999")

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
        timing = dma_timing(ser)
        if args.te is not None or any(v is not None for v in overrides.values()):
            #check before sending anything else: without Dbg_SerTimChg the letters after 't' run as their own
            #   top-level commands, and 'e' resets the EEPROM to defaults
            if timing is None:
                sys.exit("no te/tw/ty in the 't' listing: --te/--ta/--tb/--tw/--ty need a firmware built with "
                          "#define Dbg_SerTimChg in TeensyROM.h, nothing was sent")
            try:
                te_cmds = te_commands(timing, args.te) if args.te is not None else ()
            except ValueError as err:
                sys.exit(f"{err}, nothing was sent")
            timing_changed = True
            for cmd in te_cmds:
                ser.write(cmd.encode())
                drain(ser, 0.4)
            for name, val in overrides.items():
                if val is not None:
                    ser.write(f"{name}{val:03d}".encode())
                    drain(ser, 0.4)
            timing = dma_timing(ser)
            if args.te is not None and (timing is None or timing[0] != args.te):
                sys.exit(f"te didn't take, DMA setup reads {timing[0] if timing else None}")
            ser.write(b"t")  #bare listing query -- confirms every knob (ta/tb/te/tw/ty/...), not just nS_DMASetup
            print("firmware-confirmed listing (ta/tb/te/tw/ty and the rest):")
            print(drain(ser, 0.5), end="")
        applied = ", ".join(f"{n}={v}" for n, v in {"te": args.te, **overrides}.items() if v is not None)
        print(f"DMA setup {timing[0] if timing else '(not reported by this build)'}"
              f"{f' ({applied} applied)' if applied else ''}, writing ${addr:04X} "
              f"alternating $00/$FF, Ctrl+C to stop", flush=True)

        if args.blank_screen:
            dma_write(ser, 0xD011, 0x00)  #DEN=0, display off
            dma_write(ser, 0xD015, 0x00)  #all sprites off
            print("screen blanked ($D011=$00, $D015=$00)", flush=True)

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
