# MPE packages with their own runtime

The updated MPE design puts Prism+ and the matched C64 receiver in each `.MPE`
file. A separately installed MPE extension host supplies loading, files, input,
packets, bounded DMA and return to the stock menu. It does not select palettes,
convert pixels or own renderer versions. The renderer source is not part of
TeensyROM; distributing a compiled package does not make its binary secret.

This integration uses the installable host contract from PR #42. It needs no
change to the stock menu's `.mpe` discovery route or the 384 KiB host flash slot.

## Memory

The package profile gives the host 64 KiB of ITCM and the package 128 KiB at
`0x10000..0x30000`. The six physical ITCM banks remain reserved. Module data is
still `0x20014000..0x20044000`, the guest still has 512 KiB of RAM2, and the shared
stack still has at least 48 KiB. C64 display and receiver layouts are package
details and do not change to accommodate the extension loader.

An external host which implements that profile can request the stricter link
guard using:

```
node tools/build-firmware.mjs --target tr-plus --host-sketch PATH_TO_MPE_HOST --host-code-kib 64
```

The default remains 96 KiB. This option changes only the host's ITCM ceiling;
the host must implement the module profile, MPU loading permissions and callback
validation itself. It does not add profile 3 to the stock VM host or turn the
Example host into an MPE host. A host exceeding its selected ceiling fails to
link. Linker assertions for DTCM, stack and RAM2 remain in force.

## Registration and ABI boundary

`/VMS/MPE/manifest.vmi` contains six lines:

```
VM1
MPE
mpe
engine.mvm
client.crt
END
```

Those module/client files are registration assets, not a converted game.
Selecting `MONKEY.MPE` passes its path through `/VMS/launch.vml`. After reset,
the MPE host must revalidate authorization, record CRC, manifest and selected
MGC1 container; then load the container's own module and receiver. Never rename
the file to CRT or feed its compressed payload to the ordinary CRT loader.

Registry bits 20 (`0x100000`) and 21 (`0x200000`) are assigned to MHS's bounded
C64 transfer callback and CODE128 admission. An MPE registration module can
require these bits while remaining an ordinary base-profile registration image.
The stock host does not advertise them and therefore refuses that registration.
An MPE host must check the selected container's actual requirements again.

MHS's historical optional callback table and service assignments differ from
the stock ABI-2 tail. Use the upstream host wire contract for installation,
launch records and failure reporting; do not substitute the upstream module
table for the MPE module table. In particular, upstream bit 14 means module
exit and must not be advertised for MHS's historical speech callback.

## Transfer ownership

A package owns the pixels and an immutable scatter request until completion.
The host validates source/destination bounds, mailbox exclusion, at most 64
spans and PAL/NTSC slice budgets before DMA. Fresh border grants, bounded
readiness waits and bus release on failure are required. Packet replay takes
priority; while a request awaits its next grant, foreground storage or module
work must not consume that grant. The receiver must provide a write/read edge
inside its Ready loop so asynchronous DMA can acquire the bus before Ready
expires. Neither a grant mock nor successful package parsing proves this timing.

## Validation boundary

The owner confirmed HamsterOS startup and legacy games on MPE 1.4.3, then
confirmed the packaged Monkey launch correction on matched MPE/HamsterOS 1.4.4.
That is evidence for the MHS firmware and exact private test package. It is not
hardware acceptance of an MPE `.TRH` running under stock TeensyROM.

This PR supplies the build boundary and capability registration for that host.
Its remaining integration gate is the separately built MPE host and registration
bundle: install, launch the package, exercise input/audio/save, return to the
stock menu, then repeat on physical hardware. No commercial game data or Prism+
implementation is included here.
