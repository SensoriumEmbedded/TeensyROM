# Text-interface MPE host 1.2.1

The text firmware now supports the current optional auxiliary RAM profile and
fitted full-height F5 video transport. This lets compatible modules use the
same generic host services as MPE firmware 1.2.1. Travis's text menu, ordinary
cartridge cache, USB/network configuration and boot routing remain in place.

The update includes Travis's `72de295` RAM1-saving change and `0997c5a` fix for
hot-key launches of root-level SD files. Shared host source is pinned to
`8024a9107f24b586ed4ffad7cae541cfe59e7d37` in `mpe/source-lock.json`.
No emulator engine, C64 VM client, game data or GUI desktop asset is included.

## Firmware for testing

[Download the combined HEX](../mpe/review/TeensyROM+_0.8.0.4_MPE-1.2.1_full.hex).
It contains the ordinary MinimalBoot, normal text application and separate
VM host. Use TeensyROM+ Fab0.4 hardware; original TR is unsupported for VMs.
The upstream firmware version remains 0.8.0.4; MPE-1.2.1 identifies the shared
host revision in this test filename.

SHA-256: `b14a3d3e9ee518bf0f8128970faa48b73666bdbb99cea14a4e337884a7d77bd8`.

Build with `mpe/Build.ps1`. The verification record contains the full built
source-input manifest, image layout, memory measurements and package hashes:
[verification JSON](../mpe/review/host-1.2.1-verification.json) and
[host test output](../mpe/review/host-1.2.1-tests.log).

## Main-image budget

The ordinary main image retains Travis's post-FLASHMEM memory layout:

```
RAM1: variables:258596, code:219048, padding:10328   free for local variables:36316
RAM2: variables:17600  free for malloc/new:506688
```

The physical fast-code padding is 10,324 bytes because the printed counter
omits a four-byte section. The linked stack gap is 26,560 bytes: the printed
locals figure omits 9,756 bytes of non-cacheable Ethernet storage. Keep this
accounting distinct from empirically established thresholds using the printed
counter. No new main RAM1 or static RAM2 allocation is introduced by this refresh.

The ordinary MinimalBoot HEX is byte-identical to the preceding text build.
The dedicated VM image uses 93,032 reported ITCM code bytes against a 98,304-byte
ceiling. Its 16 KiB host heap, module arena, 48 KiB stack and exclusive RAM2
ownership remain bounded. Main firmware has 279 KiB before the VM region;
5,369,856 bytes above the combined image remain available for updater staging.

## Verification and hardware status

All three firmware images built with Teensy core 1.61.0 and GCC 11.3.1.
Host checks passed for files, packet replay, existing video modes, center/full
F5 conversion and bank ownership, auxiliary-profile validation and loader
bounds, and actual stock EasyFlash swap-ownership guards with legacy fallthrough.
The current public Doom package passed validation and launch routing.
All 219 protected upstream files remain unchanged, including the C64 menu assets.

These checks establish source, format and host behavior, not physical gameplay.
Retest text-menu/root hot-key launch, graphics/input/sound, reset/menu return,
normal and large cartridges, USB/networking and firmware update on hardware.
The auxiliary and F5 services require matching separately supplied modules and
clients; this firmware does not replace those packages.
