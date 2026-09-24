# SPDX-License-Identifier: MIT
"""Check every wire value against the firmware that defines it.

  python3 -m unittest discover -s tools/bench

If the firmware moves a token, this fails rather than a board.
"""
import pathlib
import re
import unittest

import hostops
import protocol
from protocol import FIRMWARE_NAMES, from_board, to_board

SOURCE = pathlib.Path(__file__).resolve().parents[2] / 'Source' / 'Teensy'
VMFAIL = SOURCE / 'MinimalBoot' / 'Common' / 'VMFail.h'
# VmFail::describe()'s arms: `case Ok:  return "handed off to client";`
DESCRIBE = re.compile(r'^\s*case\s+\w+:\s*return\s+"([^"]*)";', re.M)

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


def firmware_phrases():
    """Every string VmFail::describe() can return."""
    return set(DESCRIBE.findall(VMFAIL.read_text(errors='replace')))


def bench_phrases():
    """{name: phrases} for every VmFail string hostops asserts on. Found by shape
    rather than listed, so a new one is checked without being added here twice."""
    found = {}
    for name, value in vars(hostops).items():
        if not name.isupper():
            continue
        if isinstance(value, str):
            found[name] = (value,)
        elif isinstance(value, tuple) and value and all(isinstance(v, str) for v in value):
            found[name] = value
    return found


class FailPhrases(unittest.TestCase):
    """The bench reads the VmFail record by matching describe()'s own words, so a
    renamed string turns every assertion on it into a silent pass. hostops.py says
    why the match is positive: after an install, a removal or a launch the failures
    outnumber the successes, so an unrecognised record has to fail."""

    def test_the_arms_this_reads_are_still_there_to_read(self):
        # The control. Without it a describe() this regex stops matching leaves the
        # test below comparing against an empty set, which passes for free.
        phrases = firmware_phrases()
        self.assertGreaterEqual(len(phrases), 20, f'{VMFAIL} parsed as {phrases}')
        self.assertIn('handed off to client', phrases)

    def test_every_phrase_we_assert_on_is_one_describe_can_return(self):
        firmware = firmware_phrases()
        drifted = {name: [p for p in phrases if p not in firmware]
                   for name, phrases in bench_phrases().items()}
        self.assertEqual({n: m for n, m in drifted.items() if m}, {},
                         f'no longer returned by VmFail::describe() in {VMFAIL}')

    def test_the_phrases_a_launch_may_end_on_are_the_three_we_expect(self):
        # Pinned by name as well as by value: a code added to describe() that is a
        # normal finish has to be added to FINISHED_NORMALLY by hand, and this is
        # what makes that a decision rather than an omission.
        self.assertEqual(hostops.FINISHED_NORMALLY,
                         ('handed off to client', 'module exited',
                          'extension host returned'))


class LaunchOutcome(unittest.TestCase):
    """hostops.finished_normally() is what decides exttest.py's exit status once the
    main image has answered. A reboot is not the outcome: the extension image resets
    back the same way whether the module finished or the loader gave up on it."""

    BOOT = 'Extension boot: {} (code ${:02x}, detail $0)\n'

    def normal(self, phrase):
        return hostops.finished_normally(self.BOOT.format(phrase, 0))

    def test_each_normal_finish_passes(self):
        for phrase in hostops.FINISHED_NORMALLY:
            with self.subTest(phrase=phrase):
                self.assertTrue(self.normal(phrase))

    def test_every_other_arm_of_describe_fails(self):
        for phrase in sorted(firmware_phrases() - set(hostops.FINISHED_NORMALLY)):
            with self.subTest(phrase=phrase):
                self.assertFalse(self.normal(phrase))

    def test_a_record_that_never_arrived_fails(self):
        # The reason the match is positive. "Extension boot: no record" is what the
        # main image prints when the slot held nothing readable, and a launch always
        # leaves at least $01 -- so an absent record after one is a failure, not a
        # quiet success.
        self.assertFalse(hostops.finished_normally(
            'Extension boot: no record ($00000000 at $2027ff60)\n'))
        self.assertFalse(hostops.finished_normally(''))

    def test_a_code_this_copy_has_never_heard_of_fails(self):
        self.assertFalse(self.normal('unknown'))

    def test_the_screen_half_matches_whatever_case_it_is_drawn_in(self):
        # The record reaches the C64 too, through SendMsgPrintfln, and the case a
        # glyph carries is a property of the screen rather than of the firmware.
        self.assertTrue(hostops.finished_normally('Extension: MODULE EXITED ($04)'))
        self.assertTrue(hostops.finished_normally('Extension: module exited ($04)'))


class ByteOrder(unittest.TestCase):
    def test_a_token_goes_out_most_significant_byte_first(self):
        self.assertEqual(to_board(0x64DE), b'\x64\xde')

    def test_a_reply_comes_back_least_significant_byte_first(self):
        self.assertEqual(from_board(b'\xcc\x64'), 0x64CC)

    def test_a_wider_parameter_keeps_the_same_order(self):
        self.assertEqual(to_board(0x01020304, 4), b'\x01\x02\x03\x04')


if __name__ == '__main__':
    unittest.main()
