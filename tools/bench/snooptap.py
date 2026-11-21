#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Record C64 bus cycles on chosen pages, and take them back.   Needs an image built with DbgSnoopTap.

  snooptap.py arm PAGE:FLAGS[,PAGE:FLAGS...]   e.g. arm 03:5,df:3
  snooptap.py take                             print the ring, keep recording
  snooptap.py stop                             print the ring, stop recording

PAGE is the high byte of the address, in hex. FLAGS add up: 1 writes, 2 reads,
4 trigger (only with 1: the first write to the page freezes the ring 64 entries
later, so what led up to it survives), 8 slice marks (each DMA slice start is recorded too),
16 bus timing (on any page: isrPHI2 entries more than a PAL cycle after the last while the
CPU owns the bus, with the interrupted PC; IO decodes reached after Phi2 fell; and every DMA
assert, with why and on which cycle), 32 cycle timing (each of the page's recorded cycles
gets how long after its entry isrPHI2 read GPIO6, and how long after that it read GPIO9,
drove and released the data or sampled it, and finished the handlers). 16 and 32 need an image for a TR+.

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
# DMAControl.c's timing record kinds: no access or slice mark sets both top bits.
TIMING = 0xC0
LATE_ENTRY, LATE_ENTRY_PC, ASSERT, ASSERT_DETAIL, LATE_DECODE, LATE_DECODE_PC, CYCLE, CYCLE_DETAIL = range(0xC0, 0xC8)
ASSERT_REASONS = ('bad line read', 'read after a write', 'read loop timeout', 'immediate')
NOT_THIS_CYCLE = 0x7FFF


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


def state_name(state):
    return DMA_STATES[state] if state < len(DMA_STATES) else str(state)


def access(addr, data, read):
    return f'R ${addr:04X}        ' if read else f'W ${addr:04X} = ${data:02X}'


def decode(entries, hz):
    """The ring as (ARM cycle stamp, text) rows, oldest first. A timing record and its detail
    record make one row; a cycle timing row takes its access row's stamp, or None when the
    ring overwrote that row."""
    rows, i = [], 0
    while i < len(entries):
        cyc, addr, data, info = entries[i]
        detail = entries[i + 1] if i + 1 < len(entries) and entries[i + 1][3] == info + 1 else None
        if info in (LATE_ENTRY, ASSERT, LATE_DECODE) and detail:
            d_cyc, d_addr, d_data, _ = detail
            if info == LATE_ENTRY:
                text = (f'LATE ENTRY {addr} nS after the last  dma={state_name(data)}  '
                        f'interrupted pc=0x{d_cyc:08X} exception {d_addr} basepri=0x{d_data:02X}')
            elif info == ASSERT:
                text = (f'DMA ASSERT, {ASSERT_REASONS[data & 3]}: on {access(addr, 0, data & INFO_READ).rstrip()}'
                        f' (before it {"R" if data & 0x20 else "W"} ${d_addr:04X}); Phi2 fell {d_cyc >> 16} nS '
                        f'after entry, assert {d_cyc & 0xFFFF} nS after that; entry {d_data * 16} nS after the last')
            else:
                text = (f'LATE DECODE {access(addr, 0, data & INFO_READ).rstrip()}  dma={state_name(data & INFO_STATE)}'
                        f'  decoded {d_data * 16} nS after entry, entry {d_addr} nS after the last, '
                        f'interrupted pc=0x{d_cyc:08X}')
            rows.append((cyc, text))
            i += 2
        elif info == CYCLE and detail:
            tap, gap = detail[0], detail[1]
            at = addr & 0x7FFF
            port = ('port not driven or sampled this cycle' if at == NOT_THIS_CYCLE else
                    f'data sampled +{at}' if data == 0xFF else
                    f'data driven +{at}, released +{data * 4}' + (' (Phi2 fell first)' if addr & 0x8000 else ''))
            rows.append((rows[-1][0] if rows else None,
                         f'    GPIO6 read {cyc >> 16} nS after entry, then GPIO9 +{cyc & 0xFFFF}, {port}, '
                         f'handlers done +{tap} nS; entry {gap} nS after the last'))
            i += 2
        elif info & TIMING == TIMING:
            rows.append((cyc, f'timing record 0x{info:02X} without its detail' if info % 2 == 0 else
                              f'timing detail 0x{info:02X} without its record'))
            i += 1
        elif info & INFO_SLICE:
            rows.append((cyc, f'SLICE ${addr:04X} length low byte ${data:02X}'))
            i += 1
        else:
            rows.append((cyc, f'{access(addr, data, info & INFO_READ)}  dma={state_name(info & INFO_STATE)}'
                              + (' lock-armed' if info & INFO_LOCK_ARMED else '')))
            i += 1
    return rows


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
    rows = decode(entries, hz)
    prev = first = next((cyc for cyc, _ in rows if cyc is not None), 0)
    for cyc, text in rows:
        if cyc is None:
            print(f'{"":22}  {text}')
            continue
        at = ((cyc - first) & 0xFFFFFFFF) * 1e6 / hz
        step = ((cyc - prev) & 0xFFFFFFFF) * 1e6 / hz
        print(f'{at:12.2f} {step:+9.2f}  {text}')
        prev = cyc
    kinds = [text.split(',')[0].split('  ')[0] for _, text in rows]
    print(f'late entries {sum(k.startswith("LATE ENTRY") for k in kinds)}, '
          f'late decodes {sum(k.startswith("LATE DECODE") for k in kinds)}, '
          f'DMA asserts {sum(k.startswith("DMA ASSERT") for k in kinds)}')


if __name__ == '__main__':
    main()
