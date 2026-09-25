#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Launch an extension and report how it ended.   exttest.py <path on SD> [--no-f5]

  exttest.py /HELLO.crt

Three outcomes, told apart by which image answers afterwards -- not by whether a
serial node exists, which is the thing that reads backwards:

  * The port drops and nothing answers: the extension image has the machine, and
    a module that keeps it never gives the port back. This is the success case
    for a resident module; look at the C64. A node usually stays visible here,
    because the jump out of the minimal image does not tear down the USB device
    it enumerated -- so the node is minimal's, unserviced, and answers nothing.
    A board that died looks identical from here, which is why this prints what it
    knows rather than claiming the module is fine.
  * The port drops and the main image answers: the extension image failed or
    exited and reset back. Its boot output carries the record the loader left
    behind, which is printed here -- and read, because the reboot alone does not
    say which of the two it was. A launch that faulted comes back exactly the way
    one that finished does.
  * The port never drops: nothing entered the extension image. The launch was
    refused -- a missing file, or a gate such as `<host> lacks service $<bits>` --
    and the C64 screen carries the reason, so it is printed.

Exit status, so this can gate a run rather than only narrate one:

  0  the module is resident and holding the machine, or it came back with a
     record that names a normal finish (hostops.FINISHED_NORMALLY).
  1  nothing entered the extension image, or it came back with anything else --
     a failure code, a record that is absent or truncated, or one carrying a code
     this copy has never heard of. That last group fails rather than passes for
     the reason hostops.py gives for INSTALLED and REMOVED: after a launch the
     failures outnumber the successes, so a new one must not arrive as a pass.

After a reset it presses F5 by DMA. That switches the menu to the SD drive, which
makes the C64 ask the firmware for a directory, and that -- a WaitForTR* loop --
is the only time it reads the message the loader left. Then it prints the screen.

hostenter.py is the counterpart for a host that hands the machine back and lets
you assert on the record; this one is for the hosts that stay resident.
"""
import sys
import time

from c64 import F5, screen_rows
from trlink import Link, answering_board, neighbours
from hostops import FINISHED_NORMALLY, REBOOT_TIMEOUT, Tee, finished_normally

args = [a for a in sys.argv[1:] if not a.startswith('--')]
if len(args) != 1:
    raise SystemExit(__doc__)
path = args[0]
press_f5 = '--no-f5' not in sys.argv


def show(tr):
    """Print the rows that have anything on them, and return the whole screen.
    The record reaches the C64 as well as the serial port, and which of the two
    carries it depends on whether the menu was in a WaitForTR* loop in time, so
    the caller matches against both."""
    rows = screen_rows(tr.screen())
    for n, line in enumerate(rows):
        if line.strip():
            print(f'{n:2d} |{line}|')
    return '\n'.join(rows)


with Link() as tr:
    # Sampled before the launch, because reconnect() needs to know which nodes were
    # already there in order to recognise one that appears in the board's place.
    port, known = tr.port, neighbours(tr.port)
    tr.drain()
    tr.launch(path)
    print(f'launched {path}; watching for the jump into the extension image')
    dropped = tr.stream(25)
    if not dropped:
        print('\nport stayed up: nothing entered the extension image.')
        print('--- screen, which carries the refusal ---')
        show(tr)
        sys.exit(1)

print('\n[port dropped]')
# Tee'd rather than printed straight through: the boot output is where the VmFail record
# lands, and this has to read it as well as show it -- the same split hostops.run_step
# makes, and for the same reason.
boot = Tee(sys.stdout)
# SystemExit as well as OSError: wr() raises it when the board takes nothing, and a
# node that enumerated but is not being serviced -- which is what the extension image
# leaves behind -- eventually fills and stops taking. That is the resident case
# arriving as an exception rather than as a None image, so it lands in the same arm.
tr, image = None, None
try:
    tr, image = answering_board(time.time() + REBOOT_TIMEOUT, port, known, out=boot)
except (SystemExit, OSError) as why:
    print(f'the port stopped taking bytes ({why})')
if image != 'main':
    if tr is not None:
        tr.close()
    print('no image answered: the extension image has the machine and is holding it.')
    print('That is what a resident module looks like from here -- look at the C64.')
    print('A hung board looks the same over USB; only the screen tells them apart.')
    sys.exit(0)

with tr:
    print('[reattached] -- the main image answered, so the extension image gave up the machine')

    print('\n--- screen after the reboot ---')
    seen = show(tr)
    if press_f5:
        print('\n--- F5, to put the C64 in a wait loop ---')
        tr.key(F5)

        time.sleep(3)
        # The later screen, not both: F5 redraws, and the record only reaches the C64
        # once the menu is inside the loop this keypress puts it in.
        seen = show(tr)

if finished_normally(boot.buf.getvalue() + '\n' + seen):
    sys.exit(0)

print('\nthe extension image gave the machine back, but nothing above says the run')
print('finished normally, so this is a failed run rather than a completed one.')
print('A normal finish is one of: ' + ', '.join(repr(p) for p in FINISHED_NORMALLY) + '.')
print('Anything else is a failure -- including a record that is absent or truncated, or')
print('one carrying a code this copy has never heard of, which reads back as "unknown".')
print('VmFail::describe() in Source/Teensy/MinimalBoot/Common/VMFail.h has them all.')
sys.exit(1)
