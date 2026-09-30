#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""WriteC64Spans against a real TR+: whether slices land intact, what one costs, and
what slicing saves a C64 that counts timer NMIs.   spans.py [NMI period in cycles]

Needs acme on PATH and the C64 at the menu. Loads spanclient.a (IRQs off, a loop
that writes or only reads, and an NMI that counts), measures, then resets the C64.
"""
import os, statistics, subprocess, sys, tempfile, time
from protocol import ACK, DRIVE_SD, READ_C64_MEM, WRITE_C64_MEM, WRITE_C64_SPANS, to_board
from trlink import Link

MODE, TICKS, READY, PROBE = 0xC100, 0xC101, 0xC104, 0xC105
D011, CIA2_TA, CIA2_ICR, CIA2_CRA = 0xD011, 0xDD04, 0xDD0D, 0xDD0E
BLOCK = 0x4000
PERIOD = int(sys.argv[1]) if len(sys.argv) > 1 else 200
failed = []


def spans(tr, runs, payload, slice_bytes=0, gap_us=0, flags=0):
    """One job: (acked, reply text, seconds from send to the final reply)."""
    began = time.perf_counter()
    tr.wr(to_board(WRITE_C64_SPANS) + bytes([flags, slice_bytes, gap_us, len(runs)])
          + b''.join(to_board(addr) + to_board(length) for addr, length in runs))
    value, text = tr.status(10)
    if value == ACK:
        tr.wr(payload)
        value, text = tr.status(30)
    return value == ACK, text, time.perf_counter() - began


def job(tr, *args, **kw):
    ok, text, secs = spans(tr, *args, **kw)
    if not ok:
        failed.append(text or 'no reply')
        raise SystemExit(f'WriteC64Spans refused: {text or "no reply -- is the command in this firmware?"}')
    return secs


def ticks(tr):
    """The NMI count and when it was read. Not peek(): its 0.3 s drain would sit between the two."""
    tr.wr(to_board(READ_C64_MEM) + to_board(TICKS) + to_board(2))
    value, _ = tr.status(6)
    low, high = tr.rd(2, 5)
    if value != ACK:
        raise SystemExit('NMI count read refused')
    return low | high << 8, time.perf_counter()


def start_client(tr):
    here = os.path.dirname(os.path.abspath(__file__))
    with tempfile.TemporaryDirectory() as work:
        subprocess.run(['acme', '-f', 'cbm', '-o', os.path.join(work, 'spanclient.prg'),
                        os.path.join(here, 'spanclient.a')], check=True, cwd=work)
        tr.post(os.path.join(work, 'spanclient.prg'), '/SPANCLNT.PRG')
    tr.poke(READY, [0])
    tr.launch('/SPANCLNT.PRG', DRIVE_SD)
    end = time.time() + 15
    while time.time() < end:
        time.sleep(0.5)
        try:
            if tr.peek(READY, 1)[0] == 0x5A:
                return
        except SystemExit:
            pass
    raise SystemExit('the client never reported ready')


def integrity(tr):
    runs = [(BLOCK, 1000), (0x6000, 777), (0x7F00, 256)]
    total = sum(length for _, length in runs)
    for slice_bytes in (0, 2, 7, 64, 255):
        payload = bytes((0x55, 0xAA)[n & 1] ^ (n >> 1 & 0xFF) ^ slice_bytes for n in range(total))
        job(tr, runs, payload, slice_bytes)
        back = b""
        for addr, length in runs:
            back += tr.peek(addr, length)
        good = back == payload
        if not good:
            failed.append(f'slice {slice_bytes}: read back differs')
        print(f'  slice {slice_bytes:3d}: {total} bytes over 3 spans {"intact" if good else "CORRUPT"}')
    ok, text, _ = spans(tr, runs, bytes(total), 1)
    answered = tr.peek(READY, 1)[0] == 0x5A
    if ok or not answered:
        failed.append('2033 one-byte slices were not refused cleanly')
    print(f'  slice   1: refused before the payload ("{text}"), board {"answering" if answered else "SILENT"}')
    # A refused span list must be discarded, not parsed: this one holds a whole WriteC64Mem of $77 to PROBE.
    tr.poke(PROBE, [0])
    trap = [(WRITE_C64_MEM, PROBE), (1, 0x7700)] + [(0x0400, 1)] * 62
    ok, text, _ = spans(tr, trap, b'', flags=1)
    time.sleep(0.5)
    clean = not ok and tr.peek(PROBE, 1)[0] == 0
    if not clean:
        failed.append('a refused span list was parsed as a command')
    print(f'  flags 1: refused ("{text}"), its span list {"discarded" if clean else "PARSED AS A COMMAND"}')


def per_slice(tr, label):
    payload = bytes(range(256)) * 16
    points = []
    for slice_bytes in (255, 128, 64, 32, 16):
        secs = statistics.median(job(tr, [(BLOCK, len(payload))], payload, slice_bytes) for _ in range(5))
        points.append((-(-len(payload) // slice_bytes), secs))
    n = len(points)
    mx, my = sum(x for x, _ in points) / n, sum(y for _, y in points) / n
    slope = sum((x - mx) * (y - my) for x, y in points) / sum((x - mx) ** 2 for x, _ in points)
    print(f'  {label:32s} {slope * 1e6:8.1f} uS per slice  (4 KiB in 16..256 slices, gap 0)')


def nmi_loss(tr, gap_us):
    latch = PERIOD - 1
    tr.poke(CIA2_TA, [latch & 0xFF, latch >> 8])
    tr.poke(CIA2_ICR, [0x81])
    tr.poke(CIA2_CRA, [0x11])
    tr.drain(0.3)
    first, began = ticks(tr)
    time.sleep(2)
    last, ended = ticks(tr)
    rate = ((last - first) & 0xFFFF) / (ended - began)
    print(f'  NMI every {PERIOD} cycles: {rate:.0f}/s at rest; gap {gap_us} uS between slices')
    payload = bytes(4096)
    for slice_bytes in (0, 255, PERIOD, PERIOD - 20, PERIOD - 60, PERIOD // 2, 32):
        if not 0 <= slice_bytes <= 255:
            continue
        jobs, (first, began) = 0, ticks(tr)
        while time.perf_counter() - began < 1.5:
            job(tr, [(BLOCK, len(payload))], payload, slice_bytes, gap_us)
            jobs += 1
        last, ended = ticks(tr)
        lost = rate * (ended - began) - ((last - first) & 0xFFFF)
        print(f'    slice {slice_bytes or "whole":>5}: {lost / jobs:6.1f} NMIs lost per 4 KiB job ({jobs} jobs)')
    tr.poke(CIA2_ICR, [0x7F])
    tr.poke(CIA2_CRA, [0])


with Link() as tr:
    start_client(tr)
    print('Integrity across slice boundaries, screen on, loop writing:')
    integrity(tr)
    print('What a slice costs:')
    per_slice(tr, 'loop writing, screen on')
    tr.poke(MODE, [1])
    per_slice(tr, 'loop only reading, screen on')
    tr.poke(D011, [0x0B])
    per_slice(tr, 'loop only reading, screen blanked')
    tr.poke(D011, [0x1B])
    tr.poke(MODE, [0])
    print('Timer NMIs lost to halts, loop writing:')
    nmi_loss(tr, 40)
    tr.reset()

if failed:
    raise SystemExit('FAILED: ' + '; '.join(failed))
