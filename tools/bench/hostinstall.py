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

Past the refusal point the reboot is not the outcome either: an erase, verify or program
that fails ($31-$34, $3f) reboots exactly as a good install does. Only the record saying
the host was installed is a pass here; anything else exits non-zero -- including a record
that never arrives, which is an outcome nobody saw rather than a failure anybody did.

Build the package with:  node tools/build-host-package.mjs --hex <firmware.hex>
Remove one with:         hostuninstall.py
"""
import os
import sys

from hostops import INSTALLED, install_host, show

args = [a for a in sys.argv[1:] if not a.startswith('--')]
if not 1 <= len(args) <= 2:
    raise SystemExit(__doc__)

result = install_host(args[0], args[1] if len(args) > 1 else None)
if not result.rebooted:
    print('\nthe board is still up, so the package was refused before anything was erased:')
    show(result.screen)
    sys.exit(1)
if not result.said(INSTALLED):
    print(f'\nthe board rebooted but never said {INSTALLED!r}, so the install did not '
          'report success. A record saying so names which step failed; no readable '
          'record means the outcome was not observed. Either way the slot was erased '
          'and nothing here has seen a host written to it. What the board did say:')
    show(result.screen)
    sys.exit(1)
if result.image is None:
    print('nothing answered the firmware check; the boot output above is all there is')
print('\n--- screen ---')
show(result.screen)
