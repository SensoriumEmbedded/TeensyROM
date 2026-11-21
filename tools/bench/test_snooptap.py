# SPDX-License-Identifier: MIT
"""snooptap.py's decoding of the snoop tap ring. No hardware needed:

  python3 -m unittest discover -s tools/bench
"""
import unittest
from snooptap import (ASSERT, ASSERT_DETAIL, CYCLE, CYCLE_DETAIL, LATE_DECODE, LATE_DECODE_PC, LATE_ENTRY,
                      LATE_ENTRY_PC, decode)

HZ = 816_000_000


class Decode(unittest.TestCase):
    def test_plain_accesses_and_slices_read_as_before(self):
        rows = decode([(100, 0xDF02, 0x14, 0x00), (200, 0xDF07, 0, 0x40 | 7), (300, 0x6000, 0x40, 0x80)], HZ)
        self.assertEqual([t for _, t in rows],
                         ['W $DF02 = $14  dma=DisableReady', 'R $DF07          dma=StartAsynch',
                          'SLICE $6000 length low byte $40'])

    def test_a_late_entry_pairs_with_its_interrupted_pc(self):
        (cyc, text), = decode([(500, 1307, 0, LATE_ENTRY), (0x0002A7B2, 129, 0x20, LATE_ENTRY_PC)], HZ)
        self.assertEqual(cyc, 500)
        self.assertIn('LATE ENTRY 1307 nS', text)
        self.assertIn('pc=0x0002A7B2 exception 129 basepri=0x20', text)

    def test_an_assert_names_its_reason_and_both_cycles(self):
        (_, text), = decode([(1, 0xC674, 0x40 | 0x20 | 0, ASSERT), ((489 << 16) | 30, 0xC673, 61, ASSERT_DETAIL)], HZ)
        self.assertIn('bad line read: on R $C674 (before it R $C673)', text)
        self.assertIn('Phi2 fell 489 nS after entry, assert 30 nS after that; entry 976 nS', text)

    def test_a_late_decode_shows_when_it_decoded(self):
        (_, text), = decode([(1, 0xC5DB, 0x40, LATE_DECODE), (0xF3A, 1074, 25, LATE_DECODE_PC)], HZ)
        self.assertIn('LATE DECODE R $C5DB', text)
        self.assertIn('decoded 400 nS after entry, entry 1074 nS after the last, interrupted pc=0x00000F3A', text)

    def test_cycle_timing_follows_its_access_and_takes_its_stamp(self):
        rows = decode([(7000, 0xDF02, 0xFF, 0x00), ((26392 << 16) | 71, 368, 0xFF, CYCLE),
                       (436, 1001, 0, CYCLE_DETAIL)], HZ)
        self.assertEqual(rows[1][0], 7000)
        self.assertIn('GPIO6 read 26392 nS after entry, then GPIO9 +71, data sampled +368', rows[1][1])
        self.assertIn('handlers done +436 nS; entry 1001 nS after the last', rows[1][1])

    def test_a_read_shows_drive_release_and_an_early_release(self):
        (_, text), = decode([((175 << 16) | 65, 0x8000 | 175, 58, CYCLE), (0, 978, 0, CYCLE_DETAIL)], HZ)
        self.assertIn('data driven +175, released +232 (Phi2 fell first)', text)

    def test_a_port_the_handler_left_alone_says_so(self):
        (_, text), = decode([((175 << 16) | 65, 0x7FFF, 0xFF, CYCLE), (0, 978, 0, CYCLE_DETAIL)], HZ)
        self.assertIn('port not driven or sampled this cycle', text)

    def test_a_timing_record_cut_off_by_the_ring_edge_is_named(self):
        (_, text), = decode([(1, 1307, 0, LATE_ENTRY)], HZ)
        self.assertEqual(text, 'timing record 0xC0 without its detail')

    def test_cycle_timing_whose_access_the_ring_overwrote_has_no_stamp(self):
        rows = decode([((175 << 16) | 65, 0x7FFF, 0xFF, CYCLE), (0, 978, 0, CYCLE_DETAIL), (9000, 0xDF04, 0, 0x40)], HZ)
        self.assertEqual(rows[0][0], None)
        self.assertEqual(rows[1], (9000, 'R $DF04          dma=DisableReady'))

    def test_a_detail_whose_record_the_ring_overwrote_is_named(self):
        rows = decode([(1, 978, 0, CYCLE_DETAIL), (2, 0xDF04, 0, 0x40)], HZ)
        self.assertEqual([t for _, t in rows], ['timing detail 0xC7 without its record', 'R $DF04          dma=DisableReady'])


if __name__ == '__main__':
    unittest.main()
