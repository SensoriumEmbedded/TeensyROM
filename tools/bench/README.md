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
launching an extension is success, not a hang.

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
| `hostinstall.py <local.trh> [remote]` | Push a `.TRH` extension host package to the SD card and install it. A package the firmware refuses comes back with the C64 still running and nothing erased; one it takes reboots the board, and the install record is read off the boot output -- exiting non-zero unless that record says the host was installed, since a failed erase or program reboots the same way. Build the package with `npm run build:host-package -- --hex <firmware.hex>`. |
| `hostuninstall.py` | Remove the installed extension host: clear the tag so the slot stops reading as a host, which reboots the board. Exits non-zero unless the record on the way back up says the host was removed -- a failed erase reboots too. With nothing installed it says so and the board stays up. The payload stays in flash until the next install overwrites it. The firmware takes this command on the USB device port only; over the USB host port or the TCP listener it is refused with `Busy!`. |
| `hostcycle.py <local.trh>` | The install/remove round trip end to end against a real board, asserted and unattended: remove, refuse a second remove, install, remove, refuse again, reinstall. Every step runs whatever the ones before it did; it exits non-zero at the end, listing each step whose outcome did not match. |
| `exttest.py <path>` | Launch an extension and report how it ended, told apart by which image answers afterwards rather than by whether a serial node exists. No image answering is the success case for a resident module -- the main image's node goes away and the minimal image's usually stays behind, enumerated and serviced by nobody -- so look at the C64; the main image answering means the extension gave the machine back, and its record is printed *and read*, because a launch that faulted comes back the same way one that finished does; a port that never drops means nothing entered the image, and the screen carries the refusal. Exits 0 for a resident module or a record naming a normal finish (`hostops.FINISHED_NORMALLY`), and 1 for a launch nothing entered or a record that is a failure, absent, truncated or unrecognised -- the same positive match, and for the same reason, as `INSTALLED` and `REMOVED`. |
| `hostenter.py <path> [phrase]` | The other shape from `exttest.py`, for a host that hands the machine back: launch, the port drops, the host runs and resets, and the main image prints its `VmFail` record on the way back up. With a `phrase` that record is the assertion; without one the run reports what it saw and asserts nothing, because a launch that failed after the jump reboots the same way a good one does. A host that stays resident never gives the main image's port back, and what that looks like here depends on whether minimal's orphan node stayed behind: with no node, `REBOOT_TIMEOUT` seconds and then `the board did not come back`; with one -- the usual case -- the screen read that follows fails instead, `no reply -- DMA read is not compiled into this image, or this is not the main image`, which names the wrong diagnosis for a host that is running exactly as intended. Use `exttest.py` for those; it treats no image answering as the success it is. |
| `peek.py <hex addr> <len>` | Hex dump C64 memory. |
| `screen.py` | The C64 text screen. |
| `keypress.py [code]` | Put a key in the keyboard buffer (default `Y`; F1/F3/F5/F7 are `0x85`..`0x88`). |
| `colors.py` | Top row, its colour RAM and the VIC colour registers. |
| `listen.py [secs] [hex]` | Echo serial output, optionally sending bytes first. |
| `probe.py` | Which image is running -- main, minimal or silent -- and its build banner. |
| `ls.py [path] [drive]` | List a directory, to see that a push landed where it was aimed (first 1000 entries). |
| `reset.py` | Reset the C64 to the menu, and the board out of the minimal image. |
| `hostops.py` | Installing and removing a host, and `run_step` -- driving anything that ends in a reboot and reading which of the two shapes came back. Also the phrases a reboot is judged by: `INSTALLED`, `REMOVED` and `FINISHED_NORMALLY`. Shared by the `host*.py` scripts and by `exttest.py`, whose exit status is `finished_normally()`'s answer, so a phrase edited here moves that verdict; and by `test_bounds.py`, which patches its timeouts to test them, and `test_protocol.py`, which pins the phrases against the firmware's own `VmFail::describe()`. |
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

Run the hello extension (`npm run build:hello` writes `build/extensions/`):

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
`fwupdate.py`'s post-reboot check and for `exttest.py`'s:
`test_trlink.ExtensionRun` runs `exttest.py` end to end across a real port
drop, so the arm a reboot alone cannot classify -- finished, faulted, or no
record at all -- is covered without hardware. Its other two arms are not, and
need a board that sits still: a module that stays resident, and a launch the
firmware refuses before the port ever drops.

`screen.py`, `colors.py`, `exttest.py`, `fwupdate.py` and everything built on
`hostops.py` -- `hostinstall.py`, `hostuninstall.py`, `hostcycle.py`,
`hostenter.py` -- read the screen through `petscii_row`. Screen codes 1-26 are
the unshifted letters and 65-90 the shifted ones, and which letters those are
depends on where the VIC is pointed, so `petscii_row` takes a `charset`. It
defaults to `LOWER_UPPER`, because that is where the menu puts the C64 at
startup: `MainMenu.asm`'s `TextScreenMemColor` writes `#$17` to `$d018`. Pass
`UPPER_GFX` for a screen a launched program has switched back to the power-on
charset, where 65-90 are graphics and read as `.`.

Both letter ranges decode, so a phrase asserted against the screen is no longer
restricted to lower-case glyphs. Fold case anyway -- `hostops.Outcome.said` does,
and `fwupdate.py` now does for its `Y/N` prompt -- since which case a glyph
carries is a property of the screen, and a launched program need not stay in the
charset the menu left behind. Nobody has read a real updater screen to settle
which one it uses, so folding case is what makes that question not matter.

    python3 -m unittest discover -s tools/bench
