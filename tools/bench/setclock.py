#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Set the TR's real-time clock.   setclock.py [UTC time]

The time is UTC, to the second: this computer's clock, or an ISO 8601 time
such as 2026-10-16T17:00:00. The board refuses it with "Busy!" unless it is
sitting at its own menu.
"""
import datetime, sys
from protocol import SET_RTC, to_board
from trlink import Link

if len(sys.argv) > 1:
    when = datetime.datetime.fromisoformat(sys.argv[1])
    if when.tzinfo is None:
        when = when.replace(tzinfo=datetime.timezone.utc)
    when = when.astimezone(datetime.timezone.utc)
else:
    when = datetime.datetime.now(datetime.timezone.utc)
secs = int(when.timestamp())
if not 0 <= secs < 1 << 32:
    raise SystemExit(f'{when:%Y-%m-%d %H:%M:%S} UTC is outside 1970-2106, the range four bytes of seconds hold')
with Link() as tr:
    tr.drain(0.3)
    tr.wr(to_board(SET_RTC) + to_board(secs, 4))
    tr.ack('set clock')
print('clock set to', when.strftime('%Y-%m-%d %H:%M:%S UTC'))
