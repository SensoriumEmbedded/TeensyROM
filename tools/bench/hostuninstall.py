#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Remove the installed extension host, over USB.   hostuninstall.py

Removal is the install's own first step on its own: clear the 4-byte tag so the slot stops
reading as a host. The payload stays in flash, unreferenced, until the next install
overwrites it.

The firmware ACKs and flushes before it starts, because clearing the tag takes a sector
erase it does not return from -- so the ACK means accepted, not done. Two things can
follow it:

  * the port drops and the board reboots: the slot was cleared, and the record on the way
    back up says so;
  * the board stays up: nothing was installed, and the C64 says so.

Unlike launching an extension, none of this enters the extension image, so no part of it
needs a hand on the board.
"""
import sys

from hostops import remove_host, show

if [a for a in sys.argv[1:] if not a.startswith('--')]:
    raise SystemExit(__doc__)

result = remove_host()
if not result.rebooted:
    print('\nthe board is still up, so there was nothing installed:')
    show(result.screen)
    sys.exit(1)
if result.image is None:
    print('nothing answered the firmware check; the boot output above is all there is')
print('\n--- screen ---')
show(result.screen)
