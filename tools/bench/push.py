#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Copy files to the SD card over USB serial.   push.py <local>=<remote> [<local>=<remote> ...]

  push.py build/extensions/HELLO.crt=/HELLO.crt build/extensions/VMS/HELLO/engine.mvm=/VMS/HELLO/engine.mvm

Deletes the target first (the firmware will not overwrite). About 10 s for 6.7 MB.
"""
import sys
from trlink import Link

if len(sys.argv) < 2:
    raise SystemExit(__doc__)
with Link() as tr:
    for arg in sys.argv[1:]:
        local, sep, remote = arg.partition('=')
        if not sep:
            raise SystemExit(f'expected <local>=<remote>, got {arg!r}')
        tr.post(local, remote)
print('done')
