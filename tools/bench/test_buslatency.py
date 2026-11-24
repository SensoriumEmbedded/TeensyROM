# SPDX-License-Identifier: MIT
"""buslatency.py's reading of the board's 'U' reply. No hardware needed:

  python3 -m unittest discover -s tools/bench
"""
import pathlib
import re
import unittest
from buslatency import PROBES, parse, verdict

BUSSNOOP = pathlib.Path(__file__).resolve().parents[2] / 'Source' / 'Teensy' / 'BusSnoop.ino'

REPLY = """stray menu text
GPIO6 read from an earlier reply     12 kept     0 gave up
Bus latency, 4000 tries each, in CPU cycles at 816 MHz:
nothing (counter reads and DSB)     3990 kept     0 gave up  min    1  max    2  (1..2 nS)
GPIO6 read after a USB1 write       3871 kept     3 gave up  min   41  max   91  (50..111 nS)
USB1 ENDPTPRIME read                   0 kept  4000 gave up
USB1 USBSTS read                       0 kept     0 gave up
"""


class Parse(unittest.TestCase):
    def test_rows_after_the_header(self):
        rows = parse(REPLY)
        self.assertEqual(rows['nothing (counter reads and DSB)'], (3990, 0, 1, 2, 1, 2))
        self.assertEqual(rows['GPIO6 read after a USB1 write'], (3871, 3, 41, 91, 50, 111))
        self.assertNotIn('GPIO6 read from an earlier reply', rows)

    def test_a_probe_with_no_sample_has_no_timings(self):
        self.assertEqual(parse(REPLY)['USB1 ENDPTPRIME read'], (0, 4000, None, None, None, None))

    def test_no_header_means_the_command_is_not_in_the_image(self):
        self.assertIsNone(parse('Unknown command\n'))


KEPT = '  3990 kept     4 gave up  min    1  max    2  (1..2 nS)'


def full_reply(skip=()):
    rows = ''.join(f'{name:<34s}{KEPT}\n' for name in PROBES if name not in skip)
    return 'Bus latency, 4000 tries each, in CPU cycles at 816 MHz:\n' + rows


def with_row(name, counts):
    return full_reply().replace(f'{name:<34s}{KEPT}', f'{name:<34s}{counts}')


class Verdict(unittest.TestCase):
    def test_every_probe_keeping_samples_passes(self):
        self.assertIsNone(verdict(parse(full_reply())))

    def test_a_reply_cut_short_fails_even_when_every_row_that_came_kept_samples(self):
        problem = verdict(parse(full_reply(skip=PROBES[-1:])))
        self.assertIn(PROBES[-1], problem)

    def test_a_header_with_no_rows_fails(self):
        self.assertIsNotNone(verdict(parse(full_reply(skip=PROBES))))

    def test_a_probe_whose_tries_all_gave_up_says_so(self):
        problem = verdict(parse(with_row(PROBES[2], '     0 kept  4000 gave up')))
        self.assertIn(f'{PROBES[2]} (4000 gave up', problem)

    def test_a_probe_whose_tries_were_all_interrupted_says_none_gave_up(self):
        problem = verdict(parse(with_row(PROBES[2], '     0 kept     0 gave up')))
        self.assertIn(f'{PROBES[2]} (0 gave up', problem)

    def test_the_probe_names_are_the_firmwares(self):
        names = re.search(r'Names\[BL_NumProbes\]\[\d+\] PROGMEM = \{(.*?)\};', BUSSNOOP.read_text(), re.S)
        self.assertEqual(tuple(re.findall(r'"([^"]*)"', names[1])), PROBES)


if __name__ == '__main__':
    unittest.main()
