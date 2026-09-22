# SPDX-License-Identifier: MIT
"""Read a TeensyROM firmware hex, to check a board against the file pushed to it.

One hex holds two images at fixed flash addresses. The build pins
SOURCE_DATE_EPOCH, by default to the HEAD commit time, so GCC gives every
image in one build the same __DATE__ and __TIME__ -- and that pair is the
second line of the version banner, which makes it the one field that tells
two builds of the same version number apart.

Two builds of one commit therefore carry one stamp, so a matching stamp says
the board is running a build of that commit rather than that it was reflashed
just now. Both images carry it, so the stamp alone does not say which of the
two is running.
"""
import re

# The FLASH regions the two images are linked against, from
# tools/BootLinkerFiles/imxrt1062_t41.ld.orig and .ld.upper.
MINIMAL_BASE = 0x60000000
MAIN_BASE, MAIN_BYTES = 0x60060000, 7552 * 1024
MAIN_LIMIT = MAIN_BASE + MAIN_BYTES

DATA, END_OF_FILE, EXTENDED_ADDRESS = 0, 1, 4
ENTRY_POINT = (3, 5)         # the address is in the IVT, so this reader ignores both forms;
                             # FXUtil.cpp:211 aborts on 3 and takes 5 as a base address
RECORD_OVERHEAD = 5          # count, address, type and checksum, around the data
EXTENDED_ADDRESS_BYTES = 2
ERASED = 0xFF

# __DATE__ and __TIME__ as GCC emits them: NUL-terminated literals that do not
# continue a longer printable run.
DATE = re.compile(rb'(?<![!-~])([A-Z][a-z]{2} [ 0-3][0-9] [0-9]{4})\x00')
TIME = re.compile(rb'(?<![!-~])([0-2][0-9]:[0-5][0-9]:[0-5][0-9])\x00')


class UnsupportedRecord(ValueError):
    """A well-formed HEX record of a type this reader does not implement, as
    opposed to a file that is not Intel HEX."""


def decode(text):
    """Intel HEX as a list of (address, data) blocks in file order, honouring
    extended linear address records. Raises UnsupportedRecord for a well-formed
    record this reader does not implement, ValueError for anything else."""
    blocks, page = [], 0
    for number, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        if not line.startswith(':') or len(line) % 2 != 1:
            raise ValueError(f'line {number}: not a HEX record')
        record = bytes.fromhex(line[1:])
        if (len(record) < RECORD_OVERHEAD or len(record) != record[0] + RECORD_OVERHEAD
                or sum(record) & 0xff):
            raise ValueError(f'line {number}: bad HEX length or checksum')
        count, address, kind = record[0], record[1] << 8 | record[2], record[3]
        if kind == DATA:
            blocks.append((page + address, record[4:4 + count]))
        elif kind == EXTENDED_ADDRESS:
            if count != EXTENDED_ADDRESS_BYTES:
                raise ValueError(f'line {number}: extended address in {count} bytes, '
                                 f'not {EXTENDED_ADDRESS_BYTES}')
            page = (record[4] << 8 | record[5]) << 16
        elif kind != END_OF_FILE and kind not in ENTRY_POINT:
            raise UnsupportedRecord(f'line {number}: HEX record type {kind} is not '
                                    f'implemented')
    if not blocks:
        raise ValueError('no data records')
    return blocks


def clip(address, data, base, limit):
    """The part of one block that lands between two addresses, with its address."""
    start, stop = max(address, base), min(address + len(data), limit)
    return start, data[start - address:stop - address]


def region(blocks, base, limit):
    """The bytes those blocks place between two addresses, gaps left erased."""
    inside = [clip(a, d, base, limit) for a, d in blocks
              if a < limit and a + len(d) > base]
    if not inside:
        return b''
    low, high = min(a for a, _ in inside), max(a + len(d) for a, d in inside)
    block = bytearray([ERASED]) * (high - low)
    for address, data in inside:
        block[address - low:address - low + len(data)] = data
    return bytes(block)


def build_stamp(path):
    """The ('Sep 22 2026', '01:15:31') GCC compiled into the main image, or None
    when the file does not hold exactly one of each. Raises UnsupportedRecord or
    ValueError as decode() does."""
    with open(path) as handle:
        main = region(decode(handle.read()), MAIN_BASE, MAIN_LIMIT)
    dates = {m.group(1).decode() for m in DATE.finditer(main)}
    times = {m.group(1).decode() for m in TIME.finditer(main)}
    if len(dates) != 1 or len(times) != 1:
        return None
    return dates.pop(), times.pop()
