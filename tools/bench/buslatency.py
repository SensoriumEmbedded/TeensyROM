#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""How long a GPIO read and a USB controller register access take on the board, and how
long a GPIO read waits behind a USB access.   buslatency.py

The USB controller's registers share the CPU's peripheral port with GPIO6-9, which
isrPHI2 reads every cycle, so a USB access still in flight when Phi2 rises holds that
read back, and how long that wait can be sets how long after a Phi2 rise a USB
access has to hold off. This times each access on its own and
in the pairs that matter, from the board's cycle counter, with the USB link quiet: the
command's own reply is the only traffic. Needs an image built with Dbg_SerBusLat in
TeensyROM.h. The C64 can be at the menu: nothing is masked, and each sample starts just
after a Phi2 rise.
"""
import re
from trlink import Link

ROW = re.compile(r'^(?P<name>.+?)\s+(?P<kept>\d+) kept\s+(?P<gave_up>\d+) gave up(?:\s+min\s+(?P<min>\d+)\s+max\s+(?P<max>\d+)'
                 r'\s+\((?P<min_ns>\d+)\.\.(?P<max_ns>\d+) nS\))?\s*$')
HEADER = 'Bus latency,'
# BusLatency()'s Names[] in BusSnoop.ino, in its order
PROBES = ('nothing (counter reads and DSB)', 'GPIO6 read', 'USB1 USBSTS read', 'USB1 ENDPTPRIME read',
          'GPIO6 read after a USBSTS read', 'GPIO6 read after a USB1 write')


def parse(text):
    """The rows of a 'U' reply: {probe: (samples kept, tries that gave up waiting for a window
    to sample in, min cycles, max cycles, min nS, max nS)}, with None for the timings of a
    probe that kept no sample. Tries neither kept nor given up had an exception land in them."""
    if HEADER not in text:
        return None
    rows = {}
    for line in text[text.index(HEADER):].splitlines()[1:]:
        m = ROW.match(line.strip())
        if m:
            timings = (None,) * 4 if m['min'] is None else tuple(int(m[k]) for k in ('min', 'max', 'min_ns', 'max_ns'))
            rows[m['name']] = (int(m['kept']), int(m['gave_up'])) + timings
    return rows


def verdict(rows):
    """Why the reply fails, or None when every probe has a row that kept samples."""
    missing = [name for name in PROBES if name not in rows]
    if missing:
        return 'FAILED: the reply ended before ' + ', '.join(missing)
    empty = [f'{name} ({rows[name][1]} gave up waiting for a window to sample in, the rest were interrupted)'
             for name in PROBES if rows[name][0] == 0]
    if empty:
        return 'FAILED: no sample kept for ' + ', '.join(empty)
    return None


def main():
    with Link() as tr:
        tr.drain(0.2)
        tr.wr(b'U')
        text = tr.text(idle=0.5, timeout=5)
    rows = parse(text)
    if rows is None:
        raise SystemExit('no bus latency reply: this image was built without Dbg_SerBusLat')
    print(text[text.index(HEADER):].rstrip())
    problem = verdict(rows)
    if problem:
        raise SystemExit(problem)


if __name__ == '__main__':
    main()
