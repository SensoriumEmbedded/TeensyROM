#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Launch an extension and report how it ended.   exttest.py <path on SD> [--no-f5]

  exttest.py /HELLO.crt

Three outcomes, told apart by the serial port:

  * The port stays up and silent: the extension image is running. It is built
    USB_DISABLED, so silence is success, not a hang. Look at the C64.
  * The port drops and comes back: the extension image failed or exited and
    reset into the main image. Its serial boot output carries the failure record the
    loader left behind, which is printed here.
  * The port never drops and never speaks and the C64 did not change: the
    launch did nothing.

After a reset it presses F5 by DMA. That switches the menu to the SD drive, which
makes the C64 ask the firmware for a directory, and that -- a WaitForTR* loop --
is the only time it reads the message the loader left. Then it prints the screen.
"""
import sys
import time
from c64 import F5, screen_rows
from trlink import Link, neighbours, reconnect

args = [a for a in sys.argv[1:] if not a.startswith('--')]
if len(args) != 1:
    raise SystemExit(__doc__)
path = args[0]
press_f5 = '--no-f5' not in sys.argv


def show(tr):
    for n, line in enumerate(screen_rows(tr.screen())):
        if line.strip():
            print(f'{n:2d} |{line}|')


tr = Link()
tr.drain()
tr.launch(path)
print(f'launched {path}; watching for the jump into the extension image')
dropped = tr.stream(25)
tr.close()

if not dropped:
    print('\nport stayed up: the extension image is running (or nothing launched).')
    sys.exit(0)

print('\n[port dropped]')
# A crash lands in minimal before minimal's own boot check hands off to main --
# a second, later re-enumeration under its own new name. The same pattern as
# fwupdate.py's answering_board(): if stream() reports the port dying again
# mid-boot, that was minimal, not the image we actually want; go back for a
# fresh reconnect() rather than act on a handle that already stopped talking.
port, known = None, None
deadline = time.time() + 75
tr = None
while time.time() < deadline:
    tr = reconnect(timeout=deadline - time.time(), port=port, known=known)
    if tr is None:
        raise SystemExit('board never came back')
    print(f'[reattached] port={tr.port} -- boot output:')
    if not tr.stream(15):
        break
    print('  died again mid-boot -- that was minimal handing off; reconnecting')
    port, known = tr.port, neighbours(tr.port)
    tr.close()
else:
    raise SystemExit('board kept re-enumerating without ever settling')

print('\n--- screen after the reboot ---')
show(tr)
if press_f5:
    print('\n--- F5, to put the C64 in a wait loop ---')
    tr.key(F5)

    time.sleep(3)
    show(tr)
tr.close()
