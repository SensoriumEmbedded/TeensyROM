#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Type a line into the KERNAL keyboard buffer by DMA, a buffer-full at a time.

  type.py [--c128] '<text>'

`\\n` is RETURN and `\\xNN` is any key code.  Letters go in unshifted (uppercase on the
power-on character set).
--c128 uses the C128-mode buffer; C64 mode, including a C128 in 64 mode, is the default.
"""
import sys
import time
from c64 import KEYBUF, KEYBUF_128, KEYBUF_SIZE, KEYCOUNT, KEYCOUNT_128
from trlink import Link

DRAIN_TIMEOUT = 30


def main():
    args = sys.argv[1:]
    c128 = '--c128' in args
    args = [a for a in args if a != '--c128']
    if len(args) != 1:
        sys.exit(__doc__.strip())
    keybuf, keycount = (KEYBUF_128, KEYCOUNT_128) if c128 else (KEYBUF, KEYCOUNT)
    if not args[0].isascii():
        sys.exit('ASCII only: write any other key code as \\xNN')
    try:
        data = args[0].encode().decode('unicode_escape').encode('latin1')
    except UnicodeEncodeError:
        sys.exit('key codes go up to \\xFF')
    data = data.upper().replace(b'\n', b'\r')
    with Link() as tr:
        for i in range(0, len(data), KEYBUF_SIZE):
            wait_until_read(tr, keycount, i)
            chunk = data[i:i + KEYBUF_SIZE]
            if not (tr.poke(keybuf, chunk) and tr.poke(keycount, [len(chunk)])):
                sys.exit(f'poke refused at character {i}')
        wait_until_read(tr, keycount, len(data))


def wait_until_read(tr, keycount, at):
    start = time.time()
    while tr.peek(keycount, 1)[0]:
        if time.time() - start > DRAIN_TIMEOUT:
            sys.exit(f'keyboard buffer not read for {DRAIN_TIMEOUT} s, at character {at}')
        time.sleep(0.1)


main()
