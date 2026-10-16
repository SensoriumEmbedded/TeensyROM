#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build a C128 .crt the TR can launch that boots GEOS 128 v2 the way its boot disk does.

  geos128crt.py [--clock YYYY-MM-DDTHH:MM[:SS]] <geoboot128> <out.crt> [name]

<geoboot128> is the GEOBOOT128 file from a GEOS 128 v2 boot disk, e.g.
`c1541 boot.d81 -read geoboot128 geoboot128.prg`.  The cart unpacks it into RAM bank 1 and
starts it, and GEOS loads the rest from drive 8 as a disk boot does.  GEOS boots with the date
and time in the crt's clock record: the time it was built, or --clock.  TR firmware with C128
crt clock support keeps it at local time while the cart runs.
Also writes the raw ROM next to the .crt as <out>.bin.
"""
import argparse
import datetime
import os
import struct
import sys

LOAD, ENTRY = 0x2B19, 0x2F3C        # GEOBOOT128's load address, and where GEOS's ROM loader enters it
SIZE = 38226
DEFAULTS = 0x2C29                   # GEOS's first-boot TOD and date, operands of BOOT4 below
RESET_KEY = 0x6FF5                  # unpacks to bank 1 $FFF5 with the rest of GEOS's top page
STUB_RAM, STUB_ROM = 0x1300, 0x8021
RECORD, CLOCK_ADDR, DATA = 0x81F0, 0x8200, 0x8300   # baked into the stub and CLOCK operands
CLOCK_SCRATCH = 0x1B00
VECTORS, COLD = 0xFFF9, 0x800A

# The TR finds this record by its tag and keeps the seven bytes after it at local time.
CLOCK_TAG = b'TRCLOCK\x01'

# GEOS's first-boot defaults at DEFAULTS: CIA1 TOD 1:00 PM, then 3/23/92.
BOOT4 = bytes.fromhex('ad0fdc297f8d0fdc' 'a9818d0bdc' 'a9008d0adc8d09dc8d08dc'
                      'a95c8d1685' 'a9038d1785' 'a9178d1885')

STUB = bytes.fromhex(                # $8000, a C128 function ROM; copies $8021 on to $1300
    'ea'            # 8000  nop
    'ea'            # 8001  nop
    'ea'            # 8002  nop
    '4c0a80'        # 8003  jmp cold
    'ff'            # 8006  !byte $ff
    '43424d'        # 8007  !text "CBM"
    '78'            # 800a  cold sei      IRQs are on, and this ROM's top page is the CPU vectors
    'ad00ff'        # 800b  lda $ff00
    '293f'          # 800e  and #$3f
    '8d00ff'        # 8010  sta $ff00
    'a200'          # 8013  ldx #0
    'bd2180'        # 8015  - lda stubrom,x
    '9d0013'        # 8018  sta $1300,x
    'e8'            # 801b  inx
    'd0f7'          # 801c  bne -
    '4c0013'        # 801e  jmp $1300
    '78'            # 1300  sei
    'a93e'          # 1301  lda #$3e
    '8d00ff'        # 1303  sta $ff00
    'a906'          # 1306  lda #$06
    '8d06d5'        # 1308  sta $d506
    'a900'          # 130b  lda #0
    '8d11d0'        # 130d  sta $d011
    'a900'          # 1310  lda #<DATA
    '8502'          # 1312  sta src
    'a983'          # 1314  lda #>DATA
    '8503'          # 1316  sta src+1
    'a919'          # 1318  lda #$19
    '8504'          # 131a  sta dst
    'a92b'          # 131c  lda #$2b
    '8505'          # 131e  sta dst+1
    'a97f'          # 1320  lda #$7f
    '8d00ff'        # 1322  sta $ff00
    '208513'        # 1325  next jsr get
    'f03d'          # 1328  beq done
    '300c'          # 132a  bmi copy
    'aa'            # 132c  tax
    '208513'        # 132d  - jsr get
    '209c13'        # 1330  jsr put
    'ca'            # 1333  dex
    'd0f7'          # 1334  bne -
    'f0ed'          # 1336  beq next
    '297f'          # 1338  copy and #$7f
    '18'            # 133a  clc
    '6903'          # 133b  adc #3
    'aa'            # 133d  tax
    '208513'        # 133e  jsr get
    '8508'          # 1341  sta tmp
    '208513'        # 1343  jsr get
    '8509'          # 1346  sta tmp+1
    '38'            # 1348  sec
    'a504'          # 1349  lda dst
    'e508'          # 134b  sbc tmp
    '8506'          # 134d  sta ref
    'a505'          # 134f  lda dst+1
    'e509'          # 1351  sbc tmp+1
    '8507'          # 1353  sta ref+1
    'a000'          # 1355  - ldy #0
    'b106'          # 1357  lda (ref),y
    '209c13'        # 1359  jsr put
    'e606'          # 135c  inc ref
    'd002'          # 135e  bne +
    'e607'          # 1360  inc ref+1
    'ca'            # 1362  + dex
    'd0f0'          # 1363  bne -
    'f0be'          # 1365  beq next
    'a96a'          # 1367  done lda #$6a
    '8d00ff'        # 1369  sta $ff00
    '200082'        # 136c  jsr CLOCK
    'a97e'          # 136f  lda #$7e
    '8d00ff'        # 1371  sta $ff00
    'a91b'          # 1374  lda #$1b
    '8d11d0'        # 1376  sta $d011
    'a900'          # 1379  lda #0
    '8d30d0'        # 137b  sta $d030
    'a908'          # 137e  lda #8
    '85ba'          # 1380  sta $ba
    '4c3c2f'        # 1382  jmp $2f3c
    'a92b'          # 1385  get lda #$2b
    '8d00ff'        # 1387  sta $ff00
    'a000'          # 138a  ldy #0
    'b102'          # 138c  lda (src),y
    'a07f'          # 138e  ldy #$7f
    '8c00ff'        # 1390  sty $ff00
    'e602'          # 1393  inc src
    'd002'          # 1395  bne +
    'e603'          # 1397  inc src+1
    '48'            # 1399  + pha
    '68'            # 139a  pla
    '60'            # 139b  rts
    'a000'          # 139c  put ldy #0
    '9104'          # 139e  sta (dst),y
    'e604'          # 13a0  inc dst
    'd002'          # 13a2  bne +
    'e605'          # 13a4  inc dst+1
    '60')           # 13a6  + rts

CLOCK = bytes.fromhex(               # $8200, entered with $FF00 = $6A
    'a206'          # 8200  CLOCK ldx #6
    'bdf881'        # 8202  - lda REC,x
    '9d001b'        # 8205  sta SCR,x
    'ca'            # 8208  dex
    '10f7'          # 8209  bpl -
    'a206'          # 820b  ldx #6
    'bdf881'        # 820d  - lda REC,x
    'dd001b'        # 8210  cmp SCR,x
    'd0eb'          # 8213  bne CLOCK
    'ca'            # 8215  dex
    '10f5'          # 8216  bpl -
    'a226'          # 8218  ldx #38
    'bd292c'        # 821a  - lda DEF,x
    'dda282'        # 821d  cmp BOOT4,x
    'd069'          # 8220  bne out
    'ca'            # 8222  dex
    '10f5'          # 8223  bpl -
    'ad011b'        # 8225  lda SCR+1
    '8d422c'        # 8228  sta DEF+$19
    'ad021b'        # 822b  lda SCR+2
    '8d472c'        # 822e  sta DEF+$1e
    'ad031b'        # 8231  lda SCR+3
    '8d4c2c'        # 8234  sta DEF+$23
    'a94c'          # 8237  lda #$4c
    '8d292c'        # 8239  sta DEF
    'a941'          # 823c  lda #<(DEF+$18)
    '8d2a2c'        # 823e  sta DEF+1
    'a92c'          # 8241  lda #>(DEF+$18)
    '8d2b2c'        # 8243  sta DEF+2
    'ad0fdc'        # 8246  lda $dc0f
    '297f'          # 8249  and #$7f
    '8d0fdc'        # 824b  sta $dc0f
    'a000'          # 824e  ldy #0
    'ad041b'        # 8250  lda SCR+4
    'c90c'          # 8253  cmp #12
    '9004'          # 8255  bcc +
    'e90c'          # 8257  sbc #12
    'a080'          # 8259  ldy #$80
    '8c071b'        # 825b  + sty SCR+7
    'aa'            # 825e  tax
    'd00a'          # 825f  bne +
    'ad071b'        # 8261  lda SCR+7
    '4980'          # 8264  eor #$80
    '8d071b'        # 8266  sta SCR+7
    'a90c'          # 8269  lda #12
    '208c82'        # 826b  + jsr bcd
    '0d071b'        # 826e  ora SCR+7
    '8d0bdc'        # 8271  sta $dc0b
    'ad051b'        # 8274  lda SCR+5
    '208c82'        # 8277  jsr bcd
    '8d0adc'        # 827a  sta $dc0a
    'ad061b'        # 827d  lda SCR+6
    '208c82'        # 8280  jsr bcd
    '8d09dc'        # 8283  sta $dc09
    'a900'          # 8286  lda #0
    '8d08dc'        # 8288  sta $dc08
    '60'            # 828b  out rts
    'a2ff'          # 828c  bcd ldx #$ff
    '38'            # 828e  sec
    'e8'            # 828f  - inx
    'e90a'          # 8290  sbc #10
    'b0fb'          # 8292  bcs -
    '690a'          # 8294  adc #10
    '8d081b'        # 8296  sta SCR+8
    '8a'            # 8299  txa
    '0a'            # 829a  asl
    '0a'            # 829b  asl
    '0a'            # 829c  asl
    '0a'            # 829d  asl
    '0d081b'        # 829e  ora SCR+8
    '60'            # 82a1  rts
) + BOOT4           # 82a2

# Addresses the code above has baked in, as (code, offset of the operand, what it must be).
GET, PUT, BCD = STUB_RAM + 0x85, STUB_RAM + 0x9C, CLOCK_ADDR + 0x8C
OPERANDS = ((STUB, 0x04, COLD), (STUB, 0x16, STUB_ROM), (STUB, 0x19, STUB_RAM), (STUB, 0x1F, STUB_RAM),
            (STUB, 0x47, GET), (STUB, 0x4F, GET), (STUB, 0x52, PUT), (STUB, 0x60, GET),
            (STUB, 0x65, GET), (STUB, 0x7B, PUT), (STUB, 0x8E, CLOCK_ADDR), (STUB, 0xA4, ENTRY),
            (CLOCK, 0x03, RECORD + len(CLOCK_TAG)), (CLOCK, 0x06, CLOCK_SCRATCH),
            (CLOCK, 0x0E, RECORD + len(CLOCK_TAG)), (CLOCK, 0x11, CLOCK_SCRATCH),
            (CLOCK, 0x1B, DEFAULTS), (CLOCK, 0x1E, CLOCK_ADDR + len(CLOCK) - len(BOOT4)),
            (CLOCK, 0x26, CLOCK_SCRATCH + 1), (CLOCK, 0x29, DEFAULTS + 0x19),
            (CLOCK, 0x2C, CLOCK_SCRATCH + 2), (CLOCK, 0x2F, DEFAULTS + 0x1E),
            (CLOCK, 0x32, CLOCK_SCRATCH + 3), (CLOCK, 0x35, DEFAULTS + 0x23),
            (CLOCK, 0x3A, DEFAULTS), (CLOCK, 0x3F, DEFAULTS + 1), (CLOCK, 0x44, DEFAULTS + 2),
            (CLOCK, 0x51, CLOCK_SCRATCH + 4), (CLOCK, 0x5C, CLOCK_SCRATCH + 7),
            (CLOCK, 0x62, CLOCK_SCRATCH + 7), (CLOCK, 0x67, CLOCK_SCRATCH + 7),
            (CLOCK, 0x6C, BCD), (CLOCK, 0x6F, CLOCK_SCRATCH + 7), (CLOCK, 0x75, CLOCK_SCRATCH + 5),
            (CLOCK, 0x78, BCD), (CLOCK, 0x7E, CLOCK_SCRATCH + 6), (CLOCK, 0x81, BCD),
            (CLOCK, 0x97, CLOCK_SCRATCH + 8), (CLOCK, 0x9F, CLOCK_SCRATCH + 8))
LZ_MIN, LZ_MAX_COPY, LZ_MAX_RUN = 3, 127 + 3, 127


def lz_pack(data):
    """The stub's format.  A control byte of 0 ends; 1-127 is a run of that many literal bytes;
    $80 + n copies n + 3 bytes from the distance in the two bytes that follow (lo, hi)."""
    out, lit, seen = bytearray(), bytearray(), {}

    def flush():
        while lit:
            n = min(len(lit), LZ_MAX_RUN)
            out.append(n)
            out.extend(lit[:n])
            del lit[:n]

    i = 0
    while i < len(data):
        best, dist = 0, 0
        for j in reversed(seen.get(data[i:i + LZ_MIN], [])[-256:]):
            n = 0
            while i + n < len(data) and n < LZ_MAX_COPY and data[j + n] == data[i + n]:
                n += 1
            if n > best:
                best, dist = n, i - j
                if n == LZ_MAX_COPY:
                    break
        if best >= 4 or (best == LZ_MIN and not lit):
            flush()
            out += bytes([0x80 | (best - LZ_MIN), dist & 0xFF, dist >> 8])
            for k in range(i, i + best):
                seen.setdefault(data[k:k + LZ_MIN], []).append(k)
            i += best
        else:
            seen.setdefault(data[i:i + LZ_MIN], []).append(i)
            lit.append(data[i])
            i += 1
    flush()
    out.append(0)
    return bytes(out)


def lz_unpack(packed):
    out, i = bytearray(), 0
    while packed[i]:
        n = packed[i]
        if n < 0x80:
            out += packed[i + 1:i + 1 + n]
            i += 1 + n
        else:
            dist = packed[i + 1] | packed[i + 2] << 8
            for _ in range((n & 0x7F) + LZ_MIN):
                out.append(out[-dist])
            i += 3
    return bytes(out)


def expect(image, addr, want, what):
    at = addr - LOAD
    if image[at:at + len(want)] != want:
        sys.exit(f'{what}: expected {want.hex()} at ${addr:04X}, found '
                 f'{image[at:at + len(want)].hex()} -- not a GEOS 128 v2 GEOBOOT128?')


def break_reset_key(image):
    # GEOS puts "CBM" and $03E4 at bank 1 $FFF5, and the C128 KERNAL jumps through that on
    # every C128-mode reset until the power goes off.  The TR menu boots in C64 mode, which
    # overwrites $03E4, so the next C128 crt crashes.  GEOS's own ROM build breaks it too.
    expect(image, RESET_KEY, b'CBM' + struct.pack('<H', 0x03E4), 'reset key')
    image[RESET_KEY - LOAD] = 0


def clock_record(t):
    return CLOCK_TAG + bytes([t.year // 100, t.year % 100, t.month, t.day,
                              t.hour, t.minute, t.second, 0])


def rom_image(image, t):
    for code, offset, want in OPERANDS:
        assert code[offset] | code[offset + 1] << 8 == want, (hex(offset), hex(want))
    assert STUB[0x32] | STUB[0x36] << 8 == DATA and STUB[0x3A] | STUB[0x3E] << 8 == LOAD
    assert CLOCK[0x3D] | CLOCK[0x42] << 8 == DEFAULTS + 0x18 and CLOCK[0x19] == len(BOOT4) - 1
    assert 0x8000 + len(STUB) <= RECORD and len(STUB) - (STUB_ROM - 0x8000) <= 0x100
    assert RECORD + len(clock_record(t)) == CLOCK_ADDR and CLOCK_ADDR + len(CLOCK) <= DATA
    packed = lz_pack(bytes(image))
    assert lz_unpack(packed) == image
    if DATA + len(packed) > 0xFF00:   # the MMU registers at $FF00 hide the ROM from there up
        sys.exit(f'GEOBOOT128 packs to {len(packed)} bytes, {DATA + len(packed) - 0xFF00} too many')
    # An IRQ that arrives before the cold start's SEI is still asserted when an RTI returns,
    # and the 6502 takes it again before the next instruction, so IRQ enters the cold start.
    rom = bytearray(b'\xff' * 0x8000)
    for addr, data in ((0x8000, STUB), (RECORD, clock_record(t)), (CLOCK_ADDR, CLOCK),
                       (DATA, packed), (VECTORS, b'\x40' + struct.pack('<HHH', VECTORS, VECTORS, COLD))):
        rom[addr - 0x8000:addr - 0x8000 + len(data)] = data   # $40 is RTI
    if rom[0x4007:0x400A] == b'CBM':   # the KERNAL would take $C000 for a second function ROM
        sys.exit('GEOBOOT128 packs to "CBM" at $C007, where the KERNAL looks for a function ROM')
    return rom


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
    ap.add_argument('geoboot')
    ap.add_argument('out')
    ap.add_argument('name', nargs='?', default='GEOS 128')
    args = ap.parse_args()
    raw = os.path.splitext(args.out)[0] + '.bin'
    if os.path.abspath(raw) == os.path.abspath(args.out):
        sys.exit(f'{args.out}: the ROM is written as <out>.bin, so the .crt cannot be a .bin')
    prg = open(args.geoboot, 'rb').read()
    if len(prg) != 2 + SIZE or prg[:2] != struct.pack('<H', LOAD):
        sys.exit(f'{args.geoboot}: not GEOBOOT128 ({2 + SIZE} bytes loading at ${LOAD:04X})')
    image = bytearray(prg[2:])
    expect(image, DEFAULTS, BOOT4, "GEOS's first-boot clock")
    break_reset_key(image)
    t = args.clock or datetime.datetime.now()
    rom = rom_image(image, t)
    print(f'boot clock {t:%Y-%m-%d %H:%M:%S}')
    open(args.out, 'wb').write(crt(rom, args.name))
    open(raw, 'wb').write(rom)
    print(f'wrote {args.out}')


main()
