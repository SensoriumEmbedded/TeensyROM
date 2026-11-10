#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""WriteC64Mem and ReadC64Mem over the TCP listener, in volume, with every read
checked against what was written.

    tcpstress.py <TR IP address> [--writes 300] [--reads 300]

Run it with the C64 sitting still -- at a BASIC prompt, with no program in
memory -- because the scratch region is ordinary BASIC RAM, and anything that
runs there changes it under the check. It starts by reading the region once to
learn what is there, then sends the writes (4 KiB each) and the reads (8-16 KiB
each) in a shuffled order over one connection, each with its own time-out, and
compares every read with the bytes this run believes the region holds, and
the whole region once more after the last command, so every write is checked.
At the end it asks the board which image it is running, over the same connection and
then over a fresh one, so a board that stopped answering fails the run.

Exits 0 only when every command was acknowledged within its time-out, every
read and the final one matched, and the board answered both checks at the end.
"""
import argparse
import random
import socket
import sys
import time

from protocol import ACK, FAIL, FW_CHECK, IMAGES, READ_C64_MEM, WRITE_C64_MEM, \
    from_board, to_board

PORT = 2112
REGION = 0x2000          # BASIC RAM, free with no program loaded
REGION_BYTES = 0x8000    # up to $9FFF
WRITE_BYTES = 4096
READ_MIN, READ_MAX = 8192, 16384


class Board:
    def __init__(self, host, timeout):
        self.timeout = timeout
        self.sock = socket.create_connection((host, PORT), timeout=timeout)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    def close(self):
        self.sock.close()

    def recv(self, n):
        """Exactly n bytes, or raises TimeoutError naming how many came."""
        buf, end = bytearray(), time.monotonic() + self.timeout
        while len(buf) < n:
            left = end - time.monotonic()
            if left <= 0:
                raise TimeoutError(f'got {len(buf)} of {n} bytes')
            self.sock.settimeout(left)
            try:
                chunk = self.sock.recv(n - len(buf))
            except socket.timeout:
                raise TimeoutError(f'got {len(buf)} of {n} bytes') from None
            if not chunk:
                raise ConnectionError(f'connection closed after {len(buf)} of {n} bytes')
            buf += chunk
        return bytes(buf)

    def status(self, what):
        value = from_board(self.recv(2))
        if value == FAIL:
            self.sock.settimeout(1)
            try:
                text = self.sock.recv(200).decode('latin1', 'replace').strip()
            except socket.timeout:
                text = ''
            raise RuntimeError(f'{what}: FAIL {text}')
        if value != ACK:
            raise RuntimeError(f'{what}: unexpected 0x{value:04X}')

    def write(self, addr, data):
        self.sock.settimeout(self.timeout)
        self.sock.sendall(to_board(WRITE_C64_MEM) + to_board(addr) + to_board(len(data)) + data)
        self.status(f'write ${addr:04X}+{len(data)}')

    def read(self, addr, length):
        self.sock.settimeout(self.timeout)
        self.sock.sendall(to_board(READ_C64_MEM) + to_board(addr) + to_board(length))
        self.status(f'read ${addr:04X}+{length}')
        return self.recv(length)

    def image(self):
        self.sock.settimeout(self.timeout)
        self.sock.sendall(to_board(FW_CHECK))
        return IMAGES.get(from_board(self.recv(2)))


def plan(writes, reads, rng):
    jobs = [('w', rng.randrange(0, REGION_BYTES - WRITE_BYTES + 1), WRITE_BYTES)
            for _ in range(writes)]
    for _ in range(reads):
        length = rng.randint(READ_MIN, READ_MAX)
        jobs.append(('r', rng.randrange(0, REGION_BYTES - length + 1), length))
    rng.shuffle(jobs)
    return jobs


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('host')
    ap.add_argument('--writes', type=int, default=300)
    ap.add_argument('--reads', type=int, default=300)
    ap.add_argument('--timeout', type=float, default=5.0,
                    help='seconds each command may take (default 5)')
    ap.add_argument('--seed', type=int, default=None)
    args = ap.parse_args()

    seed = args.seed if args.seed is not None else random.randrange(1 << 32)
    rng = random.Random(seed)
    print(f'seed {seed}: {args.writes} writes of {WRITE_BYTES}, {args.reads} reads of '
          f'{READ_MIN}-{READ_MAX}, in ${REGION:04X}-${REGION + REGION_BYTES - 1:04X}')

    board = Board(args.host, args.timeout)
    shadow = bytearray(board.read(REGION, REGION_BYTES))
    failures, mismatches, slowest = [], 0, (0.0, '')
    moved, started = 0, time.monotonic()

    for n, (kind, off, length) in enumerate(plan(args.writes, args.reads, rng), 1):
        addr, t0 = REGION + off, time.monotonic()
        what = f'#{n} {"write" if kind == "w" else "read"} ${addr:04X}+{length}'
        try:
            if kind == 'w':
                data = rng.randbytes(length)
                board.write(addr, data)
                shadow[off:off + length] = data
            else:
                got = board.read(addr, length)
                if got != shadow[off:off + length]:
                    first = next(i for i in range(length) if got[i] != shadow[off + i])
                    mismatches += 1
                    failures.append(f'{what}: differs from ${addr + first:04X}')
        except (OSError, TimeoutError, RuntimeError) as problem:
            failures.append(f'{what}: {problem}')
            print(f'  {failures[-1]} -- reconnecting')
            board.close()
            board = Board(args.host, args.timeout)
            shadow[:] = board.read(REGION, REGION_BYTES)
            continue
        took = time.monotonic() - t0
        moved += length
        if took > slowest[0]:
            slowest = (took, what)
        if n % 100 == 0:
            print(f'  {n} done, {len(failures)} failed')

    elapsed = time.monotonic() - started
    print(f'{moved / 1024:.0f} KiB in {elapsed:.1f} s ({moved / 1024 / elapsed:.0f} KiB/s); '
          f'slowest {slowest[0] * 1000:.0f} ms ({slowest[1]})')

    try:
        got = board.read(REGION, REGION_BYTES)
        if got != shadow:
            first = next(i for i in range(REGION_BYTES) if got[i] != shadow[i])
            mismatches += 1
            failures.append(f'final read: differs from ${REGION + first:04X}')
    except (OSError, RuntimeError) as problem:
        failures.append(f'final read: {problem}')

    for label, fresh in (('same connection', False), ('new connection', True)):
        try:
            if fresh:
                board.close()
                board = Board(args.host, args.timeout)
            image = board.image()
        except (OSError, TimeoutError) as problem:
            failures.append(f'image check over {label}: {problem}')
            continue
        print(f'board answers over {label}: {image or "unknown reply"}')
        if image != 'main':
            failures.append(f'image check over {label}: {image}')
    board.close()

    print(f'{len(failures)} failed ({mismatches} mismatched reads)')
    for line in failures:
        print(f'  {line}')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
