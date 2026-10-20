#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Decode the drive commands a C128/C64 sent, from a VICE trace of the KERNAL entry points.

  iecdecode.py <vice.log> [image-prefix]

Prints each command channel string (M-W, M-E and the rest) with the cycle it was sent on.
For M-W it also prints sd2iec's running CRC, which is what sd2iec matches against its table
of known fast loaders (doscmd.c, fl_crc_table) when the M-E arrives.  With an image prefix,
writes the $0300-$04FF drive RAM image each M-E ran as <prefix>_<n>.bin.

Make the log with VICE's monitor: `trace exec ffb1`, `trace exec ff93`, `trace exec ffa8`
and `trace exec ffae` (LISTEN, SECOND, CIOUT, UNLSN), then `x`, with -monlog / -monlogname
set.  Only bytes sent after a SECOND to channel 15 are decoded; file data is skipped.
"""
import re
import sys

LISTEN, SECOND, CIOUT, UNLSN = 'ffb1', 'ff93', 'ffa8', 'ffae'
COMMAND_CHANNEL = 15
SA_DATA, SA_OPEN = 0x60, 0xF0
TRACE_HIT = re.compile(r'\.C:(ff\w\w)\s+4C .*A:(\w\w).*\s(\d+)$')
UPLOAD_BASE, UPLOAD_SIZE = 0x0300, 0x200


def crc16(crc, byte):
    # avr-libc _crc16_update, which sd2iec uses: poly 0xA001, reflected
    crc ^= byte
    for _ in range(8):
        crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def commands(path):
    cmds, cur = [], None
    for line in open(path, errors='replace'):
        m = TRACE_HIT.match(line.strip())
        if not m:
            continue
        entry, acc, cycle = m.group(1), int(m.group(2), 16), int(m.group(3))
        if entry == LISTEN:
            cur = None
        elif entry == SECOND:
            to_command_channel = (acc & 0x0F == COMMAND_CHANNEL
                                  and acc & 0xF0 in (SA_DATA, SA_OPEN))
            cur = (cycle, bytearray()) if to_command_channel else None
        elif entry == CIOUT and cur is not None:
            cur[1].append(acc)
        elif entry == UNLSN and cur is not None:
            cmds.append((cur[0], bytes(cur[1])))
            cur = None
    return cmds


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__.strip())
    crc, ram, images = 0xFFFF, bytearray(UPLOAD_SIZE), []
    for cycle, cmd in commands(sys.argv[1]):
        if cmd[:3] == b'M-W' and len(cmd) >= 6:
            addr, n = cmd[3] | cmd[4] << 8, cmd[5]
            payload = cmd[6:6 + n]
            for b in payload:
                crc = crc16(crc, b)
            if UPLOAD_BASE <= addr and addr + len(payload) <= UPLOAD_BASE + UPLOAD_SIZE:
                ram[addr - UPLOAD_BASE:addr - UPLOAD_BASE + len(payload)] = payload
            short = f' (only {len(payload)} sent)' if len(payload) < n else ''
            print(f'{cycle:>10} M-W ${addr:04x} len {n:3}{short} crc-so-far {crc:04x}')
        elif cmd[:3] == b'M-E' and len(cmd) >= 5:
            print(f'{cycle:>10} M-E ${cmd[3] | cmd[4] << 8:04x}  (crc {crc:04x})')
            images.append(bytes(ram))
            crc = 0xFFFF
        else:
            print(f'{cycle:>10} {cmd[:40]!r}')
    if len(sys.argv) == 3:
        for i, image in enumerate(images):
            open(f'{sys.argv[2]}_{i}.bin', 'wb').write(image)


main()
