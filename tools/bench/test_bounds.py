# SPDX-License-Identifier: MIT
"""Tests that the bench link's waits actually end. No hardware needed:

  python3 -m unittest discover -s tools/bench

Every wait here has two deadlines, and only one of them holds. wr()'s `stall` is
reset by each partial write and raw()'s `idle` by each chunk that arrives, so
both bound a *pause* -- a board that dribbles, or one that keeps printing, never
reaches either. These are the tests for the deadline that does not move, and for
the callers that have to pass what is left of their own.

The fake board is a pty with a thread on the other side, and every one of them
stops on its own after a few seconds. That is deliberate: a test that hangs
forever against a regression is a test that cannot be watched go red.
"""
import os
import pty
import threading
import time
import unittest

import hostops
from hostops import READY_TIMEOUT, _ready
from protocol import ACK, VERSION_INFO, to_board
from trlink import Link

CHATTER_GAP = 0.1      # short enough that no idle window of 0.3 s ever expires
CHATTER_FOR = 8.0      # and long enough to run well past every cap asserted here
CHATTER_LINE = b'SD card: scanning /\r\n'


def reply(token):
    """The board answers least significant byte first."""
    return bytes([token & 0xff, (token >> 8) & 0xff])


class FakePty(unittest.TestCase):
    """A pty with something on the far end, torn down in the right order: stop the
    thread, join it, then close the fds it writes to."""

    def pty_pair(self):
        master, slave = pty.openpty()
        self.addCleanup(os.close, master)
        self.addCleanup(os.close, slave)
        return master, slave

    def run_board(self, master, body):
        stop = threading.Event()
        board = threading.Thread(target=body, args=(master, stop), daemon=True)
        self.addCleanup(board.join, 2)
        self.addCleanup(stop.set)
        board.start()
        return stop

    def link_to(self, slave):
        link = Link(os.ttyname(slave), settle=0)
        self.addCleanup(link.close)
        return link


def chatter(master, stop, until=None):
    """Print a line every CHATTER_GAP seconds. This is an ordinary board coming up:
    the SD scan and the NFC probe both narrate, and neither pauses for 0.3 s."""
    end = until if until is not None else time.time() + CHATTER_FOR
    while time.time() < end and not stop.is_set():
        try:
            os.write(master, CHATTER_LINE)
        except OSError:
            return
        stop.wait(CHATTER_GAP)


class ReadBound(FakePty):
    """A board that keeps printing must not hold a read open for as long as it cares
    to keep printing."""

    def test_a_board_that_keeps_printing_does_not_hold_raw_open(self):
        master, slave = self.pty_pair()
        self.run_board(master, chatter)
        link = self.link_to(slave)

        started = time.time()
        link.raw(idle=0.3, timeout=1, total=1.5)
        took = time.time() - started

        self.assertLess(took, CHATTER_FOR / 2,
                        f'raw() capped at 1.5 s ran {took:.1f} s: every chunk pushed '
                        f'the idle deadline out and nothing else stopped it')

    def test_the_read_still_ends_on_silence_without_waiting_out_the_cap(self):
        """The cap is the backstop, not the normal exit: a board that says its piece
        and stops is still read at idle speed."""
        master, slave = self.pty_pair()
        os.write(master, b'quick answer\r\n')
        link = self.link_to(slave)

        started = time.time()
        seen = link.raw(idle=0.3, timeout=2, total=30)
        took = time.time() - started

        self.assertIn(b'quick answer', seen)
        self.assertLess(took, 2.0, f'a silent port took {took:.1f} s to fall out')


class WriteBound(FakePty):
    """A board that takes a trickle must not hold a write open forever. The stall
    deadline cannot see this: every partial write resets it."""

    def test_a_board_that_takes_a_trickle_gives_up_at_the_hard_cap(self):
        master, slave = self.pty_pair()

        def sipper(master, stop):
            end = time.time() + CHATTER_FOR
            while time.time() < end and not stop.is_set():
                try:
                    os.read(master, 64)
                except OSError:
                    return
                stop.wait(0.05)

        self.run_board(master, sipper)
        link = self.link_to(slave)

        started = time.time()
        with self.assertRaises(SystemExit) as stopped:
            link.wr(b'x' * (4 * 1024 * 1024), stall=30.0, total=1.5)
        took = time.time() - started

        self.assertLess(took, CHATTER_FOR / 2,
                        f'wr() capped at 1.5 s ran {took:.1f} s: every partial write '
                        f'pushed the stall deadline out and nothing else stopped it')
        self.assertIn('did not finish within', str(stopped.exception))

    def test_a_write_that_landed_is_not_failed_by_a_deadline_it_landed_on(self):
        """The cap must fire on work left to do, not on the clock alone: bytes that went
        went, and a caller told otherwise retries a delivered write."""
        master, slave = self.pty_pair()
        del master
        link = self.link_to(slave)
        link.wr(b'x', total=0)      # a deadline already in the past, and nothing left

    def test_a_write_nobody_reads_still_reports_the_stall_and_not_the_cap(self):
        """The stall deadline is the one that names how far the write got, and it is
        still the one that fires when the board takes nothing at all."""
        master, slave = self.pty_pair()
        del master
        link = self.link_to(slave)
        with self.assertRaises(SystemExit) as stopped:
            link.wr(b'x' * (4 * 1024 * 1024), stall=0.2, total=30.0)
        self.assertIn('board stopped reading after', str(stopped.exception))


class ReadyBound(FakePty):
    """hostops._ready() promises an answer or a failure within READY_TIMEOUT. It
    checks its deadline only where version() returns, so whatever version() waits on
    has to be inside that deadline too."""

    def setUp(self):
        self.original, hostops.READY_TIMEOUT = hostops.READY_TIMEOUT, 3
        self.addCleanup(setattr, hostops, 'READY_TIMEOUT', self.original)

    def test_the_patch_these_tests_depend_on_is_the_one_the_scripts_read(self):
        """A canary for the two tests below, which are only fair tests of the bound if
        setUp's patch took: the scripts' own import is untouched, and hostops' copy --
        the one _ready reads at call time -- is the shortened one."""
        self.assertEqual(READY_TIMEOUT, self.original)
        self.assertNotEqual(hostops.READY_TIMEOUT, self.original)

    def test_a_board_that_acks_then_talks_forever_does_not_hold_ready_open(self):
        master, slave = self.pty_pair()

        def acks_then_talks(master, stop):
            """The reproduction: the board answers the version request and then keeps
            narrating its boot. version() never returns, so the deadline after it is
            never reached."""
            end = time.time() + CHATTER_FOR
            seen = b''
            while time.time() < end and not stop.is_set():
                try:
                    seen += os.read(master, 64)
                except OSError:
                    return
                if to_board(VERSION_INFO) in seen:
                    break
                stop.wait(0.01)
            try:
                os.write(master, reply(ACK))
            except OSError:
                return
            chatter(master, stop, until=end)

        self.run_board(master, acks_then_talks)
        link = self.link_to(slave)

        started = time.time()
        try:
            _ready(link)
            gave_up = None
        except SystemExit as stopped:
            gave_up = str(stopped)
        took = time.time() - started

        self.assertLess(took, CHATTER_FOR - 1,
                        f'_ready() ran {took:.1f} s against a '
                        f'{hostops.READY_TIMEOUT} s deadline')
        # Either answer is right -- the board did Ack, so taking it as ready is fair, and
        # so is running out of deadline reading the banner behind the Ack. Pin which one
        # it was anyway: without this the test passes on a SystemExit from anywhere,
        # including a write that failed before the deadline was ever consulted.
        if gave_up is not None:
            self.assertIn('did not answer a version request', gave_up)

    def test_a_silent_board_still_fails_noisily(self):
        """The other direction of the same bound: capping the read must not turn a
        board that never answers into one that passed. The message names the deadline
        it ran against, which is also how this pins _ready to the module global rather
        than to a default argument baked in at import."""
        master, slave = self.pty_pair()
        del master
        link = self.link_to(slave)

        started = time.time()
        with self.assertRaises(SystemExit) as stopped:
            _ready(link)
        took = time.time() - started

        self.assertIn('did not answer a version request', str(stopped.exception))
        self.assertIn(f'within {hostops.READY_TIMEOUT} s', str(stopped.exception))
        self.assertLess(took, hostops.READY_TIMEOUT + 6)

    def test_the_last_attempt_waits_only_what_is_left_of_the_deadline(self):
        """The regression setUp's 3 s cannot show, and the one the cap was added for.

        _ready retries every 0.5 s, so any deadline longer than a single attempt starts
        a second one -- and that attempt's Ack wait has to be cut to what is left rather
        than run the full ANSWER_TIMEOUT it would take from a standing start. A flat
        timeout there puts the raise well past the promise: measured against this silent
        pty, 7.24 s against a 5 s deadline before the cap and 5.42 s after.

        Three seconds cannot catch it because one attempt (drain 0.3 + ANSWER_TIMEOUT 3)
        already passes the deadline, so there is never a second attempt to overrun with.
        This needs READY_TIMEOUT above one attempt plus the 0.5 s sleep, so that the last
        attempt begins with less than ANSWER_TIMEOUT left to spend.
        """
        # setUp already registered the cleanup that puts the original back.
        hostops.READY_TIMEOUT = 5
        self.assertGreater(hostops.READY_TIMEOUT, hostops.ANSWER_TIMEOUT + 0.5,
                           'this test needs a deadline that allows a second attempt')
        master, slave = self.pty_pair()
        del master  # nothing ever answers
        link = self.link_to(slave)

        started = time.time()
        with self.assertRaises(SystemExit):
            _ready(link)
        took = time.time() - started

        # Slack for drain()'s fixed 0.3 s and the read tick, not for another whole
        # answer wait -- that is the difference this asserts.
        self.assertLess(took, hostops.READY_TIMEOUT + 1.5,
                        f'_ready() ran {took:.2f} s against a {hostops.READY_TIMEOUT} s '
                        'deadline: the last attempt waited a full answer timeout '
                        'instead of what was left')


if __name__ == '__main__':
    unittest.main()
