#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""WriteREU against a real TR+: host writes into REU emulation's memory, fetched back by the
C64 with its own REU transfers.   reuwrite.py [seconds of the beside-the-loop run, default 20]

Needs Special IO set to an REU in Settings, acme on PATH and the C64 at the menu. Checks that
the command is refused at the menu, where REU emulation is off; then runs reuwclient.a, writes
patterns across bank boundaries, to the REU's last byte and over all of it but page 0, and has
the client fetch every byte back. It does that with the client idle, then with it stashing and fetching
REU page 0 in a loop while each write lands, so C64 transfers start from the PHI2 ISR in the
middle of the copies. Then the refusals past the end, a write whose sender stops part way, and
the rate. Resets the C64 when done.

The fetches are requested with WriteC64Mem and watched with ReadC64Mem, and a host DMA garbles
a C64 write to IO2 now and then (see reuwclient.a). So the loop is stopped before a fetch is
requested, and the client is left alone while it sets up the fetch.
"""
import os, random, subprocess, sys, tempfile, time
from protocol import ACK, DRIVE_SD, FAIL, READ_C64_MEM, WRITE_C64_MEM, WRITE_REU, to_board
from trlink import Link

MODE, IDLE, READY, ROUNDS, REQUEST, GO = 0xC100, 0xC101, 0xC104, 0xC110, 0xC120, 0xC125
WINDOW, WINDOW_BYTES = 0x6000, 0x2000
LOOP_PAGE_END = 0x100  # the client's own stash/fetch round trip uses REU page 0
FETCH_SECS = 0.02      # the client's settle wait, its register set-up and an 8 KiB fetch
SECONDS = int(sys.argv[1]) if len(sys.argv) > 1 else 20
failed = []


def reu_write(tr, offset, data, send=None):
    """One WriteREU: (acked, reply text, REU size or None). `send` stops the data after that
    many bytes, as a sender whose link drops does."""
    tr.wr(to_board(WRITE_REU))
    value, text = tr.status(10)
    if value not in (ACK, FAIL, None):
        text = f'answered 0x{value:04X}, not an AckToken -- the command is not in this firmware'
    if value != ACK:
        return False, text, None
    size = int.from_bytes(tr.rd(4, 5), 'little')
    tr.wr(to_board(offset, 3) + to_board(len(data), 3))
    value, text = tr.status(10)
    if value != ACK:
        return False, text, size
    tr.wr(data if send is None else data[:send])
    value, text = tr.status(60)
    return value == ACK, text, size


def poke(tr, addr, data):
    """WriteC64Mem without trlink's drain, which would cost a 0.3 s wait per window."""
    tr.wr(to_board(WRITE_C64_MEM) + to_board(addr) + to_board(len(data)) + bytes(data))
    if tr.status(6)[0] != ACK:
        raise SystemExit(f'write of ${addr:04X} refused')


def peek(tr, addr, length):
    tr.wr(to_board(READ_C64_MEM) + to_board(addr) + to_board(length))
    if tr.status(6)[0] != ACK:
        raise SystemExit(f'read of ${addr:04X} refused')
    data = tr.rd(length, 10)
    if len(data) != length:
        raise SystemExit(f'read of ${addr:04X}: got {len(data)} of {length} bytes')
    return data


def wait_until(tr, addr, value, what):
    end = time.time() + 5
    while peek(tr, addr, 1)[0] != value:
        if time.time() > end:
            raise SystemExit(f'the client {what}')


def loop(tr, on):
    poke(tr, MODE, [1 if on else 0])
    wait_until(tr, IDLE, 0 if on else 1, 'did not start its loop' if on else 'did not stop its loop')


def fetch(tr, offset, length):
    """What the client's REU fetch of `length` bytes at `offset` brings into the C64."""
    out = b''
    while length:
        piece = min(length, WINDOW_BYTES)
        poke(tr, REQUEST, [offset & 0xFF, offset >> 8 & 0xFF, offset >> 16, piece & 0xFF, piece >> 8, 1])
        time.sleep(FETCH_SECS)
        wait_until(tr, GO, 0, 'stopped serving fetches')
        out += peek(tr, WINDOW, piece)
        offset, length = offset + piece, length - piece
    return out


def start_client(tr):
    here = os.path.dirname(os.path.abspath(__file__))
    with tempfile.TemporaryDirectory() as work:
        subprocess.run(['acme', '-f', 'cbm', '-o', os.path.join(work, 'reuwclient.prg'),
                        os.path.join(here, 'reuwclient.a')], check=True, cwd=work)
        tr.post(os.path.join(work, 'reuwclient.prg'), '/REUWCLNT.PRG')
    tr.poke(READY, [0])
    tr.launch('/REUWCLNT.PRG', DRIVE_SD)
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
    rounds_lo, rounds_hi, errors_lo, errors_hi = peek(tr, ROUNDS, 4)
    return rounds_lo | rounds_hi << 8, errors_lo | errors_hi << 8


def check(label, ok):
    print(f'  {label}: {"ok" if ok else "FAILED"}')
    if not ok:
        failed.append(label)


def write_beside(tr, looping, offset, data, send=None):
    """WriteREU with the client's loop running through it when `looping`, stopped again after."""
    if looping:
        loop(tr, True)
    result = reu_write(tr, offset, data, send)
    if looping:
        loop(tr, False)
    return result


def integrity(tr, size, rng, looping):
    cases = [(LOOP_PAGE_END, WINDOW_BYTES - LOOP_PAGE_END, 'inside the first bank'),
             (0x1F80, 0x100, 'across a bank boundary'),
             (0x3FFF, 2, 'one byte either side of a boundary'),
             (size - WINDOW_BYTES, WINDOW_BYTES, 'the last bank, to the last byte'),
             (LOOP_PAGE_END, size - LOOP_PAGE_END, f'all {size // 1024} KiB but page 0')]
    print('Write and fetch back, ' + ('the client looping on REU page 0 through each write:' if looping
                                      else 'the client idle:'))
    for offset, length, what in cases:
        data = bytes(rng.randrange(256) for _ in range(length))
        ok, text, _ = write_beside(tr, looping, offset, data)
        if not ok:
            check(f'{what}: refused ({text or "no reply"})', False)
            continue
        back = fetch(tr, offset, length)
        bad = sum(a != b for a, b in zip(back, data))
        check(f'{what}, ${offset:06X} + {length} bytes' + (f', {bad} bytes wrong' if bad else ''), back == data)


def refusals(tr, size):
    print('Refusals:')
    before = fetch(tr, size - 1, 1)
    for offset, length in ((size - 1, 2), (size, 1), (0, size + 1)):
        ok, text, _ = reu_write(tr, offset, bytes(length))
        check(f'${offset:06X} + {length} bytes refused before its data ("{text}")',
              not ok and 'past the end' in text)
    check('the REU\'s last byte unchanged by them', fetch(tr, size - 1, 1) == before)
    ok, text, _ = reu_write(tr, LOOP_PAGE_END, bytes(range(256)) * 16, send=1000)
    check(f'a sender that stops after 1000 of 4096 bytes gets FAIL ("{text}")', not ok and text)
    time.sleep(1.5)  # the board discards what it is still owed until a second of quiet
    ok, text, _ = reu_write(tr, 0, b'')
    check('the board answers the next command', ok)


def rate(tr, rng):
    data = bytes(rng.randrange(256) for _ in range(0x10000))
    loop(tr, True)
    began = time.perf_counter()
    ok, text, _ = reu_write(tr, LOOP_PAGE_END, data)
    secs = time.perf_counter() - began
    loop(tr, False)
    check(f'64 KiB in {secs * 1000:.0f} mS with the client looping, {len(data) / 1024 / secs:.0f} KiB/s', ok)


def beside_the_loop(tr, size, rng):
    jobs = bad = 0
    end = time.time() + SECONDS
    while time.time() < end:
        offset = rng.randrange(LOOP_PAGE_END, size - WINDOW_BYTES)
        data = bytes(rng.randrange(256) for _ in range(WINDOW_BYTES))
        ok, text, _ = write_beside(tr, True, offset, data)
        if not ok:
            failed.append(f'beside the loop: refused ({text or "no reply"})')
            return
        jobs += 1
        bad += fetch(tr, offset, len(data)) != data
    check(f'{jobs} 8 KiB writes at random offsets, each with the loop running, {bad} fetched back wrong',
          jobs and not bad)


if __name__ == '__main__':
    with Link() as tr:
        try:
            tr.reset()
            tr.wait_for('F1 Teensy Mem', 20)
            print('At the menu:')
            ok, text, _ = reu_write(tr, 0, b'')
            check(f'refused while REU emulation is off ("{text}")', not ok and 'REU emulation is off' in text)
            start_client(tr)
            ok, text, size = reu_write(tr, 0, b'')
            if not ok:
                raise SystemExit(f'WriteREU refused under the client: {text or "no reply"} -- '
                                 'is Special IO set to an REU?')
            print(f'REU size {size // 1024} KiB')
            rng = random.Random(1)
            integrity(tr, size, rng, False)
            loop_from = counts(tr)
            integrity(tr, size, rng, True)
            beside_the_loop(tr, size, rng)
            refusals(tr, size)
            rate(tr, rng)
            loop_to = counts(tr)
            rounds, errors = loop_to[0] - loop_from[0] & 0xFFFF, loop_to[1] - loop_from[1] & 0xFFFF
            check(f'the client\'s own loop: {rounds} rounds, {errors} with page 0 wrong', rounds and not errors)
        finally:
            tr.reset()

    if failed:
        raise SystemExit('FAILED: ' + '; '.join(failed))
