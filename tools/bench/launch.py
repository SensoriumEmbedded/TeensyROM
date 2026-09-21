#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Launch a file from the SD card, then echo serial.   launch.py <path> [drive] [seconds]

Drive 1 is the SD card. What the file is decides what happens: a .hex runs the
firmware updater, a .crt or .prg runs on the C64, an extension launcher hands
over to the extension image (after which the port drops and stays silent).
"""
import sys
from trlink import Link, DRIVE_SD

if len(sys.argv) < 2:
    raise SystemExit(__doc__)
path = sys.argv[1]
drive = int(sys.argv[2]) if len(sys.argv) > 2 else DRIVE_SD
secs = float(sys.argv[3]) if len(sys.argv) > 3 else 20
with Link() as tr:
    tr.drain()
    tr.launch(path, drive)
    print(f'launched {path}')
    if tr.stream(secs):
        print('\n[port gone]')
