# MPE Third-Party Notices

TeensyROM hardware and original firmware are by Travis Smith / Sensorium
Embedded. The original MIT license and source notices are retained.

MHS created the MHS Power Engine system, module format, generic host and
transport. The MHS F1 colour converter in `vm/video/mpe_video_color_f1.h` and
`.cpp` is distributed under MIT, with its notice in the source.

Original Prism display components include software by Patai Gergely (cobbpg),
under MIT. Its template and adapted transport retain the
[display-component notice](experiments/dosvm-nuflix/THIRD-PARTY-NOTICES.md)
and [license](experiments/dosvm-nuflix/upstream-pinned/LICENSE).

The MPE build links the supplied complete MPE host library 1.2.23, including
original Prism and new MHS Prism+ services, as its independent third image.
Travis's text interface and ordinary cartridge firmware are compiled from
the public source in this repository. The archive is broader than a display
renderer; its public wrapper exposes the host setup and loop entry points.

Only the identified new MHS Prism+ contributions are covered by
[LICENSE-PRISM-PLUS.txt](mpe/library/LICENSE-PRISM-PLUS.txt). Their implementation
source remains private. The accompanying
[host integration permission](mpe/library/LICENSE-HOST-INTEGRATION.txt)
permits recipients to link those unchanged contributions into their own
firmware and redistribute the combination. Public interface and glue are
separately MIT-licensed. These terms preserve earlier MIT grants and
third-party rights; they do not relicense the rest of MPE or its dependencies.

The [library notices](mpe/library/NOTICES.md), component manifest and licenses
identify those boundaries. The library's `Relink-SDK.zip` contains matching
library objects, corresponding library source and relinking instructions,
including the Teensy core's LGPL-covered components and permissively licensed
dependencies. Preserve the matching materials, licenses, component manifest
and applicable notices when redistributing combined firmware. The
[library README](mpe/library/README.md) explains direct relinking and rebuilding
the supplied corresponding libraries, with third-party modification rights
retained. The complete MIT display-component license is also retained in
[LICENSE-DISPLAY-COMPONENTS.txt](mpe/library/LICENSE-DISPLAY-COMPONENTS.txt).

The matching [minimal/text-main library source bundle](mpe/review/firmware-1.2.24-library-sources.zip)
supplies the selected libraries, individual notices, build profiles and
provenance for the two public application images. Their application source
is in this repository. This supplement and the host relink SDK cover
different images; preserve both with the combined firmware distribution.

FlasherX and its Flasher3/4 predecessors retain their notices in
`Source/Teensy/FlashUpdate.ino` and `Source/Teensy/Flash/`.
SDK, bundled upstream applications and library notices remain those of their
authors. This document does not replace those individual notices.

No emulator engine, private game data, Custom GUI desktop asset or VM runtime
package is bundled with this firmware. Public firmware integration source
and build tools are included in this repository; the new Prism+ implementation
is supplied in compiled form under the terms above. VM engine packages have
their own licenses and corresponding-source distributions.
