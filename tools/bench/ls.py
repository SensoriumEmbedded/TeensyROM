#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""List a directory on the board.   ls.py [path] [drive]

  ls.py /VMS/HELLO        the SD card, which is drive 1 and the default
  ls.py / 0               the USB drive; 2 is the built-in menu

Useful right after push.py, to see that a file landed where it was aimed.
"""
import sys
from protocol import DRIVE_SD
from trlink import LISTING_PAGE_SIZE, Link, drive_number

path = sys.argv[1] if len(sys.argv) > 1 else '/'
drive = drive_number(sys.argv[2]) if len(sys.argv) > 2 else DRIVE_SD
with Link() as tr:
    entries = tr.listdir(path, drive)
for entry in entries:
    size = '<dir>' if entry['type'] == 'dir' else entry['size']
    print(f'{size:>10}  {entry["name"]}')
more = ' (a full page -- there may be more)' if len(entries) == LISTING_PAGE_SIZE else ''
print(f'{len(entries)} entries in {path}{more}')
