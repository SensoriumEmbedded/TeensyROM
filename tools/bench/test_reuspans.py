# SPDX-License-Identifier: MIT
"""reuspans.py's reading of reuclient.a's register read-back counters and miss log. No
hardware needed:

  python3 -m unittest discover -s tools/bench
"""
import unittest
from reuspans import (FETCH_TAG, LOG_COUNT, MISSES, REGISTERS, RETRY_TAG, kinds_by_register,
                      miss_kinds, misses_between, read_log, register_misses, snapshot)

DF03, DF04, DF05 = 1, 2, 3


def block(counts):
    lo = [n & 0xFF for n in counts] + [0]
    hi = [n >> 8 for n in counts] + [0]
    return bytes(lo + hi)


class RegisterMisses(unittest.TestCase):
    def test_counts_are_16_bit_and_per_register(self):
        misses = register_misses(block([0, 1, 0x123, 0, 0, 0xFFFF, 2]))
        self.assertEqual(misses, [0, 1, 0x123, 0, 0, 0xFFFF, 2])
        self.assertEqual(len(misses), len(REGISTERS))

    def test_a_phase_reports_only_the_registers_that_missed_in_it(self):
        start = register_misses(block([5, 0, 0, 0, 0, 7, 0]))
        stop = register_misses(block([5, 0, 0, 0, 0, 9, 0]))
        self.assertEqual(misses_between(start, stop), {'$DF07': 2})

    def test_a_counter_that_wraps_still_counts_forward(self):
        start = register_misses(block([0xFFFE, 0, 0, 0, 0, 0, 0]))
        stop = register_misses(block([1, 0, 0, 0, 0, 0, 0]))
        self.assertEqual(misses_between(start, stop), {'$DF02': 3})


class MissKinds(unittest.TestCase):
    def test_a_stash_write_lost_reads_what_the_previous_rounds_fetch_left(self):
        self.assertEqual(miss_kinds([(DF03, 0x10, 0x40, 0x51), (DF04, 0x10, 0x10, 0x0F)]), ['lost', 'lost'])

    def test_a_fetch_write_lost_reads_what_its_rounds_stash_left(self):
        self.assertEqual(miss_kinds([(DF03 | FETCH_TAG, 0x10, 0x50, 0x41),
                                     (DF05 | FETCH_TAG, 0x10, 0x00, 0x01)]), ['lost', 'lost'])

    def test_the_previous_rounds_seed_wraps(self):
        self.assertEqual(miss_kinds([(DF04, 0x00, 0x00, 0xFF)]), ['lost'])

    def test_a_second_read_of_the_value_written_is_a_late_read(self):
        self.assertEqual(miss_kinds([(DF03, 0x10, 0x40, 0x40), (DF03 | FETCH_TAG, 0x10, 0x50, 0x50)]),
                         ['late', 'late'])

    def test_anything_else_is_a_write_that_landed_wrong(self):
        self.assertEqual(miss_kinds([(DF03, 0x10, 0x40, 0x60), (DF03 | FETCH_TAG, 0x10, 0x50, 0x51)]),
                         ['wrong', 'wrong'])

    def test_a_repeated_write_lost_reads_what_the_attempt_before_left(self):
        records = [(DF05, 0x10, 0x00, 0x20), (DF05 | RETRY_TAG, 0x10, 0x00, 0x20)]
        self.assertEqual(miss_kinds(records), ['wrong', 'lost'])

    def test_a_repeated_write_that_reads_what_the_transfer_left_landed_wrong(self):
        records = [(DF05, 0x10, 0x00, 0x20), (DF05 | RETRY_TAG, 0x10, 0x00, 0x01)]
        self.assertEqual(miss_kinds(records), ['wrong', 'wrong'])


class Log(unittest.TestCase):
    def test_records_are_read_across_the_four_pages(self):
        log = bytearray(1024)
        log[0x002], log[0x102], log[0x202], log[0x302] = 0x81, 0x33, 0x50, 0x41
        self.assertEqual(read_log(bytes(log), 3)[2], (0x81, 0x33, 0x50, 0x41))
        self.assertEqual(len(read_log(bytes(log), 3)), 3)

    def test_kinds_are_tallied_per_register(self):
        records = [(DF03, 0x10, 0x40, 0x51), (DF03 | FETCH_TAG, 0x10, 0x50, 0x50), (DF05, 0x11, 0x00, 0x20)]
        self.assertEqual(kinds_by_register(records, miss_kinds(records)),
                         {'$DF03': {'lost': 1, 'late': 1, 'wrong': 0},
                          '$DF05': {'lost': 0, 'late': 0, 'wrong': 1}})


class MissingC64:
    """A C64 whose client logs and counts a $DF03 miss after every read the host makes."""
    def __init__(self):
        self.memory = bytearray(0x10000)

    def peek(self, addr, length):
        data = bytes(self.memory[addr:addr + length])
        self.memory[LOG_COUNT] += 1
        self.memory[MISSES + DF03] += 1
        return data


class Snapshot(unittest.TestCase):
    def test_the_log_count_and_the_miss_counters_come_from_one_read(self):
        _, misses, logged = snapshot(MissingC64())
        self.assertEqual(sum(misses), logged)


if __name__ == '__main__':
    unittest.main()
