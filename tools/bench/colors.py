#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Show the top screen row, its colour RAM and the VIC's colour registers.

The check for "the version banner is blank after a USB flash": if the text is in
screen RAM and colour RAM row 0 is all zeros, it is black on black. Fix over
serial with SetColorToken (0x64 0x22 0x02 0x04), then ResetC64Token (0x64 0xEE).
"""
from trlink import Link, petscii_row

with Link() as tr:
    scr = tr.peek(0x0400, 40)
    col = tr.peek(0xD800, 40)
    regs = tr.peek(0xD011, 0x20)      # $D011..$D030
print('row0 text :', petscii_row(scr))
print('row0 color:', ' '.join(f'{b & 15:x}' for b in col))
print()
print(f'$D011 ctrl1   = ${regs[0x00]:02X}   (bit4 DEN screen {"ON" if regs[0x00] & 0x10 else "OFF"})')
print(f'$D018 memptr  = ${regs[0x07]:02X}   (screen base ${((regs[0x07] >> 4) & 15) * 0x400:04X} within VIC bank)')
print(f'$D020 border  = ${regs[0x0F]:02X}   ({regs[0x0F] & 15})')
print(f'$D021 bkgnd   = ${regs[0x10]:02X}   ({regs[0x10] & 15})')
