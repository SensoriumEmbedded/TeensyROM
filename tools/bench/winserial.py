# SPDX-License-Identifier: MIT
"""The serial port trlink.py uses on Windows: the COM port through kernel32, and which
COM ports belong to a TeensyROM, from the registry. Standard library only, like the rest
of tools/bench: ctypes and winreg, no pyserial.

It imports on any platform, so test_winserial.py runs on Linux against a fake kernel32
and a fake registry. Only opening a port and listing ports need Windows.
"""
import ctypes
import re

# The firmware's USB identities, from Teensyduino 1.61's cores/teensy4/usb_desc.h. Serial
# + MIDI (USB_MIDI_SERIAL) is what build-firmware.mjs builds both the main and the minimal
# image as, so a board keeps one COM port through both. Serial (USB_SERIAL) is what the
# minimal image was before that, on a COM port of its own, and what any Teensy running a
# serial sketch is, which is why it sorts second.
USB_DEVICES = ('VID_16C0&PID_0489', 'VID_16C0&PID_0483')
LOOKED = 'COM ports of USB devices 16C0:0489 and 16C0:0483'
ENUM_USB = r'SYSTEM\CurrentControlSet\Enum\USB'
SERIALCOMM = r'HARDWARE\DEVICEMAP\SERIALCOMM'

BAUD = 115200
GENERIC_READ, GENERIC_WRITE = 0x80000000, 0x40000000
OPEN_EXISTING = 3
INVALID_HANDLE = ctypes.c_void_p(-1).value
ERROR_FILE_NOT_FOUND, ERROR_ACCESS_DENIED, ERROR_SEM_TIMEOUT = 2, 5, 121
NOPARITY = ONESTOPBIT = 0
DTR_CONTROL_ENABLE = RTS_CONTROL_ENABLE = 1

DWORD, WORD, BYTE = ctypes.c_uint32, ctypes.c_uint16, ctypes.c_ubyte


class DCB(ctypes.Structure):
    _fields_ = [('DCBlength', DWORD), ('BaudRate', DWORD),
                ('fBinary', DWORD, 1), ('fParity', DWORD, 1),
                ('fOutxCtsFlow', DWORD, 1), ('fOutxDsrFlow', DWORD, 1),
                ('fDtrControl', DWORD, 2), ('fDsrSensitivity', DWORD, 1),
                ('fTXContinueOnXoff', DWORD, 1), ('fOutX', DWORD, 1), ('fInX', DWORD, 1),
                ('fErrorChar', DWORD, 1), ('fNull', DWORD, 1), ('fRtsControl', DWORD, 2),
                ('fAbortOnError', DWORD, 1), ('fDummy2', DWORD, 17),
                ('wReserved', WORD), ('XonLim', WORD), ('XoffLim', WORD),
                ('ByteSize', BYTE), ('Parity', BYTE), ('StopBits', BYTE),
                ('XonChar', ctypes.c_char), ('XoffChar', ctypes.c_char),
                ('ErrorChar', ctypes.c_char), ('EofChar', ctypes.c_char),
                ('EvtChar', ctypes.c_char), ('wReserved1', WORD)]


class COMMTIMEOUTS(ctypes.Structure):
    _fields_ = [('ReadIntervalTimeout', DWORD), ('ReadTotalTimeoutMultiplier', DWORD),
                ('ReadTotalTimeoutConstant', DWORD), ('WriteTotalTimeoutMultiplier', DWORD),
                ('WriteTotalTimeoutConstant', DWORD)]


class COMSTAT(ctypes.Structure):
    _fields_ = [('flags', DWORD), ('cbInQue', DWORD), ('cbOutQue', DWORD)]


_kernel32 = None


def _api():
    """kernel32 with the prototypes this uses, and how to read the error its last call
    set. A WinDLL of its own, so the prototypes stay out of ctypes.windll's shared one."""
    global _kernel32
    if _kernel32 is None:
        dll = ctypes.WinDLL('kernel32', use_last_error=True)
        handle, ptr = ctypes.c_void_p, ctypes.POINTER
        dll.CreateFileW.argtypes = (ctypes.c_wchar_p, DWORD, DWORD, ctypes.c_void_p,
                                    DWORD, DWORD, handle)
        dll.CreateFileW.restype = handle
        for name, args in (
                ('ReadFile', (handle, ctypes.c_void_p, DWORD, ptr(DWORD), ctypes.c_void_p)),
                ('WriteFile', (handle, ctypes.c_char_p, DWORD, ptr(DWORD), ctypes.c_void_p)),
                ('GetCommState', (handle, ptr(DCB))),
                ('SetCommState', (handle, ptr(DCB))),
                ('SetCommTimeouts', (handle, ptr(COMMTIMEOUTS))),
                ('ClearCommError', (handle, ptr(DWORD), ptr(COMSTAT))),
                ('CloseHandle', (handle,))):
            function = getattr(dll, name)
            function.argtypes, function.restype = args, ctypes.c_int
        _kernel32 = dll
    return _kernel32, ctypes.get_last_error


def _winreg():
    import winreg
    return winreg


def canonical(port):
    r"""COM5 for com5 or \\.\COM5: the spelling ports() returns."""
    return (port[4:] if port.startswith('\\\\.\\') else port).upper()


def _each(enum, key):
    """enum(key, 0), enum(key, 1) and on, until winreg says there are no more. Not up to a
    count taken first: a board rebooting adds and removes ports between the two, and
    reading past the end raises."""
    found = []
    while True:
        try:
            found.append(enum(key, len(found)))
        except OSError:
            return found


def _rank(device):
    """Where a key under Enum\\USB sorts, or None when it is not a TeensyROM. The
    interfaces of a composite device have keys of their own, with an &MI_nn suffix."""
    name = device.upper()
    for rank, wanted in enumerate(USB_DEVICES):
        if name == wanted or name.startswith(wanted + '&'):
            return rank
    return None


def _number(port):
    match = re.fullmatch(r'COM(\d+)', port)
    return int(match.group(1)) if match else 0


def present_ports(reg=None):
    """Every COM port there is right now, of any device."""
    reg = reg or _winreg()
    try:
        key = reg.OpenKey(reg.HKEY_LOCAL_MACHINE, SERIALCOMM)
    except OSError:
        return set()    # the key exists only while some serial port does
    with key:
        return {canonical(value[1]) for value in _each(reg.EnumValue, key)}


def present(port, reg=None):
    return canonical(port) in present_ports(reg)


def ports(reg=None):
    """The COM port of every TeensyROM plugged in, current firmware first, then by
    number. Windows keeps a device's PortName under Enum\\USB after the device has gone,
    so a name counts only while SERIALCOMM lists it, which it does only while present."""
    reg = reg or _winreg()
    here = present_ports(reg)
    if not here:
        return []
    found = {}
    try:
        usb = reg.OpenKey(reg.HKEY_LOCAL_MACHINE, ENUM_USB)
    except OSError:
        return []
    with usb:
        for device in _each(reg.EnumKey, usb):
            rank = _rank(device)
            if rank is None:
                continue
            try:
                with reg.OpenKey(usb, device) as instances:
                    for instance in _each(reg.EnumKey, instances):
                        try:
                            with reg.OpenKey(instances, instance + r'\Device Parameters') as params:
                                port = canonical(reg.QueryValueEx(params, 'PortName')[0])
                        except OSError:
                            continue    # the MIDI interface: no port
                        if port in here:
                            found[port] = min(rank, found.get(port, rank))
            except OSError:
                continue
    return sorted(found, key=lambda port: (found[port], _number(port), port))


class WindowsPort:
    """A COM port, opened as trlink.py opens a tty on macOS and Linux: 115200 8N1, no
    flow control, DTR and RTS on, and reads that return as soon as anything is there."""

    def __init__(self, name, kernel32=None, last_error=None):
        """Raises OSError when `name` cannot be opened or is not a serial port."""
        self.name = name
        self.k, self.last_error = (kernel32, last_error) if kernel32 else _api()
        self.read_ms = self.write_ms = None
        # \\.\ for every port, not just COM10 and up, which cannot be opened without it.
        self.handle = self.k.CreateFileW('\\\\.\\' + name, GENERIC_READ | GENERIC_WRITE,
                                         0, None, OPEN_EXISTING, 0, None)
        if self.handle == INVALID_HANDLE:
            self.handle = None
            code = self.last_error()
            raise OSError({ERROR_FILE_NOT_FOUND: f'{name}: no such port',
                           ERROR_ACCESS_DENIED: f'{name} is open in another program'}.get(
                               code, f'{name}: cannot open it (Windows error {code})'))
        try:
            self._configure()
        except OSError:
            self.close()
            raise

    def _configure(self):
        dcb = DCB()
        dcb.DCBlength = ctypes.sizeof(DCB)
        if not self.k.GetCommState(self.handle, ctypes.pointer(dcb)):
            raise OSError(f'{self.name} is not a serial port '
                          f'(Windows error {self.last_error()})')
        dcb.BaudRate, dcb.ByteSize, dcb.Parity, dcb.StopBits = BAUD, 8, NOPARITY, ONESTOPBIT
        dcb.fBinary = 1
        dcb.fParity = dcb.fOutxCtsFlow = dcb.fOutxDsrFlow = dcb.fDsrSensitivity = 0
        dcb.fOutX = dcb.fInX = dcb.fErrorChar = dcb.fNull = 0
        # Left on, a line error would fail every read until ClearCommError.
        dcb.fAbortOnError = 0
        # On, as a Linux open leaves them.
        dcb.fDtrControl, dcb.fRtsControl = DTR_CONTROL_ENABLE, RTS_CONTROL_ENABLE
        if not self.k.SetCommState(self.handle, ctypes.pointer(dcb)):
            raise OSError(f'{self.name} refused the port settings '
                          f'(Windows error {self.last_error()})')
        self._wait(200, 200)

    def _gone(self, call):
        return OSError(f'{self.name} went away: {call} failed '
                       f'(Windows error {self.last_error()})')

    def _wait(self, read_ms=None, write_ms=None):
        """How long the next ReadFile waits for its bytes, and the next WriteFile for the
        board to take all of its chunk. A timeout of 0 would mean none at all, so never
        less than 1 ms."""
        read_ms = self.read_ms if read_ms is None else max(1, read_ms)
        write_ms = self.write_ms if write_ms is None else max(1, write_ms)
        if (read_ms, write_ms) != (self.read_ms, self.write_ms):
            timeouts = COMMTIMEOUTS(0, 0, read_ms, 0, write_ms)
            if not self.k.SetCommTimeouts(self.handle, ctypes.pointer(timeouts)):
                raise self._gone('SetCommTimeouts')
            self.read_ms, self.write_ms = read_ms, write_ms

    def _waiting(self):
        errors, status = DWORD(), COMSTAT()
        if not self.k.ClearCommError(self.handle, ctypes.pointer(errors),
                                     ctypes.pointer(status)):
            raise self._gone('ClearCommError')
        return status.cbInQue

    def _read(self, n):
        buf, got = ctypes.create_string_buffer(n), DWORD()
        if not self.k.ReadFile(self.handle, buf, n, ctypes.pointer(got), None):
            raise self._gone('ReadFile')
        return buf.raw[:got.value]

    def read(self, n, timeout):
        """Up to `n` bytes as soon as any are waiting, or b'' when none arrive within
        `timeout` seconds. Raises OSError once the port has gone, which is how a board
        that rebooted, or jumped into the extension image, shows up here: every call on
        the handle fails from then on."""
        waiting = self._waiting()
        if waiting:
            return self._read(min(n, waiting))
        # One byte, so ReadFile returns the moment anything arrives rather than waiting
        # out the timeout for `n`; then whatever came in with it.
        self._wait(read_ms=round(timeout * 1000))
        first = self._read(1)
        rest = self._waiting() if first and n > 1 else 0
        return first + (self._read(min(n - 1, rest)) if rest else b'')

    def write(self, chunk, timeout):
        """How many of `chunk`'s bytes the port took, waiting up to `timeout` seconds for
        the board to take them. Raises TimeoutError when it did not: usbser.sys fails a
        write that times out (error 121), and a failed write does not say how much of the
        chunk went, so none of it can be sent again. Raises OSError once the port has
        gone."""
        self._wait(write_ms=round(timeout * 1000))
        data, took = bytes(chunk), DWORD()
        if not self.k.WriteFile(self.handle, data, len(data), ctypes.pointer(took), None):
            if self.last_error() == ERROR_SEM_TIMEOUT:
                raise TimeoutError(f'{self.name}: the board did not take a write '
                                   f'within {timeout:g} s')
            raise self._gone('WriteFile')
        return took.value

    def close(self):
        if self.handle is not None:
            self.k.CloseHandle(self.handle)
            self.handle = None
