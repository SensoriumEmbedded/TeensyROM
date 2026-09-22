#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Which image is running on the board, and what it was built from.

Asks FWCheckToken, which both the main and the minimal image answer whatever
else they are doing, then prints the build banner. Silence means the extension
image (built with USB disabled) or a hung board, and exits non-zero.

The port name is a second opinion: the main image renames its USB device, so it
enumerates as usbmodem2101 where the other two use the Teensy's serial number.
"""
import sys
from trlink import Link

with Link() as tr:
    image = tr.fwcheck()
    print(f'port   {tr.port}')
    print(f'image  {image or "silent -- the extension image, or a hung board"}')
    if image:
        print(tr.version())
sys.exit(0 if image else 1)
