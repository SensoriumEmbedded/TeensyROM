# C128 tools

Scripts for running C128 cartridges on the TR and for finding out why one does not.
Python 3, standard library only.

| Script | Does |
|--------|------|
| `geos128crt.py [--clock T] <geoboot128> <out.crt> [name]` | Build a C128 `.crt` that boots GEOS 128 v2 from its boot disk's GEOBOOT128 (below). Also writes the ROM as `<out>.bin`. |
| `iecdecode.py <log> [prefix]` | Decode the drive commands in a VICE trace of the KERNAL serial entry points: each M-W, M-E and command string, with sd2iec's fast-loader CRC. With a prefix, dumps the drive RAM each M-E ran. |
| `vicemon.py "<before>" "<after>"` | Run VICE remote-monitor commands, resume, and run more once a breakpoint stops it, on one connection. |

The C128 needs TR firmware that maps a 16 KB + 16 KB C128 `.crt`
(`c128-crt-load-address`). GEOS is copyrighted, so it is not here; build the `.crt` from
your own boot disk and launch it from SD.

## The GEOS 128 crt

The cart does what a disk boot does, without the disk's boot loader: it unpacks GEOBOOT128
into RAM bank 1 at `$2B19`, where the boot loader would have loaded it, and enters it at
`$2F3C` with the setup GEOS's own ROM loader uses. GEOS then reads the rest of itself from drive 8, so the boot disk still has to be
there, and its configuration decides the RAM expansion: the stock GEOS 128 2.0 GEOBOOT for an
REU, or the GEOBOOT and `128 CONFIG 2.0R` from GEOS 2.0r's GeoRAM disk (`GR81128`) with the
TR's Special IO set to GeoRAM. Take the file off the disk with VICE's `c1541`:

    c1541 boot.d81 -read geoboot128 geoboot128.prg
    python3 tools/c128/geos128crt.py geoboot128.prg geos128.crt

GEOBOOT128 is packed (an LZ format of the tool's own, unpacked by the stub) into the cart's
32 KB from `$8300`. The C128 calls a function ROM's cold start with IRQs enabled and the
ROM's top page mapped over the CPU vectors, so the cold start masks IRQs first, and `$FFF9`
holds an `RTI` that the vectors point to. The stub runs from `$1300` with the bottom 8 KB
common to both banks, reading the ROM with `$FF00 = $2B` and writing bank 1 with `$7F`.

GEOS keeps a RAM reset key in bank 1: `CBM` at `$FFF5` and a vector to `$03E4`. The C128
KERNAL jumps through it on every reset into C128 mode, before it initializes anything, and
it survives every reset. The TR menu runs in C64 mode, which reuses
`$03E4`, so after GEOS the next C128 crt of any kind crashed at reset. The tool breaks the
`C` in GEOBOOT128 (`$6FF5`), as GEOS's own ROM build does. GEOS booted from disk still leaves
the key, so a C128 crt launched after a disk boot of GEOS needs a power cycle first.

## The boot clock

GEOS 128 handles years past 1999 itself: it keeps a two-digit year, rolls 12/31/99 over to
01/01/00, and its leap-year test holds through 2099. What it lacks is a clock source. A cold
boot sets CIA1's time-of-day clock to 1:00 PM and the date to 3/23/92, as immediate operands
in GEOBOOT128 (`$2C29`).

Once GEOBOOT128 is unpacked, the stub maps the cartridge in (`$FF00 = $6A`) and runs a
routine at `$8200`. It reads a clock record, sets the TOD from it, writes its year, month and
day over GEOS's defaults, and jumps GEOS past its own TOD set. If GEOS's defaults are not the
expected bytes, it leaves them alone.

The record sits at `$81F0`: the tag `TRCLOCK`, a version byte `$01`, then century, year
(0–99), month, day, hour (0–23), minute and second. The tool fills it with the time the
`.crt` was built, or `--clock`. TR firmware with C128 crt clock support (`c128-crt-clock`)
finds the record by its tag and keeps it at local time while the cart runs, so GEOS boots
with the TR's clock. The routine reads the record until two reads agree, since the TR can
rewrite it mid-read.

## Tracing drive commands in VICE

The headless VICE build (`--enable-headlessui`) runs without a window. It cannot save
screenshots, so read results from memory through the monitor instead (`bsave`, or `m`).

    x128 -default -80col -drive8type 1581 -8 disk.d81 -cartcrt geos.crt \
         -remotemonitor -monlog -monlogname iec.log -limitcycles 2000000000 &
    printf 'trace exec ffb1\ntrace exec ff93\ntrace exec ffa8\ntrace exec ffae\nx\n' | nc 127.0.0.1 6510
    python3 tools/c128/iecdecode.py iec.log
