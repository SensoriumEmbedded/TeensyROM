# MPE host library with Prism+ 1.2.23

This compiled library supplies the complete MPE host, including Prism+, for
TeensyROM's independent third image. The ordinary text menu and cartridge
image remain separately built. The library is not a renderer-only API.

`include/MpeHost.h` declares `mpeHostSetup()` and `mpeHostLoop()`.
`MPEBoot/MPEBoot.ino` provides the small Arduino entry-point wrapper. Build
the wrapper against Teensyduino 1.61.0 with GNU Arm 11.3.1, Cortex-M7 Thumb,
hard floating-point ABI and FPv5-D16. Required definitions are USB_DISABLED,
Fab04_Features, and MHS_VM_PROFILE_192_320. Include SD.h, SPI.h and EEPROM.h
so Arduino discovers SD, SdFat, SPI and EEPROM. Do not compile the old
source-based MPE host into the same image.

Link the supplied archive using `--whole-archive` and `--no-whole-archive`
around that archive only. The accompanying profile reserves flash
0x60280000 through 0x602dffff. Its bootdata points at that image; the main
image remains at 0x60060000. The loader must consume the one-shot request
before entering this host. The host records MinBootInd_FromMin so reset
returns to the menu without repeating autolaunch. The EEPROM magic and
launch-field addresses match TeensyROM 0.8.0.6.

The guarded RAM profile retains 16 KiB host heap, 48 KiB stack, 192 KiB VM
module data, the 96 KiB module ITCM window, and all 512 KiB of RAM2 for the
guest. No PSRAM is required.

The supporting library objects, corresponding source, linker profile and
`relink.mjs` are inside `Relink-SDK.zip`. Extract that ZIP into a new folder
using your archive tool, then open a terminal in the extracted folder and
rebuild the public wrapper and link the supplied objects:

```
node relink.mjs --toolchain /path/to/teensy-compile/11.3.1/arm --output /path/to/output
```

Add `--rebuild-libraries` to compile the supplied corresponding library
source before relinking. Add `--verify-original` to require the reference
third-image hash. The result is MPEBoot.ino.hex, to be combined with the
independently built minimal and text-main images using the main builder's
partition and self-update headroom checks. It is not a complete firmware
and should not be flashed by itself.

The public headers, wrapper and relink script are MIT-licensed. The original
host/third-party permissions are retained. LICENSE-HOST-INTEGRATION.txt
expressly permits anyone to link the unmodified covered MHS Prism+
contributions into their own firmware and distribute the combination.
The Prism+ implementation source remains private. Corresponding LGPL
source and relinking materials are supplied without restricting their
existing modification rights. Preserve the notices and matching relinking
materials when redistributing a combined firmware.

The package contains public interface/glue source and third-party library
source. It contains no Prism+ implementation source, GUI image, or VM engine.
`manifest.json` identifies the archive and third-image reference hashes. Its
`sourceFirmwareSha256` identifies the released GUI firmware 1.2.23 whose host
was adapted for the consumed boot request and public entry points; it is
not the hash of this relocated third image or the combined text firmware.
File paths beginning `sources/`, `objects/` and `profiles/` in the notices
and build recipes are relative to the extracted SDK folder.

`mpe/source-lock.json` remains the historical source-based host record. The
old `sync-host.mjs` import command is disabled for this compiled-library
integration. Update the archive, public interface, manifest and matching
relink SDK together as a reviewed package; do not import a private checkout.
