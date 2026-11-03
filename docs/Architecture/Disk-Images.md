# Disk images (D64, D71, D81)

One engine reads and writes Commodore disk images for every feature that looks inside one: the GEOS support, the
[TR block device](../TR_Block_Device.md), and VDrive, the virtual IEC drive from
[teensyrom-vdrive](https://github.com/wesbiggs/teensyrom-vdrive). It lives in `Source/Teensy/DiskImage/`.

| File | What it is |
|---|---|
| `d64.h`, `d64.c` | The engine: geometry, BAM, directory, files (read, write, append, scratch, rename, REL), format, validate, raw sector access. Portable C99 that also compiles as C++; no `malloc`, all state in caller structs. |
| `DiskImage.h` | What the firmware and the host harnesses include. It places the code in flash (`FLASHMEM`) and the tables in `PROGMEM`, and pulls `d64.c` in once. |
| `MenuDisk.h` | The TR menu's read-only view of an image (`D64.ino`): opening by the kind the file's extension names, and loading a file. |

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

`d64_open_exact()` takes only the sizes in the table and refuses anything between them. The TR block device mounts
through it, so a truncated or padded file is no image rather than one read past its end.

## Who uses it

| Caller | What it takes from the engine |
|---|---|
| TR block device (`BlockDevice/TRBlockImage.h`) | `d64_open_exact`, the geometry, `d64_block_read`/`d64_block_write`; `INFO` reports `fmt + 1` and `d64_sectors()`. It leaves the error table unread. |
| GEOS (`GEOS/GeosDiskPatch.h`, `GEOS/GeosBoot.h`) | `d64_open_exact`, `d64_dir_check` before anything reads the directory, `d64_find` and the file reader for the boot file, the directory iterator and the file reader for setting up CONFIGURE, `d64_block_read`/`d64_block_write` for bringing an old TR mouse driver up to date and rewriting CONFIGURE's records in place, and for adding the TR input driver `d64_dir_room`, `d64_blocks_allocatable`, `d64_create`/`d64_write`/`d64_close_write` and `d64_alloc_next`. What stays GEOS's own is the info block, the GEOS bytes of the directory entry, and moving the driver ahead of the other input drivers. |
| TR menu (`D64.ino`, through `MenuDisk.h`) | `d64_open` with the track count the extension and size give, the geometry, and the file reader to load a program into RAM. It reads only. It opens short D64s down to 171520 bytes (some copiers write them; `Realm.d64` is one), so a sector past the file's end fails to read instead of the image being refused. |

## Allocation: one allocator, as 1541 DOS does it

Everything the firmware writes to an image takes its sectors from the engine's one allocator, the way a 1541, 1571
or 1581 would:

* A file's first sector is on the data track nearest the directory (17 on a D64 or D71, 39 on a D81), then the tracks
  further out on that side, then the other side (19 up, and on a D71 the second side the same way from track 52, or on
  a D81 from track 41). Each next sector is 10 sectors on from the last on the same track while it has one free.
* A track gives a sector only while its BAM count is above 0 and its bitmap shows one free; the count and the
  bitmap are both brought down.
* A directory that fills takes a sector on the directory track, 3 on from its last, never the header or (on a D81)
  the BAM sectors.

The GEOS installer used to have an allocator of its own, which took the first free sector from track 1 up. GEOS does
not care where its files are, so it now uses the shared one, and the driver lands where DOS would have saved it.
`tools/geos/trmouse.py` models the same allocator, and `geos-disk-patch.test.js` holds the firmware's result to the
script's byte for byte.

Refusals come before the first write, so a refused image is unchanged:

| Check | Refusal |
|---|---|
| `d64_dir_check` | the header links nowhere, or the chain leaves the directory track or comes back on itself |
| `d64_dir_room`, only when no slot is free | a directory sector the BAM shows free (growing the directory could hand it out again), or no sector the directory can grow into |
| `d64_blocks_allocatable` | fewer sectors than the file and its info block need. It counts, per track, the smaller of the BAM count and the free bits: exactly what the allocator can hand out, so a BAM whose count disagrees with its bitmap is no longer refused but cannot run the write out of room partway. |

## Tests

`Source/Teensy/tests/disk-image.test.js` builds the engine with the harness flags as C and as C++, saves and reads a
file on a blank image of each kind, and checks the size table above. `trblock.test.js` runs the block device on each
kind, a 40-track D64 and images with error bytes included. `geos-disk-patch.test.js` and `geos-boot.test.js` run the
GEOS installer and boot-file reader on synthetic boot disks. `menu-disk.test.js` loads files the way the TR menu does,
from each kind, short and odd-sized images, and broken chains.
