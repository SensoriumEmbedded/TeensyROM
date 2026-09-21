#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Is the main image alive? Sends 'b' (BusAnalysis, read-only) and prints the reply.

No reply means the port is silent: the extension image (USB disabled), the
minimal image, or a hung board. Serial-number-named devices are not the main image.
"""
import io
from trlink import Link

with Link() as tr:
    tr.wr(b'b')
    out = io.StringIO()
    tr.stream(6, out)
print(out.getvalue() or '(no response)')
