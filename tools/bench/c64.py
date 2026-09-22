# SPDX-License-Identifier: MIT
"""C64 memory locations and screen codes, for the scripts that peek and poke."""

SCREEN_RAM = 0x0400
SCREEN_COLS, SCREEN_ROWS = 40, 25
SCREEN_BYTES = SCREEN_COLS * SCREEN_ROWS
COLOR_RAM = 0xD800

VIC_CTRL1 = 0xD011          # bit 4 is DEN, the screen-on bit
VIC_MEMPTR = 0xD018
VIC_BORDER = 0xD020
VIC_BACKGROUND = 0xD021
VIC_DEN = 0x10

KEYBUF, KEYCOUNT = 0x0277, 0x00C6  # KERNAL keyboard buffer, and the count GETIN reads
PETSCII_Y = 0x59
F1, F3, F5, F7 = 0x85, 0x86, 0x87, 0x88

REVERSE_VIDEO = 0x80
SCREEN_CODE_AT = 0
SCREEN_CODE_A, SCREEN_CODE_Z = 1, 26


def petscii_row(row):
    """Screen codes as text, uppercase/graphics charset only: 1-26 decode to A-Z
    there, and the lower/uppercase charset's 65-90 do not decode. Digits and
    punctuation survive either way; anything undecoded is '.'."""
    out = []
    for code in row:
        code &= ~REVERSE_VIDEO
        if code == SCREEN_CODE_AT:
            out.append('@')
        elif SCREEN_CODE_A <= code <= SCREEN_CODE_Z:
            out.append(chr(code - SCREEN_CODE_A + ord('A')))
        elif ord(' ') <= code <= ord('?'):   # digits and punctuation share PETSCII values
            out.append(chr(code))
        else:
            out.append('.')
    return ''.join(out)


def screen_rows(screen):
    return [petscii_row(screen[r * SCREEN_COLS:(r + 1) * SCREEN_COLS]) for r in range(SCREEN_ROWS)]
