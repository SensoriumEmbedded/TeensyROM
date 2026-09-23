# Bench scripts

Drive a real TeensyROM from a host computer: reflash it, launch things, read and
poke the C64's memory. They were written to verify firmware without anyone at the
machine. Python 3, standard library only, macOS or Linux (they use `termios`).

You need a TR or TR+ in a C64/C128, powered on at the menu with an SD card in it,
and a USB cable from the cartridge to the host. These scripts are the hands-free
route; `docs/General_Usage.md` lists the others, including the program-button one.

`tools/Debug/` holds the scope and bus-timing scripts. They are a separate stack:
their own port handling, and pyserial rather than the standard library.

## What talks, and what does not

Which image is running decides how much of this works. The **main image**
answers all of it. The **minimal image** answers reset, launch, version and the
firmware check, and fails other commands with `Busy!`. The **extension image
runs with USB disabled** and answers nothing at all, so a silent port after
launching an extension is success, not a hang. (That image arrives with the
extension loader, PR #31; the other two are here today.)

`probe.py` asks the board whether it is main or minimal, and reports silence
otherwise -- which is the extension image or a hung board. On macOS the port
name is a second opinion: the main image renames its USB device, so it
enumerates as `usbmodem2101` where the other two use the Teensy's serial number.

The port is `$TR_PORT`, else the first `/dev/cu.usbmodem*` on macOS or
`/dev/ttyACM*` on Linux. Do not hardcode it. After a reboot `$TR_PORT` is a
preference rather than a pin: the USB serial number changes across some firmware
changes, so a board that does not come back under its old name is picked up from
whatever node appeared beside it.

`peek`, `poke` and everything built on them (`screen`, `keypress`, `colors`,
`fwupdate`'s prompt answering) use DMA and need a Fab 0.4 board (a TR+).

## Scripts

| Script | Does |
|--------|------|
| `fwupdate.py <hex> [remote]` | Push a hex to the SD card, launch it, answer the C64's Y/N prompt by DMA, echo the updater until the board reboots, then check that the board came back on the main image reporting the hex's build stamp. A file that is not Intel HEX is refused before anything is pushed; a hex this reader cannot fully decode is flashed and the board is still checked for the main image, without the stamp comparison. |
| `push.py <local>=<remote> ...` | Copy files to the SD card. Deletes the target first; the firmware will not overwrite. |
| `launch.py <path> [drive] [secs]` | Launch a file from the SD card and echo serial. |
| `hostinstall.py <local.trh> [remote]` | Push a `.TRH` extension host package to the SD card and install it. A package the firmware refuses comes back with the C64 still running and nothing erased; one it takes reboots the board, and the install record is read off the boot output. Build the package with `npm run build:host-package -- --hex <firmware.hex>`. |
| `hostuninstall.py` | Remove the installed extension host: clear the tag so the slot stops reading as a host, which reboots the board. With nothing installed it says so and the board stays up. The payload stays in flash until the next install overwrites it. |
| `hostcycle.py <local.trh>` | The install/remove round trip end to end against a real board, asserted and unattended: remove, refuse a second remove, install, remove, refuse again, reinstall. Exits non-zero on the first step whose outcome does not match. |
| `exttest.py <path>` | Launch an extension and report how it ended: still running, or reset to the menu with a failure record. |
| `peek.py <hex addr> <len>` | Hex dump C64 memory. |
| `screen.py` | The C64 text screen. |
| `keypress.py [code]` | Put a key in the keyboard buffer (default `Y`; F1/F3/F5/F7 are `0x85`..`0x88`). |
| `colors.py` | Top row, its colour RAM and the VIC colour registers. |
| `listen.py [secs] [hex]` | Echo serial output, optionally sending bytes first. |
| `probe.py` | Which image is running -- main, minimal or silent -- and its build banner. |
| `ls.py [path] [drive]` | List a directory, to see that a push landed where it was aimed (first 1000 entries). |
| `reset.py` | Reset the C64 to the menu, and the board out of the minimal image. |
| `hostops.py` | Installing and removing a host, shared by the three scripts above. |
| `trlink.py` | The shared library the above are built on. |
| `protocol.py` | Every value that goes on the wire, named once. |
| `c64.py` | C64 memory locations, and screen codes as text. |
| `hexfile.py` | Reads a firmware hex: the two image regions, and the build stamp in the main one. |
| `test_*.py` | Tests against a fake board on a pty, and against the firmware's own definitions; no hardware. |

## Recipes

Reflash without the program button (build for **your** cartridge: `--target tr-plus` for a TR+):

    python3 tools/bench/fwupdate.py "build/firmware/TeensyROM+_<version>_full.hex"

That ends by asking the rebooted board which image it came up as and comparing
its banner with the build stamp compiled into the hex, because answering the Y/N
prompt is the step that silently does nothing when the screen is not what the
script expects, and the board then keeps running the old firmware and says so
nowhere. The minimal image can carry the same stamp, so a board that fell back
to it is a failed update however its banner reads.

The stamp is `SOURCE_DATE_EPOCH`, which the build sets to the HEAD commit time
unless the environment already holds one, so it identifies a commit and not a
build: commit before building if you want two runs told apart.

Run the hello extension (`build/extensions/` comes with PR #31):

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
board does. `test_protocol.py` checks every constant in `protocol.py` against the
`Source/Teensy/` definition it came from, so a moved token fails a test rather
than a board. The fake drops and re-publishes its pty, under a new name, for
`fwupdate.py`'s post-reboot check; `exttest.py`'s own reconnect path still
needs a real reset.

`screen.py`, `colors.py`, `exttest.py` and `fwupdate.py` read the screen through
`petscii_row`, which decodes the uppercase/graphics charset. A C64 in the
lower/uppercase charset shows its letters as `.`, which is also why `fwupdate.py`
would miss the `Y/N` prompt there and time out rather than answer it.

    python3 -m unittest discover -s tools/bench
