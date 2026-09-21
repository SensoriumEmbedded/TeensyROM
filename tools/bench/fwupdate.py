#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reflash the cartridge with nobody at it.   fwupdate.py <hex> [remote path, default /fwupdate.hex]

Pushes the hex to the SD card, launches it (a .hex runs TeensyROM's own SD
updater, the supported path), waits for the C64's "UPDATE TEENSYROM FIRMWARE?
Y/N" prompt, answers Y by writing to the keyboard buffer, then echoes the
updater's serial output until the board reboots. No program button needed,
unlike `teensy_loader_cli -w`.

Needs the main image running and a hex built for THIS cartridge (--target
tr-plus for a TR+); the updater refuses a mismatched target string. Which image
came up afterwards shows in the port name: the main image has a location-based
name (usbmodem2101), minimal and the extension image a serial-number one.

The banner's build date cannot verify a rebuild: SOURCE_DATE_EPOCH is the HEAD
commit time, so commit before building if you want the stamp to prove it.
"""
import sys
import time
from trlink import Link, screen_rows, KEYBUF, KEYCOUNT, PETSCII_Y

if len(sys.argv) < 2:
    raise SystemExit(__doc__)
local = sys.argv[1]
remote = sys.argv[2] if len(sys.argv) > 2 else '/fwupdate.hex'

with Link() as tr:
    tr.drain()
    tr.post(local, remote)
    tr.launch(remote)
    print('launched; waiting for the C64 confirm prompt')

    end = time.time() + 30
    while time.time() < end:
        try:
            rows = screen_rows(tr.screen())
        except SystemExit:
            rows = []
        if 'Y/N' in ''.join(rows):
            print('prompt is up; answering Y')
            tr.poke(KEYBUF, [PETSCII_Y])
            tr.poke(KEYCOUNT, [1])
            break
        time.sleep(1)
    else:
        print('prompt never appeared:')
        print('\n'.join(rows[:8]))
        sys.exit(1)

    print('--- updater serial ---')
    if tr.stream(180):
        print('\n[port dropped -- rebooting]')
