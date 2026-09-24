#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reflash the cartridge with nobody at it.   fwupdate.py <hex> [remote path, default /fwupdate.hex]

Pushes the hex to the SD card, launches it (a .hex runs TeensyROM's own SD
updater, the supported path), waits for the C64's "UPDATE TEENSYROM FIRMWARE?
Y/N" prompt, answers Y by writing to the keyboard buffer, then echoes the
updater's serial output until the board reboots. No program button needed;
docs/General_Usage.md has the routes that want one.

Finally it asks the rebooted board which image it came up as and compares its
banner against the build stamp compiled into the hex; the minimal image can
carry the same stamp, so the check is the image as well as the stamp. A file
that is not Intel HEX at all is refused before anything is pushed; a hex this
reader cannot fully decode, or one without a single stamp, is flashed and the
board is still checked for the main image, without the stamp comparison.

Needs the main image running and a hex built for THIS cartridge (--target
tr-plus for a TR+); the updater refuses a mismatched target string.

The stamp comes from SOURCE_DATE_EPOCH, which the build sets to the HEAD
commit time unless the environment already holds one, so it identifies a
commit rather than a build: commit before building if you want it to tell two
runs apart.
"""
import sys
import time

from c64 import KEYBUF, KEYCOUNT, PETSCII_Y, screen_rows
from hexfile import UnsupportedRecord, build_stamp
from protocol import FW_FULL, IMAGES
from trlink import Link, answering_board, neighbours

CONFIRM_TIMEOUT, UPDATE_TIMEOUT, REBOOT_TIMEOUT = 30, 180, 90
MAIN_IMAGE = IMAGES[FW_FULL]
SILENT = 'silent -- nothing answered the firmware check'


def wanted_stamp(path):
    """The banner line a board running this hex should print, and when there is
    none, why. Raises ValueError when the file does not read as Intel HEX."""
    try:
        stamp = build_stamp(path)
    except UnsupportedRecord as problem:
        return None, str(problem)
    if not stamp:
        return None, 'no single build stamp in the main image'
    return f'{stamp[0]}, {stamp[1]}', ''


if len(sys.argv) < 2:
    raise SystemExit(__doc__)
local = sys.argv[1]
remote = sys.argv[2] if len(sys.argv) > 2 else '/fwupdate.hex'

try:
    expected, why_not = wanted_stamp(local)
except ValueError as problem:
    raise SystemExit(f'{local}: {problem}; this is not a firmware hex')
print(f'{local}: build stamp {expected}' if expected else
      f'{local}: {why_not}; the stamp cannot be checked')

with Link() as tr:
    port = tr.port
    known = neighbours(port)
    tr.drain()
    tr.post(local, remote)
    tr.launch(remote)
    print('launched; waiting for the C64 confirm prompt')

    end = time.time() + CONFIRM_TIMEOUT
    while time.time() < end:
        try:
            rows = screen_rows(tr.screen())
        except SystemExit:
            rows = []
        # Case-folded: which case these glyphs carry depends on where the VIC is
        # pointed, and the updater is a launched program that need not be in the
        # charset the menu left behind. 'Y/N' and 'y/n' are the same prompt.
        if 'y/n' in ''.join(rows).lower():
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
    if not tr.stream(UPDATE_TIMEOUT):
        print('\nthe updater never rebooted the board')
        sys.exit(1)
    print('\n[port dropped -- rebooting]')

tr, image = answering_board(time.time() + REBOOT_TIMEOUT, port, known)
if tr is None:
    raise SystemExit('the board never came back on USB; check it with probe.py')
banner = tr.version() if image else ''
tr.close()

print(f'\nimage  {image or SILENT}')
print(banner)
if image != MAIN_IMAGE:
    raise SystemExit(f'FAILED: the board came back as "{image or SILENT}" rather than '
                     f'the main image, so the update did not take')
if not expected:
    print('OK: the board came back on the main image; the stamp went unchecked')
elif expected not in banner:
    raise SystemExit(f'FAILED: the board does not report "{expected}"; it is still '
                     f'running the firmware above, so the update did not take')
else:
    print(f'OK: the main image reports the build stamp from {local}')
