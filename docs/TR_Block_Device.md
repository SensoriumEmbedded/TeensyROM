# TeensyROM block device

A small register interface that lets software on the C64 or C128 read and write 256-byte blocks of a disk image file on the TeensyROM's SD card, with no drive on the IEC bus. The first user is the GEOS disk driver in [`tools/geos`](../tools/geos), but nothing here is specific to GEOS: any program that finds the registers can use them.

This document is the whole contract. The firmware side is [`TRBlock.h`](../Source/Teensy/BlockDevice/TRBlock.h); the registers sit beside the USB mouse's, served from [`TRBlockIO.c`](../Source/Teensy/MinimalBoot/Common/IO_Handlers/TRBlockIO.c).

## Table of contents
  * [What it is and is not](#what-it-is-and-is-not)
  * [Where the registers are](#where-the-registers-are)
  * [Register map](#register-map)
  * [Running a command](#running-a-command)
  * [Commands](#commands)
  * [Status and errors](#status-and-errors)
  * [Disk addresses](#disk-addresses)
  * [Writes reach the card later](#writes-reach-the-card-later)
  * [What the firmware does not promise](#what-the-firmware-does-not-promise)
  * [A reader's checklist](#a-readers-checklist)

## What it is and is not
* One image at a time, chosen by the firmware (for GEOS, when the menu launches it). The C64 cannot choose or replace the image.
* Blocks are 256 bytes. A block is addressed by two bytes whose meaning depends on the image kind ([Disk addresses](#disk-addresses)).
* It is available while the GeoRAM handler or the `REU+USB Mouse/Disk` handler (TR+ only) is active, which is also where the USB mouse registers are. It is not available under other handlers, the plain REU included, and not under a Host Extension.
* It is not an IEC drive. Software that talks to the IEC bus cannot see it.

## Where the registers are
The registers share a page with the USB mouse registers, which start it and identify it. The base `B` is `$DF00` beside GeoRAM, in the IO2 page GeoRAM leaves free, and `$DE00` beside the REU, whose registers repeat across IO2.

The block device is present when `TRB` is at `B+$10..B+$12`. A handler that does not carry it leaves those bytes unanswered, which reads as whatever the bus last held, so check all three bytes. Look at `$DF10` before `$DE10`: beside the REU, `$DF10..$DF12` read `$FF`, as a 1764's unused registers do, while `$DE10` beside GeoRAM is its window, which can hold any bytes.

Everything below is an offset from `B`.

## Register map

| Offset | Dir | Name | Meaning |
|-------:|:---:|------|---------|
| `$08` | write | CMD | command; writing it starts the command |
| `$09` | write | ADDR0 | block address, first byte |
| `$0A` | write | ADDR1 | block address, second byte |
| `$0B` | write | PAGE | which 64-byte quarter of the block the window shows, 0 to 3 (higher bits ignored) |
| `$0C` | write | TOKEN | a non-zero byte the driver chooses for the next command |
| `$0D` | read | STATUS | `$80` busy, `$5A` done, `$E0` and up failed, `$00` no command run yet |
| `$0E` | read | ECHO | the token of the command STATUS reports on |
| `$10..$12` | read | ID | `TRB` |
| `$40..$7F` | read/write | WINDOW | the quarter of the block PAGE selects |

Reading a register marked write is not answered. Offsets not listed are not answered.

## Running a command
1. Write ADDR0, ADDR1 and PAGE as the command needs, and TOKEN, a non-zero byte that differs from the last command's.
2. For a write, fill the window, one PAGE at a time (see [`WRITE`](#commands)).
3. Write CMD. STATUS reads `$80` from the moment this write completes, so a read of STATUS after it never sees the previous command's result.
4. Read STATUS until it is not `$80`, then check ECHO equals your TOKEN. A result with another token is not yours: wait, or run the command again.
5. `$5A` is success. `$E0` and up is a failure ([Status and errors](#status-and-errors)).

Two rules that Wes Biggs' [teensyrom-vdrive](https://github.com/wesbiggs/teensyrom-vdrive) learned on hardware with a similar register interface, and which hold here too:
* **Read STATUS until two consecutive reads agree** before believing a value other than `$80`. A read can land while the firmware is updating it.
* **Read back what you write into the window** when the data matters, and compare, before sending CMD.

On an NTSC C64 with a Kawari VIC-II, a TR+ read now and then comes back with the previous read's bits ORed in, so **read every byte until two consecutive reads agree**, the window and ID included, and compare a read-back again before calling it a fault.

Do not write ADDR0, ADDR1, PAGE or TOKEN, or the window, while STATUS is `$80`. The firmware latches the command's operands when CMD is written, so a stray register write cannot change a command already running, but writes to the window during a command are ignored.

## Commands

| CMD | Name | What it does |
|----:|------|--------------|
| `$01` | `READ` | reads the block at ADDR0, ADDR1 into the firmware's 256-byte buffer. Afterwards the window shows the PAGE quarter of it, and each write to PAGE shows another |
| `$02` | `WRITE` | writes the 256-byte buffer to the block at ADDR0, ADDR1. The buffer is what the window wrote, one PAGE at a time, since the last command |
| `$03` | `FLUSH` | forces the card to hold every write accepted so far |
| `$04` | `INFO` | describes the image: five bytes at the start of the window (see below) |
| `$05` | `BOOT` | reads page ADDR1:ADDR0 (ADDR0 the low byte) of the launch's boot image into the buffer, as `READ` does; a last page shorter than 256 bytes is padded with zeros |

`READ`, `INFO` and `BOOT` fill the buffer; a `WRITE` after either writes what they left unless the window changed it. A `WRITE` for a block the driver has not filled is the driver's mistake, not the firmware's to guard.

`INFO` answers, in window bytes 0 to 4:

| Byte | Meaning |
|-----:|---------|
| 0 | image kind: `1` D64, `2` D71, `3` D81 |
| 1 | flags; bit 0 set when the image can be written |
| 2..4 | blocks in the image, 24 bits, low byte first |

The boot image is what a launch that cannot start from a PRG hands its boot code: GEOS 128's C128 boot ROM pulls `GEOBOOT128`, with the TR driver already in it, through `BOOT`. The firmware sets it when it mounts the disk image, and it goes with that image. A launch that mounts an image without one leaves `BOOT` failing with `$E1`, and `BOOT` past the boot image's end fails with `$E2`.

Any other CMD fails with `$E6`.

## Status and errors

| STATUS | Meaning |
|-------:|---------|
| `$00` | no command has been run since the handler started |
| `$80` | a command is running |
| `$5A` | the command with ECHO's token succeeded |
| `$E1` | no image is mounted, or for `BOOT`, no boot image |
| `$E2` | the address is outside the image, or for `BOOT`, past the boot image's end |
| `$E3` | the card could not be read |
| `$E4` | the card could not be written |
| `$E5` | the image is read-only |
| `$E6` | not a command |

A failed `READ` or `BOOT` leaves the buffer unspecified. A failed `WRITE` that was rejected before the card was touched (`$E1`, `$E2`, `$E5`) changes nothing; one the card failed (`$E4`) may have changed part of the block.

## Disk addresses
For the disk image kinds (D64, D71, D81) ADDR0 is the track and ADDR1 the sector, as the 1541, 1571 and 1581 number them: tracks start at 1, sectors at 0. The 1541 and 1571 use the usual four speed zones (21, 19, 18 and 17 sectors a track, the 1571's second side repeating them); the 1581 has 40 sectors on every track. An address off the end of the image or past a track's last sector fails with `$E2`.

The firmware does this arithmetic so that a 6502 driver does not have to. A driver that wants raw sector numbers instead is a different image kind, which this version does not define.

## Writes reach the card later
`WRITE` returns `$5A` when the block is accepted, not when the card holds it. The firmware writes the block to the file at once and flushes the file a quarter of a second after the last `WRITE`, or on `FLUSH`. A `FLUSH` the card fails returns `$E4`. A flush the card fails is tried again every quarter second until the card takes it. A program that must know a write survived power loss sends `FLUSH` and waits for `$5A`. GEOS does not, so a disk saved and then powered off within that quarter second can be torn.

## What the firmware does not promise
* **Speed.** A command takes as long as the card takes. Poll; do not time it.
* **C128 2 MHz.** The registers have not been exercised at 2 MHz. A driver on the C128 should run at 1 MHz around a transfer until that is measured.
* **Concurrent users.** One command at a time, one driver at a time. Two programs polling the same STATUS will read each other's results; the token tells them apart but does not queue them.
* **The image.** It must not be changed on the card while it is mounted. A card swapped while it is mounted is noticed when the firmware next starts the card, as a GeoRAM save does; commands then fail with `$E1` rather than write to the other card.

## A reader's checklist
A driver that follows this document:
* checks `TRB` at `$DF10`;
* writes TOKEN, the address, PAGE, then CMD;
* reads STATUS until two reads agree and the value is not `$80`, then checks ECHO;
* moves a block as four PAGEs of the window;
* sends `FLUSH` where a write must survive power loss.
