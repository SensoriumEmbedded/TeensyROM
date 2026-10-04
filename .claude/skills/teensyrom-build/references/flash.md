# Flashing a board over USB

`tools/flash-firmware.mjs` (`npm run flash`) writes a built `.hex` to a TeensyROM on USB.
Before writing, it asks each attached board what it is and refuses an image built for the
other cartridge. After writing, it checks that the board runs the new image.
`npm run flash -- --help` lists every option with examples.

## What you need

- The TeensyROM in a C64 or C128 that is switched on, with a USB cable to the computer.
  The Teensy's USB power is cut during assembly, so the computer cannot power it.
- **Windows:** Teensyduino (the Teensy core from the Boards Manager is enough). The tool
  uses its `teensy_ports` and `teensy_post_compile`, and writes through the Teensy Loader
  app with no button to press.
- **macOS, Linux:** `teensy_loader_cli` (`brew install teensy_loader_cli` on macOS). The
  write starts when you press the program button on the Teensy.

## Commands

```
npm run flash -- --check                  # identify every board, show what would be written, write nothing
npm run flash                             # newest .hex in build/firmware, to the board it is built for
npm run flash -- --hex <file>             # a given image
npm run flash -- --hex <file> --uid <id>  # a given board, by chip ID (--check lists them)
npm run flash -- --hex <file> --port COM12
npm run flash -- --all                    # every board, each with the newest image built for its type
```

## Which board is written

1. `--uid` or `--port`, when given.
2. Otherwise the only board attached.
3. Otherwise the one board whose type matches the image.

It refuses, and lists the attached boards, when it cannot tell: two boards of the image's
type (name one with `--uid`), a board that does not answer, or none of the image's type
(then the image is for the other cartridge). A board that does not answer is usually one
whose serial port another program holds, such as a TeensyROM app or a serial monitor. A board
already waiting in the bootloader cannot be asked what it is. On its own it is written on
the image's word; beside another board the tool refuses, because the Teensy Loader writes
to whichever board it finds in the bootloader.

That choice decides the write on Windows. On macOS and Linux, `teensy_loader_cli` writes
whichever board enters the bootloader first, which is the one whose program button you
press, so with several boards attached press the button on the board the tool names.
Without Teensyduino's `teensy_ports` there, the tool sees only the first serial device,
and after the write it takes the one device it finds as the written board, whatever that
device is now called; the checks on its answer confirm it.

`--all` writes each attached board with the image for its type, one at a time, and stops
at the first failure. Without `--hex` it takes the newest TeensyROM and the newest
TeensyROM+ image in `build/firmware`; with `--hex` given twice, those two. Boards it has
no image for are skipped and reported.

## What the output tells you

```
Boards:
  COM4  TeensyROM+ v0.8.0.11, built Sep 25 2026, 03:03:51  chip 19277260
  COM12  TeensyROM v0.8.0.11, built Sep 25 2026, 03:03:51  chip 14470230
```

- **The build date**, for an image from `build-firmware.mjs`, is the time of the commit it
  was built from, in UTC ([firmware-build.md](firmware-build.md) shows how to find the
  commit). Edits that were never committed build with the previous commit's date, and an
  Arduino IDE build carries the time it was compiled.
- **`chip`** is the Teensy's factory ID. It tells two boards of one type apart, and it is
  what `--uid` takes.
- **`(MinimalBoot)`** after the version means the small boot program answered, not the
  main firmware. TeensyROM stays in it to run a large cartridge image, and it reports its
  own build date.

After each write the board shows up for a moment as MinimalBoot before the main firmware
starts. With firmware where MinimalBoot is Serial + MIDI like main, that is the same COM
port; older firmware's MinimalBoot came up on a COM port of its own. The tool waits for the main firmware to answer, then
checks that:

- the written board answers with the build date recorded in the image;
- it answers with a chip ID when the image's firmware reports one, the same as before if
  it reported one then;
- every other board still reports what it did before.

A reply missing the date or the chip ID is asked for again, for up to 20 seconds, before
the check fails.

On Windows it then closes the Teensy Loader, whether the run succeeded or failed, so the
image cannot be written to another board later by a stray press of a program button. It
closes any open Teensy Loader window, including one you opened yourself. A single write
takes about 45 seconds; `--all` on two boards about a minute and a half.

## When something goes wrong

The Teensy bootloader lives in a separate chip, so a failed or wrong write cannot brick a
board: it can always be put back into the bootloader and written again.

- **`STOPPED: another board went into the bootloader`**: the reboot reached the wrong
  board. The tool closed the Teensy Loader, which stops the write if it had not started;
  it may already have begun. Unplug every other board, then flash that board with its own
  image. If it is still in the bootloader, the tool writes it on the image's word, so
  choose the image carefully.
- **`CHECK FAILED`**: read the listed problems. A board that took the wrong image: unplug
  the others, press its program button, and flash it with its own image.
- **`The board did not come back within 2 minutes`**: look at the Teensy Loader window,
  then run `--check`.
- **The version text is invisible on the C64 screen after a USB flash** (black on black):
  a known display quirk, not a failed flash. The fix is under "Blank version banner" in
  [tools/bench/README.md](/tools/bench/README.md).

TeensyROM settings survive a USB flash.

## Other ways to update

- **From the SD card or a USB drive, on the C64:** the firmware's own updater. See the
  firmware update section of [docs/General_Usage.md](/docs/General_Usage.md). It needs no
  computer, and it refuses a wrong image only after the file has been carried over.
- **Through the SD card, remotely:** `tools/bench/fwupdate.py` (macOS and Linux) pushes
  the image to the SD card, launches the updater and answers its prompt. It needs a TR+.
  See [tools/bench/README.md](/tools/bench/README.md).
- **The Teensy Loader app by hand:** open the hex and press the program button. There is
  no check that the image fits the board.
