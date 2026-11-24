#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Host DMA writes while the C64 runs REU transfers: each side's bytes must land where it
sent them.   reuspans.py [seconds, default 30]

Needs Special IO set to an REU in Settings, acme on PATH and the C64 at the menu. Runs
reuclient.a, which stashes a page to the REU and fetches it back in a loop, while the
host writes $6000 with WriteC64Mem, whole and 64 bytes at a time, sliced WriteC64Spans
and WriteC64Spans locked to the client's $D418 store, reading each back. A transfer the C64 starts inside a host job's
setup used to take over the job's DMA state: the host's bytes went to the REU's address,
which the C64 counts as a mismatch, or the read-back finds them missing. The C64 runs the
same time with no host jobs first, so a fault of the REU alone shows apart. The client
also reads back each REU register it writes, so a C64 IO2 write isrPHI2 lost or latched
late counts as a miss on that register, in either phase, and reads it a second time. The
second read names the miss: what the transfer before (or the attempt before) left there
means the write was lost, the value written means the first read was answered late, and
anything else means the write landed with the wrong value. A lost write cannot show where
the transfer before left the register holding what the write stores: $DF02 always, and
$DF04 and $DF06 on the fetch, which shares its stash's REU address and bank. The client
logs the first 255 misses; the rest are counted but not named.
Resets the C64 when done.
"""
import os, random, subprocess, sys, tempfile, time
from protocol import ACK, DRIVE_SD, WRITE_C64_MEM, to_board
from spans import spans
from trlink import Link

READY, LOG_COUNT, ROUNDS, MISSES, LOG = 0xC104, 0xC106, 0xC110, 0xC120, 0x7000
FETCH_TAG, RETRY_TAG = 0x80, 0x40
SOURCE_PAGE, DEST_PAGE = 0x40, 0x50  # reuclient.a's source and dest
KINDS = {'lost': 'write lost', 'late': 'read answered late', 'wrong': 'write landed wrong'}
REGISTERS = ['$DF02', '$DF03', '$DF04', '$DF05', '$DF06', '$DF07', '$DF08']
CIA2_TA, CIA2_ICR, CIA2_CRA = 0xDD04, 0xDD0D, 0xDD0E
NMI_PERIOD = 200
TARGET, LENGTH = 0x6000, 4096
SAMPLE_STORE = 0xD418


def start_client(tr):
    here = os.path.dirname(os.path.abspath(__file__))
    with tempfile.TemporaryDirectory() as work:
        subprocess.run(['acme', '-f', 'cbm', '-o', os.path.join(work, 'reuclient.prg'),
                        os.path.join(here, 'reuclient.a')], check=True, cwd=work)
        tr.post(os.path.join(work, 'reuclient.prg'), '/REUCLNT.PRG')
    tr.poke(READY, [0])
    tr.launch('/REUCLNT.PRG', DRIVE_SD)
    end = time.time() + 15
    while time.time() < end:
        time.sleep(0.5)
        try:
            if tr.peek(READY, 1)[0] == 0x5A:
                return
        except SystemExit:
            pass
    raise SystemExit('the client never reported ready')


def counts(tr):
    rounds_lo, rounds_hi, errors_lo, errors_hi = tr.peek(ROUNDS, 4)
    return rounds_lo | rounds_hi << 8, errors_lo | errors_hi << 8


def register_misses(block):
    """The client's 16 bytes at MISSES: each register's 16-bit miss count."""
    return [block[k] | block[8 + k] << 8 for k in range(len(REGISTERS))]


def misses_between(start, stop):
    """Each register's misses from one register_misses() snapshot to a later one, for the
    registers that missed."""
    return {name: (after - before) & 0xFFFF for name, before, after
            in zip(REGISTERS, start, stop) if (after - before) & 0xFFFF}


def left_by_transfer(page, seed):
    """$DF02-$DF08 after the client's transfer of `page` with `seed`, as IOH_REU.c's
    DirectREU leaves them: both addresses advanced by the length of $0100, the length $0001."""
    return [0, page + 1 & 0xFF, seed, 1, seed & 1, 1, 0]


def left_before(register, fetch, seed):
    """What a register held before the client wrote it in the round of `seed`: a fetch
    follows its round's stash, a stash the previous round's fetch."""
    if fetch:
        return left_by_transfer(SOURCE_PAGE, seed)[register]
    return left_by_transfer(DEST_PAGE, seed - 1 & 0xFF)[register]


def read_log(block, count):
    """The client's log at LOG, `count` (tag, seed, written, second read) records."""
    return [tuple(block[page * 256 + k] for page in range(4)) for k in range(count)]


def miss_kinds(records):
    """The kind of each logged miss, a key of KINDS."""
    kinds, previous = [], None
    for record in records:
        tag, seed, written, second = record
        before = previous[3] if tag & RETRY_TAG else left_before(tag & 7, tag & FETCH_TAG, seed)
        kinds.append('late' if second == written else 'lost' if second == before else 'wrong')
        previous = record
    return kinds


def kinds_by_register(records, kinds):
    """{register name: {kind: count}} for the logged misses."""
    tally = {}
    for (tag, *_), kind in zip(records, kinds):
        per_kind = tally.setdefault(REGISTERS[tag & 7], dict.fromkeys(KINDS, 0))
        per_kind[kind] += 1
    return tally


def snapshot(tr):
    """The round counts, the miss counters and the log count. The counters and the log count
    come from one DMA read: the client keeps looping between two peeks, and a miss landing
    between them would put a record in one phase and its count in the other."""
    block = tr.peek(LOG_COUNT, MISSES + 16 - LOG_COUNT)
    return counts(tr), register_misses(block[MISSES - LOG_COUNT:]), block[0]


def poke_in_pieces(tr, data, piece=64):
    """WriteC64Mem a piece at a time, without poke()'s drain: many short halts, each next to the REU loop."""
    for offset in range(0, len(data), piece):
        tr.wr(to_board(WRITE_C64_MEM) + to_board(TARGET + offset) + to_board(piece) + data[offset:offset + piece])
        value, text = tr.status(6)
        if value != ACK:
            return False, text
    return True, ''


WRITERS = {
    'WriteC64Mem': lambda tr, data: (tr.poke(TARGET, data), 'refused'),
    'WriteC64Mem 64 bytes': poke_in_pieces,
    'WriteC64Spans sliced': lambda tr, data: spans(tr, [(TARGET, len(data))], data, 64, 40)[:2],
    'WriteC64Spans locked': lambda tr, data: spans(
        tr, [(SAMPLE_STORE, 0), (TARGET, len(data))], data, 33, 200, flags=1)[:2],
}


def report(label, start, stop, misses, kinds):
    rounds, errors = stop[0] - start[0] & 0xFFFF, stop[1] - start[1] & 0xFFFF
    totals = [f'{sum(per_kind[kind] for per_kind in kinds.values())} {text}' for kind, text in KINDS.items()]
    print(f'{label}: {rounds} REU stash/fetch rounds, {errors} with a page that came back wrong, '
          f'{sum(misses.values())} register read-back misses' + (f' ({", ".join(totals)})' if misses else ''))
    for name, n in misses.items():
        per_kind = kinds.get(name, {})
        named = [f'{per_kind[kind]} {text}' for kind, text in KINDS.items() if per_kind.get(kind)]
        unlogged = n - sum(per_kind.values())
        if unlogged:
            named.append(f'{unlogged} past the end of the log')
        print(f'    {name}: {n} misses: ' + ', '.join(named))
    return errors > 0 or bool(misses)


def main():
    seconds = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    with Link() as tr:
        try:
            start_client(tr)
            time.sleep(1)
            before = counts(tr)
            if before[0] == 0 or before[1]:
                raise SystemExit(f'the client is not seeing an REU ({before[0]} rounds, {before[1]} mismatches): '
                                 'is Special IO set to an REU?')
            latch = NMI_PERIOD - 1
            tr.poke(CIA2_TA, [latch & 0xFF, latch >> 8])
            tr.poke(CIA2_ICR, [0x81])
            tr.poke(CIA2_CRA, [0x11])
            idle_from, idle_misses_from, idle_log_from = snapshot(tr)
            time.sleep(seconds)
            idle_to, idle_misses_to, idle_log_to = snapshot(tr)
            tally = {name: [0, 0, ''] for name in WRITERS}
            rng = random.Random(1)
            end = time.time() + seconds
            while time.time() < end:
                for name, write in WRITERS.items():
                    data = bytes(rng.randrange(256) for _ in range(LENGTH))
                    ok, text = write(tr, data)
                    if not ok:
                        tally[name][2] = text or 'no reply'
                        time.sleep(1)  # a refused job's payload is discarded for up to a second
                        continue
                    tally[name][0] += 1
                    if tr.peek(TARGET, LENGTH) != data:
                        tally[name][1] += 1
            tr.poke(CIA2_ICR, [0x7F])
            tr.poke(CIA2_CRA, [0])
            busy_to, busy_misses_to, busy_log_to = snapshot(tr)
            records = read_log(tr.peek(LOG, 1024), busy_log_to)
        finally:
            tr.reset()
    kinds = miss_kinds(records)
    phase = lambda start, stop: kinds_by_register(records[start:stop], kinds[start:stop])
    failed = report('C64, no host jobs  ', idle_from, idle_to, misses_between(idle_misses_from, idle_misses_to),
                    phase(idle_log_from, idle_log_to))
    failed |= report('C64, beside the jobs', idle_to, busy_to, misses_between(idle_misses_to, busy_misses_to),
                     phase(idle_log_to, busy_log_to))
    for name, (jobs, bad, refusal) in tally.items():
        failed |= bad > 0 or jobs == 0
        print(f'{name:22s}: {jobs} jobs, {bad} read back wrong' + (f'; refused: {refusal}' if refusal else ''))
    if failed:
        raise SystemExit('FAILED')


if __name__ == '__main__':
    main()
