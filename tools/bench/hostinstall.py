#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Install an extension host from a .TRH package, over USB.   hostinstall.py <local.trh> [remote]

  hostinstall.py build/firmware/TEENSYROM.TRH

Pushes the package to the SD card and launches it, which is the same path as selecting it
in the menu. Validation reads only, so a package this firmware will not take comes back as
a refusal with the C64 still running and nothing erased. Past that point the board holds
the 6510 in reset, blanks the screen, rewrites the slot and reboots -- up to 45 seconds
during which it must not lose power.

The install reports either way. Its outcome rides in the VmFail record the main image
collects on the way back up, so this waits for the board to re-enumerate and echoes that
boot output. A refusal never reboots, so a board still answering after the launch is the
refusal case, and the C64 screen carries the reason.

Build the package with:  node tools/build-host-package.mjs --hex <firmware.hex>
"""
import os
import sys
import time

from c64 import screen_rows
from trlink import Link, answering_board, neighbours

REBOOT_TIMEOUT = 120   # the erase alone is allowed 45 s, and the boot follows it

args = [a for a in sys.argv[1:] if not a.startswith('--')]
if not 1 <= len(args) <= 2:
    raise SystemExit(__doc__)
local = args[0]
remote = args[1] if len(args) > 1 else '/' + os.path.basename(local).upper()

if not local.lower().endswith('.trh'):
    raise SystemExit(f'{local}: not a .TRH package -- the firmware routes on the extension')
size = os.path.getsize(local)
if size < 64:
    raise SystemExit(f'{local}: {size} bytes, too short to hold a TRH1 header')


def show(tr):
    for n, line in enumerate(screen_rows(tr.screen())):
        if line.strip():
            print(f'{n:2d} |{line}|')


tr = Link()
# Sampled before the install: once the board goes, there is nothing to ask.
port = tr.port
known = neighbours(port)
tr.drain()
tr.post(local, remote)
tr.launch(remote)
print(f'launched {remote}; the board reboots if it got as far as writing')

# A refusal leaves the port up; an install takes it down with the reboot.
dropped = tr.stream(60)
if not dropped:
    print('\nthe board is still up, so the package was refused before anything was erased:')
    show(tr)
    tr.close()
    sys.exit(1)

print('\n[port dropped -- installing, do not power off]')
tr.close()

# Not a bare reconnect(): the main image renames its USB device as it comes up, so the
# first node to appear is one that is about to disappear again.
tr, image = answering_board(time.time() + REBOOT_TIMEOUT, port, known)
if tr is None:
    raise SystemExit(f'the board did not come back within {REBOOT_TIMEOUT} s; '
                     'check it before power cycling')
if image is None:
    print('nothing answered the firmware check; the boot output above is all there is')

print('\n--- screen ---')
show(tr)
tr.close()
