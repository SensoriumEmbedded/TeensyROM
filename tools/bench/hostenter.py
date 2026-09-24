#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Launch an extension, and report what the host left behind.   hostenter.py <path> [phrase]

  hostenter.py /NOSVC.crt
  hostenter.py /NOSVC.crt "extension host returned"

For a host that runs and hands the machine back rather than staying resident: the port
drops, the host does its work, it resets, and the main image prints the VmFail record on
the way back up. That record is the assertion -- when `phrase` is given. Without one the
only thing checked is that the board rebooted, and a reboot is not entry: every way a
launch fails after the jump reboots exactly the same way -- $01 "image did not start",
which minimal stamps *before* it jumps and which survives an image that never ran, through
$20 "module refused" (Source/Teensy/MinimalBoot/Common/VMFail.h). So a run with no phrase
reports what it saw and does not call it a pass.

This is the counterpart to hostcycle.py, which never enters the extension image at all.
Here the whole point is that it does, which is what makes it the test a third-party host
author wants -- docs/Architecture/Extension-Hosts.md calls the record one of the four
things a host owes, and this is what checks it was paid.

A host that stays resident instead (this repo's own, running a module) cannot be told
apart from a board that died. Every host image is built USB_DISABLED -- tools/
build-firmware.mjs passes it, and both Min_TeensyROM.h profiles #error without it -- so a
resident host takes the port down and never brings it back, which is what a hung board
looks like from here too: hostops.REBOOT_TIMEOUT seconds of waiting, then "the board did
not come back", which for a resident host is the right answer wearing the wrong words.
Use exttest.py for those, where silence is success. That same bound is the ceiling on how
long a returning host may work before it reads as dead.

`phrase` is optional and is matched case-insensitively against the serial output and the
C64 screen together. Leave it off to see what a host says before deciding what to assert;
note that the phrase comes from VmFail::describe() in the firmware *on the board*, so a
code newer than that firmware reads back as "unknown" rather than by name. The screen half of the
match goes through petscii_row, which decodes the charset the menu selects (MainMenu.asm
writes #$17 to $d018), so both cases read back -- see the Limits note in
tools/bench/README.md for the screen a launched program has switched.
"""
import sys

from hostops import run_step, show

# No flags here, so anything flag-shaped is a mistake -- exttest.py's --no-f5 travelling
# to the wrong script, or a phrase that starts with a dash. Silently dropping it drops
# the assertion with it, so say so instead.
args = [a for a in sys.argv[1:] if not a.startswith('--')]
if len(args) != len(sys.argv) - 1 or not 1 <= len(args) <= 2:
    raise SystemExit(__doc__)
path = args[0]
phrase = args[1] if len(args) == 2 else None


def prepare(tr):
    tr.drain()
    tr.launch(path)
    print(f'launched {path}; the host is entered if the port goes')


# 30 s: the launch itself is a reboot into minimal and then into the extension image, and
# the host's own work happens after that, bounded by REBOOT_TIMEOUT rather than by this.
# A launch the main image refuses never takes the port down at all, and that is what this
# window is really waiting to rule out.
result = run_step(prepare, 30)

print('\n--- screen ---')
show(result.screen)

if not result.rebooted:
    print(f'\nthe port stayed up on {result.image!r}: nothing entered the extension image.')
    print('A launch refused before the jump looks like this, but not which refusal it was:')
    print('tryLaunch (Common/VMRegistry.h) declines nine ways -- path too long, bad')
    print('client, missing package, ambiguous registry, failed preflight, no host')
    print('installed, host ABI, host services, launch record write failed -- and a path')
    print('that is not an extension at all just falls through to an ordinary launch in')
    print('four more places. Every one of those declines reports through')
    print('SendMsgPrintfln, which goes to the C64 and not to serial, and which the C64')
    print('reads only inside a WaitForTR* loop -- but a remote launch is itself one, so')
    print('the reason is usually already in the screen dump above. Measured: launching')
    print('/HELLO.crt against a host publishing no services printed')
    print('"Example host lacks service $1f" there with nothing else driving the C64.')
    raise SystemExit(1)

if phrase is None:
    print(f'\nthe board rebooted and came back on {result.image!r}. That alone does not say '
          'it entered the\nextension image -- a launch that failed after the jump comes back '
          'the same way. Re-run\nwith a phrase from the record above to assert which it was.')
    raise SystemExit(0)

if not result.said(phrase):
    raise SystemExit(f'\nthe board came back on {result.image!r}, but nothing said {phrase!r}')

print(f'\nPASS: entered the extension image and came back on {result.image!r}, '
      f'saying {phrase!r}')
