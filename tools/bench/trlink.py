# SPDX-License-Identifier: MIT
"""Serial link to a TeensyROM's main image, for driving a cartridge from the bench.

The main image answers a small set of binary "tokens" over USB serial. Each is
a 16-bit token, low byte second (0x64 0xFD reads C64 memory), and the board
replies 0x64CC for OK or 0x9B7F for a failure followed by a message. Integers
inside a request are big-endian.

Only the main image speaks this. Minimal and the extension image have no such
commands, and the extension image runs with USB disabled, so it is silent.
Reading and writing C64 memory (peek/poke) additionally needs a Fab 0.4 board
(Fab04_FullDMACapable).

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

ACK, FAIL = 0x64CC, 0x9B7F

TOKEN_POST   = b'\x64\xBB'   # PostFileToken: write a file to the SD card
TOKEN_DELETE = b'\x64\xCF'   # DeleteFileToken
TOKEN_LAUNCH = b'\x64\x44'   # LaunchFileToken
TOKEN_WRITE  = b'\x64\xFB'   # WriteC64MemToken
TOKEN_READ   = b'\x64\xFD'   # ReadC64MemToken

DRIVE_SD = 1
SCREEN_RAM = 0x0400          # 25 rows of 40 characters
KEYBUF, KEYCOUNT = 0x0277, 0x00C6  # KERNAL keyboard buffer and its length
PETSCII_Y = 0x59
F1, F3, F5, F7 = 0x85, 0x86, 0x87, 0x88


def ports():
    return sorted(glob.glob('/dev/cu.usbmodem*'))


def find_port():
    port = os.environ.get('TR_PORT') or next(iter(ports()), None)
    if not port:
        raise SystemExit('no TeensyROM serial port found (/dev/cu.usbmodem*); '
                         'set TR_PORT, and note the extension image has no USB')
    return port


def be(n, width):
    """The firmware's GetUInt reads the most significant byte first."""
    return bytes((n >> (8 * i)) & 0xff for i in range(width - 1, -1, -1))


def petscii_row(row):
    """Screen codes as text: letters, digits and punctuation survive, the rest is '.'."""
    out = []
    for c in row:
        c &= 0x7f
        out.append('@' if c == 0 else chr(c + 64) if 1 <= c <= 26 else chr(c) if 32 <= c <= 63 else '.')
    return ''.join(out)


def screen_rows(screen):
    return [petscii_row(screen[r * 40:(r + 1) * 40]) for r in range(25)]


class Link:
    def __init__(self, port=None, settle=0.4):
        self.port = port or find_port()
        self.fd = os.open(self.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        a = termios.tcgetattr(self.fd)
        a[0] = a[1] = a[3] = 0
        a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        a[4] = a[5] = termios.B115200
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
        b = self.rd(2, timeout)
        if len(b) < 2:
            return None, ''
        v = b[0] | (b[1] << 8)
        return v, (self.rd(200, 1.0).decode('latin1', 'replace').strip() if v == FAIL else '')

    def ack(self, what, timeout=10):
        v, text = self.status(timeout)
        if v is None:
            raise SystemExit(f'{what}: no reply')
        if v == FAIL:
            raise SystemExit(f'{what}: FAIL - {text}')
        if v != ACK:
            raise SystemExit(f'{what}: unexpected 0x{v:04X}')

    # -- C64 memory (DMA) ---------------------------------------------------
    def peek(self, addr, length):
        self.drain(0.3)
        self.wr(TOKEN_READ + bytes([addr >> 8, addr & 0xff, length >> 8, length & 0xff]))
        v, text = self.status(6)
        if v is None:
            raise SystemExit('no reply -- DMA read is not compiled into this image, '
                             'or this is not the main image')
        if v != ACK:
            raise SystemExit(f'read of ${addr:04X} refused: 0x{v:04X} {text}')
        data = self.rd(length, 10)
        if len(data) != length:
            raise SystemExit(f'read of ${addr:04X}: got {len(data)} of {length} bytes')
        return data

    def poke(self, addr, data):
        self.drain(0.3)
        data = bytes(data)
        self.wr(TOKEN_WRITE + bytes([addr >> 8, addr & 0xff, len(data) >> 8, len(data) & 0xff]) + data)
        v, _ = self.status(6)
        return v == ACK

    def screen(self):
        return self.peek(SCREEN_RAM, 1000)

    def key(self, code):
        """Puts one key in the C64 keyboard buffer, so the next GETIN returns it."""
        return self.poke(KEYBUF, [code]) and self.poke(KEYCOUNT, [1])

    # -- SD card and launching ----------------------------------------------
    def delete(self, remote):
        # GetFileStream refuses to overwrite, so a post starts with a delete.
        self.drain(0.6)
        self.wr(TOKEN_DELETE)
        if len(self.rd(2)) < 2:
            return
        self.wr(bytes([DRIVE_SD]) + remote.encode() + b'\0')
        self.rd(2, 5)
        self.drain(0.6)

    def post(self, local, remote, log=print):
        data = open(local, 'rb').read()
        self.delete(remote)
        started = time.time()
        self.wr(TOKEN_POST)
        self.ack('post token')
        self.wr(be(len(data), 4) + be(sum(data) & 0xffff, 2) + bytes([DRIVE_SD]) + remote.encode() + b'\0')
        self.ack('post header')
        self.wr(data)
        self.ack('post data', 180)
        log(f'pushed {remote} ({len(data)} bytes, {time.time() - started:.1f}s)')

    def launch(self, path, drive=DRIVE_SD):
        self.drain(0.6)
        self.wr(TOKEN_LAUNCH)
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
