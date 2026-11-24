# SPDX-License-Identifier: MIT
"""buslatency.py's reading of the board's 'U' reply. No hardware needed:

  python3 -m unittest discover -s tools/bench
"""
import unittest
from buslatency import parse

REPLY = """stray menu text
GPIO6 read from an earlier reply     12 kept
Bus latency, 4000 tries each, in CPU cycles at 816 MHz:
nothing (counter reads and DSB)     3990 kept  min    1  max    2  (1..2 nS)
GPIO6 read after a USB1 write       3871 kept  min   41  max   91  (50..111 nS)
USB1 ENDPTPRIME read                   0 kept
"""


class Parse(unittest.TestCase):
    def test_rows_after_the_header(self):
        rows = parse(REPLY)
        self.assertEqual(rows['nothing (counter reads and DSB)'], (3990, 1, 2, 1, 2))
        self.assertEqual(rows['GPIO6 read after a USB1 write'], (3871, 41, 91, 50, 111))
        self.assertNotIn('GPIO6 read from an earlier reply', rows)

    def test_a_probe_with_no_sample_has_no_timings(self):
        self.assertEqual(parse(REPLY)['USB1 ENDPTPRIME read'], (0, None, None, None, None))

    def test_no_header_means_the_command_is_not_in_the_image(self):
        self.assertIsNone(parse('Unknown command\n'))


if __name__ == '__main__':
    unittest.main()
