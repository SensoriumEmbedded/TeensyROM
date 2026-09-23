#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Serial link to a TeensyROM, for driving a cartridge from the bench.

The board answers a small set of binary commands over USB serial; protocol.py
holds every value that goes on the wire, and docs/ControlComms.md describes
them all.

The main image answers everything here. The minimal image answers reset,
launch, version and the firmware check, and fails other 0x64 commands with
"Busy!"; it is silent to anything else. The extension image runs with USB
disabled and answers nothing at all. `Link.fwcheck()` is how you tell the three
apart. Reading and writing C64 memory (peek/poke) additionally needs a Fab 0.4
board (Fab04_FullDMACapable).

The port is $TR_PORT, else the first matching device found: cu.usbmodem* on
macOS, ttyACM* on Linux, or -- Windows, which has neither -- any enumerated
port reporting PJRC's USB vendor ID (0x16C0). Do not hardcode it: on macOS the
main image renames its USB device (MidiDevName_AppendUniqueID), so its node
differs from the one minimal and the extension image enumerate as.

macOS and Linux: termios, no third-party packages. Windows: pyserial
(`pip install pyserial`) -- termios does not exist there, and a Windows COM
port needs different handling (see Link's Windows branch and alive() below).
"""
import glob
import json
import os
import sys
import time

if sys.platform == 'win32':
    import serial
    import serial.tools.list_ports
else:
    import select
    import termios

from c64 import KEYBUF, KEYCOUNT, SCREEN_BYTES, SCREEN_RAM
from protocol import (ACK, DELETE_FILE, DIR_END, DIR_START, DRIVE_NAMES,
                      DRIVE_SD, FAIL, FW_CHECK, GET_DIR_NDJSON, IMAGES,
                      LAUNCH_FILE, POST_FILE, READ_C64_MEM, RESET_C64,
                      VERSION_INFO, WRITE_C64_MEM, board_reply, from_board,
                      to_board)

BAUD = 115200 if sys.platform == 'win32' else termios.B115200
LISTING_PAGE_SIZE = 1000
PORT_DIR, PORT_GLOBS = '/dev', ('cu.usbmodem*', 'ttyACM*')
TEENSY_VID = 0x16C0  # PJRC's registered USB vendor ID, every Teensy image
REPLY_BYTES = 2
READ_TICK = 0.2
ASK_AGAIN_AFTER = 2.0
WRITE_STALL_LIMIT = 30.0


def ports(beside=None):
    """Every node that looks like a TeensyROM, in /dev or in the directory that
    `beside` names. A Teensy is cu.usbmodem* on macOS and ttyACM* on Linux. On
    Windows, where neither /dev nor those names exist, every enumerated port
    reporting PJRC's vendor ID is a candidate instead; `beside` has no meaning
    there, since COM ports carry no directory to sample."""
    if sys.platform == 'win32':
        return sorted(p.device for p in serial.tools.list_ports.comports()
                      if p.vid == TEENSY_VID)
    folder = os.path.dirname(beside) if beside else PORT_DIR
    return sorted(node for pattern in PORT_GLOBS
                  for node in glob.glob(os.path.join(folder, pattern)))


def neighbours(port):
    """The nodes beside `port`. Sample this while the board is still on `port`:
    once it has gone, a node it came back under looks like one that was there
    all along."""
    return set(ports(port)) - {port}


def find_port():
    port = os.environ.get('TR_PORT') or next(iter(ports()), None)
    if not port:
        looked = ' or '.join(f'{PORT_DIR}/{pattern}' for pattern in PORT_GLOBS)
        raise SystemExit(f'no TeensyROM serial port found ({looked}); '
                         'set TR_PORT, and note the extension image has no USB')
    return port


def configure(fd):
    """POSIX only. Put a serial fd in raw 115200 8N1 with non-blocking reads."""
    a = termios.tcgetattr(fd)
    a[0] = a[1] = a[3] = 0
    a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    a[4] = a[5] = BAUD
    a[6][termios.VMIN] = 0
    a[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a)


def echo(data, out):
    """Write what the board printed, and say how many bytes that was."""
    if data:
        out.write(data.decode('latin1', 'replace'))
        out.flush()
    return len(data)


def drive_number(text):
    """The drive a command-line argument names."""
    if text.isdigit() and int(text) in DRIVE_NAMES:
        return int(text)
    named = ', '.join(f'{n} the {name}' for n, name in DRIVE_NAMES.items())
    raise SystemExit(f'drive {text!r}: use {named}')


def image_in(seen):
    """The image named by a firmware-check reply at the end of `seen`."""
    for token, name in IMAGES.items():
        if seen.endswith(board_reply(token)):
            return name
    return None


class Link:
    def __init__(self, port=None, settle=0.4):
        """Raises OSError when `port` is not a serial port this can talk to."""
        self.port = port or find_port()
        if sys.platform == 'win32':
            self.ser = None
            try:
                # A small positive timeout on both directions, not 0: pyserial
                # still raises on a genuine disconnect regardless of the
                # timeout value, so this loses nothing there, but strictly
                # non-blocking (timeout=0) reads interleaved with writes on
                # the same handle are flaky on Windows -- a write that just
                # succeeded can have the very next one fail outright
                # (WriteFile/ERROR_BAD_COMMAND) once a non-blocking read has
                # happened in between.
                self.ser = serial.Serial(self.port, baudrate=BAUD, timeout=0.05, write_timeout=2)
            except serial.SerialException as problem:
                raise OSError(f'{self.port} is not a serial port: {problem}') from problem
        else:
            self.fd = os.open(self.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            try:
                configure(self.fd)
            except termios.error as problem:
                self.close()
                raise OSError(f'{self.port} is not a serial port: {problem}') from problem
        time.sleep(settle)

    def close(self):
        if sys.platform == 'win32':
            if self.ser is not None:
                try:
                    self.ser.close()
                except OSError:
                    pass
        else:
            try:
                os.close(self.fd)
            except OSError:
                pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def rd(self, n, timeout=10):
        """Raises OSError (serial.SerialException on Windows) if the port dies
        outright mid-read, rather than block or a benign 0-byte timeout --
        callers that watch for a reboot's second, later drop (e.g. fwupdate.py's
        answering_board()) catch it and reconnect() again."""
        buf, end = b'', time.time() + timeout
        while len(buf) < n and time.time() < end:
            if sys.platform == 'win32':
                chunk = self.ser.read(n - len(buf))
                if chunk:
                    buf += chunk
            elif select.select([self.fd], [], [], 0.2)[0]:
                try:
                    buf += os.read(self.fd, n - len(buf))
                except BlockingIOError:
                    pass
        return buf

    def wr(self, data, stall=WRITE_STALL_LIMIT):
        """Raises SystemExit when the board has taken nothing for `stall`
        seconds. On Windows, a harder failure (SerialException that is not
        just a write timing out) is not swallowed into that retry: it
        propagates as the OSError it is, the same signal rd() gives, for a
        caller to catch and reconnect() again rather than hammer a handle
        that is not coming back (e.g. a port mid-way through a second, later
        re-enumeration)."""
        view, off, deadline = memoryview(data), 0, time.time() + stall
        while off < len(view):
            if sys.platform == 'win32':
                try:
                    n = self.ser.write(view[off:off + 2048])
                except serial.SerialTimeoutException:
                    n = 0
                if n:
                    off += n
                    deadline = time.time() + stall
                elif time.time() >= deadline:
                    raise SystemExit(f'{self.port}: board stopped reading after '
                                     f'{off} of {len(view)} bytes')
                else:
                    time.sleep(0.05)
            else:
                try:
                    off += os.write(self.fd, view[off:off + 2048])
                    deadline = time.time() + stall
                except BlockingIOError:
                    if time.time() >= deadline:
                        raise SystemExit(f'{self.port}: board stopped reading after '
                                         f'{off} of {len(view)} bytes')
                    time.sleep(0.002)

    def drain(self, secs=0.4):
        end = time.time() + secs
        while time.time() < end:
            if sys.platform == 'win32':
                self.ser.read(4096)
            elif select.select([self.fd], [], [], 0.1)[0]:
                try:
                    os.read(self.fd, 4096)
                except BlockingIOError:
                    pass

    def stream(self, secs, out=sys.stdout):
        """Echo whatever the board prints. Returns True if the port dropped,
        which is what a reboot, or the jump into the extension image, looks like."""
        end = time.time() + secs
        while time.time() < end:
            if sys.platform == 'win32':
                try:
                    chunk = self.ser.read(4096)
                except OSError:
                    return True
                if chunk:
                    echo(chunk, out)
                continue
            try:
                if select.select([self.fd], [], [], 0.4)[0]:
                    chunk = os.read(self.fd, 4096)
                    if not chunk:
                        return True
                    echo(chunk, out)
            except BlockingIOError:
                continue
            except OSError:
                return True
        return False

    def raw(self, idle=0.3, timeout=5):
        """Whatever the board sends next: waits `timeout` for the first byte,
        then until the port has been quiet for `idle` seconds. Stops early if
        the port drops, which is what a reset looks like from here."""
        out, deadline = b'', time.time() + timeout
        while time.time() < deadline:
            if sys.platform == 'win32':
                try:
                    chunk = self.ser.read(4096)
                except OSError:
                    break
                if chunk:
                    out, deadline = out + chunk, time.time() + idle
                continue
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    chunk = os.read(self.fd, 4096)
                except BlockingIOError:
                    continue
                except OSError:
                    break
                if not chunk:
                    break
                out, deadline = out + chunk, time.time() + idle
        return out

    def text(self, idle=0.3, timeout=5):
        return self.raw(idle, timeout).decode('latin1', 'replace')

    def status(self, timeout=10):
        """Reads a 16-bit reply: (value, text). A failure carries its message."""
        reply = self.rd(REPLY_BYTES, timeout)
        if len(reply) < REPLY_BYTES:
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

    def fwcheck(self, timeout=3):
        """'main', 'minimal', or None when nothing answered -- which is what the
        extension image looks like, since it runs with USB disabled. Reads past
        anything the board says unprompted, such as the line the main image
        prints as it loads an IO handler."""
        self.drain(0.3)
        self.wr(to_board(FW_CHECK))
        seen, image, end = b'', None, time.time() + timeout
        while time.time() < end and not image:
            seen += self.rd(1, READ_TICK)
            image = image_in(seen)
        if seen and not image:
            raise SystemExit(f'firmware check answered {seen!r}: not a TeensyROM')
        return image

    def await_image(self, deadline, out=sys.stdout):
        """The image name once the board answers a firmware check, echoing the
        boot output it prints meanwhile, or None if nothing answers by
        `deadline`. fwcheck() gives up on the first thing it does not
        recognise; a board still coming up has to be asked again."""
        seen, shown, asked = b'', 0, 0.0
        while time.time() < deadline:
            if time.time() - asked >= ASK_AGAIN_AFTER:
                self.wr(to_board(FW_CHECK))
                asked = time.time()
            seen += self.rd(1, READ_TICK)
            shown += echo(seen[shown:len(seen) - REPLY_BYTES], out)
            image = image_in(seen)
            if image:
                return image
        echo(seen[shown:], out)
        return None

    def version(self, timeout=5):
        """The build banner. Both the main and the minimal image answer this."""
        self.drain(0.3)
        self.wr(to_board(VERSION_INFO))
        self.ack('version', timeout)
        return self.text(timeout=timeout).strip()

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
        try:
            with open(local, 'rb') as source:
                data = source.read()
        except OSError as problem:
            raise SystemExit(f'cannot read {local}: {problem.strerror}')
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

    def listdir(self, path='/', drive=DRIVE_SD, skip=0, take=LISTING_PAGE_SIZE):
        """One dict per entry, as the board's NDJSON listing gives them:
        {'type': 'dir', 'name': ...} or {'type': 'file', 'name': ..., 'size': ...}."""
        self.drain(0.6)
        self.wr(to_board(GET_DIR_NDJSON))
        self.ack('dir token', 5)
        self.wr(bytes([drive]) + to_board(skip) + to_board(take) + path.encode() + b'\0')
        self.ack(f'dir {path}', 10)
        start = self.rd(2, 10)
        if len(start) < 2 or from_board(start) != DIR_START:
            raise SystemExit(f'listing of {path} did not start: {start!r}')
        listing, marker, _ = self.raw(idle=0.5, timeout=30).partition(board_reply(DIR_END))
        if not marker:
            raise SystemExit(f'listing of {path} stopped before its end marker; '
                             'the board went quiet part way through')
        return [json.loads(line) for line in listing.split(b'\r\n') if line.strip()]

    def reset(self):
        """Reset the C64 back to the menu. From the minimal image this also
        returns the board to the main one, so the port drops and comes back
        under a different name. Answers with a line, not an Ack."""
        self.drain(0.4)
        self.wr(to_board(RESET_C64))
        return self.text().strip()

    def launch(self, path, drive=DRIVE_SD):
        self.drain(0.6)
        self.wr(to_board(LAUNCH_FILE))
        self.ack('launch token', 5)
        self.wr(bytes([drive]) + path.encode() + b'\0')
        self.ack('launch name', 5)

    def alive(self, timeout=1.5):
        """A quick, bounded check that this Link can actually talk to
        something. reconnect() calls this on every candidate rather than
        trusting a clean open alone: the original single-attempt reconnect()
        can grab a board mid-way through a second, later re-enumeration (a
        crash landing in minimal, which immediately hands off to main) just as
        easily on macOS/Linux as on Windows -- fwupdate.py's own
        answering_board() already has to retry for exactly this reason. On
        Windows specifically, a COM port that has only just re-enumerated can
        also open cleanly and then fail every write for the rest of that
        handle's life; retrying the write does not clear that, only opening a
        fresh handle does, which is the other reason this exists."""
        try:
            self.wr(to_board(FW_CHECK), stall=timeout)
            self.rd(1, 0.2)  # harmless whether or not anything answers
        except (SystemExit, OSError):
            return False
        return True


def candidates(wanted, known):
    """The ports to try after a reboot, best first: the one asked for while it
    is still there, then any node that has appeared beside it. With none asked
    for, whatever is there."""
    if not wanted:
        return ports()
    # os.path.exists(wanted) served this on macOS/Linux, where a port is a
    # /dev path; a bare "COM18" is not a filesystem path at all on Windows,
    # so ask the same place ports() does instead, uniformly.
    if wanted in ports():
        return [wanted]
    return [p for p in ports(wanted) if p not in known]


def reconnect(timeout=60, interval=0.05, port=None, known=None):
    """Waits for a board to enumerate and returns a Link, or None. Use it after
    a reboot: the port vanishes and comes back, sometimes under a new name,
    because the USB serial number changes across some firmware changes. The port
    asked for -- `port`, else $TR_PORT -- is a preference here and not a pin:
    when it does not come back, a node that has appeared beside it is taken
    instead. `known` is neighbours() from before the reboot; the default samples
    it here, which is only right while the board is still away.

    A candidate that opens but fails alive()'s smoke test is closed and
    skipped rather than returned -- see alive()'s own docstring for why."""
    wanted = port or os.environ.get('TR_PORT')
    known = neighbours(wanted) if known is None else known
    end = time.time() + timeout
    while time.time() < end:
        for candidate in candidates(wanted, known):
            try:
                link = Link(candidate)
            except OSError:
                continue
            if link.alive():
                return link
            link.close()
        time.sleep(interval)
    return None
