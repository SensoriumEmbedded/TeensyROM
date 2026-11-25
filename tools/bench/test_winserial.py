# SPDX-License-Identifier: MIT
"""Tests for the Windows serial port and COM port discovery, against a fake kernel32 and
a fake registry, so they run on any platform. No hardware needed:

  python3 -m unittest discover -s tools/bench

The fakes answer the way Windows documents the real calls answering. They prove what
winserial.py and trlink.py do with those answers; they cannot prove what usbser.sys does.
"""
import ctypes
import io
import os
import time
import unittest
from unittest import mock

import trlink
import winserial
from protocol import FW_CHECK, FW_FULL, board_reply, to_board
from winserial import ERROR_SEM_TIMEOUT, INVALID_HANDLE, WindowsPort

HANDLE = 0x1F4
ERROR_INVALID_FUNCTION, ERROR_ACCESS_DENIED, ERROR_BAD_COMMAND = 1, 5, 22
ERROR_INVALID_PARAMETER = 87
GONE = None    # in FakeKernel32.arriving: the board drops off USB

# A bench as the registry describes it. Windows keeps a device's PortName after it has
# gone; only SERIALCOMM says which ports are there now.
BENCH = {
    # Current firmware: the serial interface has the port and the MIDI one has none.
    # COM3 is a board that was plugged in once.
    'VID_16C0&PID_0489&MI_00': {'6&1a2b&0&0000': 'COM7', '6&3c4d&0&0000': 'COM3'},
    'VID_16C0&PID_0489&MI_02': {'6&1a2b&0&0002': None},
    # The minimal image on firmware from before it became Serial + MIDI.
    'VID_16C0&PID_0483': {'12345670': 'COM12'},
    # The Teensy bootloader, which has no port.
    'VID_16C0&PID_0478': {'0018CD6D': None},
    # Somebody else's serial device.
    'VID_2341&PID_0043': {'85736323': 'COM4'},
}
PRESENT = {r'\Device\USBSER000': 'COM7', r'\Device\USBSER001': 'COM12',
           r'\Device\USBSER002': 'COM4', r'\Device\Serial0': 'COM1'}


class FakeKernel32:
    """A COM port with a board on the other end. `queued` is what the board has
    already sent. `arriving` is what it sends while a read waits, one item per wait:
    bytes, or GONE for the port going away, after which every call on the handle fails
    as a removed USB device's do. `answer` is the board's reply to what is written."""

    def __init__(self):
        self.error = 0
        self.open_error = 0
        self.not_serial = False
        self.refuses_settings = False
        self.opened = []
        self.state = None
        self.timeouts = []
        self.queued = bytearray()
        self.arriving = []
        self.written = bytearray()
        self.takes = None
        self.answer = lambda data: b''
        self.gone = False
        self.closed = []

    def last_error(self):
        return self.error

    def fail(self, code):
        self.error = code
        return 0

    def CreateFileW(self, path, access, share, security, disposition, flags, template):
        self.opened.append(path)
        if self.open_error:
            self.error = self.open_error
            return INVALID_HANDLE
        return HANDLE

    def GetCommState(self, handle, dcb):
        if self.not_serial:
            return self.fail(ERROR_INVALID_FUNCTION)
        # What another program might have left the port in.
        old = dcb.contents
        old.BaudRate, old.ByteSize, old.Parity, old.StopBits = 9600, 7, 2, 2
        old.fOutxCtsFlow = old.fOutxDsrFlow = old.fOutX = old.fInX = 1
        old.fAbortOnError = old.fNull = old.fParity = 1
        old.fDtrControl = old.fRtsControl = 2
        return 1

    def SetCommState(self, handle, dcb):
        if self.refuses_settings:
            return self.fail(ERROR_INVALID_PARAMETER)
        self.state = winserial.DCB.from_buffer_copy(dcb.contents)
        return 1

    def SetCommTimeouts(self, handle, timeouts):
        t = timeouts.contents
        self.timeouts.append((t.ReadIntervalTimeout, t.ReadTotalTimeoutMultiplier,
                              t.ReadTotalTimeoutConstant, t.WriteTotalTimeoutMultiplier,
                              t.WriteTotalTimeoutConstant))
        return 1

    def ClearCommError(self, handle, errors, status):
        if self.gone:
            return self.fail(ERROR_BAD_COMMAND)
        status.contents.cbInQue = len(self.queued)
        return 1

    def ReadFile(self, handle, buf, n, got, overlapped):
        if not self.queued and self.arriving:
            item = self.arriving.pop(0)
            if item is GONE:
                self.gone = True
            else:
                self.queued += item
        if self.gone:
            return self.fail(ERROR_BAD_COMMAND)
        data = bytes(self.queued[:n])
        del self.queued[:n]
        ctypes.memmove(buf, data, len(data))
        got.contents.value = len(data)
        if not data:
            time.sleep(0.001)    # the timeout, shortened
        return 1

    def WriteFile(self, handle, data, n, took, overlapped):
        if self.gone:
            return self.fail(ERROR_BAD_COMMAND)
        if self.takes == 0:
            return self.fail(ERROR_SEM_TIMEOUT)    # what usbser.sys does at the timeout
        k = n if self.takes is None else min(n, self.takes)
        self.written += data[:k]
        self.queued += self.answer(data[:k])
        took.contents.value = k
        return 1

    def CloseHandle(self, handle):
        self.closed.append(handle)
        return 1


class FakeKey:
    def __init__(self, values=None, subkeys=None):
        self.values = dict(values or {})
        self.subkeys = dict(subkeys or {})

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


class FakeRegistry:
    """The two keys winserial.py reads. `present` is SERIALCOMM's values, None for no
    key at all, which is what a machine with no serial port has; `devices` maps a key
    under Enum\\USB to its instances and each one's PortName."""
    HKEY_LOCAL_MACHINE = 'HKLM'

    def __init__(self, present, devices=BENCH):
        self.root = FakeKey()
        self.put(winserial.ENUM_USB, FakeKey(subkeys={
            device: FakeKey(subkeys={
                instance: FakeKey(subkeys={'Device Parameters': FakeKey(
                    values={} if port is None else {'PortName': port})})
                for instance, port in instances.items()})
            for device, instances in devices.items()}))
        if present is not None:
            self.put(winserial.SERIALCOMM, FakeKey(values=present))

    def put(self, path, key):
        *parents, name = path.split('\\')
        node = self.root
        for part in parents:
            node = node.subkeys.setdefault(part, FakeKey())
        node.subkeys[name] = key

    def OpenKey(self, key, path):
        node = self.root if key == self.HKEY_LOCAL_MACHINE else key
        for part in path.split('\\'):
            found = [k for name, k in node.subkeys.items() if name.lower() == part.lower()]
            if not found:
                raise FileNotFoundError(path)
            node = found[0]
        return node

    # Past the last one, winreg raises OSError (ERROR_NO_MORE_ITEMS).
    def EnumKey(self, key, i):
        names = list(key.subkeys)
        if i >= len(names):
            raise OSError(259, 'No more data is available')
        return names[i]

    def EnumValue(self, key, i):
        values = list(key.values.items())
        if i >= len(values):
            raise OSError(259, 'No more data is available')
        name, data = values[i]
        return name, data, 1

    def QueryValueEx(self, key, name):
        if name not in key.values:
            raise FileNotFoundError(name)
        return key.values[name], 1


class Discovery(unittest.TestCase):
    def test_only_a_teensyrom_that_is_there_is_found_current_firmware_first(self):
        self.assertEqual(winserial.ports(FakeRegistry(PRESENT)), ['COM7', 'COM12'])

    def test_ports_sort_by_number_and_not_by_name(self):
        reg = FakeRegistry(
            {'a': 'COM10', 'b': 'COM9', 'c': 'COM2'},
            {'VID_16C0&PID_0489&MI_00': {'x': 'COM10', 'y': 'COM9'},
             'VID_16C0&PID_0483': {'z': 'COM2'}})
        self.assertEqual(winserial.ports(reg), ['COM9', 'COM10', 'COM2'])

    def test_a_machine_with_no_serial_port_at_all_has_none_to_find(self):
        reg = FakeRegistry(None)
        self.assertEqual(winserial.ports(reg), [])
        self.assertFalse(winserial.present('COM7', reg))

    def test_a_board_that_drops_off_during_the_walk_is_left_out_without_an_error(self):
        reg = FakeRegistry(PRESENT)
        read = reg.EnumValue

        def com12_reboots_after_the_first_value(key, i):
            value = read(key, i)
            key.values.pop(r'\Device\USBSER001', None)
            return value
        reg.EnumValue = com12_reboots_after_the_first_value
        self.assertEqual(winserial.ports(reg), ['COM7'])

    def test_any_spelling_of_a_port_is_the_same_port(self):
        reg = FakeRegistry(PRESENT)
        self.assertTrue(winserial.present('com7', reg))
        self.assertTrue(winserial.present('\\\\.\\COM7', reg))
        self.assertFalse(winserial.present('COM3', reg))


class Port(unittest.TestCase):
    def setUp(self):
        self.k = FakeKernel32()

    def open(self, name='COM7'):
        port = WindowsPort(name, self.k, self.k.last_error)
        self.addCleanup(port.close)
        return port

    def test_it_opens_115200_8n1_with_dtr_and_rts_on_and_no_flow_control(self):
        self.open('COM12')
        # COM10 and up open only through \\.\, so every port goes that way.
        self.assertEqual(self.k.opened, ['\\\\.\\COM12'])
        s = self.k.state
        self.assertEqual((s.BaudRate, s.ByteSize, s.Parity, s.StopBits), (115200, 8, 0, 0))
        self.assertEqual((s.fBinary, s.fParity, s.fOutxCtsFlow, s.fOutxDsrFlow,
                          s.fOutX, s.fInX, s.fNull, s.fAbortOnError), (1, 0, 0, 0, 0, 0, 0, 0))
        self.assertEqual((s.fDtrControl, s.fRtsControl), (1, 1))

    def test_a_write_waits_as_long_as_it_is_told(self):
        port = self.open()
        port.write(b'x', 2.5)
        self.assertEqual(self.k.timeouts[-1][3:], (0, 2500))
        # A write timeout of 0 would wait for ever on a board that has stopped reading.
        port.write(b'x', 0)
        self.assertEqual(self.k.timeouts[-1][3:], (0, 1))

    def test_a_write_the_board_does_not_take_in_time_is_a_timeout_not_a_drop(self):
        port = self.open()
        self.k.takes = 0
        with self.assertRaisesRegex(TimeoutError, 'COM7: the board did not take a write within 2 s'):
            port.write(b'x', 2)

    def test_a_port_another_program_has_open_says_so(self):
        self.k.open_error = ERROR_ACCESS_DENIED
        with self.assertRaisesRegex(OSError, 'COM7 is open in another program'):
            self.open()
        self.assertEqual(self.k.closed, [])

    def test_a_handle_that_is_not_a_serial_port_is_refused_and_closed(self):
        self.k.not_serial = True
        with self.assertRaisesRegex(OSError, 'COM7 is not a serial port'):
            self.open()
        self.assertEqual(self.k.closed, [HANDLE])

    def test_a_port_that_refuses_the_settings_says_so_and_is_closed(self):
        self.k.refuses_settings = True
        with self.assertRaisesRegex(OSError, r'COM7 refused the port settings \(Windows error 87\)'):
            self.open()
        self.assertEqual(self.k.closed, [HANDLE])

    def test_what_has_arrived_is_read_without_waiting(self):
        port = self.open()
        self.k.queued += b'hello'
        before = list(self.k.timeouts)
        self.assertEqual(port.read(4096, 0.4), b'hello')
        self.assertEqual(self.k.timeouts, before)

    def test_a_read_waits_for_the_first_byte_then_takes_what_came_with_it(self):
        port = self.open()
        self.k.arriving = [b'abc']
        self.assertEqual(port.read(4096, 0.25), b'abc')
        self.assertEqual(self.k.timeouts[-1][:3], (0, 0, 250))

    def test_a_read_that_nothing_answers_is_empty(self):
        self.assertEqual(self.open().read(4096, 0.05), b'')

    def test_no_wait_is_still_a_wait_and_not_for_ever(self):
        # All three read timeouts 0 means ReadFile never times out.
        self.open().read(1, 0)
        self.assertEqual(self.k.timeouts[-1][:3], (0, 0, 1))

    def test_a_port_that_went_away_raises_on_every_call(self):
        port = self.open()
        self.k.gone = True
        with self.assertRaisesRegex(OSError, 'COM7 went away'):
            port.read(1, 0.1)
        with self.assertRaisesRegex(OSError, 'COM7 went away'):
            port.write(b'x', 1)

    def test_a_write_returns_what_the_port_took(self):
        port = self.open()
        self.k.takes = 3
        self.assertEqual(port.write(memoryview(b'abcdef'), 1), 3)
        self.assertEqual(self.k.written, b'abc')

    def test_closing_twice_closes_the_handle_once(self):
        port = self.open()
        port.close()
        port.close()
        self.assertEqual(self.k.closed, [HANDLE])


class LinkOnWindows(unittest.TestCase):
    """trlink.Link over WindowsPort, and trlink's port choice from the registry."""

    def setUp(self):
        self.k = FakeKernel32()
        self.reg = FakeRegistry(PRESENT)
        for patch in (mock.patch.object(trlink, 'WINDOWS', True),
                      mock.patch.object(winserial, '_api',
                                        lambda: (self.k, self.k.last_error)),
                      mock.patch.object(winserial, '_winreg', lambda: self.reg),
                      mock.patch.dict(os.environ)):
            patch.start()
            self.addCleanup(patch.stop)
        os.environ.pop('TR_PORT', None)

    def link(self):
        link = trlink.Link(settle=0)
        self.addCleanup(link.close)
        return link

    def test_the_first_teensyrom_is_opened_and_answers_a_firmware_check(self):
        self.k.answer = lambda data: board_reply(FW_FULL) if data == to_board(FW_CHECK) else b''
        link = self.link()
        self.assertEqual(link.port, 'COM7')
        self.assertEqual(link.fwcheck(), 'main')

    def test_tr_port_is_taken_in_any_case(self):
        os.environ['TR_PORT'] = 'com12'
        self.assertEqual(self.link().port, 'COM12')
        self.assertEqual(self.k.opened, ['\\\\.\\COM12'])

    def test_no_teensyrom_names_what_was_looked_for(self):
        self.reg = FakeRegistry(None)
        with self.assertRaisesRegex(SystemExit, 'COM ports of USB devices 16C0:0489'):
            trlink.find_port()

    def test_stream_echoes_until_the_port_drops_and_says_it_did(self):
        link, out = self.link(), io.StringIO()
        self.k.arriving = [b'Erasing...\r\n', GONE]
        self.assertTrue(link.stream(5, out))
        self.assertEqual(out.getvalue(), 'Erasing...\r\n')

    def test_raw_keeps_what_arrived_before_the_drop(self):
        link = self.link()
        self.k.arriving = [b'Reset cmd received\n', GONE]
        self.assertEqual(link.raw(), b'Reset cmd received\n')

    def test_a_write_the_board_never_takes_gives_up(self):
        link = self.link()
        self.k.takes = 0
        with self.assertRaisesRegex(SystemExit, 'stopped reading after 0 of 10 bytes'):
            link.wr(b'x' * 10, stall=0.2)
        # The port did the waiting, for the stall wr() was given.
        self.assertEqual(self.k.timeouts[-1][4], 200)

    def test_neighbours_are_the_other_teensyroms(self):
        self.assertEqual(trlink.neighbours('COM7'), {'COM12'})

    def test_after_a_reboot_the_old_port_wins_while_it_is_there(self):
        self.assertEqual(trlink.candidates('COM7', {'COM12'}), ['COM7'])

    def test_after_a_reboot_a_new_port_stands_in_for_one_that_did_not_come_back(self):
        self.reg = FakeRegistry({'a': 'COM9', 'b': 'COM12'},
                                {'VID_16C0&PID_0489&MI_00': {'x': 'COM9', 'y': 'COM12'}})
        self.assertEqual(trlink.candidates('COM7', {'COM12'}), ['COM9'])


class Output(unittest.TestCase):
    """What the board prints is decoded as latin1, and a redirected stdout on Windows is
    cp1252, which has no characters for 0x80-0x9F."""

    def stdout(self):
        return io.TextIOWrapper(io.BytesIO(), encoding='cp1252', errors='strict')

    def test_a_byte_cp1252_cannot_show_would_end_the_script(self):
        with self.assertRaises(UnicodeEncodeError):
            trlink.echo(b'A\x81', self.stdout())

    def test_so_it_is_written_as_an_escape(self):
        out = self.stdout()
        trlink.backslash_unencodable(out)
        trlink.echo(b'A\x81\xe9', out)
        self.assertEqual(out.buffer.getvalue(), b'A\\x81\xe9')


if __name__ == '__main__':
    unittest.main()
