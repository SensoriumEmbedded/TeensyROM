#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reset the C64 back to the TeensyROM menu.   reset.py

Also the way out of the minimal image: the reset returns the board to the main
one, which re-enumerates USB under a different name. Every command the firmware
refuses with "Busy!" becomes available again afterwards.
"""
from trlink import Link

with Link() as tr:
    print(tr.reset() or '(no reply -- the port may have dropped, which a reset can do)')
