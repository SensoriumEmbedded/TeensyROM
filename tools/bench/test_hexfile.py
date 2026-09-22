# SPDX-License-Identifier: MIT
"""Tests for reading a firmware hex.   python3 -m unittest discover -s tools/bench"""
import os
import pathlib
import re
import tempfile
import unittest

from hexfile import (MAIN_BASE, MAIN_LIMIT, MINIMAL_BASE, UnsupportedRecord,
                     build_stamp, decode, region)

LINKER = pathlib.Path(__file__).resolve().parents[1] / 'BootLinkerFiles'
FLASH = re.compile(r'FLASH\s*\([^)]*\)\s*:\s*ORIGIN\s*=\s*(0x[0-9A-Fa-f]+)\s*,'
                   r'\s*LENGTH\s*=\s*(\d+)K')
RECORD_DATA, RECORD_EXTENDED_ADDRESS, RECORD_SEGMENT_ADDRESS = 0, 4, 2
PAYLOAD_START = 9        # ':' plus count, address and type, as hex digits
END_OF_FILE_RECORD = ':00000001FF'


def flash_region(script):
    origin, length = FLASH.search((LINKER / script).read_text()).groups()
    return int(origin, 16), int(length) * 1024


def record(address, kind, data):
    body = bytes([len(data), (address >> 8) & 0xff, address & 0xff, kind]) + data
    return ':' + (body + bytes([-sum(body) & 0xff])).hex().upper()


def intel_hex(blocks):
    """{flash address: bytes} as an Intel HEX file. Blocks stay inside one 64K page."""
    lines, page = [], None
    for address in sorted(blocks):
        for offset in range(0, len(blocks[address]), 16):
            at = address + offset
            if at >> 16 != page:
                page = at >> 16
                lines.append(record(0, RECORD_EXTENDED_ADDRESS, bytes([page >> 8, page & 0xff])))
            lines.append(record(at & 0xffff, RECORD_DATA, blocks[address][offset:offset + 16]))
    return '\n'.join(lines + [END_OF_FILE_RECORD]) + '\n'


def stamped(date, time):
    return b'\x00' + date.encode() + b'\x00' + time.encode() + b'\x00'


class FlashLayout(unittest.TestCase):
    def test_the_main_image_still_links_where_we_look_for_it(self):
        origin, length = flash_region('imxrt1062_t41.ld.upper')
        self.assertEqual((MAIN_BASE, MAIN_LIMIT), (origin, origin + length))

    def test_the_minimal_image_still_links_at_the_base_of_flash(self):
        self.assertEqual(MINIMAL_BASE, flash_region('imxrt1062_t41.ld.orig')[0])


class Decoding(unittest.TestCase):
    def test_extended_addresses_place_bytes_in_flash(self):
        self.assertEqual(decode(intel_hex({MAIN_BASE + 0x10: b'\xde\xad'})),
                         [(MAIN_BASE + 0x10, b'\xde\xad')])

    def test_a_flipped_byte_is_refused_by_the_record_checksum(self):
        good = intel_hex({MAIN_BASE: b'\x01\x02'})
        data = good.splitlines()[1]
        flipped = data[:PAYLOAD_START] + ('0' if data[PAYLOAD_START] != '0' else '1') \
            + data[PAYLOAD_START + 1:]
        with self.assertRaises(ValueError):
            decode(good.replace(data, flipped))

    def test_a_record_type_this_reader_lacks_is_told_apart_from_a_bad_file(self):
        with self.assertRaises(UnsupportedRecord):
            decode(record(0, RECORD_SEGMENT_ADDRESS, b'\x5a') + '\n' + END_OF_FILE_RECORD)

    def test_a_file_that_is_not_hex_at_all_is_not_an_unsupported_record(self):
        for text in ('this is not a hex file\n', ':\n' + END_OF_FILE_RECORD):
            with self.assertRaises(ValueError) as caught:
                decode(text)
            self.assertNotIsInstance(caught.exception, UnsupportedRecord)

    def test_a_record_too_short_to_have_a_length_is_refused(self):
        with self.assertRaises(ValueError):
            decode(':\n' + END_OF_FILE_RECORD)

    def test_an_extended_address_record_carrying_no_address_is_refused(self):
        with self.assertRaises(ValueError):
            decode(record(0, RECORD_EXTENDED_ADDRESS, b'') + '\n' + END_OF_FILE_RECORD)

    def test_region_fills_gaps_so_a_match_cannot_span_them(self):
        block = region(decode(intel_hex({MAIN_BASE: b'AB', MAIN_BASE + 0x20: b'CD'})),
                       MAIN_BASE, MAIN_LIMIT)
        self.assertEqual(block[:2] + block[-2:], b'ABCD')
        self.assertEqual(set(block[2:-2]), {0xFF})

    def test_a_block_straddling_the_base_keeps_the_part_inside_it(self):
        self.assertEqual(region([(MAIN_BASE - 2, b'\x01\x02\x03\x04')],
                                MAIN_BASE, MAIN_LIMIT), b'\x03\x04')


class BuildStamp(unittest.TestCase):
    def written(self, blocks):
        handle = tempfile.NamedTemporaryFile('w', suffix='.hex', delete=False)
        handle.write(intel_hex(blocks))
        handle.close()
        self.addCleanup(os.unlink, handle.name)
        return handle.name

    def test_the_stamp_comes_from_the_main_image_not_the_minimal_one(self):
        path = self.written({MINIMAL_BASE: stamped('Jan  1 1999', '11:11:11'),
                             MAIN_BASE: stamped('Sep 22 2026', '01:15:31')})
        self.assertEqual(build_stamp(path), ('Sep 22 2026', '01:15:31'))

    def test_two_candidate_stamps_report_nothing_rather_than_a_guess(self):
        path = self.written({MAIN_BASE: stamped('Sep 22 2026', '01:15:31'),
                             MAIN_BASE + 0x40: stamped('Sep 22 2026', '02:00:00')})
        self.assertIsNone(build_stamp(path))

    def test_a_hex_with_no_main_image_reports_nothing(self):
        path = self.written({MINIMAL_BASE: stamped('Sep 22 2026', '01:15:31')})
        self.assertIsNone(build_stamp(path))


if __name__ == '__main__':
    unittest.main()
