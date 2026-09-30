#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Service bit 20 against a real TR+: timer ticks a C64 loses when it grants each slice.
   grants.py [slice sizes, to run only those rows]

Packages grants/module.cpp and grants/client.a as /VMS/GRANTS, then launches one content
file per slice size. The client runs a CIA2 timer NMI every 200 cycles and grants a slice
from its handler; the module writes 4 KiB to $4000 over and over for 1.5 s and exits with
its job count and the ticks the client lost, which the main image prints on the way back.
The client grants a slice only when it will end before the next tick, so a slice too long
for the period is never granted and its row ends VM_C64_NO_GRANT; that is reported, not
failed. The client sums what it holds at $4000 at the end, so a write that did not land fails
the row. The first row asks the client never to grant, and expects the write to fail for that.

Needs acme on PATH, a host serving bit 20 installed, and the C64 at the menu. Every launch
reboots the board twice, so a run takes a few minutes.
"""
import os, re, subprocess, sys, tempfile
from hostops import run_step
from protocol import DRIVE_SD
from trlink import Link

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
SERVICES = 0x1F | 0x4000 | 0x100000          # base profile, exit, C64 transfer
NO_GRANT = 0x80000000 | 0xFF                   # VM_C64_NO_GRANT, as module.cpp reports it
CORRUPT = 0x800000EE                           # the client's sum disagreed
RECORD = re.compile(r'Extension boot: (.+?) \(code \$([0-9a-f]+), detail \$([0-9a-f]+)\)')
failed = []
quiet = open(os.devnull, 'w')


def install(tr):
    with tempfile.TemporaryDirectory() as work:
        subprocess.run(['node', os.path.join(ROOT, 'tools/build-extension.mjs'), '--id', 'GRANTS',
                        '--extensions', 'gnt', '--source', os.path.join(HERE, 'grants/module.cpp'),
                        '--client-source', os.path.join(HERE, 'grants/client.a'),
                        '--services', hex(SERVICES), '--out', work], check=True, cwd=ROOT)
        package = os.path.join(work, 'VMS', 'GRANTS')
        for name in sorted(os.listdir(package)):
            tr.post(os.path.join(package, name), f'/VMS/GRANTS/{name}')


def launch(slice_bytes, grant):
    """The detail the module exited with, or None when it did not exit."""
    name = f'S{slice_bytes}{"" if grant else "N"}.GNT'

    def prepare(tr):
        with tempfile.NamedTemporaryFile(suffix='.gnt', delete=False) as f:
            f.write(bytes([slice_bytes & 0xFF, slice_bytes >> 8, grant]))
        try:
            tr.post(f.name, f'/GRANTS/{name}')
        finally:
            os.unlink(f.name)
        tr.drain()
        tr.launch(f'/GRANTS/{name}', DRIVE_SD)

    result = run_step(prepare, 30, out=quiet)
    record = RECORD.search(result.boot)
    if not result.rebooted or not record:
        failed.append(f'{name}: no exit record')
        return None
    what, code, detail = record.group(1), int(record.group(2), 16), int(record.group(3), 16)
    if code != 0x04:
        failed.append(f'{name}: {what} (code ${code:02x}, detail ${detail:x})')
        return None
    return detail


with Link() as tr:
    install(tr)
ONLY = [int(a) for a in sys.argv[1:]]
# First, because it takes no DMA: a client that never grants shows the rest of the run works.
if not ONLY:
    detail = launch(64, 0)
    print(f'A client that never grants: {"refused as ungranted" if detail == NO_GRANT else detail}')
    if detail != NO_GRANT:
        failed.append(f'an ungranted write ended with {detail}, not VM_C64_NO_GRANT')
print('Timer NMIs lost with a grant per tick that fits, every 200 cycles:')
for slice_bytes in ONLY or (32, 64, 100, 120, 130, 140, 150, 160, 0):
    detail = launch(slice_bytes, 1)
    if detail is None:
        continue
    if detail == NO_GRANT:
        print(f'  slice {slice_bytes or "whole":>5}: never fits before the next tick, so never granted')
        continue
    if detail == CORRUPT:
        failed.append(f'slice {slice_bytes}: the C64 did not hold what was written')
        continue
    if detail >> 28 == 0xE:
        failed.append(f'slice {slice_bytes}: job {detail & 0xFFFF} still pending after a second')
        continue
    if detail >> 28 == 0xD:
        failed.append(f'slice {slice_bytes}: the client stopped answering (state {detail & 0xFF}, '
                      f'{detail >> 8 & 0xFFFF} jobs)')
        continue
    if detail & 0x80000000:
        failed.append(f'slice {slice_bytes}: the write failed with {detail & 0xFF:#x}')
        continue
    jobs, lost = detail >> 16, detail & 0xFFFF
    print(f'  slice {slice_bytes or "whole":>5}: {lost / max(jobs, 1):6.1f} NMIs lost per 4 KiB job ({jobs} jobs)')

if failed:
    raise SystemExit('FAILED: ' + '; '.join(failed))
