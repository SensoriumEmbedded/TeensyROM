#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Show the top screen row, its colour RAM and the VIC's colour registers.

The check for "the version banner is blank after a USB flash": if the text is in
screen RAM and colour RAM row 0 is all zeros, it is black on black. Fix over
serial with SetColorToken (0x64 0x22 0x02 0x04), then ResetC64Token (0x64 0xEE).
"""
from c64 import (COLOR_RAM, SCREEN_COLS, SCREEN_RAM, VIC_BACKGROUND, VIC_BORDER,
                 VIC_CTRL1, VIC_DEN, VIC_MEMPTR, petscii_row)
from trlink import Link

LOW_NIBBLE = 0x0F


def register(address):
    return vic[address - VIC_CTRL1]


with Link() as tr:
    text = tr.peek(SCREEN_RAM, SCREEN_COLS)
    color = tr.peek(COLOR_RAM, SCREEN_COLS)
    vic = tr.peek(VIC_CTRL1, VIC_BACKGROUND - VIC_CTRL1 + 1)

print('row0 text :', petscii_row(text))
print('row0 color:', ' '.join(f'{b & LOW_NIBBLE:x}' for b in color))
print()
print(f'${VIC_CTRL1:04X} ctrl1   = ${register(VIC_CTRL1):02X}   '
      f'(screen {"ON" if register(VIC_CTRL1) & VIC_DEN else "OFF"})')
print(f'${VIC_MEMPTR:04X} memptr  = ${register(VIC_MEMPTR):02X}   '
      f'(screen base ${((register(VIC_MEMPTR) >> 4) & LOW_NIBBLE) * 0x400:04X} within VIC bank)')
print(f'${VIC_BORDER:04X} border  = ${register(VIC_BORDER):02X}   ({register(VIC_BORDER) & LOW_NIBBLE})')
print(f'${VIC_BACKGROUND:04X} bkgnd   = ${register(VIC_BACKGROUND):02X}   '
      f'({register(VIC_BACKGROUND) & LOW_NIBBLE})')
