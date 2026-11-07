#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Record C64 bus cycles on chosen pages, and take them back.   Needs an image built with DbgSnoopTap.

  snooptap.py arm PAGE:FLAGS[,PAGE:FLAGS...]   e.g. arm 03:5,df:3
  snooptap.py take                             print the ring, keep recording
  snooptap.py stop                             print the ring, stop recording

PAGE is the high byte of the address, in hex. FLAGS add up: 1 writes, 2 reads,
4 trigger (the first write to the page freezes the ring 64 entries later, so what
led up to it survives), 8 slice marks (each DMA slice start is recorded too).

The port can only be held by one program, so arm, close, run whatever is under
test, then take once it has let go of the port. The ring keeps the last 1,024
entries.
"""
import struct
import sys

from protocol import SNOOP_TAP, to_board
from trlink import Link

ENTRY = struct.Struct('<IHBB')
HEADER = struct.Struct('<IIIIBH')
MODES = {'take': 0, 'arm': 1, 'stop': 2}
# Common_Defs.h enum DMA_States, in order.
DMA_STATES = ('DisableReady', 'ActiveReady', 'TransferReady', 'TransferExecuting',
              'TransferComplete', 'BeginStartStates', 'StartDisable', 'StartAsynch',
              'StartAsynch_Wait_LRd', 'StartAsynch_Wait_LWr', 'StartImmediate')
INFO_STATE, INFO_LOCK_ARMED, INFO_READ, INFO_SLICE = 0x1F, 0x20, 0x40, 0x80


def pages_from(spec):
    pages = bytearray(256)
    for item in spec.split(','):
        page, flags = item.split(':')
        pages[int(page, 16)] = int(flags)
    return bytes(pages)


def take(tr, mode, pages=b''):
    tr.drain(0.3)
    tr.wr(to_board(SNOOP_TAP) + bytes([mode]) + pages)
    tr.ack('SnoopTap')
    head = tr.rd(HEADER.size)
    if len(head) != HEADER.size:
        raise SystemExit('SnoopTap: short header')
    hz, total, skipped, taps, triggered, count = HEADER.unpack(head)
    body = tr.rd(ENTRY.size * count)
    if len(body) != ENTRY.size * count:
        raise SystemExit(f'SnoopTap: {len(body) // ENTRY.size} of {count} entries arrived')
    return (hz, total, skipped, taps, triggered), list(ENTRY.iter_unpack(body))


def describe(entry, first, prev, hz):
    cyc, addr, data, info = entry
    at = ((cyc - first) & 0xFFFFFFFF) * 1e6 / hz
    step = ((cyc - prev) & 0xFFFFFFFF) * 1e6 / hz
    when = f'{at:12.2f} {step:+9.2f}  '
    if info & INFO_SLICE:
        return when + f'SLICE ${addr:04X} length low byte ${data:02X}'
    state = info & INFO_STATE
    name = DMA_STATES[state] if state < len(DMA_STATES) else str(state)
    access = f'R ${addr:04X}        ' if info & INFO_READ else f'W ${addr:04X} = ${data:02X}'
    return when + f'{access}  dma={name}' + (' lock-armed' if info & INFO_LOCK_ARMED else '')


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in MODES or (sys.argv[1] == 'arm') != (len(sys.argv) == 3):
        raise SystemExit(__doc__)
    mode = MODES[sys.argv[1]]
    with Link() as tr:
        (hz, total, skipped, taps, triggered), entries = take(
            tr, mode, pages_from(sys.argv[2]) if mode == 1 else b'')
    print(f'recorded {total}, skipped {skipped}, page cycles {taps}, '
          f'triggered {bool(triggered)}, sent {len(entries)}, ARM {hz} Hz')
    if mode == 1:
        return
    print('          us   step us')
    prev = first = entries[0][0] if entries else 0
    for entry in entries:
        print(describe(entry, first, prev, hz))
        prev = entry[0]


if __name__ == '__main__':
    main()
