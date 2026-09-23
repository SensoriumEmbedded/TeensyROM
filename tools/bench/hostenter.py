#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Launch an extension, and report what the host left behind.   hostenter.py <path> [phrase]

  hostenter.py /NOSVC.crt
  hostenter.py /NOSVC.crt "extension host returned"

For a host that runs and hands the machine back rather than staying resident: the port
drops, the host does its work, it resets, and the main image prints the VmFail record on
the way back up. That record is the assertion.

This is the counterpart to hostcycle.py, which never enters the extension image at all.
Here the whole point is that it does, which is what makes it the test a third-party host
author wants -- docs/Architecture/Extension-Hosts.md calls the record one of the four
things a host owes, and this is what checks it was paid.

A host that stays resident instead (this repo's own, running a module) does not come back,
so the port stays down and this reports that rather than waiting for a reboot that is not
coming. Use exttest.py for those.

`phrase` is optional and is matched case-insensitively against the serial output and the
C64 screen together. Leave it off to see what a host says before deciding what to assert;
note that the phrase comes from VmFail::describe() in the firmware *on the board*, so a
code newer than that firmware reads back as "unknown" rather than by name.
"""
import sys

from hostops import run_step, show

args = [a for a in sys.argv[1:] if not a.startswith('--')]
if not 1 <= len(args) <= 2:
    raise SystemExit(__doc__)
path = args[0]
phrase = args[1] if len(args) == 2 else None


def prepare(tr):
    tr.drain()
    tr.launch(path)
    print(f'launched {path}; the host is entered if the port goes')


# 30 s: the launch itself is a reboot into minimal and then into the extension image, and
# the host's own work happens after that. A host that never gives the port up is the
# resident case, reported below rather than waited out.
result = run_step(prepare, 30)

print(f'\n--- screen ---')
show(result.screen)

if not result.rebooted:
    print(f'\nthe port stayed up on {result.image!r}: nothing entered the extension image.')
    print('A launch refused before the jump looks like this -- the C64 screen carries the')
    print('reason. So does a host that is still running, if it kept USB, which none do.')
    raise SystemExit(1)

if phrase and not result.said(phrase):
    raise SystemExit(f'\nthe board came back on {result.image!r}, but nothing said {phrase!r}')

print(f'\nPASS: entered the extension image and came back on {result.image!r}' +
      (f', saying {phrase!r}' if phrase else ''))
