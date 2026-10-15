#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Wrap a GEOS 128 v2 U36 function ROM (32 KB, raw) as a C128 .crt the TR can launch.

  geos128crt.py [--clock YYYY-MM-DDTHH:MM[:SS]] <rom> <out.crt> [name]

GEOS boots with the date and time in the crt's clock record: the time it was built, or
--clock.  TR firmware with C128 crt clock support keeps it at local time while the cart runs.
Also writes the patched raw ROM next to the .crt as <out>.bin.
"""
import argparse
import datetime
import hashlib
import os
import struct
import sys

AM1581_SHA256 = 'cbe786823301f3f1830f0ab32341d480be20220b8d5d900b2b5ee20b28c8042a'

STUB_RAM, STUB_ROM = 0x1300, 0x22   # the loader runs from $1300, copied from ROM offset $22
ROUTINES = 0x8120                   # ROM filler the loader never copies
CLOCK_ADDR, FIX1581_ADDR = 0x8130, 0x81F9   # baked into the hook's JSRs and the routines' operands
CLOCK_SCRATCH = 0x1370              # CLOCK's buffer in the stub's filler, just past the hook

# The TR finds this record by its tag and keeps the seven bytes after it at local time.
CLOCK_TAG = b'TRCLOCK\x01'

# GEOS's first-boot defaults, as unpacked to bank 1 $2D88: CIA1 TOD 1:00 PM, then 3/23/92.
BOOT4 = bytes.fromhex('ad0fdc297f8d0fdc' 'a9818d0bdc' 'a9008d0adc8d09dc8d08dc'
                      'a95c8d1685' 'a9038d1785' 'a9178d1885')

CLOCK = bytes.fromhex(               # $8130, entered with $FF00 = $6A
    'a206'          # 8130  ldx #6           read the record until two reads agree
    'bd2881'        # 8132  lda $8128,x
    '9d7013'        # 8135  sta $1370,x
    'ca10f7'        # 8138  dex / bpl $8132
    'a206'          # 813b  ldx #6
    'bd2881'        # 813d  lda $8128,x
    'dd7013'        # 8140  cmp $1370,x
    'd0eb'          # 8143  bne $8130
    'ca10f5'        # 8145  dex / bpl $813d
    'a226'          # 8148  ldx #38          leave GEOS alone unless its defaults are there
    'bd882d'        # 814a  lda $2d88,x
    'ddd281'        # 814d  cmp $81d2,x
    'd069'          # 8150  bne $81bb
    'ca10f5'        # 8152  dex / bpl $814a
    'ad7113'        # 8155  lda $1371
    '8da12d'        # 8158  sta $2da1        LDA #year
    'ad7213'        # 815b  lda $1372
    '8da62d'        # 815e  sta $2da6        LDA #month
    'ad7313'        # 8161  lda $1373
    '8dab2d'        # 8164  sta $2dab        LDA #day
    'a94c8d882d'    # 8167  lda #$4c / sta $2d88
    'a9a08d892d'    # 816c  lda #$a0 / sta $2d89
    'a92d8d8a2d'    # 8171  lda #$2d / sta $2d8a    JMP $2DA0, past GEOS's own TOD set
    'ad0fdc'        # 8176  lda $dc0f        CRB bit 7 clear: writes set the time, not the alarm
    '297f'          # 8179  and #$7f
    '8d0fdc'        # 817b  sta $dc0f
    'a000'          # 817e  ldy #0           0-23 -> 12-hour BCD with the PM flag
    'ad7413'        # 8180  lda $1374
    'c90c'          # 8183  cmp #12
    '9004'          # 8185  bcc $818b
    'e90c'          # 8187  sbc #12
    'a080'          # 8189  ldy #$80
    '8c7713'        # 818b  sty $1377
    'aa'            # 818e  tax
    'd00a'          # 818f  bne $819b
    'ad7713'        # 8191  lda $1377        the 6526 flips AM/PM when 12 is written
    '4980'          # 8194  eor #$80
    '8d7713'        # 8196  sta $1377
    'a90c'          # 8199  lda #12
    '20bc81'        # 819b  jsr $81bc
    '0d7713'        # 819e  ora $1377
    '8d0bdc'        # 81a1  sta $dc0b
    'ad7513'        # 81a4  lda $1375
    '20bc81'        # 81a7  jsr $81bc
    '8d0adc'        # 81aa  sta $dc0a
    'ad7613'        # 81ad  lda $1376
    '20bc81'        # 81b0  jsr $81bc
    '8d09dc'        # 81b3  sta $dc09
    'a900'          # 81b6  lda #0
    '8d08dc'        # 81b8  sta $dc08        tenths last: restarts the clock
    '60'            # 81bb  rts
    'a2ff38'        # 81bc  ldx #$ff / sec   A = 0-99 -> BCD
    'e8'            # 81bf  inx
    'e90a'          # 81c0  sbc #10
    'b0fb'          # 81c2  bcs $81bf
    '690a'          # 81c4  adc #10
    '8d7813'        # 81c6  sta $1378
    '8a0a0a0a0a'    # 81c9  txa / asl x4
    '0d7813'        # 81ce  ora $1378
    '60'            # 81d1  rts
) + BOOT4           # 81d2

# sd2iec starts its GEOS fast loader only when the M-W stream's CRC is one it knows.  The Am1581
# ROM uploads a variant of Configure 2.0's 1581 code that differs in 12 bytes, so the CRC misses
# 0x3F8D and GEOS restarts.  This copies Configure's bytes over the unpacked drive code.
FIX1581 = bytes.fromhex(             # $81f9
    'a205'          # 81f9  ldx #5
    'bd0b82'        # 81fb  lda $820b,x
    '9d737a'        # 81fe  sta $7a73,x      drive $0410
    'bd1182'        # 8201  lda $8211,x
    '9dc57a'        # 8204  sta $7ac5,x      drive $0462
    'ca10f1'        # 8207  dex / bpl $81fb
    '60'            # 820a  rts
    'a54148a54248'  # 820b  Configure's LDA $41 PHA LDA $42 PHA
    '688542688541') # 8211  Configure's PLA STA $42 PLA STA $41


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


def clock_record(t):
    return CLOCK_TAG + bytes([t.year // 100, t.year % 100, t.month, t.day,
                              t.hour, t.minute, t.second, 0])


def patch_boot_hook(rom, t, am1581):
    # The stub's call to the unpacker goes to a hook in its $BD filler.  Once GEOS is unpacked
    # into bank 1, the hook maps the cartridge in and runs the routines in the ROM filler.
    expect(rom, stub(0x1343), bytes.fromhex('20402c'), 'JSR $2C40 (unpack)')
    expect(rom, stub(0x135C), b'\xbd' * 34, '$BD filler')
    code = clock_record(t) + CLOCK + (FIX1581 if am1581 else b'')
    start = ROUTINES - 0x8000
    fill = rom[start]
    expect(rom, start, bytes([fill]) * len(code), 'filler for the boot routines')
    assert ROUTINES + len(clock_record(t)) == CLOCK_ADDR
    assert CLOCK_ADDR + len(CLOCK) == FIX1581_ADDR
    rom[start:start + len(code)] = code
    rom[stub(0x1343):stub(0x1346)] = bytes.fromhex('205c13')         # JSR $135C
    hook = bytes.fromhex(
        '20402c'    # JSR $2C40
        'a96a'      # LDA #$6A           RAM bank 1, I/O, cartridge ROM
        '8d00ff'    # STA $FF00
        '203081'    # JSR $8130          clock
        + ('20f981' if am1581 else '') +   # JSR $81F9   1581 fix
        'a97f'      # LDA #$7F
        '8d00ff'    # STA $FF00
        '60')       # RTS
    assert 0x135C + len(hook) <= CLOCK_SCRATCH
    rom[stub(0x135C):stub(0x135C) + len(hook)] = hook


def crt(rom, name):
    header = (b'C128 CARTRIDGE  ' + struct.pack('>IHHBBB', 0x40, 0x0100, 0, 1, 1, 0)
              + b'\0' * 5 + name.encode()[:32].ljust(32, b'\0'))
    chips = b''
    for i, load in enumerate((0x8000, 0xC000)):
        chips += (b'CHIP' + struct.pack('>IHHHH', 0x4010, 0, 0, load, 0x4000)
                  + rom[i * 0x4000:(i + 1) * 0x4000])
    return header + chips


def main():
    ap = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    ap.add_argument('--clock', type=datetime.datetime.fromisoformat,
                    help='date and time GEOS boots with when the TR does not set it '
                         '(default: now)')
    ap.add_argument('rom')
    ap.add_argument('out')
    ap.add_argument('name', nargs='?', default='GEOS 128')
    args = ap.parse_args()
    raw = os.path.splitext(args.out)[0] + '.bin'
    if os.path.abspath(raw) == os.path.abspath(args.out):
        sys.exit(f'{args.out}: the patched ROM is written as <out>.bin, so the .crt cannot be a .bin')
    rom = bytearray(open(args.rom, 'rb').read())
    if len(rom) != 0x8000 or rom[7:10] != b'CBM':
        sys.exit(f'{args.rom}: not a 32 KB C128 function ROM')
    am1581 = hashlib.sha256(rom).hexdigest() == AM1581_SHA256
    patch_cart_port(rom)
    t = args.clock or datetime.datetime.now()
    patch_boot_hook(rom, t, am1581)
    print(f'boot clock {t:%Y-%m-%d %H:%M:%S}' + (', Am1581: added the sd2iec 1581 fix' if am1581 else ''))
    open(args.out, 'wb').write(crt(rom, args.name))
    open(raw, 'wb').write(rom)
    print(f'wrote {args.out}')


main()
