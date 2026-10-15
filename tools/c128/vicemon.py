#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run VICE remote-monitor commands across a breakpoint, on one connection.

  vicemon.py "<cmds before x>" "<cmds after the break>"

Commands are separated by ';'.  The first set runs, then `x` resumes the emulator; once the
monitor prompt comes back (a breakpoint or watchpoint hit), the second set runs.  Start
VICE with -remotemonitor (127.0.0.1:6510, or $VICE_MONITOR as host or host:port).
"""
import os
import re
import socket
import sys

PROMPT = re.compile(rb'\(C:\$[0-9a-fA-F]{4}\) $')
COMMAND_TIMEOUT = 30
GREETING_WAIT = 0.5
DEFAULT_HOST, DEFAULT_PORT = '127.0.0.1', 6510


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__.strip())
    host, _, port = os.environ.get('VICE_MONITOR', DEFAULT_HOST).partition(':')
    s = socket.create_connection((host or DEFAULT_HOST, int(port) if port else DEFAULT_PORT))

    def send(cmd):
        s.sendall((cmd + '\n').encode())

    def until_prompt(timeout, what):
        s.settimeout(timeout)
        out = b''
        try:
            while not PROMPT.search(out):
                data = s.recv(65536)
                if not data:
                    sys.exit(f'VICE closed the monitor connection {what}')
                out += data
        except socket.timeout:
            sys.exit(f'no monitor prompt within {timeout} s {what}')
        return out.decode('latin1')

    def run(cmds, show):
        for cmd in commands(cmds):
            send(cmd)
            out = until_prompt(COMMAND_TIMEOUT, f'after {cmd!r}')
            if show:
                print(out)

    s.settimeout(GREETING_WAIT)
    try:
        s.recv(65536)
    except socket.timeout:
        pass
    run(sys.argv[1], show=False)
    send('x')
    print(until_prompt(None, 'before stopping'))
    run(sys.argv[2], show=True)


def commands(cmds):
    return [c.strip() for c in cmds.split(';') if c.strip()]


main()
