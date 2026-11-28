# Disk images (D64, D71, D81)

One engine reads and writes Commodore disk images for every feature that looks inside one: the GEOS support, the
[TR block device](../TR_Block_Device.md), and VDrive, the virtual IEC drive from
[teensyrom-vdrive](https://github.com/wesbiggs/teensyrom-vdrive). It lives in `Source/Teensy/DiskImage/`.

| File | What it is |
|---|---|
| `d64.h`, `d64.c` | The engine: geometry, BAM, directory, files (read, write, append, scratch, rename, REL), format, validate, raw sector access. Portable C99 that also compiles as C++; no `malloc`, all state in caller structs. |
| `DiskImage.h` | What the firmware and the host harnesses include. It places the code in flash (`FLASHMEM`) and the tables in `PROGMEM`, and pulls `d64.c` in once. |

The engine came from teensyrom-vdrive's `d64/` (commit `b74ce37`), where it is tested against VICE's `c1541`. The
copy here differs only in what the firmware needs: the SPDX lines, the placement macros moving into `d64.h`, and its
three constant tables marked `D64_FLASH`. Changes to the engine are made here, and teensyrom-vdrive takes them from
here.

## How a caller uses it

The engine never touches a file. A caller gives it a sector reader (and a writer, or none for a read-only image)
that turns a track and sector into a read of 256 bytes; `d64_sector_offset()` gives the byte offset in the image
file. `d64_open_size()` picks the kind from the file size, taking any size from one row's first number up to the
next row as that row's kind:

| Size | Kind |
|---|---|
| 174848, or 175531 with error bytes | D64, 35 tracks |
| 196608, or 197376 with error bytes | D64, 40 tracks |
| 349696, or 351062 with error bytes | D71 |
| 819200, or 822400 with error bytes | D81 |

## Tests

`Source/Teensy/tests/disk-image.test.js` builds the engine with the harness flags as C and as C++, saves and reads a
file on a blank image of each kind, and checks the size table above.
