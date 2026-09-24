# Contributing to TeensyROM

Thanks for your interest in TeensyROM! This is a hobby project maintained largely by one
person (Travis Smith / Sensorium Embedded) in spare time, with a small number of community
contributions over the years (menu features, IO handlers, docs, and the separate
[TeensyROM-Web](https://github.com/MetalHexx/TeensyROM-Web)/[CLI](https://github.com/MetalHexx/TeensyROM-CLI)
tools). Review bandwidth is limited, so this doc exists to help your contribution land
smoothly on the first pass.

## Ways to contribute

- **Bug reports** — open a GitHub issue. Include your hardware revision (TR v0.2/0.3 vs
  TR+ v0.4), firmware version, C64/C128 model, and NTSC/PAL. For anything
  timing/DMA/hardware-quirk related, the exact machine model matters a lot (see
  [Compatibility](README.md#compatibility)).
- **Feature ideas / questions** — the
  [TeensyROM Discord](https://discord.gg/ubSAb74S5U) is the best place to float an idea or
  ask "would this even work" before writing code. For anything non-trivial (new IO handler,
  new hardware interface, changes near the ISR or memory layout), please discuss it there or
  in an issue first — it's a lot easier to redirect an idea than a finished PR.
- **Code** — see below.
- **Hardware/PCB feedback** — only PCB v0.2/v0.3 is public (`PCB/v0.3/EaglePCB/`); v0.4
  (TeensyROM+) is not yet open. PCB contributions should target v0.3.
- **Docs** — usage docs under `docs/` are always welcome fixes, especially from people
  who just got something working and remember where the instructions were unclear.

## Before you start on code

1. **Read [docs/Architecture/Overview.md](docs/Architecture/Overview.md)** for the repo
   layout and the two-toolchain build model.
2. **Read [docs/Architecture/Constraints.md](docs/Architecture/Constraints.md)** — hard
   rules that nothing enforces at compile time:
   - No new logic in the per-C64-cycle ISR path (`isrPHI2()` and the active IO handler's
     `ROMLHndlr`/`ROMHHndlr`/`IO1Hndlr`/`IO2Hndlr`/`CycleHndlr`) — no flash, EEPROM, SD, or
     USB access from that path either. Slow work goes through the existing
     poll-handshake pattern instead.
   - RAM budgets are hard caps, not soft targets, in both the full firmware and
     MinimalBoot — see the doc for current headroom numbers before adding anything
     RAM1-resident.
   - Build against **Teensyduino 1.61.0**, not 1.62.0 (confirmed toolchain regression,
     not a TeensyROM bug — don't try to work around it in source).
3. If you're adding a new cartridge/peripheral IO handler or a new built-in menu program,
   there are step-by-step checklists in `.claude/skills/add-io-handler/SKILL.md` and
   `.claude/skills/add-menu-program/SKILL.md` — useful reading even without Claude Code,
   since they cover the exact files that have to stay in sync and the gotchas found
   integrating past handlers.

## Building

Two independent toolchains, and **order matters**:

1. C64-side 6502 assembly builds first (`npm run build:c64`) and generates headers
   consumed by the Teensy build.
2. Teensy firmware builds second, embedding those headers.

Skipping step 1 after a C64-side change means the Teensy build silently uses stale menu
code. Full setup and tool-version details: [Source/BuildInfo.md](Source/BuildInfo.md) and
[docs/Architecture/Build-System.md](docs/Architecture/Build-System.md).

## Testing

- JS-based tests live alongside the code they cover (`*.test.mjs`, `*.test.js`), e.g.
  `Source/Teensy/tests/recovery-flash-source.test.js`, `mpe/tests/startup.test.mjs`,
  `mpe/tools/hex.test.mjs`. Run one with `node --test <path>`.
- Host-side C++ tests under `vm/tests/` and `mpe/tests/` are compiled and run individually
  against a native `g++`; `mpe/tools/verify.mjs` shows the exact compile/run invocation for
  each if you need to reproduce one locally.
- If you changed firmware that affects RAM footprint, rebuild and check the link report
  against the budgets in `Constraints.md` (`npm run build:tr-plus -- --skip-minimal-build
  --skip-combine` prints it). Add `--no-extensions` to measure the configuration
  `Constraints.md` reasons about, and use the same configuration for both halves of a
  before/after comparison; drop it when you need the headroom of the image that ships,
  which carries the extension loader.
- CI runs the tool tests, the bench tests and both firmware builds on every push and
  pull request (`.github/workflows/build.yml`). Run `npm test` and `python3 -m unittest
  discover -s tools/bench` locally first; the bench tests need macOS or Linux, since
  they use `termios` and `pty`.

There's no expectation that every contributor has physical TR/TR+ hardware to test
against. Say so in your PR — hardware-untested changes get flagged and tested before
release rather than rejected outright, but flagging it up front saves a review cycle.

## Documentation and compatibility

- **Update docs in the same PR**, not a follow-up. If your change affects behavior, check
  whether `docs/Architecture/*.md` (especially `Overview.md`, `Constraints.md`,
  `Known-Issues.md`) or a user-facing doc under `docs/` needs updating too — the
  architecture docs are dense/reference-style by design and go stale fast if only the code
  changes.
- **The external protocol is a stable surface other people's projects depend on.**
  [docs/ControlComms.md](docs/ControlComms.md) documents the USB/Ethernet remote command
  protocol, and the cartridge register map it's built on is relied on by downstream
  projects — [TeensyROM-Web](https://github.com/MetalHexx/TeensyROM-Web),
  [TeensyROM-CLI](https://github.com/MetalHexx/TeensyROM-CLI), and
  [c64cast](https://github.com/kfox/c64cast) (which uses TR+'s Remote DMA feature). If your
  change alters register meanings, command formats, or timing those tools rely on, call it
  out explicitly in the PR — a silent break there breaks someone else's project, not just
  TeensyROM.

## Submitting changes

- Fork and branch off `main` (day-to-day integration happens on feature branches like
  `DMA_Timing` or `LittleFS` before merging up, but `main` is the stable base to start
  from).
- Keep PRs scoped to one change — easier to review and to revert if something regresses on
  hardware later.
- Write commit messages that explain *why*, not just what (see `git log` for the house
  style — e.g. "Fix RR38 freeze-bit set logic and remove no-op special-button stubs" rather
  than "update IOH_RetroReplay.c").
- If your change touches the shared cartridge register map, remember
  `Source/C64/MainMenuCRT/source/Menu_Regs.i` and `Source/Teensy/.../Menu_Regs.h` define it
  independently with no build-time check — update both.
- In the PR description, note what you tested it on (hardware model, NTSC/PAL, or "host
  build only, not hardware tested").

## License

TeensyROM is [MIT licensed](LICENSE.md). Contributions must be open source: submit only
your own original work, or work you have the right to relicense, under terms compatible
with MIT. By submitting a contribution, you agree it can be distributed under the same
license.

**Closed-source or binary-only contributions are not accepted** — no proprietary blobs
without source, no code under a license that restricts modification or redistribution,
regardless of the license terms offered around it. If you have something that genuinely
can't ship as open source, raise it directly with Travis (see contact in the
[README](README.md)) before doing any integration work, rather than opening a PR.

## Questions

Open an issue, or ask in the [TeensyROM Discord](https://discord.gg/ubSAb74S5U) — it's
generally faster for back-and-forth than GitHub issue comments.
