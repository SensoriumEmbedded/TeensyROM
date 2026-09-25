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
SCREEN_CODE_SHIFT_A, SCREEN_CODE_SHIFT_Z = 65, 90

# Which glyph set the VIC is pointed at, because screen codes 1-26 and 65-90 mean
# different letters in each. $17 is what MainMenu.asm's TextScreenMemColor writes to
# $d018, so the menu -- and anything reached from it -- is in LOWER_UPPER; UPPER_GFX is
# the power-on default a launched program may go back to.
LOWER_UPPER = 'lower/uppercase'
UPPER_GFX = 'uppercase/graphics'


def petscii_row(row, charset=LOWER_UPPER):
    """Screen codes as text, decoded for `charset` -- LOWER_UPPER by default, because
    that is the one the menu puts the C64 in. Codes 1-26 are the unshifted letters and
    65-90 the shifted ones, so in LOWER_UPPER they read a-z and A-Z, and in UPPER_GFX
    they read A-Z and graphics. Digits and punctuation survive either way; anything with
    no letter in the selected set -- graphics, the line-drawing codes -- is '.'.

    Callers that match a phrase should still fold case (hostops.Outcome.said does), since
    which case a glyph carries is a property of the screen, not of what was asserted."""
    lower_upper = charset == LOWER_UPPER
    out = []
    for code in row:
        code &= ~REVERSE_VIDEO
        if code == SCREEN_CODE_AT:
            out.append('@')
        elif SCREEN_CODE_A <= code <= SCREEN_CODE_Z:
            base = ord('a') if lower_upper else ord('A')
            out.append(chr(code - SCREEN_CODE_A + base))
        elif lower_upper and SCREEN_CODE_SHIFT_A <= code <= SCREEN_CODE_SHIFT_Z:
            out.append(chr(code - SCREEN_CODE_SHIFT_A + ord('A')))
        elif ord(' ') <= code <= ord('?'):   # digits and punctuation share PETSCII values
            out.append(chr(code))
        else:
            out.append('.')
    return ''.join(out)


def screen_rows(screen, charset=LOWER_UPPER):
    return [petscii_row(screen[r * SCREEN_COLS:(r + 1) * SCREEN_COLS], charset)
            for r in range(SCREEN_ROWS)]
