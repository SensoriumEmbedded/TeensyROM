#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Remove the installed extension host, over the USB device port.   hostuninstall.py

Removal opens the way an install does -- clear the 4-byte tag so the slot stops reading as
a host -- and then erases the rest of the slot, so nothing of the host is left for firmware
without the extension loader to trip over.

The firmware takes this command on the USB device port only -- the same token over the
USB host port or the TCP listener is refused with "Busy!".

That gate covers this token, not flash in general: LaunchFileToken is served on every
channel, ahead of the gate, and launching a .TRH still reaches DoHostInstall (which
erases and programs this same slot) and a .hex still reaches DoFlashUpdate. Do not read
the refusal here as the LAN being unable to write flash.

The firmware ACKs and flushes before it starts, because clearing the tag takes a sector
erase it does not return from -- so the ACK means accepted, not done. Four things can
follow it:

  * the port drops, the board reboots, and the record on the way back up says the host
    was removed with detail $0: every sector of the slot was erased;
  * the port drops and the board reboots saying anything else, or nothing readable at
    all: the removal did not report success. A record saying so ($41) means the tag did
    not clear; no readable record at all means the outcome was not observed, which is
    not the same thing. Either way this exits non-zero, because a reboot on its own is
    not the outcome. Run it again to find out which: with the slot blank it refuses
    without rebooting, and that is the slot answering rather than the last run;
  * the board reboots saying the host was removed, but with a detail: the slot no longer
    reads as a host and a sector would not erase. That exits non-zero too, because
    what is left is what firmware without the extension loader trips over;
  * the board stays up: the slot was already blank, and the C64 says so. A slot that holds
    no host but is not blank -- what an install that failed part way leaves -- is cleared
    like a host would be.

Unlike launching an extension, none of this enters the extension image, so no part of it
needs a hand on the board.
"""
import sys

from hostops import REMOVED, remove_host, show

if [a for a in sys.argv[1:] if not a.startswith('--')]:
    raise SystemExit(__doc__)

result = remove_host()
if not result.rebooted:
    print('\nthe board is still up, so the slot was already blank:')
    show(result.screen)
    sys.exit(1)
if not result.said(REMOVED):
    print(f'\nthe board rebooted but never said {REMOVED!r}, so the removal did not '
          'report success -- the tag may or may not have cleared, and nothing here has '
          'seen which. Run this again: a refusal without a reboot means the slot is '
          'clear after all. What the board did say:')
    show(result.screen)
    sys.exit(1)
if not result.removed_cleanly():
    print('\nthe slot no longer reads as a host, but the record carries a detail: a sector '
          'would not erase, so part of the slot is still programmed. Run this again to '
          'retry. What the board did say:')
    show(result.screen)
    sys.exit(1)
if result.image is None:
    print('nothing answered the firmware check; the boot output above is all there is')
print('\n--- screen ---')
show(result.screen)
