# MPE host library notices

The archive contains the shared MPE host, original Prism, and the MHS Prism+
contributions. It contains no GUI image or VM engine. Only the identified new
MHS Prism+ contributions carry the restricted license; previous MIT grants
and all third-party licenses remain effective.

| Component | Applicable notices |
| --- | --- |
| New MHS Prism+ contributions | LICENSE-PRISM-PLUS.txt, LICENSE-HOST-INTEGRATION.txt, PRISM-PLUS-COMPONENTS.json |
| Public interface and entry-point glue | LICENSE-PUBLIC-INTERFACE-MIT.txt |
| TeensyROM and previously published MHS infrastructure/converter | LICENSE-TEENSYROM-MHS-MIT.txt |
| Original Prism display components | LICENSE-DISPLAY-COMPONENTS.txt, including copyright Patai Gergely |
| Teensyduino 1.61.0 core, EEPROM and SPI | Per-file notices in sources/hardware; LGPL-2.1 and permissive components; LICENSE-LGPL-2.1.txt |
| SD and SdFat | Per-file notices in sources/hardware/libraries/SD and SdFat; MIT |

Teensy core components under LGPL-2.1 are used by the linked host. The SDK
contains their corresponding source, modified bootdata, USB-disabled yield
fix, build recipes, linkable host archive and library objects. You may
modify those libraries and relink using relink.mjs --rebuild-libraries.
The GNU LGPL-2.1 own-use modification and debugging permissions remain
effective for the combined work. The supplied GNU GPL texts accompany
related source notices; no GPL-only library unit is required by this host.

Library examples or optional utilities retain their own notices. Their
presence in a source tree does not mean they are linked into this image.

The host owns the VM RAM profile only in its dedicated third image. Hardware
operation and PAL/NTSC gameplay still require testing on a TeensyROM+ board.
