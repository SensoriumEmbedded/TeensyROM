#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Hex dump C64 memory by DMA.   peek.py <hex address> <length>     e.g. peek.py FF00 32"""
import sys
from trlink import Link

if len(sys.argv) != 3:
    raise SystemExit(__doc__)
addr, length = int(sys.argv[1], 16), int(sys.argv[2])
with Link() as tr:
    data = tr.peek(addr, length)
    print(f'port {tr.port}  ${addr:04X}  {len(data)} bytes')
for off in range(0, len(data), 16):
    print(f'${addr + off:04X}  ' + ' '.join(f'{b:02X}' for b in data[off:off + 16]))
