# SPDX-License-Identifier: MIT
"""reuspans.py's reading of reuclient.a's register read-back counters. No hardware needed:

  python3 -m unittest discover -s tools/bench
"""
import contextlib, io, unittest
from reuspans import REGISTERS, misses_between, register_misses, report, require_an_reu, rounds_between


def block(counts, last):
    lo = [n & 0xFF for n in counts] + [0]
    hi = [n >> 8 for n in counts] + [0]
    return bytes(lo + hi + list(last) + [0])


class RegisterMisses(unittest.TestCase):
    def test_counts_are_16_bit_and_the_last_read_is_per_register(self):
        misses = register_misses(block([0, 1, 0x123, 0, 0, 0xFFFF, 2], [0, 0xFF, 0x14, 0, 0, 0x01, 0]))
        self.assertEqual(misses[1], (1, 0xFF))
        self.assertEqual(misses[2], (0x123, 0x14))
        self.assertEqual(misses[5], (0xFFFF, 0x01))
        self.assertEqual(len(misses), len(REGISTERS))

    def test_a_phase_reports_only_the_registers_that_missed_in_it(self):
        start = register_misses(block([5, 0, 0, 0, 0, 7, 0], [0] * 7))
        stop = register_misses(block([5, 0, 0, 0, 0, 9, 0], [0, 0, 0, 0, 0, 0x01, 0]))
        self.assertEqual(misses_between(start, stop), {'$DF07': (2, 0x01)})

    def test_a_counter_that_wraps_still_counts_forward(self):
        start = register_misses(block([0xFFFE, 0, 0, 0, 0, 0, 0], [0] * 7))
        stop = register_misses(block([1, 0, 0, 0, 0, 0, 0], [0x20] + [0] * 6))
        self.assertEqual(misses_between(start, stop), {'$DF02': (3, 0x20)})


class Report(unittest.TestCase):
    def test_a_round_whose_page_came_back_wrong_is_still_a_round_run(self):
        with contextlib.redirect_stdout(io.StringIO()) as out:
            report('phase', (0xFFF0, 3), (0x00B9, 4), {})
        self.assertIn('202 REU stash/fetch rounds, 1 with a page that came back wrong', out.getvalue())


class RoundsBetween(unittest.TestCase):
    def test_rounds_run_count_the_wrong_pages_too(self):
        self.assertEqual(rounds_between((10, 0), (20, 5)), (15, 5))

    def test_both_counters_wrap(self):
        self.assertEqual(rounds_between((0xFFFF, 0xFFFE), (1, 1)), (5, 3))


class RequireAnReu(unittest.TestCase):
    def test_a_bad_page_among_good_rounds_is_not_a_missing_reu(self):
        require_an_reu(202, 1)

    def test_no_good_round_is_a_missing_reu(self):
        with self.assertRaises(SystemExit):
            require_an_reu(0, 37)


if __name__ == '__main__':
    unittest.main()
