#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Put the TR USB mouse driver on a GEOS 128 (or, with --target 64, GEOS 64) boot disk.

    python3 tools/geos/trmouse.py boot.d81 out.d81     (or a .d64 or .d71)
    python3 tools/geos/trmouse.py --c-header out.h     (the driver file, for the firmware)

Add --target 64 to either form for GEOS 64.

GEOS loads the first input driver in the boot disk's directory, so the driver
goes into the entry ahead of every other one, and the input driver that was
first moves to the slot the new file would have taken. Needs acme on PATH.

The sectors are the ones the firmware's disk image engine (Source/Teensy/DiskImage)
picks, as 1541 DOS does: see docs/Architecture/Disk-Images.md. The firmware's
installer is tested byte for byte against this script.
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
TARGETS = {'128': (0xfd00, 15), '64': (0xfe80, 10)}   # where GEOS loads an input driver, and its type
AREA_END = {'128': 0xfe80, '64': 0xfffa}   # the 64's area stops at the hardware vectors
LOAD, INPUT_TYPE = TARGETS['128']
TARGET = '128'

SECTOR_SIZE = 256
DIR_ENTRY_SIZE = 32

USR_CLOSED = 0x83
GEOS_SEQUENTIAL = 0
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
        subprocess.run(['acme', f'-DMOUSE_BASE={LOAD}', *(['-DGEOS64=1'] if TARGET == '64' else []), '-f', 'plain', '-o', str(out), str(SOURCE)], check=True)
        code = out.read_bytes()
    room = AREA_END[TARGET] - LOAD
    if len(code) > room:
        sys.exit(f'the driver is {len(code)} bytes; GEOS {TARGET} has room for {room}')
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


def d64_40_sectors(track):
    return zone_sectors(track) if track <= 35 else 17


def d64_order():
    """1541 DOS hands out data tracks nearest the directory first: 17 down to 1, then 19 up to 35."""
    return [*range(17, 0, -1), *range(19, 36)]


def d71_order():
    return [*d64_order(), *range(52, 35, -1), *range(54, 71)]


def d81_order():
    return [*range(39, 0, -1), *range(41, 81)]


class Format:
    def __init__(self, name, tracks, sectors, dir_track, first_dir, order, bam):
        self.name, self.tracks, self.sectors = name, tracks, sectors
        self.dir_track, self.first_dir = dir_track, first_dir
        self.order = order    # data tracks in the order DOS allocates them
        self.bam = bam        # track -> (count, bitmap) as (track, sector, byte)
        self.starts = [0]
        for track in range(1, tracks + 1):
            self.starts.append(self.starts[-1] + sectors(track))

    @property
    def blocks(self):
        return self.starts[-1]


def d64_bam(track):
    return (18, 0, 4 + (track - 1) * 4), (18, 0, 5 + (track - 1) * 4)


def d71_bam(track):
    if track <= 35:
        return d64_bam(track)
    return (18, 0, 0xdd + track - 36), (53, 0, (track - 36) * 3)


def d81_bam(track):
    sector, entry = 1 + (track - 1) // 40, 0x10 + (track - 1) % 40 * 6
    return (40, sector, entry), (40, sector, entry + 1)


# Each kind with and without the error bytes some copiers append (one a sector, after the data)
FORMATS = {}
for f in (
    Format('D64', 35, zone_sectors, 18, 1, d64_order(), d64_bam),
    Format('D64', 40, d64_40_sectors, 18, 1, d64_order(), d64_bam),
    Format('D71', 70, zone_sectors, 18, 1, d71_order(), d71_bam),
    Format('D81', 80, lambda track: 40, 40, 3, d81_order(), d81_bam),
):
    FORMATS[f.blocks * SECTOR_SIZE] = FORMATS[f.blocks * (SECTOR_SIZE + 1)] = f

FILE_INTERLEAVE = 10
DIR_INTERLEAVE = 3


class Disk:
    """A disk image written the way the firmware's disk image engine (Source/Teensy/DiskImage) writes it."""

    def __init__(self, image):
        self.format = FORMATS.get(len(image))
        if self.format is None:
            sys.exit(f'not a D64, D71 or D81 image ({len(image)} bytes)')
        self.image = bytearray(image)
        if self.format.name == 'D71' and not self.image[self.offset(18, 0) + 3] & 0x80:
            sys.exit('this D71 is formatted single-sided')
        self.chain = self.check_directory()

    def offset(self, track, sector):
        return (self.format.starts[track - 1] + sector) * SECTOR_SIZE

    def at(self, place):
        track, sector, byte = place
        return self.offset(track, sector) + byte

    def link(self, track, sector):
        o = self.offset(track, sector)
        return self.image[o], self.image[o + 1]

    def check_directory(self):
        """The header must link to a chain of distinct sectors on the directory track."""
        track, sector = self.link(self.format.dir_track, 0)
        if not track:
            sys.exit('the disk header links to no directory')
        chain = []
        while track:
            in_directory = (track == self.format.dir_track and self.format.first_dir <= sector < self.format.sectors(track))
            if (track, sector) in chain or not in_directory:
                sys.exit(f'the directory chain is broken at {track}/{sector}')
            chain.append((track, sector))
            track, sector = self.link(track, sector)
        return chain

    def is_free(self, track, sector):
        _, bitmap = self.format.bam(track)
        return bool(self.image[self.at(bitmap) + sector // 8] & 1 << (sector % 8))

    def free_sectors(self, track):
        return [s for s in range(self.format.sectors(track)) if self.is_free(track, s)]

    def count(self, track):
        return self.image[self.at(self.format.bam(track)[0])]

    def take(self, track, sector):
        _, bitmap = self.format.bam(track)
        self.image[self.at(bitmap) + sector // 8] &= ~(1 << (sector % 8))
        self.image[self.at(self.format.bam(track)[0])] -= 1
        return track, sector

    def allocatable(self):
        """Data sectors the allocator can hand out: a track gives one while its count is above 0 and a bit is free."""
        return sum(min(self.count(t), len(self.free_sectors(t))) for t in self.format.order)

    def allocate(self, after=None):
        """The next data sector, as DOS takes it: after the file's last sector with an interleave of 10, else the
        first free sector of the first track in DOS order that has one."""
        order = self.format.order
        if after:
            track, hint = after[0], after[1] + FILE_INTERLEAVE
            tracks = order[order.index(track):]
        else:
            tracks, hint = order, 0
        for track in tracks:
            if self.count(track):
                n = self.format.sectors(track)
                for i in range(n):
                    if self.is_free(track, (hint + i) % n):
                        return self.take(track, (hint + i) % n)
            hint = 0
        sys.exit('the disk is full')

    def slots(self, track, sector):
        base = self.offset(track, sector)
        return [base + i * DIR_ENTRY_SIZE for i in range(SECTOR_SIZE // DIR_ENTRY_SIZE)]

    def dir_slots(self):
        for track, sector in self.chain:
            yield from self.slots(track, sector)

    def grow_room(self):
        """Refuses a directory that could not take one more sector."""
        for track, sector in self.chain:
            if self.is_free(track, sector):
                sys.exit(f'the BAM shows directory sector {track}/{sector} free')
        track = self.format.dir_track
        if not self.count(track) or not any(s >= self.format.first_dir for s in self.free_sectors(track)):
            sys.exit('the directory is full')

    def grow_directory(self):
        """Chain one more directory sector, 3 after the last one, as DOS does when a directory fills."""
        last = self.chain[-1]
        track, n = self.format.dir_track, self.format.sectors(self.format.dir_track)
        for i in range(n):
            sector = (last[1] + DIR_INTERLEAVE + i) % n
            if sector >= self.format.first_dir and self.is_free(track, sector):
                break
        new = self.take(track, sector)
        o = self.offset(*last)
        self.image[o:o + 2] = bytes(new)
        o = self.offset(*new)
        self.image[o:o + SECTOR_SIZE] = padded(b'\x00\xff', SECTOR_SIZE)
        self.chain.append(new)
        return self.slots(*new)

    def write_file(self, data, info):
        """Write a GEOS sequential file, its info block in the sector after the last data sector; returns its
        first data sector, the info sector and the block count."""
        sectors = [self.allocate()]
        while len(sectors) < (len(data) + 253) // 254:
            sectors.append(self.allocate(sectors[-1]))
        for i, (track, sector) in enumerate(sectors):
            chunk = data[i * 254:(i + 1) * 254]
            if i + 1 < len(sectors):
                link = bytes(sectors[i + 1])
            else:
                link = bytes((0, len(chunk) + 1))
            o = self.offset(track, sector)
            self.image[o:o + SECTOR_SIZE] = padded(link + chunk, SECTOR_SIZE)
        info_ts = self.allocate(sectors[-1])
        o = self.offset(*info_ts)
        self.image[o:o + SECTOR_SIZE] = info
        return sectors[0], info_ts, len(sectors) + 1


def info_block(size):
    info = bytearray(256)
    info[0:2] = b'\x00\xff'
    info[2:5] = bytes((3, 21, INFO_ICON_BITMAP))
    info[5:0x44] = icon_bits()
    info[0x44:0x47] = bytes((USR_CLOSED, INPUT_TYPE, GEOS_SEQUENTIAL))
    info[0x47:0x49] = LOAD.to_bytes(2, 'little')
    info[0x49:0x4b] = (LOAD + size).to_bytes(2, 'little')
    info[0x4d:0x61] = padded(padded(CLASS_NAME, CLASS_NAME_SIZE, b' ') + b'V1.0', 20)
    info[0x61:0x75] = padded(b'TeensyROM', 20)
    info[0xa0:0x100] = padded(b'A USB mouse plugged into a TeensyROM.', 0x60)
    return bytes(info)


def in_use(type_byte):
    """What DOS finds by name: a file of any type, or a closed DEL."""
    return type_byte & 7 or type_byte & 0x80


def install(image, code):
    disk = Disk(image)
    slots = list(disk.dir_slots())
    if any(in_use(disk.image[s + 2]) and disk.image[s + 5:s + 21].rstrip(b'\xa0') == NAME for s in slots):
        sys.exit(f'{NAME.decode()} is already on this disk')
    free = next((s for s in slots if disk.image[s + 2] == 0), None)
    if free is None:
        disk.grow_room()
    if disk.allocatable() < (len(code) + 253) // 254 + 1:
        sys.exit('the disk is full')

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
    entry[21:23] = bytes((GEOS_SEQUENTIAL, INPUT_TYPE))
    entry[23:28] = bytes((now.year % 100, now.month, now.day, now.hour, now.minute))
    entry[28:30] = blocks.to_bytes(2, 'little')
    disk.image[free + 2:free + 32] = entry

    drivers = [s for s in slots if disk.image[s + 2] & 7 and disk.image[s + 24] == INPUT_TYPE]
    if drivers[0] != free:
        a, b = drivers[0], free
        disk.image[a + 2:a + 32], disk.image[b + 2:b + 32] = \
            disk.image[b + 2:b + 32], disk.image[a + 2:a + 32]
    return bytes(disk.image)


def c_array(name, data):
    rows = (', '.join(f'0x{b:02x}' for b in data[i:i + 16]) for i in range(0, len(data), 16))
    body = ',\n'.join(f'   {row}' for row in rows)
    return f'static const uint8_t {name}[{len(data)}] PROGMEM = {{\n{body}\n}};\n'


def c_header(code, target):
    return (
        '// SPDX-License-Identifier: MIT\n'
        '// Generated by tools/geos/trmouse.py --c-header from tools/geos/trmouse128.asm; do not edit.\n'
        '#pragma once\n'
        '#include <stdint.h>\n'
        '#ifdef ARDUINO\n'
        '#include <avr/pgmspace.h>\n'
        '#else\n'
        '#define PROGMEM\n'
        '#endif\n\n'
        f'static const char TRMouse{target}Name[] PROGMEM = "{NAME.decode()}";\n'
        + c_array(f'TRMouse{target}Code', code)
        + c_array(f'TRMouse{target}Info', info_block(len(code))))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('disk', type=pathlib.Path, nargs='?')
    parser.add_argument('out', type=pathlib.Path, nargs='?')
    parser.add_argument('--c-header', type=pathlib.Path, metavar='OUT')
    parser.add_argument('--target', choices=TARGETS, default='128')
    args = parser.parse_args()
    global LOAD, INPUT_TYPE, TARGET
    TARGET = args.target
    LOAD, INPUT_TYPE = TARGETS[args.target]
    if args.c_header:
        if args.disk:
            parser.error('--c-header takes no disk images')
        args.c_header.write_bytes(c_header(assemble(), args.target).encode())
        return
    if not args.out:
        parser.error('needs a boot disk and an output image')
    args.out.write_bytes(install(args.disk.read_bytes(), assemble()))


if __name__ == '__main__':
    main()
