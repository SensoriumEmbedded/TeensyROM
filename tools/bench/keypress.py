#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Put a key in the C64 keyboard buffer by DMA, so the next GETIN returns it.

  keypress.py [code]      PETSCII/keyboard code, default 0x59 ('Y'). F1/F3/F5/F7 are 0x85..0x88.

The menu only reads firmware messages while it sits in a WaitForTR* loop, so a
keypress that starts a menu operation (F5 switches to the SD drive) is how you
make a pending message appear.
"""
import sys
from trlink import Link, PETSCII_Y

code = int(sys.argv[1], 0) if len(sys.argv) > 1 else PETSCII_Y
with Link() as tr:
    ok = tr.key(code)
print(f'key 0x{code:02X}: {"queued" if ok else "NOT acknowledged"}')
sys.exit(0 if ok else 1)
