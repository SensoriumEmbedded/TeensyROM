#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Install/remove round trip against a real board, unattended.   hostcycle.py <local.trh>

  hostcycle.py build/firmware/TEENSYROM.TRH

Six steps, each asserted:

  1. remove          -- normalise: whatever was there, the slot is empty after this
  2. remove again    -- must refuse without rebooting: nothing left to clear
  3. install         -- must reboot and report the host installed
  4. remove          -- must reboot and report it removed
  5. remove again    -- must refuse again: the tag really is gone, not just overwritten
  6. install         -- must reboot and report installed, leaving the board usable

Steps 2 and 5 are the ones worth the extra minute. Without them an install that never
wrote and a removal that never cleared both still "pass": every reboot looks alike from
here, and the board's own report is the only witness. Asking a second time makes the
board answer from flash rather than from what it just did.

The two kinds of step are checked differently, because the board has two ways of
answering. A step that writes flash reboots, and the main image prints its VmFail record
over serial on the way back up -- that text is the assertion. A step that declines has
nothing to say over serial at all: SendMsgPrintfln goes to the C64, and the menu redraws
over it within seconds, so by the time the screen can be read it is gone. A decline is
checked by what it did instead -- the firmware ACKed, the port stayed up, and the board
is still answering on its main image, which together hold only if the early return was
taken. The step after it carries the rest: an install that follows a refused remove had
nothing to overwrite.

None of this enters the extension image, so none of it needs a hand on the board -- that
is the whole point. Running a module still does, until the client calls the exit service.

The board must be on its main image (not in an extension) when this starts. If a module
is running, reset it first.
"""
import sys
import time

from hostops import INSTALLED, REMOVED, install_host, remove_host, show

package = None
for a in sys.argv[1:]:
    if a.startswith('--'):
        raise SystemExit(__doc__)
    package = a
if package is None:
    raise SystemExit(__doc__)

failures = []
step = 0


def check(label, result, rebooted, phrase=None):
    """`phrase` is asserted only where a reboot carries it; see the note above on why a
    decline cannot be checked that way."""
    global step
    step += 1
    why = []
    if result.rebooted != rebooted:
        why.append(f'expected rebooted={rebooted}, got {result.rebooted}')
    elif phrase and not result.said(phrase):
        why.append(f'the boot record never said {phrase!r}')
    elif not rebooted and result.image != 'main':
        why.append(f'expected the board still on its main image, got {result.image!r}')
    print(f'\n=== step {step}: {label} -- {"FAIL" if why else "PASS"} ===')
    if why:
        for w in why:
            print(f'  {w}')
        print('  --- what it did say ---')
        show(result.screen)
        failures.append(f'step {step} ({label}): ' + '; '.join(why))
    return not why


started = time.time()
print(f'round trip with {package}; no button presses needed')

# Whatever state the board was left in, clear it -- and accept either answer, because
# "already empty" is a legitimate starting point rather than a failure.
first = remove_host()
print(f'\n=== step 0: normalise -- slot was {"populated" if first.rebooted else "empty"} ===')

check('remove with nothing installed refuses', remove_host(), False)
check('install writes the slot', install_host(package), True, INSTALLED)
check('remove clears the tag', remove_host(), True, REMOVED)
check('the tag is gone, not just stale', remove_host(), False)
check('reinstall works after a removal', install_host(package), True, INSTALLED)

mins = (time.time() - started) / 60
print(f'\n{"=" * 60}')
if failures:
    print(f'FAILED after {mins:.1f} min')
    for f in failures:
        print(f'  {f}')
    sys.exit(1)
print(f'PASSED: 5/5 steps in {mins:.1f} min. A host is installed and the board is usable.')
