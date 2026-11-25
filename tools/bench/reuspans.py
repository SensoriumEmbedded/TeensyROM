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
late counts as a miss on that register, in either phase, except where the transfer before
left the register holding what the write stores: $DF02 always, and $DF04 and $DF06 on
the fetch, which shares its stash's REU address and bank.
Resets the C64 when done.
"""
import os, random, subprocess, sys, tempfile, time
from protocol import ACK, DRIVE_SD, WRITE_C64_MEM, to_board
from spans import spans
from trlink import Link

READY, ROUNDS, MISSES = 0xC104, 0xC110, 0xC120
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
    """The client's 24 bytes at MISSES: per register, its 16-bit miss count and the value a
    second read found after its last miss."""
    return [(block[k] | block[8 + k] << 8, block[16 + k]) for k in range(len(REGISTERS))]


def misses_between(start, stop):
    """Each register's misses from one register_misses() snapshot to a later one, with the
    later snapshot's last read, for the registers that missed."""
    return {name: ((after - before) & 0xFFFF, last) for name, (before, _), (after, last)
            in zip(REGISTERS, start, stop) if (after - before) & 0xFFFF}


def rounds_between(start, stop):
    """Rounds run and rounds whose page came back wrong, from one counts() snapshot to a
    later one. The client counts each round once, as a good page or as a wrong one."""
    good, wrong = ((after - before) & 0xFFFF for before, after in zip(start, stop))
    return good + wrong, wrong


def require_an_reu(good, wrong):
    # Without an REU no page comes back right. A wrong page alone can be the first-byte loss
    # in Known-Issues.md.
    if good == 0:
        raise SystemExit(f'the client is not seeing an REU (no page of {wrong} rounds came back right): '
                         'is Special IO set to an REU?')


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


def report(label, start, stop, misses):
    rounds, errors = rounds_between(start, stop)
    print(f'{label}: {rounds} REU stash/fetch rounds, {errors} with a page that came back wrong, '
          f'{sum(n for n, _ in misses.values())} register read-back misses')
    for name, (n, last) in misses.items():
        print(f'    {name}: {n} misses, the last read back ${last:02X} on a second read')
    return errors > 0 or bool(misses)



def main():
    seconds = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    with Link() as tr:
        try:
            start_client(tr)
            time.sleep(1)
            require_an_reu(*counts(tr))
            latch = NMI_PERIOD - 1
            tr.poke(CIA2_TA, [latch & 0xFF, latch >> 8])
            tr.poke(CIA2_ICR, [0x81])
            tr.poke(CIA2_CRA, [0x11])
            idle_from, idle_misses_from = counts(tr), register_misses(tr.peek(MISSES, 24))
            time.sleep(seconds)
            idle_to, idle_misses_to = counts(tr), register_misses(tr.peek(MISSES, 24))
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
            busy_to, busy_misses_to = counts(tr), register_misses(tr.peek(MISSES, 24))
        finally:
            tr.reset()
    failed = report('C64, no host jobs  ', idle_from, idle_to, misses_between(idle_misses_from, idle_misses_to))
    failed |= report('C64, beside the jobs', idle_to, busy_to, misses_between(idle_misses_to, busy_misses_to))
    for name, (jobs, bad, refusal) in tally.items():
        failed |= bad > 0 or jobs == 0
        print(f'{name:22s}: {jobs} jobs, {bad} read back wrong' + (f'; refused: {refusal}' if refusal else ''))
    if failed:
        raise SystemExit('FAILED')


if __name__ == '__main__':
    main()
