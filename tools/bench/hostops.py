# SPDX-License-Identifier: MIT
"""Installing and removing an extension host, as operations rather than scripts.

hostinstall.py, hostuninstall.py and hostcycle.py all drive the same two, and read the
outcome the same way, so it lives here once.

Both operations validate before they erase, so both split the same way: either the board
is still answering, and nothing was written, or the port drops and the board reboots with
its reason in the VmFail record the main image prints on the way back up. `rebooted` is
the fact the rest hangs off.

Neither operation enters the extension image -- installing and removing are main-image
work -- so neither needs a hand on the board. That is what lets hostcycle.py run a whole
install/remove round trip unattended.
"""
import io
import os
import sys
import time

from c64 import screen_rows
from trlink import Link, answering_board, neighbours

# The erase alone is allowed 45 s by the message the firmware prints, and the boot
# follows it. Removal clears one sector and is much quicker, but shares the path.
REBOOT_TIMEOUT = 120
# A board that has just come back spends a few seconds on NFC and the SD scan before it
# answers commands, and the port is open for all of it. Every operation here can follow
# a reboot -- the round trip runs six in a row -- so each one waits for a real answer
# rather than treating an open port as a ready board.
READY_TIMEOUT = 30


class Tee:
    """Boot output has two readers: someone watching a CLI run, and the assertions in
    hostcycle.py. Splitting it here keeps the scripts chatty without making the test
    parse a terminal."""

    def __init__(self, out):
        self.out, self.buf = out, io.StringIO()

    def write(self, s):
        self.buf.write(s)
        if self.out:
            self.out.write(s)
        return len(s)

    def flush(self):
        if self.out:
            self.out.flush()


class Outcome:
    def __init__(self, rebooted, boot, screen, image):
        self.rebooted = rebooted      # did the board write flash and restart
        self.boot = boot              # serial output captured across the restart
        self.screen = screen          # the C64 screen once it was answering again
        self.image = image            # firmware name the board reported, or None

    def said(self, phrase):
        return phrase.lower() in (self.boot + '\n' + self.screen).lower()

    def __str__(self):
        return (self.boot + '\n' + self.screen).strip()


def show(screen, out=sys.stdout):
    for n, line in enumerate(screen.splitlines()):
        if line.strip():
            print(f'{n:2d} |{line}|', file=out)


def _screen_text(tr):
    return '\n'.join(screen_rows(tr.screen()))


def _ready(tr):
    deadline = time.time() + READY_TIMEOUT
    while True:
        try:
            tr.version(timeout=3)
            return
        except (SystemExit, OSError):
            if time.time() >= deadline:
                raise SystemExit(f'the port is open but the board did not answer a version '
                                 f'request within {READY_TIMEOUT} s')
            time.sleep(0.5)


def _run(prepare, settle, out=sys.stdout):
    echo = Tee(out)
    tr = Link()
    _ready(tr)
    # Sampled before: once the board goes there is nothing left to ask.
    port, known = tr.port, neighbours(tr.port)
    prepare(tr)
    if not tr.stream(settle, out=echo):
        # Still up, so nothing was written. Name the image anyway: a caller checking that
        # an operation declined wants to know the board is still the one it asked, not
        # that it fell into the minimal image on the way to declining.
        image = tr.await_image(time.time() + 5, echo)
        screen = _screen_text(tr)
        tr.close()
        return Outcome(False, echo.buf.getvalue(), screen, image)
    tr.close()
    # Not a bare reconnect(): the main image renames its USB device as it comes up, so
    # the first node to appear is one that is about to disappear again.
    tr, image = answering_board(time.time() + REBOOT_TIMEOUT, port, known, out=echo)
    if tr is None:
        raise SystemExit(f'the board did not come back within {REBOOT_TIMEOUT} s; '
                         'check it before power cycling')
    screen = _screen_text(tr)
    tr.close()
    return Outcome(True, echo.buf.getvalue(), screen, image)


def install_host(local, remote=None, out=sys.stdout):
    """Push a .TRH to the card and launch it, which is the same path as selecting it in
    the menu. rebooted=False is the refusal case: nothing was erased."""
    if not local.lower().endswith('.trh'):
        raise SystemExit(f'{local}: not a .TRH package -- the firmware routes on the extension')
    size = os.path.getsize(local)
    if size < 64:
        raise SystemExit(f'{local}: {size} bytes, too short to hold a TRH1 header')
    target = remote or '/' + os.path.basename(local).upper()

    def prepare(tr):
        tr.drain()
        tr.post(local, target, log=(print if out else (lambda *a, **k: None)))
        tr.launch(target)
        if out:
            print(f'launched {target}; the board reboots if it got as far as writing', file=out)

    return _run(prepare, 60, out)


def remove_host(out=sys.stdout):
    """Clear the 4-byte tag so the slot stops reading as a host. The payload stays in
    flash, unreferenced, until the next install overwrites it. rebooted=False means
    there was nothing installed to clear."""
    def prepare(tr):
        tr.remove_host()
        if out:
            print('accepted; the board reboots if it had a host to remove', file=out)

    return _run(prepare, 30, out)
