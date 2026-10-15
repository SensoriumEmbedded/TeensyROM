# C128 tools

Scripts for running C128 cartridges on the TR and for finding out why one does not.
Python 3, standard library only.

| Script | Does |
|--------|------|
| `geos128crt.py <rom> <out.crt> [name]` | Wrap a GEOS 128 v2 U36 function ROM (32 KB raw) as a C128 `.crt`. Patches the loader to read the external function ROM (`$8046`: `$17`→`$2B`) and to stay at 1 MHz while copying (`$8033`: `STY $D030`→`BIT`). For the English 1581 ROM it also adds the sd2iec hook below. Also writes the patched ROM as `<out>.bin`. |
| `iecdecode.py <log> [prefix]` | Decode the drive commands in a VICE trace of the KERNAL serial entry points: each M-W, M-E and command string, with sd2iec's fast-loader CRC. With a prefix, dumps the drive RAM each M-E ran. |
| `vicemon.py "<before>" "<after>"` | Run VICE remote-monitor commands, resume, and run more once a breakpoint stops it, on one connection. |

The C128 needs TR firmware that maps a 16 KB + 16 KB C128 `.crt`
(`c128-crt-load-address`). The GEOS ROMs and disks are copyrighted, so they are not here;
build the `.crt` from your own ROM dump and launch it from SD.

## The sd2iec 1581 hook

sd2iec runs GEOS's fast loader only when the CRC of the M-W upload stream is one it knows
(`doscmd.c`, `fl_crc_table`). The 1581 ROM uploads its own variant of Configure 2.0's 1581
drive code, 12 bytes different (drive `$0410` and `$0462`), so sd2iec does not recognise
it and GEOS restarts about every half second. The drive code is compressed in the ROM, so
the hook goes in the loader: its call to the unpacker (`JSR $2C40`) goes to a routine in
the loader's `$BD` filler that unpacks, then copies Configure's bytes over the unpacked
code in RAM bank 1. The upload then matches Configure's byte for byte (CRC `$3F8D`), and a
real 1581 boots the same. The German 1581 ROM has the same loader, but where its drive
code unpacks to has not been checked, so it gets no hook.

## Tracing drive commands in VICE

The headless VICE build (`--enable-headlessui`) runs without a window. It cannot save
screenshots, so read results from memory through the monitor instead (`bsave`, or `m`).

    x128 -default -80col -drive8type 1581 -8 disk.d81 -cartcrt geos.crt \
         -remotemonitor -monlog -monlogname iec.log -limitcycles 2000000000 &
    printf 'trace exec ffb1\ntrace exec ffa8\ntrace exec ffae\nx\n' | nc 127.0.0.1 6510
    python3 tools/c128/iecdecode.py iec.log
