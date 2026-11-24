#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""IO2 reads beside host DMA jobs: how many of the C64's reads of the REU's registers come
back wrong while the host writes C64 memory.
    reuhammer.py [--quiet SECS] [--jobs SECS] [--sweep] [--tap PAGE:FLAGS,...] [MODE ...]

Needs Special IO set to an REU, acme on PATH and the C64 at the menu. Runs reuhammer.a,
which writes $DF02-$DF08 once and reads them back, one IO2 read every 9 cycles, with a
CIA2 timer NMI storing to $D418 every 200 cycles. The C64 runs a quiet phase with no
host jobs first, then one phase per MODE (default: locked), each a run of WriteC64Spans
jobs back to back, 1 KiB to $6000 in 33-byte slices:
  locked    each slice starts on the NMI's $D418 store, waiting up to 200 us for it
  unlocked  200 us between slices
--sweep follows each job with a DMA read of $0000-$CFFF, and checks the job's bytes in it.

A wrong read is named by a second read of the register: one that matches means the first
read was answered late; one that misses too is counted apart, as a run of wrong reads
longer than the few cycles between the two, or a register that changed. Each
phase also lists the values the wrong reads found.

--tap arms the snoop tap on those pages (snooptap.py's PAGE:FLAGS) and takes its ring
every second, which is how its cost on the reads is measured; it needs an image built
with DbgSnoopTap. Resets the C64 when done.
"""
import argparse, os, random, subprocess, sys, tempfile, time
from protocol import ACK, DRIVE_SD, READ_C64_MEM, SNOOP_TAP, to_board
from snooptap import ENTRY, HEADER, pages_from
from spans import spans
from trlink import Link

REGISTERS = ['$DF02', '$DF03', '$DF04', '$DF05', '$DF06', '$DF07', '$DF08']
VALUES = [0x3C, 0xC3, 0x5A, 0xA5, 0x0F, 0xF0, 0x96]  # reuhammer.a's V0-V6
REPEATS = 32                                          # and its REPEATS
COUNTS, COUNTS_BYTES, READY = 0x8000, 0xE18, 0x8E40
PASSES, HELD, HELD_HI, HIST_HI = 0xE00, 0xE08, 0xE10, 0x700
CIA2_TA, CIA2_ICR, CIA2_CRA = 0xDD04, 0xDD0D, 0xDD0E
NMI_PERIOD = 200
TARGET, LENGTH, SLICE, WAIT_US = 0x6000, 1024, 33, 200
SAMPLE_STORE = 0xD418
SWEEP = 0xD000
MODES = {
    'locked': lambda tr, data: spans(tr, [(SAMPLE_STORE, 0), (TARGET, len(data))], data, SLICE, WAIT_US, flags=1)[:2],
    'unlocked': lambda tr, data: spans(tr, [(TARGET, len(data))], data, SLICE, WAIT_US)[:2],
}


def counts_from(block):
    """reuhammer.a's counters: (passes, per register: wrong second reads, and wrong first
    reads by value)."""
    passes = int.from_bytes(block[PASSES:PASSES + 4], 'little')
    held = [block[HELD + k] | block[HELD_HI + k] << 8 for k in range(len(REGISTERS))]
    hist = [[block[k * 256 + v] | block[HIST_HI + k * 256 + v] << 8 for v in range(256)]
            for k in range(len(REGISTERS))]
    return passes, held, hist


def counts_between(start, stop):
    """What the C64 counted from one counts_from() to a later one: (reads, per register
    {'late': n, 'twice': n}, {(register, value read): n})."""
    (passes0, held0, hist0), (passes1, held1, hist1) = start, stop
    reads = ((passes1 - passes0) & 0xFFFFFFFF) * REPEATS * len(REGISTERS)
    kinds, values = [], {}
    for k in range(len(REGISTERS)):
        firsts = 0
        for v in range(256):
            n = (hist1[k][v] - hist0[k][v]) & 0xFFFF
            if n:
                values[k, v] = n
                firsts += n
        twice = (held1[k] - held0[k]) & 0xFFFF
        kinds.append({'late': firsts - twice, 'twice': twice})
    return reads, kinds, values


def describe_value(register, value):
    """A wrong read's value, against the register's own and the one read before it."""
    before = (register - 1) % len(REGISTERS)
    text = f'{REGISTERS[register]}: ${VALUES[register]:02X} read as ${value:02X}'
    if value == VALUES[before]:
        text += f' ({REGISTERS[before]}\'s value, the IO2 read before)'
    return text


def report(label, seconds, jobs, between, tap_entries=None):
    reads, kinds, values = between
    late = sum(k['late'] for k in kinds)
    twice = sum(k['twice'] for k in kinds)
    line = f'{label}: {seconds:.0f} s, {jobs} jobs, {reads} IO2 reads, {late + twice} wrong'
    if late + twice:
        line += f' ({late} right on a second read, {twice} wrong on a second read too)'
    if tap_entries is not None:
        line += f', tap recorded {tap_entries}'
    print(line, flush=True)
    for (register, value), n in sorted(values.items(), key=lambda item: -item[1])[:8]:
        print(f'    {n:8d}  {describe_value(register, value)}')
    for k, per_kind in enumerate(kinds):
        if per_kind['twice']:
            print(f'    {REGISTERS[k]} read wrong twice in a row {per_kind["twice"]} times')
    return late + twice


def read_mem(tr, addr, length):
    """ReadC64Mem without peek()'s drain, which would cost every job 0.3 s."""
    tr.wr(to_board(READ_C64_MEM) + to_board(addr) + to_board(length))
    value, text = tr.status(6)
    if value != ACK:
        raise SystemExit(f'read of ${addr:04X} refused: {text or "no reply"}')
    data = tr.rd(length, 10)
    if len(data) != length:
        raise SystemExit(f'read of ${addr:04X}: got {len(data)} of {length} bytes')
    return data


def take_ring(tr, mode, pages=b''):
    """snooptap.take() without its drain; returns the entries recorded since the last take."""
    tr.wr(to_board(SNOOP_TAP) + bytes([mode]) + pages)
    tr.ack('SnoopTap')
    head = tr.rd(HEADER.size)
    if len(head) != HEADER.size:
        raise SystemExit('SnoopTap: short header')
    _, recorded, _, _, _, sent = HEADER.unpack(head)
    if len(tr.rd(ENTRY.size * sent)) != ENTRY.size * sent:
        raise SystemExit('SnoopTap: short ring')
    return recorded


def start_client(tr):
    here = os.path.dirname(os.path.abspath(__file__))
    with tempfile.TemporaryDirectory() as work:
        subprocess.run(['acme', '-f', 'cbm', '-o', os.path.join(work, 'reuhammer.prg'),
                        os.path.join(here, 'reuhammer.a')], check=True, cwd=work)
        tr.post(os.path.join(work, 'reuhammer.prg'), '/REUHAMMR.PRG')
    tr.poke(READY, [0])
    tr.launch('/REUHAMMR.PRG', DRIVE_SD)
    end = time.time() + 15
    while time.time() < end:
        time.sleep(0.5)
        try:
            if tr.peek(READY, 1)[0] == 0x5A:
                return
        except SystemExit:
            pass
    raise SystemExit('the client never reported ready')


def check_reu(tr):
    """Open bus answers most of the reads wrong where no REU is."""
    start = counts_from(read_mem(tr, COUNTS, COUNTS_BYTES))
    time.sleep(1)
    reads, kinds, _ = counts_between(start, counts_from(read_mem(tr, COUNTS, COUNTS_BYTES)))
    wrong = sum(k['late'] + k['twice'] for k in kinds)
    if reads == 0 or wrong * 2 > reads:
        raise SystemExit(f'the client is not seeing an REU ({reads} reads, {wrong} wrong): '
                         'is Special IO set to an REU?')


def run_phase(tr, seconds, write, sweep, tap):
    """(jobs, refusals, jobs whose bytes the sweep found wrong, tap entries) over `seconds`,
    with `write` (None for no jobs) run back to back."""
    jobs = refused = bad = tapped = 0
    rng = random.Random(1)
    now = time.time()
    end, next_take = now + seconds, now + 1
    while time.time() < end:
        if write:
            data = rng.randbytes(LENGTH)
            ok, _ = write(tr, data)
            if ok:
                jobs += 1
                if sweep and read_mem(tr, 0, SWEEP)[TARGET:TARGET + LENGTH] != data:
                    bad += 1
            else:
                refused += 1
                time.sleep(1)  # a refused job's payload is discarded for up to a second
        else:
            time.sleep(min(0.05, max(0, end - time.time())))
        if tap and time.time() >= next_take:
            tapped += take_ring(tr, 0)
            next_take += 1
    return jobs, refused, bad, tapped


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('modes', nargs='*', choices=sorted(MODES), metavar='MODE', default=['locked'])
    parser.add_argument('--quiet', type=float, default=60, help='seconds with no host jobs (default 60)')
    parser.add_argument('--jobs', type=float, default=300, help='seconds of jobs per MODE (default 300)')
    parser.add_argument('--sweep', action='store_true', help='read $0000-$CFFF after each job')
    parser.add_argument('--tap', metavar='PAGE:FLAGS', help='arm the snoop tap and take its ring every second')
    args = parser.parse_args()
    failed = False
    with Link() as tr:
        try:
            start_client(tr)
            check_reu(tr)
            latch = NMI_PERIOD - 1
            tr.poke(CIA2_TA, [latch & 0xFF, latch >> 8])
            tr.poke(CIA2_ICR, [0x81])
            tr.poke(CIA2_CRA, [0x11])
            if args.tap:
                take_ring(tr, 1, pages_from(args.tap))
            phases = [('quiet', args.quiet, None)] + [(mode, args.jobs, MODES[mode]) for mode in args.modes]
            for label, seconds, write in phases:
                start = counts_from(read_mem(tr, COUNTS, COUNTS_BYTES))
                began = time.time()
                jobs, refused, bad, tapped = run_phase(tr, seconds, write, args.sweep, args.tap)
                stop = counts_from(read_mem(tr, COUNTS, COUNTS_BYTES))
                between = counts_between(start, stop)
                if between[0] == 0:
                    raise SystemExit(f'the client stopped reading during the {label} phase')
                failed |= report(label + (' + sweep' if args.sweep and write else ''), time.time() - began,
                                 jobs, between, tapped if args.tap else None) > 0
                if refused or bad:
                    failed = True
                    print(f'    {refused} jobs refused, {bad} read back wrong in the sweep')
            if args.tap:
                take_ring(tr, 2)
            tr.poke(CIA2_ICR, [0x7F])
            tr.poke(CIA2_CRA, [0])
        finally:
            tr.reset()
    if failed:
        raise SystemExit('FAILED')


if __name__ == '__main__':
    main()
