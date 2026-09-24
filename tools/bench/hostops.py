# SPDX-License-Identifier: MIT
"""Installing and removing an extension host, as operations rather than scripts.

hostinstall.py, hostuninstall.py and hostcycle.py all drive the same two, and read the
outcome the same way, so it lives here once.

Both operations validate before they erase, so both split the same way: either the board
is still answering, and nothing was written, or the port drops and the board reboots with
its reason in the VmFail record the main image prints on the way back up. `rebooted` is
the first fact the rest hangs off, but it is not the outcome: the firmware reboots for a
failed install ($31-$34, $3f) and a failed removal ($41) exactly as it does for a good
one. What separates them is the record, so a caller asks Outcome.said(INSTALLED) or
said(REMOVED) rather than treating any reboot as a success.

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
# How long one attempt within that waits for the Ack saying the board heard the version
# request. Short, because a board that is up answers it at once and one that is not needs
# asking again rather than waiting on.
ANSWER_TIMEOUT = 3
# The firmware reports the outcome of both operations in the VmFail record the main image
# prints on the way back up, and it has more ways to fail than to succeed: $31-$34 and $3f
# for an install, $41 for a removal. So the check is for the one success phrase rather
# than a list of failures -- a record that is absent, truncated, or carries a code this
# copy has never heard of then fails noisily instead of passing as "it rebooted, didn't
# it". The strings are VmFail::describe()'s, in Source/Teensy/MinimalBoot/Common/VMFail.h.
INSTALLED = 'extension host installed'   # VmFail::Installed, $30
REMOVED = 'extension host removed'       # VmFail::Removed, $40


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
    """Returns once the board answers a version request, and raises by READY_TIMEOUT if
    it never does. Both of version()'s waits take what is left of that deadline, not just
    the banner read: `total=` is what makes the deadline real, since version()'s own
    timeout is pushed out by every chunk that arrives and a board printing steadily with
    gaps under the idle window never lets version() return; and capping `timeout=` the
    same way keeps the last attempt's Ack wait from running past the deadline it was
    started under. What is left over is drain()'s fixed 0.3 s, which runs before the
    capped wait rather than inside it, so the raise lands a fraction of a second past
    READY_TIMEOUT rather than on it -- measured at 5.42 s against a 5 s deadline, where
    an uncapped Ack wait took 7.24 s."""
    deadline = time.time() + READY_TIMEOUT
    while True:
        left = max(0.0, deadline - time.time())
        try:
            tr.version(timeout=min(ANSWER_TIMEOUT, left), total=left)
            return
        except (SystemExit, OSError):
            pass
        if time.time() >= deadline:
            raise SystemExit(f'the port is open but the board did not answer a version '
                             f'request within {READY_TIMEOUT} s')
        time.sleep(0.5)


def run_step(prepare, settle, out=sys.stdout):
    """Do something that may or may not reboot the board, and report which happened.

    `prepare(tr)` is the action, run against a board already answering. `settle` is how
    long to wait for the port to go before concluding it is not going to. The Outcome
    carries everything a caller can assert on afterwards: whether it rebooted, the serial
    text (which is where the VmFail record lands on the way back up), the C64 screen, and
    which image is running.

    Public because install and removal are not the only things worth driving this way --
    anything that ends in a reboot has the same two shapes and the same trap, which is
    that the main image renames its USB device as it comes up, so the first node to
    appear is one that is about to disappear again.
    """
    # `with` on both links, not close() on the way out: every step in here can raise --
    # _ready and prepare() raise SystemExit, await_image and the screen read raise OSError
    # when the port goes while they are reading it -- and a return-path close() covers
    # none of those.
    echo = Tee(out)
    with Link() as tr:
        _ready(tr)
        # Sampled before: once the board goes there is nothing left to ask.
        port, known = tr.port, neighbours(tr.port)
        prepare(tr)
        if not tr.stream(settle, out=echo):
            # Still up, so nothing was written. Name the image anyway: a caller checking
            # that an operation declined wants to know the board is still the one it
            # asked, not that it fell into the minimal image on the way to declining.
            image = tr.await_image(time.time() + 5, echo)
            return Outcome(False, echo.buf.getvalue(), _screen_text(tr), image)
    # Not a bare reconnect(): the main image renames its USB device as it comes up, so
    # the first node to appear is one that is about to disappear again.
    tr, image = answering_board(time.time() + REBOOT_TIMEOUT, port, known, out=echo)
    if tr is None:
        raise SystemExit(f'the board did not come back within {REBOOT_TIMEOUT} s; '
                         'check it before power cycling')
    with tr:
        return Outcome(True, echo.buf.getvalue(), _screen_text(tr), image)


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
        # Everything install_host prints goes to `out`, including the board's own output
        # via Tee -- post()'s progress line has to as well, or a caller that passed
        # somewhere other than stdout gets its run split across two streams.
        tr.post(local, target,
                log=((lambda line: print(line, file=out)) if out
                     else (lambda *a, **k: None)))
        tr.launch(target)
        if out:
            print(f'launched {target}; the board reboots if it got as far as writing', file=out)

    return run_step(prepare, 60, out)


def remove_host(out=sys.stdout):
    """Clear the 4-byte tag so the slot stops reading as a host. The payload stays in
    flash, unreferenced, until the next install overwrites it. rebooted=False means
    there was nothing installed to clear."""
    def prepare(tr):
        tr.remove_host()
        if out:
            print('accepted; the board reboots if it had a host to remove', file=out)

    return run_step(prepare, 30, out)
