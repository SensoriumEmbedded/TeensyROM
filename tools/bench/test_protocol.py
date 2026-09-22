# SPDX-License-Identifier: MIT
"""Check every wire value against the firmware that defines it.

  python3 -m unittest discover -s tools/bench

If the firmware moves a token, this fails rather than a board.
"""
import pathlib
import re
import unittest

import protocol
from protocol import FIRMWARE_NAMES, from_board, to_board

SOURCE = pathlib.Path(__file__).resolve().parents[2] / 'Source' / 'Teensy'

DEFINITIONS = (
    re.compile(r'^\s*#define\s+(\w+)\s+(0x[0-9A-Fa-f]+)\b', re.M),
    re.compile(r'^\s*const\s+uint\d+_t\s+(\w+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;', re.M),
    re.compile(r'^\s*(\w+)\s*=\s*(\d+)\s*,', re.M),          # enum members
)


def firmware_definitions():
    """{name: {values}} for every constant the firmware defines, so a name
    defined twice with two values cannot silently satisfy a lookup."""
    found = {}
    for path in SOURCE.rglob('*'):
        if path.suffix not in ('.h', '.c', '.cpp', '.ino'):
            continue
        text = path.read_text(errors='replace')
        for pattern in DEFINITIONS:
            for name, value in pattern.findall(text):
                base = 16 if value.startswith('0x') else 10
                found.setdefault(name, set()).add(int(value, base))
    return found


class WireValues(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.firmware = firmware_definitions()

    def test_the_firmware_still_defines_every_name_we_use(self):
        missing = sorted(set(FIRMWARE_NAMES) - set(self.firmware))
        self.assertEqual(missing, [], f'not defined anywhere under {SOURCE}')

    def test_every_wire_value_we_define_is_one_we_check(self):
        checked = set(FIRMWARE_NAMES.values())
        unchecked = sorted(name for name, value in vars(protocol).items()
                           if name.isupper() and isinstance(value, int)
                           and value not in checked)
        self.assertEqual(unchecked, [], 'missing from FIRMWARE_NAMES')

    def test_every_constant_still_matches_the_firmware(self):
        drifted = {name: (ours, sorted(self.firmware[name]))
                   for name, ours in FIRMWARE_NAMES.items()
                   if name in self.firmware and self.firmware[name] != {ours}}
        self.assertEqual(drifted, {})


class ByteOrder(unittest.TestCase):
    def test_a_token_goes_out_most_significant_byte_first(self):
        self.assertEqual(to_board(0x64DE), b'\x64\xde')

    def test_a_reply_comes_back_least_significant_byte_first(self):
        self.assertEqual(from_board(b'\xcc\x64'), 0x64CC)

    def test_a_wider_parameter_keeps_the_same_order(self):
        self.assertEqual(to_board(0x01020304, 4), b'\x01\x02\x03\x04')


if __name__ == '__main__':
    unittest.main()
