#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Wrap a GEOS 128 v2 U36 function ROM (32 KB, raw) as a C128 .crt the TR can launch.

  geos128crt.py <rom> <out.crt> [name]

Also writes the patched raw ROM next to the .crt as <out>.bin.
"""
import hashlib
import struct
import sys

AM1581_SHA256 = 'cbe786823301f3f1830f0ab32341d480be20220b8d5d900b2b5ee20b28c8042a'

STUB_RAM, STUB_ROM = 0x1300, 0x22   # the loader runs from $1300, copied from ROM offset $22


def stub(addr):
    return addr - STUB_RAM + STUB_ROM


def expect(rom, offset, want, what):
    if rom[offset:offset + len(want)] != want:
        sys.exit(f'{what}: expected {want.hex()} at ${0x8000 + offset:04X}, found '
                 f'{rom[offset:offset + len(want)].hex()} -- not a GEOS 128 v2 U36 ROM?')


def patch_cart_port(rom):
    expect(rom, 0x45, b'\xa2\x17', 'LDX #$17 (internal function ROM)')
    rom[0x46] = 0x2B                 # read config: external function ROM lo+hi, RAM bank 0
    expect(rom, 0x33, b'\x8c\x30\xd0', 'STY $D030')
    rom[0x33] = 0x2C                 # BIT $D030: stay at 1 MHz while reading the cartridge


def patch_sd2iec_1581(rom):
    # sd2iec starts its GEOS fast loader only when the M-W stream's CRC is one it knows.  The
    # ROM uploads a variant of Configure 2.0's 1581 code that differs in 12 bytes, so the CRC
    # misses 0x3F8D and GEOS restarts.  The drive code is compressed in ROM: hook the stub's
    # call to the unpacker and copy Configure's bytes over the unpacked copy in bank 1.
    expect(rom, stub(0x1343), bytes.fromhex('20402c'), 'JSR $2C40 (unpack)')
    expect(rom, stub(0x135C), b'\xbd' * 34, '$BD filler')
    rom[stub(0x1343):stub(0x1346)] = bytes.fromhex('205c13')         # JSR $135C
    rom[stub(0x135C):stub(0x137D)] = bytes.fromhex(
        '20402c'    # JSR $2C40
        'a205'      # LDX #5
        'bd7113'    # LDA $1371,X
        '9d737a'    # STA $7A73,X    drive $0410
        'bd7713'    # LDA $1377,X
        '9dc57a'    # STA $7AC5,X    drive $0462
        'ca10f1'    # DEX / BPL
        '60'        # RTS
        'a54148a54248'   # $1371: Configure's LDA $41 PHA LDA $42 PHA
        '688542688541')  # $1377: Configure's PLA STA $42 PLA STA $41


def crt(rom, name):
    header = (b'C128 CARTRIDGE  ' + struct.pack('>IHHBBB', 0x40, 0x0100, 0, 1, 1, 0)
              + b'\0' * 5 + name.encode()[:32].ljust(32, b'\0'))
    chips = b''
    for i, load in enumerate((0x8000, 0xC000)):
        chips += (b'CHIP' + struct.pack('>IHHHH', 0x4010, 0, 0, load, 0x4000)
                  + rom[i * 0x4000:(i + 1) * 0x4000])
    return header + chips


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__.strip())
    src, dst = sys.argv[1], sys.argv[2]
    name = sys.argv[3] if len(sys.argv) == 4 else 'GEOS 128'
    rom = bytearray(open(src, 'rb').read())
    if len(rom) != 0x8000 or rom[7:10] != b'CBM':
        sys.exit(f'{src}: not a 32 KB C128 function ROM')
    sha = hashlib.sha256(rom).hexdigest()
    patch_cart_port(rom)
    if sha == AM1581_SHA256:
        patch_sd2iec_1581(rom)
        print('Am1581: added the sd2iec 1581 hook')
    open(dst, 'wb').write(crt(rom, name))
    open(dst.rsplit('.', 1)[0] + '.bin', 'wb').write(rom)
    print(f'wrote {dst}')


main()
