# SPDX-License-Identifier: MIT
"""reuspans.py's reading of reuclient.a's register read-back counters. No hardware needed:

  python3 -m unittest discover -s tools/bench
"""
import unittest
from reuspans import REGISTERS, misses_between, register_misses, require_an_reu


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


class RequireAnReu(unittest.TestCase):
    def test_a_bad_page_among_good_rounds_is_not_a_missing_reu(self):
        require_an_reu(202, 1)

    def test_no_good_round_is_a_missing_reu(self):
        with self.assertRaises(SystemExit):
            require_an_reu(0, 37)


if __name__ == '__main__':
    unittest.main()
