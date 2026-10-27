# C128 tools

Scripts for working on C128 code for the TR in VICE. Python 3, standard library only.

| Script | Does |
|--------|------|
| `vicemon.py "<before>" "<after>"` | Run VICE remote-monitor commands, resume, and run more once a breakpoint stops it, on one connection. |

## Headless VICE

The headless VICE build (`--enable-headlessui`) runs without a window. It cannot save
screenshots, so read results from memory through the monitor instead (`bsave`, or `m`).

    x128 -default -remotemonitor -monlog -monlogname mon.log -limitcycles 2000000000 &
    python3 tools/c128/vicemon.py "break 2f3c" "m 2f3c 2f4f"
