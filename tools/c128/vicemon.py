#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run VICE remote-monitor commands across a breakpoint, on one connection.

  vicemon.py "<cmds before x>" "<cmds after the break>"

Commands are separated by ';'.  The first set runs, then `x` resumes the emulator; once the
monitor prompt comes back (a breakpoint or watchpoint hit), the second set runs.  Start
VICE with -remotemonitor (127.0.0.1:6510, or $VICE_MONITOR as host or host:port).
"""
import os
import socket
import sys

PROMPT = b'(C:$'
DEFAULT_HOST, DEFAULT_PORT = '127.0.0.1', 6510


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__.strip())
    host, _, port = os.environ.get('VICE_MONITOR', DEFAULT_HOST).partition(':')
    s = socket.create_connection((host or DEFAULT_HOST, int(port) if port else DEFAULT_PORT))

    def send(cmd):
        s.sendall((cmd + '\n').encode())

    def drain(secs):
        s.settimeout(secs)
        out = b''
        try:
            while True:
                data = s.recv(65536)
                if not data:
                    break
                out += data
        except socket.timeout:
            pass
        return out.decode('latin1')

    for cmd in sys.argv[1].split(';'):
        send(cmd)
    drain(1)
    send('x')
    s.settimeout(None)
    buf = b''
    while PROMPT not in buf:
        data = s.recv(4096)
        if not data:
            sys.exit('VICE closed the monitor connection before stopping')
        buf += data
    print(buf.decode('latin1'))
    for cmd in sys.argv[2].split(';'):
        send(cmd)
        print(drain(2))


main()
