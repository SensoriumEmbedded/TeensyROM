#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Echo the board's serial output.   listen.py [seconds] [hex bytes to send first]

  listen.py 10            watch for 10 s (boot banner, CrashReport, updater output)
  listen.py 6 62          send 'b' (BusAnalysis, a read-only liveness probe) and print the reply
"""
import sys
from trlink import Link

secs = float(sys.argv[1]) if len(sys.argv) > 1 else 10
with Link(settle=0.4) as tr:
    if len(sys.argv) > 2:
        tr.wr(bytes.fromhex(sys.argv[2]))
    if tr.stream(secs):
        print('\n[port dropped]')
