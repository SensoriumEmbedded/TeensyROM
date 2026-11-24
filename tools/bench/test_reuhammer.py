# SPDX-License-Identifier: MIT
"""reuhammer.py's reading of reuhammer.a's counters. No hardware needed:

  python3 -m unittest discover -s tools/bench
"""
import os, re, unittest
from reuhammer import (COUNTS, COUNTS_BYTES, HELD, HELD_HI, HIST_HI, PASSES, READY, REGISTERS, REPEATS,
                       VALUES, counts_between, counts_from, describe_value)

CLIENT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'reuhammer.a')


def block(passes=0, held=(), hist=()):
    """A counters block: held is [(register, count)], hist [(register, value, count)]."""
    data = bytearray(COUNTS_BYTES)
    data[PASSES:PASSES + 4] = passes.to_bytes(4, 'little')
    for k, n in held:
        data[HELD + k], data[HELD_HI + k] = n & 0xFF, n >> 8
    for k, v, n in hist:
        data[k * 256 + v], data[HIST_HI + k * 256 + v] = n & 0xFF, n >> 8
    return bytes(data)


class Counts(unittest.TestCase):
    def test_counters_are_read_where_the_client_keeps_them(self):
        passes, held, hist = counts_from(block(0x12345678, [(6, 0x1234)], [(3, 0xA4, 0x0201)]))
        self.assertEqual(passes, 0x12345678)
        self.assertEqual(held, [0, 0, 0, 0, 0, 0, 0x1234])
        self.assertEqual(hist[3][0xA4], 0x0201)
        self.assertEqual(sum(map(sum, hist)), 0x0201)

    def test_a_phase_counts_reads_and_splits_each_registers_misses_by_kind(self):
        start = counts_from(block(10, [(1, 4)], [(1, 0x00, 7)]))
        stop = counts_from(block(12, [(1, 5)], [(1, 0x00, 9), (1, 0x3C, 1), (5, 0x0F, 3)]))
        reads, kinds, values = counts_between(start, stop)
        self.assertEqual(reads, 2 * REPEATS * len(REGISTERS))
        self.assertEqual(kinds[1], {'late': 2, 'wrong': 1})
        self.assertEqual(kinds[5], {'late': 3, 'wrong': 0})
        self.assertEqual(values, {(1, 0x00): 2, (1, 0x3C): 1, (5, 0x0F): 3})

    def test_counters_that_wrap_still_count_forward(self):
        start = counts_from(block(0xFFFFFFFF, [(0, 0xFFFF)], [(0, 0x11, 0xFFFF)]))
        stop = counts_from(block(1, [(0, 0)], [(0, 0x11, 1)]))
        reads, kinds, values = counts_between(start, stop)
        self.assertEqual(reads, 2 * REPEATS * len(REGISTERS))
        self.assertEqual(kinds[0], {'late': 1, 'wrong': 1})
        self.assertEqual(values, {(0, 0x11): 2})


class Values(unittest.TestCase):
    def test_a_wrong_read_holding_the_previous_registers_value_says_so(self):
        self.assertIn("$DF03's value", describe_value(2, VALUES[1]))
        self.assertIn("$DF08's value", describe_value(0, VALUES[6]))
        self.assertNotIn('value,', describe_value(2, 0x00))


class Client(unittest.TestCase):
    """The constants reuhammer.py shares with reuhammer.a."""
    def setUp(self):
        with open(CLIENT) as f:
            self.source = f.read()

    def symbol(self, name):
        return int(re.search(rf'^{name}\s*=\s*\$([0-9A-Fa-f]+)', self.source, re.M).group(1), 16)

    def test_register_values_match(self):
        self.assertEqual([self.symbol(f'V{k}') for k in range(len(REGISTERS))], VALUES)

    def test_repeats_match(self):
        self.assertEqual(int(re.search(r'^REPEATS\s*=\s*(\d+)', self.source, re.M).group(1)), REPEATS)

    def test_layout_matches(self):
        self.assertEqual(self.symbol('hist'), COUNTS)
        self.assertEqual(self.symbol('passes'), COUNTS + PASSES)
        self.assertEqual(self.symbol('held'), COUNTS + HELD)
        self.assertEqual(self.symbol('heldhi'), COUNTS + HELD_HI)
        self.assertEqual(self.symbol('ready'), READY)
        self.assertEqual(self.symbol('histhi'), COUNTS + HIST_HI)


if __name__ == '__main__':
    unittest.main()
