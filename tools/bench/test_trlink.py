# SPDX-License-Identifier: MIT
"""Tests for the bench scripts against a fake board on a pty. No hardware needed:

  python3 -m unittest discover -s tools/bench

The fake answers the same commands as a real board, using protocol.py's names
but its own byte order, so a script and the fake cannot drift together. It
proves the framing and reply handling; it cannot prove what a real board does.
"""
import json
import os
import pathlib
import pty
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import unittest

from hexfile import MAIN_BASE, MINIMAL_BASE
from protocol import (ACK, DELETE_FILE, DIR_END, DIR_START, FAIL, FW_CHECK,
                      FW_FULL, FW_MINIMAL, GET_DIR_NDJSON, LAUNCH_FILE,
                      POST_FILE, READ_C64_MEM, RESET_C64, VERSION_INFO,
                      WRITE_C64_MEM)
from test_hexfile import RECORD_SEGMENT_ADDRESS, intel_hex, record, stamped
from trlink import Link, ports

HERE = os.path.dirname(os.path.abspath(__file__))
COMMAND_PREFIX = 0x64
STAMP_DATE, STAMP_TIME = 'Sep 22 2026', '01:15:31'
OTHER_DATE, OTHER_TIME = 'Jan  1 1999', '11:11:11'
STAMP = f'{STAMP_DATE}, {STAMP_TIME}'
BANNER = f'\n  FW: TeensyROM+ v0.8.0.9\n      {STAMP}\n'
UPDATER_OUTPUT = b'Erasing...\r\nProgramming...\r\n'
CHATTER = b'Loading IO handler: TeensyROM\n'
PORT_DOWN, BEFORE_REBOOT, BOOT_SILENCE = 0.2, 0.8, 2.0
IO_TIMEOUT, IO_RETRY, IO_CHUNK = 3, 0.005, 65536
# The names a TR+ enumerated under either side of a firmware change: the USB
# serial number moves, so the node does too.
ORIGINAL_PORT, RENAMED_PORT = 'cu.usbmodem192885901', 'cu.usbmodem2101'


def reply(token):
    """The board answers least significant byte first."""
    return bytes([token & 0xff, (token >> 8) & 0xff])


class FakeBoard:
    def __init__(self, port=None):
        """`port` is a path to publish the pty under, for a board that has to
        drop it and come back; without one the pty's own name is the port."""
        self.link = port
        self.mem = bytearray(65536)
        self.files = {}
        self.launched = []
        self.entries_by_path = {}
        self.listed = []
        self.resets = 0
        self.refuse_reads = False
        self.chatter = b''
        self.silence = 0         # seconds after a boot in which it answers nothing
        self.stop_listing_early = False
        self.image = FW_FULL     # None models the extension image: USB is off
        self.buf = b''
        self.stop = False
        self.detached = True
        self.port_lock = threading.Lock()
        self.attach()
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def attach(self):
        with self.port_lock:
            if self.stop:
                return
            self.master, self._slave = pty.openpty()  # the slave stays open, so no hangup
            os.set_blocking(self.master, False)
            self.buf = b''
            self.answers_at = time.time() + self.silence
            self.detached = False
            self.port = self.link or os.ttyname(self._slave)
            if self.link:
                pending = self.link + '.pending'
                os.symlink(os.ttyname(self._slave), pending)
                os.replace(pending, self.link)

    def detach(self):
        with self.port_lock:
            if self.detached:
                return
            self.detached = True
            if self.link:
                os.remove(self.link)
            os.close(self.master)
            os.close(self._slave)

    def reboot(self, port=None):
        """Drop the port and bring it back under a new pty, as a board that has
        just been reflashed does. `port` publishes it at a different path, which
        the board does when its USB serial number changes."""
        self.detach()
        time.sleep(PORT_DOWN)
        self.link = port or self.link
        self.attach()

    def close(self):
        self.stop = True
        self.thread.join(2)
        self.detach()

    def receive(self):
        """Whatever the port has, or nothing once detach() has taken it away."""
        with self.port_lock:
            if self.detached:
                return b''
            try:
                return os.read(self.master, IO_CHUNK)
            except (BlockingIOError, OSError):
                return b''

    def take(self, n):
        end = time.time() + IO_TIMEOUT
        while len(self.buf) < n and time.time() < end and not self.stop:
            chunk = self.receive()
            self.buf += chunk
            if not chunk:
                time.sleep(IO_RETRY)
        got, self.buf = self.buf[:n], self.buf[n:]
        return got

    def word(self):
        """A parameter, most significant byte first, as the firmware reads them."""
        data = self.take(2)
        return data[0] << 8 | data[1]

    def cstring(self):
        out = b''
        while True:
            c = self.take(1)
            if not c or c == b'\0':
                return out.decode()
            out += c

    def send(self, data):
        """Write a reply, or nothing once detach() has taken the port away."""
        view, end = memoryview(data), time.time() + IO_TIMEOUT
        while view and time.time() < end:
            with self.port_lock:
                if self.detached:
                    return
                try:
                    view = view[os.write(self.master, view):]
                except BlockingIOError:
                    pass
                except OSError:
                    return
            if view:
                time.sleep(IO_RETRY)

    def run(self):
        """Answer commands until close(), surviving a command a detach truncated."""
        while not self.stop:
            try:
                self.respond()
            except (IndexError, OSError):
                pass

    def respond(self):
        """Read one command and answer it."""
        first = self.take(1)
        if first == b'b':          # the one single-byte command: BusAnalysis
            self.send(b'bus ok\n')
            return
        if first != bytes([COMMAND_PREFIX]):
            return
        second = self.take(1)
        if not second:
            return
        command = COMMAND_PREFIX << 8 | second[0]
        if command == FW_CHECK:
            if self.image and time.time() >= self.answers_at:
                self.send(self.chatter + reply(self.image))
        elif command == VERSION_INFO:
            self.send(reply(ACK) + BANNER.encode())
        elif command == RESET_C64:
            self.resets += 1
            self.send(b'Reset cmd received\n')
        elif command == READ_C64_MEM:
            address, length = self.word(), self.word()
            if self.refuse_reads:
                self.send(reply(FAIL) + b'no DMA')
            else:
                self.send(reply(ACK) + bytes(self.mem[address:address + length]))
        elif command == WRITE_C64_MEM:
            address, length = self.word(), self.word()
            data = self.take(length)
            if len(data) == length:
                self.mem[address:address + length] = data
            self.send(reply(ACK))
        elif command == DELETE_FILE:
            self.send(reply(ACK))
            self.take(1)
            self.files.pop(self.cstring(), None)
            self.send(reply(ACK))
        elif command == POST_FILE:
            self.send(reply(ACK))
            header = self.take(4)
            size, checksum = int.from_bytes(header, 'big'), self.word()
            self.take(1)
            name = self.cstring()
            self.send(reply(ACK))
            data = self.take(size)
            if sum(data) & 0xffff == checksum:
                self.files[name] = data
                self.send(reply(ACK))
            else:
                self.send(reply(FAIL) + b'checksum')
        elif command == GET_DIR_NDJSON:
            self.send(reply(ACK))
            drive = self.take(1)[0]
            page = self.take(4)
            skip, take = page[0] << 8 | page[1], page[2] << 8 | page[3]
            path = self.cstring()
            self.listed.append((drive, path, skip, take))
            self.send(reply(ACK) + reply(DIR_START))
            for entry in self.entries_by_path.get(path, [])[skip:skip + take]:
                self.send(json.dumps(entry, separators=(',', ':')).encode() + b'\r\n')
            if not self.stop_listing_early:
                self.send(reply(DIR_END))
        elif command == LAUNCH_FILE:
            self.send(reply(ACK))
            self.take(1)
            self.launched.append(self.cstring())
            self.send(reply(ACK))


def run(board, script, *args, timeout=30):
    env = dict(os.environ, TR_PORT=board.port)
    return subprocess.run([sys.executable, os.path.join(HERE, script), *args], env=env,
                          capture_output=True, text=True, timeout=timeout)


class BenchScripts(unittest.TestCase):
    def setUp(self):
        self.board = FakeBoard()

    def tearDown(self):
        self.board.close()

    def test_peek_reads_big_endian_addresses_and_lengths(self):
        self.board.mem[0xFF00:0xFF04] = bytes([0x78, 0xA2, 0xFF, 0x9A])
        out = run(self.board, 'peek.py', 'FF00', '4')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertIn('$FF00  78 A2 FF 9A', out.stdout)

    def test_screen_decodes_screen_codes(self):
        self.board.mem[0x400:0x400 + 5] = bytes([8, 5, 12, 12, 15])  # HELLO
        out = run(self.board, 'screen.py')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertIn(' 0 |HELLO', out.stdout)

    def test_keypress_writes_buffer_then_count(self):
        out = run(self.board, 'keypress.py', '0x87')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertEqual((self.board.mem[0x277], self.board.mem[0xC6]), (0x87, 1))

    def test_push_delivers_the_file_with_a_valid_checksum(self):
        with tempfile.NamedTemporaryFile() as f:
            f.write(bytes(range(256)) * 40)
            f.flush()
            out = run(self.board, 'push.py', f'{f.name}=/VMS/T/engine.mvm')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertEqual(self.board.files['/VMS/T/engine.mvm'], bytes(range(256)) * 40)

    def test_push_replaces_an_existing_file(self):
        self.board.files['/A.bin'] = b'old'
        with tempfile.NamedTemporaryFile() as f:
            f.write(b'new')
            f.flush()
            out = run(self.board, 'push.py', f'{f.name}=/A.bin')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertEqual(self.board.files['/A.bin'], b'new')

    def test_launch_sends_drive_then_path(self):
        out = run(self.board, 'launch.py', '/HELLO.crt', '1', '1')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertEqual(self.board.launched, ['/HELLO.crt'])

    def test_a_refused_read_is_reported_with_the_boards_message(self):
        self.board.refuse_reads = True
        out = run(self.board, 'peek.py', '0400', '4')
        self.assertNotEqual(out.returncode, 0)
        self.assertIn('no DMA', out.stderr)

    def test_probe_names_the_running_image_and_prints_its_banner(self):
        out = run(self.board, 'probe.py')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertIn('image  main', out.stdout)
        self.assertIn('v0.8.0.9', out.stdout)

    def test_probe_reports_minimal_separately_from_main(self):
        self.board.image = FW_MINIMAL
        out = run(self.board, 'probe.py')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertIn('image  minimal', out.stdout)

    def test_probe_calls_a_silent_port_out_and_fails(self):
        self.board.image = None
        out = run(self.board, 'probe.py')
        self.assertEqual(out.returncode, 1)
        self.assertIn('silent', out.stdout)

    def test_ls_shows_directories_and_file_sizes(self):
        self.board.entries_by_path['/VMS'] = [
            {'type': 'dir', 'name': 'HELLO'},
            {'type': 'file', 'name': 'engine.mvm', 'size': 8192}]
        out = run(self.board, 'ls.py', '/VMS')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertIn('<dir>  HELLO', out.stdout)
        self.assertIn('8192  engine.mvm', out.stdout)
        self.assertIn('2 entries in /VMS', out.stdout)

    def test_ls_sends_the_drive_the_path_and_big_endian_paging(self):
        out = run(self.board, 'ls.py', '/GAMES', '0')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertEqual(self.board.listed, [(0, '/GAMES', 0, 1000)])

    def test_ls_fails_loudly_when_the_listing_never_ends(self):
        self.board.entries_by_path['/VMS'] = [{'type': 'dir', 'name': 'HELLO'}]
        self.board.stop_listing_early = True
        out = run(self.board, 'ls.py', '/VMS')
        self.assertNotEqual(out.returncode, 0)
        self.assertIn('stopped before its end marker', out.stderr)

    def test_ls_rejects_a_drive_that_is_not_one(self):
        out = run(self.board, 'ls.py', '/', 'sd')
        self.assertNotEqual(out.returncode, 0)
        self.assertIn("drive 'sd'", out.stderr)

    def test_probe_reads_past_what_the_board_says_unprompted(self):
        self.board.chatter = b'Loading IO handler: TeensyROM\n'
        out = run(self.board, 'probe.py')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertIn('image  main', out.stdout)

    def test_reset_prints_what_the_board_answered(self):
        out = run(self.board, 'reset.py')
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertIn('Reset cmd received', out.stdout)
        self.assertEqual(self.board.resets, 1)

    def test_no_board_is_a_clear_error(self):
        env = {k: v for k, v in os.environ.items() if k != 'TR_PORT'}
        # Cannot assume no usbmodem device exists on the machine running the test.
        out = subprocess.run([sys.executable, '-c',
                              'import trlink; trlink.ports = lambda: []; trlink.find_port()'],
                             cwd=HERE, env=env, capture_output=True, text=True)
        self.assertIn('no TeensyROM serial port', out.stderr)


class PortNames(unittest.TestCase):
    """A Teensy enumerates under a different name on each platform."""

    def test_the_macos_and_linux_node_names_are_found_and_others_are_not(self):
        folder = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, folder, ignore_errors=True)
        for name in ('cu.usbmodem2101', 'ttyACM0',
                     'tty.usbmodem2101', 'ttyS0', 'cu.Bluetooth'):
            pathlib.Path(folder, name).touch()
        found = ports(os.path.join(folder, 'anything'))
        self.assertEqual([os.path.basename(node) for node in found],
                         ['cu.usbmodem2101', 'ttyACM0'])


class PortChoice(unittest.TestCase):
    """What Link does with a node that is not a serial port, which reconnect()
    reaches for on its own."""

    def test_a_node_that_is_not_a_serial_port_is_an_oserror_and_is_not_kept(self):
        with tempfile.NamedTemporaryFile(suffix='cu.usbmodem0') as decoy:
            open_fds = len(os.listdir('/dev/fd'))
            with self.assertRaises(OSError):
                Link(decoy.name, settle=0)
            self.assertEqual(len(os.listdir('/dev/fd')), open_fds)


class Reflash(unittest.TestCase):
    """fwupdate end to end, against a board that really drops its port and
    comes back."""

    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir, ignore_errors=True)
        self.board = FakeBoard(port=os.path.join(self.dir, ORIGINAL_PORT))
        self.addCleanup(self.board.close)
        self.board.chatter = CHATTER
        row = [ord(c) - 64 if c.isalpha() else ord(c) for c in 'Y/N']
        self.board.mem[0x400 + 40:0x400 + 40 + 3] = bytes(row)
        self.image = intel_hex({MINIMAL_BASE: stamped(STAMP_DATE, STAMP_TIME),
                                MAIN_BASE: stamped(STAMP_DATE, STAMP_TIME)})
        self.hex = os.path.join(self.dir, 'fw.hex')
        pathlib.Path(self.hex).write_text(self.image)

    def reboot_when_answered(self, comes_back_as, drops, renamed):
        deadline = time.time() + 30
        while time.time() < deadline and self.board.mem[0xC6] != 1:
            time.sleep(0.05)
        self.board.send(UPDATER_OUTPUT)
        self.board.image = comes_back_as
        node = renamed and os.path.join(self.dir, renamed)
        for drop in range(drops):
            time.sleep(BEFORE_REBOOT)
            self.board.reboot(port=node if drop == drops - 1 else None)

    def undecodable(self):
        """A hex carrying a record type hexfile.py does not implement."""
        return (record(0, RECORD_SEGMENT_ADDRESS, b'\x00\x00') + '\n'
                + intel_hex({MAIN_BASE: stamped(STAMP_DATE, STAMP_TIME)}))

    def reflash(self, comes_back_as=FW_FULL, drops=1, renamed=None):
        threading.Thread(target=self.reboot_when_answered, daemon=True,
                         args=(comes_back_as, drops, renamed)).start()
        return run(self.board, 'fwupdate.py', self.hex, timeout=120)

    def test_a_reflash_that_took_is_checked_against_the_hex_it_pushed(self):
        out = self.reflash()
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertEqual(self.board.launched, ['/fwupdate.hex'])
        self.assertEqual((self.board.mem[0x277], self.board.mem[0xC6]), (0x59, 1))
        self.assertEqual(self.board.files['/fwupdate.hex'], self.image.encode())
        self.assertIn(f'build stamp {STAMP}', out.stdout)
        self.assertIn('OK: the main image reports the build stamp', out.stdout)
        self.assertIn(CHATTER.decode(), out.stdout)
        self.assertEqual(out.stdout.count('--- boot output ---'), 1)

    def test_the_minimal_image_reporting_the_right_stamp_is_still_a_failure(self):
        out = self.reflash(comes_back_as=FW_MINIMAL)
        self.assertIn('image  minimal', out.stdout)
        self.assertIn(f'v0.8.0.9\n      {STAMP}', out.stdout)
        self.assertNotEqual(out.returncode, 0)
        self.assertIn('came back as "minimal"', out.stderr)

    def test_a_second_drop_in_the_boot_window_is_waited_out(self):
        self.board.silence = BOOT_SILENCE
        out = self.reflash(drops=2)
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertIn('OK: the main image reports the build stamp', out.stdout)
        self.assertEqual(out.stdout.count('--- boot output ---'), 2)

    def test_a_board_still_running_the_old_firmware_is_caught_by_the_stamp(self):
        pathlib.Path(self.hex).write_text(
            intel_hex({MAIN_BASE: stamped(OTHER_DATE, OTHER_TIME)}))
        out = self.reflash()
        self.assertNotEqual(out.returncode, 0)
        self.assertIn('image  main', out.stdout)
        self.assertIn(f'does not report "{OTHER_DATE}, {OTHER_TIME}"', out.stderr)

    def test_a_hex_with_no_single_stamp_is_flashed_and_still_checked_for_main(self):
        pathlib.Path(self.hex).write_text(intel_hex({
            MAIN_BASE: stamped(STAMP_DATE, STAMP_TIME),
            MAIN_BASE + 0x40: stamped(OTHER_DATE, OTHER_TIME)}))
        out = self.reflash()
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertIn('no single build stamp in the main image', out.stdout)
        self.assertIn('/fwupdate.hex', self.board.files)
        self.assertIn('--- boot output ---', out.stdout)
        self.assertIn('came back on the main image', out.stdout)

    def test_a_board_that_comes_back_under_a_new_name_is_still_found(self):
        out = self.reflash(renamed=RENAMED_PORT)
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertEqual(os.path.basename(self.board.port), RENAMED_PORT)
        self.assertIn('OK: the main image reports the build stamp', out.stdout)

    def test_a_rename_on_a_second_drop_is_not_read_as_another_device(self):
        self.board.silence = BOOT_SILENCE
        out = self.reflash(drops=2, renamed=RENAMED_PORT)
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertEqual(os.path.basename(self.board.port), RENAMED_PORT)
        self.assertIn('OK: the main image reports the build stamp', out.stdout)

    def test_a_record_type_this_reader_lacks_costs_the_stamp_not_the_image_check(self):
        pathlib.Path(self.hex).write_text(self.undecodable())
        out = self.reflash()
        self.assertEqual(out.returncode, 0, out.stderr)
        self.assertIn('record type 2 is not implemented', out.stdout)
        self.assertIn('/fwupdate.hex', self.board.files)
        self.assertIn('came back on the main image', out.stdout)

    def test_an_unreadable_stamp_does_not_excuse_a_fallback_to_minimal(self):
        pathlib.Path(self.hex).write_text(self.undecodable())
        out = self.reflash(comes_back_as=FW_MINIMAL)
        self.assertNotEqual(out.returncode, 0)
        self.assertIn('came back as "minimal"', out.stderr)

    def test_a_hex_that_will_not_decode_is_refused_before_anything_is_pushed(self):
        pathlib.Path(self.hex).write_text('this is not a hex file\n')
        out = run(self.board, 'fwupdate.py', self.hex)
        self.assertNotEqual(out.returncode, 0)
        self.assertIn('not a HEX record', out.stderr)
        self.assertEqual(self.board.files, {})


if __name__ == '__main__':
    unittest.main()
