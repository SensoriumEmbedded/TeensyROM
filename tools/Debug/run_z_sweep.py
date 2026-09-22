#!/usr/bin/env python3
"""Trigger the 'z' DMA bit-transition sweep (SerUSBIO.ino) directly over serial and capture
the output. Unlike the on-device "TR+ C64 Expansion Port Test" menu diagnostic, 'z' runs all
5 TestDMAPattern() sub-patterns unconditionally -- no short-circuit on the first failure --
so it always gives the full picture in one shot. --te/--ta/--tb/--tw/--ty all need a firmware
built with Dbg_SerTimChg -- without it, the letters sent run as their own unrelated top-level
serial commands instead ('e' resets the EEPROM to defaults), so don't pass any of them unless
you've confirmed the build has it (e.g. via dma_scope_write.py, which checks first).

The C64 is reset to the menu first, same as dma_scope_write.py -- without this, z999 runs
against whatever the C64 was last left doing, including a state a previous bad DMA write may
have corrupted, which silently contaminates the capture.  Pass --no-reset to skip it (e.g. to
re-run z999 against a deliberately-left-dirty state).

  python run_z_sweep.py                          the only Teensy found, 999 passes, current timing
  python run_z_sweep.py COM5 --passes 200         fewer passes (200*256 = 51200 bytes/pattern)
  python run_z_sweep.py --te 430                  force nS_DMASetup=430 first, restore after
  python run_z_sweep.py --tw 350                  sweep nS_DMADataHold alone, te/ty left at their current value
  python run_z_sweep.py --te 445 --tw 350         te445's te/ty, but tw overridden to 350 instead of 395

Close the TeensyROM UI first, it holds the port. A badly-faulted board can stall mid-sweep,
so the capture window is generous (45s of quiet) before giving up."""
import argparse, sys, time
import serial
from dma_scope_write import open_port, firmware, reset_from_minimal, FW_FULL, FW_MINIMAL


def drain(ser, idle):
    """read until the port has been quiet for idle seconds, return it as raw bytes"""
    data, last = b"", time.time()
    while time.time() - last < idle:
        if ser.in_waiting:
            data += ser.read(ser.in_waiting)
            last = time.time()
        else:
            time.sleep(0.02)
    return data


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", nargs="?", help="serial port, e.g. COM5 (default: the only Teensy found)")
    ap.add_argument("--passes", type=int, default=999, help="passes per pattern, 1-999 (default 999)")
    ap.add_argument("--te", type=int, help="force nS_DMASetup (0-820) first, with tw=840-te and ty=820-te; needs "
                                           "a build with Dbg_SerTimChg, and the detected defaults are restored on exit")
    ap.add_argument("--ta", type=int, help="force nS_DMAAssert (0-999) directly, independent of --te")
    ap.add_argument("--tb", type=int, help="force nS_DMABAWait (0-999) directly, independent of --te")
    ap.add_argument("--tw", type=int, help="force nS_DMADataHold (0-999) directly, independent of --te -- applied "
                                           "after --te, so it overrides the tw --te would otherwise set")
    ap.add_argument("--ty", type=int, help="force nS_DMADataSetup (0-999) directly, independent of --te -- applied "
                                           "after --te, so it overrides the ty --te would otherwise set")
    ap.add_argument("--no-reset", action="store_true", help="don't reset the C64 to the menu first")
    ap.add_argument("--out", default="z_sweep.log", help="log file to write (default z_sweep.log)")
    args = ap.parse_args()
    if args.te is not None and not 0 <= args.te <= 820:
        ap.error("--te must be 0-820, so tw and ty stay 3-digit and positive")
    overrides = {"ta": args.ta, "tb": args.tb, "tw": args.tw, "ty": args.ty}
    for name, val in overrides.items():
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
        drain(ser, 6)  # the menu reports the machine type, which re-selects the detected timing
    if fw != FW_FULL:
        sys.exit(f"no TeensyROM firmware answering on this port (firmware check got {fw.hex() or 'nothing'})")

    timing_changed = args.te is not None or any(v is not None for v in overrides.values())
    confirm = b""
    if args.te is not None:
        for c in (f"te{args.te:03d}", f"tw{840 - args.te:03d}", f"ty{820 - args.te:03d}"):
            ser.write(c.encode())
            confirm = drain(ser, 0.4)  # each 't' subcommand echoes the full current listing (ta/tb/te/tw/ty/...)
    for name, val in overrides.items():
        if val is not None:
            ser.write(f"{name}{val:03d}".encode())
            confirm = drain(ser, 0.4)
    if timing_changed:
        applied = ", ".join(f"{n}={v}" for n, v in {"te": args.te, **overrides}.items() if v is not None)
        print(f"timing overrides applied: {applied}", flush=True)
        print("firmware-confirmed listing after the last override (ta/tb/te/tw/ty and the rest):", flush=True)
        listing = confirm.decode(errors="replace")
        sys.stdout.write(listing)

    cmd = f"z{args.passes:03d}"
    print(f"sending {cmd!r} on {ser.port}...", flush=True)
    ser.write(cmd.encode())

    out = drain(ser, 45)  # generous quiet window -- a badly-faulted board can stall mid-sweep
    text = out.decode(errors="replace")
    sys.stdout.write(text)
    logged = (f"timing overrides applied: {applied}\n{listing}\n" if timing_changed else "") + text
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(logged)
    print(f"\n--- {len(text)} chars captured to {args.out} ---", flush=True)
    if timing_changed:
        ser.write(b"td")
        drain(ser, 1)
    ser.close()


if __name__ == "__main__":
    main()
