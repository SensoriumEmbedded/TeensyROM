# Text-interface MPE retest, 2026-09-06

This is a build for physical testing, requested after a reported Doom startup
failure. It keeps Travis's text menu and existing boot/handoff implementation.
No Doom engine, launcher or game-data changes were made.

The generic VM host is refreshed from committed Custom GUI revision
`69a711a42fd8e07d7857872073c9d2c7dee5bcc9` (firmware 1.1.12). Uncommitted video
experiments are excluded. Per-file source hashes are in `mpe/source-lock.json`;
`mpe/tools/sync-host.mjs` refreshes the generic files from a committed revision
while preserving the documented platform adapters.

Firmware source is recorded in commit `437a8ffdc6dfbaf144d8886bd0cb5fe16db27235`.
Subsequent review documentation changes do not alter the built firmware inputs.

Build with the existing `mpe/Build.ps1` command. Output:
`TeensyROM+_0.8.0.4_MPE-review2_full.hex`

SHA-256: `ecc8efdc8e0c3074d47a5bc3219673c295dfdb1dd9ffb500ae7892092aa7103f`

The VM host uses 91,432 bytes of the 98,304-byte ITCM code window. All three
images compile; memory boundaries, updater staging space, package preflight,
launch routing, generic file services, packet replay and current video host
tests pass. The same 219 upstream files remain unchanged. These checks do not
establish physical boot, Doom gameplay or a fix for the reported failure.
The [machine-readable verification result](../mpe/review/review2-verification.json)
records this HEX and the exact Doom package pairing.

The current public Doom package already matches the committed development
package. Its files were used unchanged for preflight:

| File | SHA-256 |
| --- | --- |
| `engine.mvm` | `281a460123b09967679e8830bd084614921e63a774e09fe12fed6ea008953e09` |
| `client.crt` / `DOOMVM.crt` | `3f1de49341ff00875fdd859f37e47e72a7b8e0d301094c16e343030afe4637ed` |
| `manifest.vmi` | `802de5ae380853970ce2d635749f3274127beb239a7b64a7d6fe2d2314fe1264` |

Use TeensyROM+ PCB v0.4 / Teensy 4.1. Original TR hardware is unsupported for
VMs. Only DoomVM is publicly distributed; no withheld VM engines are included.
The earlier `MPE-REVIEW-RESULTS.md` records review1, not this retest build.
