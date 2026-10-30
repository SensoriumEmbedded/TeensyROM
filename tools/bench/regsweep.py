#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Read margin of the Special IO handler in use: raise nS_PLAprop while regread.a
reads the cartridge's registers, and count the wrong answers at each step.
    regsweep.py DWELL VALUE...

Needs acme on PATH, an image built with Dbg_SerTimChg, the Special IO handler
under test chosen in Settings, and the C64 at the menu. Restores the default
timing when done, and leaves regread running.
"""
import os, subprocess, sys, tempfile, time
from protocol import DRIVE_SD
from trlink import Link

RESULTS = 0xC000
LAYOUTS = {0: 'plain REU', 1: 'REU with the TR page at $DE00', 2: 'GeoRAM with the TR page at $DF00'}


def counters(tr):
    b = tr.peek(RESULTS, 9)
    return dict(passes=b[0] | b[1] << 8, reu=b[2] | b[3] << 8, ff=b[4] | b[5] << 8,
                ids=b[6] | b[7] << 8, layout=b[8])


def start_regread(tr):
    here = os.path.dirname(os.path.abspath(__file__))
    with tempfile.TemporaryDirectory() as work:
        subprocess.run(['acme', '-f', 'cbm', '-o', os.path.join(work, 'regread.prg'),
                        os.path.join(here, 'regread.a')], check=True, cwd=work)
        tr.post(os.path.join(work, 'regread.prg'), '/REGREAD.PRG')
    tr.launch('/REGREAD.PRG', DRIVE_SD)
    time.sleep(3)
    first = counters(tr)['passes']
    time.sleep(1)
    if counters(tr)['passes'] == first:
        raise SystemExit('regread is not running')


def timing(tr, command):
    tr.drain(0.2)
    tr.wr(command)
    return tr.text()


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    dwell = float(sys.argv[1])
    values = [int(v) for v in sys.argv[2:]]
    with Link() as tr:
        if 'nS_PLAprop' not in timing(tr, b't'):
            raise SystemExit('no timing commands: this image was built without Dbg_SerTimChg')
        start_regread(tr)
        print(f'layout: {LAYOUTS.get(counters(tr)["layout"], "unknown")}')
        try:
            for value in values:
                shown = timing(tr, b'tp%03d' % value)
                if 'nS_PLAprop   %03d' % value not in shown:
                    raise SystemExit(f'tp{value:03d} not taken:\n{shown}')
                time.sleep(0.5)
                before = counters(tr)
                time.sleep(dwell)
                after = counters(tr)
                added = {k: after[k] - before[k] & 0xFFFF for k in ('passes', 'reu', 'ff', 'ids')}
                print(f'nS_PLAprop {value:3d}  passes +{added["passes"]:5d}  REU registers +{added["reu"]:5d}  '
                      f'$FF page +{added["ff"]:5d}  TR page IDs +{added["ids"]:5d}', flush=True)
        finally:
            if 'Defaults set' not in timing(tr, b'td'):
                raise SystemExit('td not confirmed: the board is still running the last value')


if __name__ == '__main__':
    main()
