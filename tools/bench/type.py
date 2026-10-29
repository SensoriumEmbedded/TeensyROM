#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Type a line into the KERNAL keyboard buffer by DMA, a buffer-full at a time.

  type.py [--c128] '<text>'

`\\n` is RETURN, `\\\\` a backslash and `\\xNN` any key code.  Letters go in unshifted (uppercase on the
power-on character set).
--c128 uses the C128-mode buffer; C64 mode, including a C128 in 64 mode, is the default.
"""
import re
import sys
import time
from c64 import KEYBUF, KEYBUF_128, KEYBUF_SIZE, KEYCOUNT, KEYCOUNT_128
from trlink import Link

DRAIN_TIMEOUT = 30
RETURN = 0x0D
TOKEN = re.compile(r'\\x([0-9a-fA-F]{2})|\\(.?)|(.)', re.DOTALL)


def main():
    args = sys.argv[1:]
    c128 = '--c128' in args
    args = [a for a in args if a != '--c128']
    if len(args) != 1:
        sys.exit(__doc__.strip())
    keybuf, keycount = (KEYBUF_128, KEYCOUNT_128) if c128 else (KEYBUF, KEYCOUNT)
    try:
        data = keys(args[0])
    except ValueError as e:
        sys.exit(str(e))
    with Link() as tr:
        for i in range(0, len(data), KEYBUF_SIZE):
            wait_until_read(tr, keycount, i)
            chunk = data[i:i + KEYBUF_SIZE]
            if not (tr.poke(keybuf, chunk) and tr.poke(keycount, [len(chunk)])):
                sys.exit(f'poke refused at character {i}')
        wait_until_read(tr, keycount, len(data))


def keys(text):
    if not text.isascii():
        raise ValueError('ASCII only: write any other key code as \\xNN')
    out = bytearray()
    for code, escaped, plain in TOKEN.findall(text):
        if code:
            out.append(int(code, 16))
        elif plain:
            out += plain.upper().encode()
        elif escaped == 'n':
            out.append(RETURN)
        elif escaped == '\\':
            out += b'\\'
        else:
            raise ValueError(f'unknown escape \\{escaped}: use \\n, \\\\ or \\xNN')
    return bytes(out)


def wait_until_read(tr, keycount, at):
    start = time.time()
    while tr.peek(keycount, 1)[0]:
        if time.time() - start > DRAIN_TIMEOUT:
            sys.exit(f'keyboard buffer not read for {DRAIN_TIMEOUT} s, at character {at}')
        time.sleep(0.1)


if __name__ == '__main__':
    main()
