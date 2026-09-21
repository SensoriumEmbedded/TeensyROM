#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Print the C64 text screen ($0400, 25 x 40) by DMA."""
from trlink import Link, screen_rows

with Link() as tr:
    rows = screen_rows(tr.screen())
for n, line in enumerate(rows):
    print(f'{n:2d} |{line}|')
