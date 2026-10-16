#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Put the TR USB mouse driver on a GEOS 128 boot disk.

    python3 tools/geos/trmouse.py boot.d81 out.d81     (or a .d64 or .d71)

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
CLASS_NAME = b'TR USB Mouse'
CLASS_NAME_SIZE = 12   # GEOS reads the version right after it
LOAD = 0xfd00
MAX_SIZE = 0x180   # GEOS 128's input driver area, $FD00-$FE7F

SECTOR_SIZE = 256
DIR_ENTRY_SIZE = 32

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


def zone_sectors(track):
    """A 1541's sectors per track, which a 1571 repeats on its second side."""
    track = (track - 1) % 35 + 1
    return 21 if track <= 17 else 19 if track <= 24 else 18 if track <= 30 else 17


class Format:
    def __init__(self, name, tracks, sectors, dir_track, dir_sectors, reserved, bam):
        self.name, self.tracks, self.sectors = name, tracks, sectors
        self.dir_track, self.dir_sectors = dir_track, dir_sectors
        self.reserved = reserved   # tracks DOS never hands out for file data
        self.bam = bam             # track -> (count, bitmap) as (track, sector, byte)
        self.starts = [0]
        for track in range(1, tracks + 1):
            self.starts.append(self.starts[-1] + sectors(track))

    @property
    def size(self):
        return self.starts[-1] * SECTOR_SIZE


def d64_bam(track):
    return (18, 0, 4 + (track - 1) * 4), (18, 0, 5 + (track - 1) * 4)


def d71_bam(track):
    if track <= 35:
        return d64_bam(track)
    return (18, 0, 0xdd + track - 36), (53, 0, (track - 36) * 3)


def d81_bam(track):
    sector, entry = 1 + (track - 1) // 40, 0x10 + (track - 1) % 40 * 6
    return (40, sector, entry), (40, sector, entry + 1)


FORMATS = {f.size: f for f in (
    Format('D64', 35, zone_sectors, 18, range(1, 19), (18,), d64_bam),
    Format('D71', 70, zone_sectors, 18, range(1, 19), (18, 53), d71_bam),
    Format('D81', 80, lambda track: 40, 40, range(3, 40), (40,), d81_bam),
)}


class Disk:
    def __init__(self, image):
        self.format = FORMATS.get(len(image))
        if self.format is None:
            sizes = ', '.join(f'{f.name} {f.size}' for f in FORMATS.values())
            sys.exit(f'not a D64, D71 or D81 without error bytes ({len(image)} bytes; expected {sizes})')
        self.image = bytearray(image)
        if self.format.name == 'D71' and not self.image[self.offset(18, 0) + 3] & 0x80:
            sys.exit('this D71 is formatted single-sided')

    def offset(self, track, sector):
        return (self.format.starts[track - 1] + sector) * SECTOR_SIZE

    def at(self, place):
        track, sector, byte = place
        return self.offset(track, sector) + byte

    def take(self, track):
        """Mark the first free sector of a track in use; None if it has none."""
        count, bitmap = (self.at(p) for p in self.format.bam(track))
        for sector in range(self.format.sectors(track)):
            byte, bit = bitmap + sector // 8, 1 << (sector % 8)
            if self.image[byte] & bit:
                if not self.image[count]:
                    sys.exit(f'the BAM count for track {track} disagrees with its bitmap')
                self.image[byte] &= ~bit
                self.image[count] -= 1
                return track, sector
        return None

    def allocate(self):
        for track in range(1, self.format.tracks + 1):
            if track not in self.format.reserved:
                found = self.take(track)
                if found:
                    return found
        sys.exit('the disk is full')

    def dir_sectors(self):
        header = self.offset(self.format.dir_track, 0)
        track, sector = self.image[header], self.image[header + 1]
        if not track:
            sys.exit('the disk header links to no directory')
        seen = set()
        while track:
            in_directory = track == self.format.dir_track and sector in self.format.dir_sectors
            if (track, sector) in seen or not in_directory:
                sys.exit(f'the directory chain is broken at {track}/{sector}')
            seen.add((track, sector))
            yield track, sector
            base = self.offset(track, sector)
            track, sector = self.image[base], self.image[base + 1]

    def slots(self, track, sector):
        base = self.offset(track, sector)
        return [base + i * DIR_ENTRY_SIZE for i in range(SECTOR_SIZE // DIR_ENTRY_SIZE)]

    def dir_slots(self):
        for track, sector in self.dir_sectors():
            yield from self.slots(track, sector)

    def grow_directory(self):
        """Chain one more directory sector, as DOS does when a directory fills; returns its slots."""
        chain = list(self.dir_sectors())
        new = self.take(self.format.dir_track)
        if new is None or new[1] not in self.format.dir_sectors:
            sys.exit('the directory is full')
        if new in chain:
            sys.exit(f'the BAM shows directory sector {new[0]}/{new[1]} free')
        o = self.offset(*chain[-1])
        self.image[o:o + 2] = bytes(new)
        o = self.offset(*new)
        self.image[o:o + SECTOR_SIZE] = padded(b'\x00\xff', SECTOR_SIZE)
        return self.slots(*new)

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
            self.image[o:o + SECTOR_SIZE] = padded(link + chunk, SECTOR_SIZE)
        info_ts = self.allocate()
        o = self.offset(*info_ts)
        self.image[o:o + SECTOR_SIZE] = info
        return sectors[0], info_ts, len(sectors) + 1


def info_block(size):
    info = bytearray(256)
    info[0:2] = b'\x00\xff'
    info[2:5] = bytes((3, 21, INFO_ICON_BITMAP))
    info[5:0x44] = icon_bits()
    info[0x44:0x47] = bytes((USR_CLOSED, INPUT_128, GEOS_SEQUENTIAL))
    info[0x47:0x49] = LOAD.to_bytes(2, 'little')
    info[0x49:0x4b] = (LOAD + size).to_bytes(2, 'little')
    info[0x4d:0x61] = padded(padded(CLASS_NAME, CLASS_NAME_SIZE, b' ') + b'V1.0', 20)
    info[0x61:0x75] = padded(b'TeensyROM', 20)
    info[0xa0:0x100] = padded(b'A USB mouse plugged into a TeensyROM.', 0x60)
    return bytes(info)


def install(image, code):
    disk = Disk(image)
    slots = list(disk.dir_slots())
    entries = [s for s in slots if disk.image[s + 2] & 7]
    if any(disk.image[s + 5:s + 21].rstrip(b'\xa0') == NAME for s in entries):
        sys.exit(f'{NAME.decode()} is already on this disk')
    free = next((s for s in slots if disk.image[s + 2] == 0), None)
    if free is None:
        grown = disk.grow_directory()
        slots += grown
        free = grown[0]

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
