# SPDX-License-Identifier: MIT
"""The TeensyROM control protocol: every value that goes on the wire.

Each name here is one the firmware also uses, and FIRMWARE_NAMES maps them to
the C identifiers they came from. test_protocol.py reads those out of
Source/Teensy/ and fails if any pair has drifted.

Commands and the integers inside them go to the board most significant byte
first; the board answers least significant byte first. See docs/ControlComms.md.
"""

# Commands. Always available, whatever else the board is doing:
LAUNCH_FILE = 0x6444
RESET_C64 = 0x64EE
VERSION_INFO = 0x6476
FW_CHECK = 0x64E0
READ_C64_MEM = 0x64FD      # TR+ only: needs Fab04_FullDMACapable
WRITE_C64_MEM = 0x64FB     # TR+ only
# Refused with FAIL + "Busy!" unless the board is sitting at its own menu:
POST_FILE = 0x64BB
DELETE_FILE = 0x64CF
GET_DIR_NDJSON = 0x64DE

# Replies.
ACK = 0x64CC
FAIL = 0x9B7F
FW_FULL = 0x64E2
FW_MINIMAL = 0x64E1
DIR_START = 0x5A5A
DIR_END = 0xA5A5

# Which image answered FW_CHECK, under the names this package uses for them.
IMAGES = {FW_FULL: 'main', FW_MINIMAL: 'minimal'}

# Drive numbers, in file and directory commands.
DRIVE_USB, DRIVE_SD, DRIVE_TEENSY = 0, 1, 2
DRIVE_NAMES = {DRIVE_USB: 'USB drive', DRIVE_SD: 'SD card',
               DRIVE_TEENSY: 'built-in menu'}

FIRMWARE_NAMES = {
    'LaunchFileToken': LAUNCH_FILE,
    'ResetC64Token': RESET_C64,
    'VersionInfoToken': VERSION_INFO,
    'FWCheckToken': FW_CHECK,
    'ReadC64MemToken': READ_C64_MEM,
    'WriteC64MemToken': WRITE_C64_MEM,
    'PostFileToken': POST_FILE,
    'DeleteFileToken': DELETE_FILE,
    'GetDirNDJSONToken': GET_DIR_NDJSON,
    'AckToken': ACK,
    'FailToken': FAIL,
    'FWFullToken': FW_FULL,
    'FWMinimalToken': FW_MINIMAL,
    'StartDirectoryListToken': DIR_START,
    'EndDirectoryListToken': DIR_END,
    'rmtUSBDrive': DRIVE_USB,
    'rmtSD': DRIVE_SD,
    'rmtTeensy': DRIVE_TEENSY,
}


def to_board(value, width=2):
    """A token or an integer parameter, most significant byte first."""
    return bytes((value >> (8 * i)) & 0xff for i in range(width - 1, -1, -1))


def from_board(data):
    """The 16-bit value in a two-byte reply, least significant byte first."""
    return data[0] | (data[1] << 8)


def board_reply(value):
    """The two bytes the board sends for a 16-bit value: the inverse of
    from_board, for finding a marker in a stream."""
    return bytes((value & 0xff, (value >> 8) & 0xff))
