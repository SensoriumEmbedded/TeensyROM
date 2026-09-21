# Bench scripts

Drive a real TeensyROM from a host computer: reflash it, launch things, read and
poke the C64's memory. They were written to verify firmware without anyone at the
machine. Python 3, standard library only, macOS or Linux (they use `termios`).

You need a TR or TR+ in a C64/C128, powered on at the menu with an SD card in it,
and a USB cable from the cartridge to the host. See `tools/flash-firmware.mjs` for
the button-pressing route (`teensy_loader_cli`); these scripts are the hands-free one.

## What talks, and what does not

Everything here speaks to the **main image** over USB serial. The minimal image
has no such commands, and the **extension image runs with USB disabled**, so it is
silent. A silent port after launching an extension is success, not a hang.

The port is `$TR_PORT`, else the first `/dev/cu.usbmodem*`. Do not hardcode it:
the main image renames its USB device, so its node differs from the one minimal
and the extension image enumerate as (`usbmodem2101` versus `usbmodem<serial>1`).
That difference is also how you tell which image is running.

`peek`, `poke` and everything built on them (`screen`, `keypress`, `colors`,
`fwupdate`'s prompt answering) use DMA and need a Fab 0.4 board (a TR+).

## Scripts

| Script | Does |
|--------|------|
| `fwupdate.py <hex> [remote]` | Push a hex to the SD card, launch it, answer the C64's Y/N prompt by DMA, echo the updater until the board reboots. |
| `push.py <local>=<remote> ...` | Copy files to the SD card. Deletes the target first; the firmware will not overwrite. |
| `launch.py <path> [drive] [secs]` | Launch a file from the SD card and echo serial. |
| `exttest.py <path>` | Launch an extension and report how it ended: still running, or reset to the menu with a failure record. |
| `peek.py <hex addr> <len>` | Hex dump C64 memory. |
| `screen.py` | The C64 text screen. |
| `keypress.py [code]` | Put a key in the keyboard buffer (default `Y`; F1/F3/F5/F7 are `0x85`..`0x88`). |
| `colors.py` | Top row, its colour RAM and the VIC colour registers. |
| `listen.py [secs] [hex]` | Echo serial output, optionally sending bytes first. |
| `probe.py` | Liveness check: sends `b` (read-only) and prints the reply. |
| `trlink.py` | The shared library the above are built on. |
| `test_trlink.py` | Tests against a fake board on a pty; no hardware. |

## Recipes

Reflash without the program button (build for **your** cartridge: `--target tr-plus` for a TR+):

    python3 tools/bench/fwupdate.py "build/firmware/TeensyROM+_<version>_full.hex"

Commit before building if you want the banner to prove which build is on the board:
`SOURCE_DATE_EPOCH` is the HEAD commit time, so two builds of one commit carry one stamp.

Run the hello extension:

    python3 tools/bench/push.py build/extensions/HELLO.crt=/HELLO.crt \
        build/extensions/VMS/HELLO/manifest.vmi=/VMS/HELLO/manifest.vmi \
        build/extensions/VMS/HELLO/engine.mvm=/VMS/HELLO/engine.mvm \
        build/extensions/VMS/HELLO/client.crt=/VMS/HELLO/client.crt
    python3 tools/bench/exttest.py /HELLO.crt

Blank version banner after a USB flash (black on black, not a regression): `colors.py`
shows text present and colour RAM row 0 all zeros. Fix it over serial with
`SetColorToken` (`0x64 0x22 0x02 0x04`) then `ResetC64Token` (`0x64 0xEE`).

## Limits

The fake board in `test_trlink.py` proves the framing and byte order, not what a
board does. `exttest.py` needs a real reset to exercise its reconnect path, and the
fake does not cover it.

    python3 -m unittest discover -s tools/bench
