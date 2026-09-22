#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Serial link to a TeensyROM, for driving a cartridge from the bench.

The board answers a small set of binary commands over USB serial; protocol.py
holds every value that goes on the wire, and docs/ControlComms.md describes
them all.

The main image answers everything here. The minimal image answers reset,
launch, version and the firmware check, and fails other 0x64 commands with
"Busy!"; it is silent to anything else. The extension image runs with USB
disabled and answers nothing at all. Reading and writing C64 memory (peek/poke)
additionally needs a Fab 0.4 board (Fab04_FullDMACapable).

The port is $TR_PORT, else the first /dev/cu.usbmodem*. Do not hardcode it: the
main image renames its USB device (MidiDevName_AppendUniqueID), so its node
differs from the one minimal and the extension image enumerate as.

macOS and Linux only (termios); no third-party packages.
"""
import glob
import os
import select
import sys
import termios
import time

from c64 import KEYBUF, KEYCOUNT, SCREEN_BYTES, SCREEN_RAM
from protocol import (ACK, DELETE_FILE, DRIVE_SD, FAIL, LAUNCH_FILE,
                      POST_FILE, READ_C64_MEM, WRITE_C64_MEM, from_board,
                      to_board)

BAUD = termios.B115200


def ports():
    return sorted(glob.glob('/dev/cu.usbmodem*'))


def find_port():
    port = os.environ.get('TR_PORT') or next(iter(ports()), None)
    if not port:
        raise SystemExit('no TeensyROM serial port found (/dev/cu.usbmodem*); '
                         'set TR_PORT, and note the extension image has no USB')
    return port


class Link:
    def __init__(self, port=None, settle=0.4):
        self.port = port or find_port()
        self.fd = os.open(self.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        a = termios.tcgetattr(self.fd)
        a[0] = a[1] = a[3] = 0
        a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        a[4] = a[5] = BAUD
        a[6][termios.VMIN] = 0
        a[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        time.sleep(settle)

    def close(self):
        try:
            os.close(self.fd)
        except OSError:
            pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # -- raw I/O ------------------------------------------------------------
    def rd(self, n, timeout=10):
        buf, end = b'', time.time() + timeout
        while len(buf) < n and time.time() < end:
            if select.select([self.fd], [], [], 0.2)[0]:
                try:
                    buf += os.read(self.fd, n - len(buf))
                except BlockingIOError:
                    pass
        return buf

    def wr(self, data):
        view, off = memoryview(data), 0
        while off < len(view):
            try:
                off += os.write(self.fd, view[off:off + 2048])
            except BlockingIOError:
                time.sleep(0.002)

    def drain(self, secs=0.4):
        end = time.time() + secs
        while time.time() < end:
            if select.select([self.fd], [], [], 0.1)[0]:
                try:
                    os.read(self.fd, 4096)
                except BlockingIOError:
                    pass

    def stream(self, secs, out=sys.stdout):
        """Echo whatever the board prints. Returns True if the port dropped,
        which is what a reboot, or the jump into the extension image, looks like."""
        end = time.time() + secs
        while time.time() < end:
            try:
                if select.select([self.fd], [], [], 0.4)[0]:
                    chunk = os.read(self.fd, 4096)
                    if chunk:
                        out.write(chunk.decode('latin1', 'replace'))
                        out.flush()
            except OSError:
                return True
        return False

    # -- replies ------------------------------------------------------------
    def status(self, timeout=10):
        """Reads a 16-bit reply: (value, text). A failure carries its message."""
        reply = self.rd(2, timeout)
        if len(reply) < 2:
            return None, ''
        value = from_board(reply)
        return value, (self.rd(200, 1.0).decode('latin1', 'replace').strip()
                       if value == FAIL else '')

    def ack(self, what, timeout=10):
        value, text = self.status(timeout)
        if value is None:
            raise SystemExit(f'{what}: no reply')
        if value == FAIL:
            raise SystemExit(f'{what}: FAIL - {text}')
        if value != ACK:
            raise SystemExit(f'{what}: unexpected 0x{value:04X}')

    # -- C64 memory (DMA) ---------------------------------------------------
    def peek(self, addr, length):
        self.drain(0.3)
        self.wr(to_board(READ_C64_MEM) + to_board(addr) + to_board(length))
        value, text = self.status(6)
        if value is None:
            raise SystemExit('no reply -- DMA read is not compiled into this image, '
                             'or this is not the main image')
        if value != ACK:
            raise SystemExit(f'read of ${addr:04X} refused: 0x{value:04X} {text}')
        data = self.rd(length, 10)
        if len(data) != length:
            raise SystemExit(f'read of ${addr:04X}: got {len(data)} of {length} bytes')
        return data

    def poke(self, addr, data):
        self.drain(0.3)
        data = bytes(data)
        self.wr(to_board(WRITE_C64_MEM) + to_board(addr) + to_board(len(data)) + data)
        value, _ = self.status(6)
        return value == ACK

    def screen(self):
        return self.peek(SCREEN_RAM, SCREEN_BYTES)

    def key(self, code):
        """Puts one key in the C64 keyboard buffer, so the next GETIN returns it."""
        return self.poke(KEYBUF, [code]) and self.poke(KEYCOUNT, [1])

    # -- SD card and launching ----------------------------------------------
    def delete(self, remote):
        # GetFileStream refuses to overwrite, so a post starts with a delete.
        self.drain(0.6)
        self.wr(to_board(DELETE_FILE))
        if len(self.rd(2)) < 2:
            return
        self.wr(bytes([DRIVE_SD]) + remote.encode() + b'\0')
        self.rd(2, 5)
        self.drain(0.6)

    def post(self, local, remote, log=print):
        data = open(local, 'rb').read()
        self.delete(remote)
        started = time.time()
        self.wr(to_board(POST_FILE))
        self.ack('post token')
        self.wr(to_board(len(data), 4) + to_board(sum(data) & 0xffff)
                + bytes([DRIVE_SD]) + remote.encode() + b'\0')
        self.ack('post header')
        self.wr(data)
        self.ack('post data', 180)
        log(f'pushed {remote} ({len(data)} bytes, {time.time() - started:.1f}s)')

    def launch(self, path, drive=DRIVE_SD):
        self.drain(0.6)
        self.wr(to_board(LAUNCH_FILE))
        self.ack('launch token', 5)
        self.wr(bytes([drive]) + path.encode() + b'\0')
        self.ack('launch name', 5)


def reconnect(timeout=60, interval=0.05, port=None):
    """Waits for a board to enumerate and returns a Link, or None. Use it after
    a reboot: the port vanishes and comes back, sometimes under a new name."""
    end = time.time() + timeout
    while time.time() < end:
        try:
            return Link(port, settle=0.4)
        except (OSError, SystemExit):
            time.sleep(interval)
    return None
