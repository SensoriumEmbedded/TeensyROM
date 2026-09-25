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
removed_cleanly() rather than treating any reboot as a success.

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

# Once the port drops: the boot, and whatever the board has left to do before it. The
# erase itself happens with the port still up, so it is each operation's `settle` that
# has to cover it, not this.
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
REMOVED = 'extension host removed'       # VmFail::Removed, $40; see removed_cleanly()
# A launch has more than one normal finish, so it gets a set rather than a phrase: the
# module can be handed the machine and never come back ($00), ask to be finished with
# through the exit service ($04), or -- for a host that is not this one -- hand the
# machine back itself ($50). Every other code printBoot can print after a launch is a
# failure, so exttest.py checks for these positively for the same reason the two above
# are checked positively: the failures outnumber the successes and a new one must not
# arrive as a pass.
FINISHED_NORMALLY = (
    'handed off to client',              # VmFail::Ok, $00
    'module exited',                     # VmFail::Exited, $04
    'extension host returned',           # VmFail::HostReturned, $50
)


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


def finished_normally(text):
    """Whether a VmFail record in `text` names a launch that ended the way it should.

    Case-folded, like Outcome.said and for the same reason: the record reaches the C64
    as well as the serial port, and which case a glyph carries is a property of the
    screen. False for text carrying no record at all, which is the whole point -- an
    absent, truncated or unrecognised record has to read as a failure rather than as
    "it rebooted, didn't it".
    """
    lowered = text.lower()
    return any(phrase in lowered for phrase in FINISHED_NORMALLY)


class Outcome:
    def __init__(self, rebooted, boot, screen, image):
        self.rebooted = rebooted      # did the board write flash and restart
        self.boot = boot              # serial output captured across the restart
        self.screen = screen          # the C64 screen once it was answering again
        self.image = image            # firmware name the board reported, or None

    def said(self, phrase):
        """`phrase` is one string, or a tuple of alternatives any one of which will do."""
        text = (self.boot + '\n' + self.screen).lower()
        phrases = (phrase,) if isinstance(phrase, str) else phrase
        return any(p.lower() in text for p in phrases)

    def removed_cleanly(self):
        """said(REMOVED), and with $0 for the detail. $40 means the slot no longer reads as
        a host, and a sector that would not erase still gets it -- with EraseFailed ($d)
        as the detail and part of the payload still in flash, for firmware without the
        extension loader to trip over. The record is printed in two places and either
        will do: printBoot's serial line always names the detail, and the C64's line
        leaves it off when it is zero."""
        return self.said((f'{REMOVED} (code $40, detail $0)', f'{REMOVED} ($40)'))

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

    There is a third shape it cannot report, so it raises instead: the port drops and
    nothing comes back inside REBOOT_TIMEOUT. That is a board to go and look at for an
    install or a removal, which is what the SystemExit says -- but it is also what a step
    that hands the machine to an extension image looks like when the image keeps it,
    since that image is built USB_DISABLED. A caller whose step can legitimately end with
    the board gone has to say so itself; see hostenter.py.

    Public because install and removal are not the only things worth driving this way --
    anything that ends in a reboot has the same two shapes and the same trap, which is
    that the main image renames its USB device as it comes up (on macOS; see trlink.py),
    so the first node to appear is one that is about to disappear again.
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
    """Clear the 4-byte tag so the slot stops reading as a host, then erase the rest of
    the slot. Runs for a slot holding anything, a failed install's leftovers included,
    so rebooted=False means the slot was already blank."""
    def prepare(tr):
        tr.remove_host()
        if out:
            print('accepted; the board reboots if the slot held anything', file=out)

    # The same 45 s erase an install warns of, with the port up for all of it, and the
    # same allowance install_host gives it.
    return run_step(prepare, 60, out)
