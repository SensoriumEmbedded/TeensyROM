# SPDX-License-Identifier: MIT
"""Tests for the bench scripts against a fake board on a pty. No hardware needed:

  python3 -m unittest discover -s tools/bench

The fake speaks the same tokens as the main image (see trlink.py). It proves the
framing, byte order and reply handling; it cannot prove what a real board does.
"""
import os
import pty
import subprocess
import sys
import tempfile
import threading
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ACK, FAIL = b'\xcc\x64', b'\x7f\x9b'


class FakeBoard:
    def __init__(self):
        self.master, slave = pty.openpty()
        self.port = os.ttyname(slave)
        self._slave = slave  # keep it open so the master never sees a hangup
        self.mem = bytearray(65536)
        self.files = {}
        self.launched = []
        self.refuse_reads = False
        self.buf = b''
        self.stop = False
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def close(self):
        self.stop = True
        self.thread.join(2)
        os.close(self.master)
        os.close(self._slave)

    def take(self, n):
        end = time.time() + 3
        while len(self.buf) < n and time.time() < end and not self.stop:
            try:
                self.buf += os.read(self.master, 65536)
            except (BlockingIOError, OSError):
                time.sleep(0.005)
        got, self.buf = self.buf[:n], self.buf[n:]
        return got

    def cstring(self):
        out = b''
        while True:
            c = self.take(1)
            if not c or c == b'\0':
                return out.decode()
            out += c

    def send(self, data):
        os.write(self.master, data)

    def run(self):
        os.set_blocking(self.master, False)
        while not self.stop:
            tok = self.take(1)
            if tok == b'b':          # the one single-byte command: BusAnalysis
                self.send(b'bus ok\n')
                continue
            if tok != b'\x64':
                continue
            tok += self.take(1)
            if tok == b'\x64\xFD':
                a = self.take(4)
                addr, n = a[0] << 8 | a[1], a[2] << 8 | a[3]
                if self.refuse_reads:
                    self.send(FAIL + b'no DMA')
                else:
                    self.send(ACK + bytes(self.mem[addr:addr + n]))
            elif tok == b'\x64\xFB':
                a = self.take(4)
                addr, n = a[0] << 8 | a[1], a[2] << 8 | a[3]
                self.mem[addr:addr + n] = self.take(n)
                self.send(ACK)
            elif tok == b'\x64\xCF':
                self.send(ACK)
                self.take(1)
                self.files.pop(self.cstring(), None)
                self.send(ACK)
            elif tok == b'\x64\xBB':
                self.send(ACK)
                h = self.take(7)
                size, checksum = int.from_bytes(h[:4], 'big'), int.from_bytes(h[4:6], 'big')
                name = self.cstring()
                self.send(ACK)
                data = self.take(size)
                if sum(data) & 0xffff == checksum:
                    self.files[name] = data
                    self.send(ACK)
                else:
                    self.send(FAIL + b'checksum')
            elif tok == b'\x64\x44':
                self.send(ACK)
                self.take(1)
                self.launched.append(self.cstring())
                self.send(ACK)


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

    def test_probe_prints_the_reply(self):
        out = run(self.board, 'probe.py')
        self.assertIn('bus ok', out.stdout)

    def test_no_board_is_a_clear_error(self):
        env = {k: v for k, v in os.environ.items() if k != 'TR_PORT'}
        # Cannot assume no usbmodem device exists on the machine running the test.
        out = subprocess.run([sys.executable, '-c',
                              'import trlink; trlink.ports = lambda: []; trlink.find_port()'],
                             cwd=HERE, env=env, capture_output=True, text=True)
        self.assertIn('no TeensyROM serial port', out.stderr)

    def test_fwupdate_pushes_launches_and_answers_the_prompt(self):
        row = [ord(c) - 64 if c.isalpha() else ord(c) for c in 'Y/N']
        self.board.mem[0x400 + 40:0x400 + 40 + 3] = bytes(row)
        with tempfile.NamedTemporaryFile(suffix='.hex') as f:
            f.write(b':00000001FF\n')
            f.flush()
            proc = subprocess.Popen([sys.executable, os.path.join(HERE, 'fwupdate.py'), f.name],
                                    env=dict(os.environ, TR_PORT=self.board.port),
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            deadline = time.time() + 20
            while time.time() < deadline and self.board.mem[0xC6] != 1:
                time.sleep(0.1)
            proc.kill()
            proc.communicate()
        self.assertEqual(self.board.launched, ['/fwupdate.hex'])
        self.assertEqual((self.board.mem[0x277], self.board.mem[0xC6]), (0x59, 1))


if __name__ == '__main__':
    unittest.main()
