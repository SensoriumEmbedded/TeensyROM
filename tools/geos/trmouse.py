#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Put the TR USB mouse driver on a GEOS 128 boot disk.

    python3 tools/geos/trmouse.py boot.d81 out.d81

GEOS loads the first input driver in the boot disk's directory, so the driver
goes into the entry ahead of every other one, and the input driver that was
first moves to the slot the new file would have taken. Needs acme on PATH.
"""

import argparse
import datetime
import pathlib
import subprocess
import sys
import tempfile

SOURCE = pathlib.Path(__file__).with_name('trmouse128.asm')
NAME = b'TR USB MOUSE'
LOAD = 0xfd00
MAX_SIZE = 0x180   # GEOS 128's input driver area, $FD00-$FE7F

D81_SIZE = 819200
SECTORS = 40
DIR_TRACK = 40
BAM_SECTORS = (1, 2)   # tracks 1-40, 41-80
FIRST_DIR_SECTOR = 3
BAM_ENTRIES = 0x10
BAM_ENTRY_SIZE = 6

USR_CLOSED = 0x83
GEOS_SEQUENTIAL = 0
INPUT_128 = 15
INFO_ICON_BITMAP = 0xbf   # an uncompressed 63-byte icon follows

ICON = """
........................
.........######.........
.......##..#...##.......
......#....#.....#......
.....#.....#......#.....
.....#.....#......#.....
.....#.....#......#.....
.....##############.....
.....#............#.....
.....#............#.....
.....#............#.....
.....#............#.....
.....#............#.....
.....#............#.....
......#..........#......
.......##......##.......
.........######.........
........................
...####.####.####.......
.....#..#..#.#..#.......
.....#..###..####.......
"""


def assemble():
    with tempfile.TemporaryDirectory() as tmp:
        out = pathlib.Path(tmp) / 'trmouse128.bin'
        subprocess.run(['acme', '-f', 'plain', '-o', str(out), str(SOURCE)], check=True)
        code = out.read_bytes()
    if len(code) > MAX_SIZE:
        sys.exit(f'the driver is {len(code)} bytes; GEOS 128 has room for {MAX_SIZE}')
    return code


def icon_bits():
    rows = ICON.split()
    assert len(rows) == 21 and all(len(r) == 24 for r in rows)
    out = bytearray()
    for row in rows:
        bits = int(row.replace('#', '1').replace('.', '0'), 2)
        out += bits.to_bytes(3, 'big')
    return bytes(out)


def padded(text, size, pad=b'\0'):
    assert len(text) <= size
    return text + pad * (size - len(text))


class D81:
    def __init__(self, image):
        if len(image) != D81_SIZE:
            sys.exit(f'not a D81 without error bytes ({len(image)} bytes, expected {D81_SIZE})')
        self.image = bytearray(image)

    def offset(self, track, sector):
        return ((track - 1) * SECTORS + sector) * 256

    def bam_entry(self, track):
        sector = BAM_SECTORS[(track - 1) // SECTORS]
        return self.offset(DIR_TRACK, sector) + BAM_ENTRIES + ((track - 1) % SECTORS) * BAM_ENTRY_SIZE

    def allocate(self):
        for track in range(1, 81):
            if track == DIR_TRACK:
                continue
            entry = self.bam_entry(track)
            for sector in range(SECTORS):
                byte, bit = entry + 1 + sector // 8, 1 << (sector % 8)
                if self.image[byte] & bit:
                    self.image[byte] &= ~bit
                    self.image[entry] -= 1
                    return track, sector
        sys.exit('the disk is full')

    def dir_slots(self):
        track, sector = DIR_TRACK, FIRST_DIR_SECTOR
        while track:
            base = self.offset(track, sector)
            for i in range(8):
                yield base + i * 32
            track, sector = self.image[base], self.image[base + 1]

    def write_file(self, data, info):
        """Write a GEOS sequential file; returns its first data sector."""
        sectors = [self.allocate() for _ in range((len(data) + 253) // 254)]
        for i, (track, sector) in enumerate(sectors):
            chunk = data[i * 254:(i + 1) * 254]
            if i + 1 < len(sectors):
                link = bytes(sectors[i + 1])
            else:
                link = bytes((0, len(chunk) + 1))
            o = self.offset(track, sector)
            self.image[o:o + 256] = padded(link + chunk, 256)
        info_ts = self.allocate()
        o = self.offset(*info_ts)
        self.image[o:o + 256] = info
        return sectors[0], info_ts, len(sectors) + 1


def info_block(size):
    info = bytearray(256)
    info[0:2] = b'\x00\xff'
    info[2:5] = bytes((3, 21, INFO_ICON_BITMAP))
    info[5:0x44] = icon_bits()
    info[0x44:0x47] = bytes((USR_CLOSED, INPUT_128, GEOS_SEQUENTIAL))
    info[0x47:0x49] = LOAD.to_bytes(2, 'little')
    info[0x49:0x4b] = (LOAD + size).to_bytes(2, 'little')
    info[0x4d:0x61] = padded(b'TR USB Mouse V1.0', 20)
    info[0x61:0x75] = padded(b'TeensyROM', 20)
    info[0xa0:0x100] = padded(b'A USB mouse plugged into a TeensyROM.', 0x60)
    return bytes(info)


def install(image, code):
    disk = D81(image)
    slots = list(disk.dir_slots())
    entries = [s for s in slots if disk.image[s + 2] & 7]
    if any(disk.image[s + 5:s + 21].rstrip(b'\xa0') == NAME for s in entries):
        sys.exit(f'{NAME.decode()} is already on this disk')
    free = next((s for s in slots if disk.image[s + 2] == 0), None)
    if free is None:
        sys.exit('the directory is full')

    first, info_ts, blocks = disk.write_file(code, info_block(len(code)))
    now = datetime.datetime.now()
    entry = bytearray(30)
    entry[0] = USR_CLOSED
    entry[1:3] = bytes(first)
    entry[3:19] = padded(NAME, 16, b'\xa0')
    entry[19:21] = bytes(info_ts)
    entry[21:23] = bytes((GEOS_SEQUENTIAL, INPUT_128))
    entry[23:28] = bytes((now.year % 100, now.month, now.day, now.hour, now.minute))
    entry[28:30] = blocks.to_bytes(2, 'little')
    disk.image[free + 2:free + 32] = entry

    drivers = [s for s in entries if disk.image[s + 24] == INPUT_128]
    if drivers and slots.index(drivers[0]) < slots.index(free):
        a, b = drivers[0], free
        disk.image[a + 2:a + 32], disk.image[b + 2:b + 32] = \
            disk.image[b + 2:b + 32], disk.image[a + 2:a + 32]
    return bytes(disk.image)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('disk', type=pathlib.Path)
    parser.add_argument('out', type=pathlib.Path)
    args = parser.parse_args()
    args.out.write_bytes(install(args.disk.read_bytes(), assemble()))


if __name__ == '__main__':
    main()
