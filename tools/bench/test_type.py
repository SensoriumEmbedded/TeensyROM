# SPDX-License-Identifier: MIT
"""Tests for type.py's text-to-key-codes step.   python3 -m unittest discover -s tools/bench"""
import importlib
import unittest

keys = importlib.import_module('type').keys


class Keys(unittest.TestCase):
    def test_letters_go_in_unshifted(self):
        self.assertEqual(keys('list'), b'LIST')

    def test_newline_is_return(self):
        self.assertEqual(keys('run\\n'), b'RUN\r')

    def test_hex_escape_is_sent_as_written(self):
        self.assertEqual(keys('\\x61\\x0a\\x85'), b'\x61\x0a\x85')

    def test_backslash_escapes_itself(self):
        self.assertEqual(keys('a\\\\b'), b'A\\B')

    def test_malformed_escape_is_refused(self):
        for text in ('ab\\', '\\xZZ', '\\x6', '\\t', '\\u0041'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                keys(text)

    def test_non_ascii_is_refused(self):
        with self.assertRaises(ValueError):
            keys('café')


if __name__ == '__main__':
    unittest.main()
