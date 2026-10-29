# CLAUDE.md

Guidance for Claude Code working in this repository. Everything here is also
plain advice for a human contributor; [CONTRIBUTING.md](CONTRIBUTING.md) is the
human-facing version of the same rules.

TeensyROM is a Teensy 4.1 cartridge for the C64/C128. One firmware codebase
builds for two boards (TR, Fab 0.2/0.3; TR+, Fab 0.4), as a dual-boot image
(MinimalBoot plus the main firmware, and on TR+ an extension image), and it
embeds 6502 programs from `Source/C64/` that are built first. Start with
[docs/Architecture/Overview.md](docs/Architecture/Overview.md). **Read
[docs/Architecture/Constraints.md](docs/Architecture/Constraints.md) before
changing any firmware**: its rules are the ones the compiler cannot check, and
breaking one shows up as a glitch on one machine, not as a failed build.

## Skills

| Skill | Use it to |
|---|---|
| [`teensyrom-build`](.claude/skills/teensyrom-build/SKILL.md) | build the C64 programs and the firmware, flash a board, work out what needs rebuilding |
| [`add-io-handler`](.claude/skills/add-io-handler/SKILL.md) | add or review a cartridge/peripheral IO handler |
| [`add-menu-program`](.claude/skills/add-menu-program/SKILL.md) | add a program, game, SID or picture to the built-in menu |
| [`ship`](.claude/skills/ship/SKILL.md) | take a change from branch to a ready-for-review PR |
| [`panel-review`](.claude/skills/panel-review/SKILL.md) | the four-lane review, for a large or risky change |

## Gates

Nothing here needs the Teensy toolchain except the firmware builds.

| Gate | When |
|---|---|
| `npm test` | always: tool tests, plus the host harnesses under `Source/Teensy/tests/` |
| `npm run verify:extensions` | a change to the extension loader, `vm/`, or `Source/Teensy/VMBoot/` |
| `python3 -m unittest discover -s tools/bench` | a change under `tools/bench/` (macOS or Linux only) |
| `npm run build:c64` | a change under `Source/C64/` or to `Menu_Regs.h`; commit the regenerated headers |
| `npm run build:tr-plus` and `npm run build:tr` | any change the firmware compiles; show the memory report as the build skill says |

CI runs all of them on every push. A firmware change is not done until both
targets build: `Fab04_Features` compiles TR+-only code in and out, so one
target can break while the other builds.

## The rules that bite hardest

[Constraints.md](docs/Architecture/Constraints.md) has the reasons and the
history; this is the list to hold in your head.

- **Nothing new on the PHI2 path.** `isrPHI2()` and the active handler's
  `ROMLHndlr`/`ROMHHndlr`/`IO1Hndlr`/`IO2Hndlr`/`CycleHndlr` run on every bus
  cycle, about 1 µs. No new branch, no flash (`FLASHMEM` code or EEPROM, reads
  included), no SD, no USB. Slow work goes through the polling handshake.
- **`fBusSnoop` has several owners.** Stage through `PendingfBusSnoop`, and
  never clear it without checking that you still own it.
- **RAM is a hard budget.** RAM1 is shared between ITCM code and DTCM data in
  32 KB banks; one bank crossed costs 32 KB of stack. Measure before and after
  with the same build configuration, and never compare against numbers in a doc.
- **Code lands in ITCM unless it says otherwise.** A thread-mode function
  without `FLASHMEM` costs RAM1, and a helper the compiler used to inline lands
  there once it gains a second caller, is called through a function pointer, or
  is a template or lambda instance. Only the build's memory report shows this,
  never the diff. Mark thread-mode code `FLASHMEM`; on anything the PHI2 ISR
  reaches, `FLASHMEM` is a timing bug, not a saving.
- **Strings and tables go to flash.** New messages use `PSTR()` and new const
  tables `PROGMEM`, unless the ISR reads them; `F()` is only a cast on Teensy 4,
  so its strings stay in DTCM. A buffer nothing uses any more is deleted. DTCM
  grows in 1 KiB and 4 KiB alignment steps, so a few hundred bytes of new
  strings can cost the plain TR 4 KiB of free-for-locals. RAM2 and the rest
  are in CONTRIBUTING.md's "Before you open a PR" checklist.
- **A feature costs nothing while it is off.** Its background work runs from
  its IO handler's `PollingHndlr`, not from `loop()`. A mode that does not need
  a register does not answer at it: other cartridges and programs probe IO1 and
  IO2. The handler's teardown flushes and closes what it opened. A command
  arriving over USB or TCP can land while the C64 is mid-read, so it never
  touches state the C64 is reading, such as `SerialStringBuf` and
  `ptrSerialString`.
- **Every entry path, not just the menu.** A file or feature is reached from
  the TR menu, a remote launch (`RemoteControl.ino`), the host app's directory
  listing (`FileTransfer.ino`), NFC (random pick included), hot keys and
  autolaunch. A change to how something launches covers each of them. Resolve
  everything that can fail (the file, the folder, the image) before changing
  any state, so a failure leaves things as they were.
- **Failure paths tell the truth.** A failed write leaves no partial file
  behind, a failure is never reported as success, and a command that was
  acknowledged never refuses silently afterwards. Error codes match the
  contract their caller reads, such as [ControlComms.md](docs/ControlComms.md)
  for the remote protocol.
- **Emulated chips and patched 6502 code match the real thing.** Check a chip
  register's behavior against VICE's model of that chip, and a patch to ROM or
  C64 code against the actual ROM bytes, not a datasheet summary: the CIA's TOD
  flips AM/PM when its hour is written as 12, and while CRB bit 7 is set a TOD
  write sets the alarm instead of the clock. Leave every register you touched
  in the state the code that runs next expects.
- **The main loop is single-threaded.** A blocking call in a command handler
  stalls ASID playback, NFC and TCP for as long as it blocks.
- **Hand-synced lists.** `enumIOHandlers` (`Menu_Regs.h`) and `IOHandler[]`
  (`IOHandlers.h`) must keep the same order. `Menu_Regs.i` is generated from
  `Menu_Regs.h`; edit only the `.h`.
- **Teensyduino 1.61.0**, not 1.62.0.
- **C128 timing differs.** PAL, NTSC and NTSC C128 each have their own DMA
  constants, and DMA right after a C128's reset release can hang its boot.

## Workflow

Non-trivial changes go through the [`ship`](.claude/skills/ship/SKILL.md)
skill. **Never merge**: hand over a green PR; the merge is the maintainer's.
Every PR opens as a Draft and is marked ready only when nothing is left to do.

**Work in a worktree.** Call `EnterWorktree` with a `name` before the first
edit, so the change lands in `.claude/worktrees/<name>` on its own branch. The
primary checkout may have someone's uncommitted work in it, and a `git pull`
there moves HEAD out from under a session. A worktree has no `tools/.cache`, so
point `ARDUINO_CLI` at the primary checkout's
`tools/.cache/*/arduino-cli*`. On Windows, where the toolchain refuses long paths, build with
`--out` under a short folder such as `C:/tr-build/`.

**Commits** say why, not what, with an area prefix: `firmware:`, `c64:`,
`tools:`, `docs:`, `test:`, `ci:`. One concern per commit.

**Comments** say only what the code cannot: an external constraint (a timing
margin, a register another file must match, a toolchain limit), or why
something that looks wrong is right. A comment that restates the code is a
review finding, and its fix is deleting it. Timing constants keep their
comments on what breaks if they move.

### Findings: fix it, file it, or drop it

These rules bind every session and subagent here: a ship run, a one-off fix, a
reviewer, a review lane. An agent's prompt may narrow its lane; it never
loosens these. A prompt handing work to a subagent says "follow CLAUDE.md →
Findings" rather than paraphrasing them.

1. **A finding is a real defect:** a concrete input or state that produces a
   wrong result, a crash, a hang, a memory overrun, a timing miss or a security
   hole. Something proves it before the fix lands, in this order of preference:
   a host test that fails (`Source/Teensy/tests/` harnesses, `vm/tests/`,
   `tools/**/*.test.mjs`); a build whose report or guard shows it; a hardware
   reproduction with the board, machine, video standard, commit and steps
   written down. Style, refactors and prose as writing are not findings.
2. **Fix it — the default.** Grep for the pattern you changed and fix every
   sibling site in the same commit. Look in particular at the full-firmware and
   MinimalBoot copies of same-named functions, and at the TR and TR+ paths
   (`Fab04_Features`). Another instance of the class the change fixes is fixed
   in this change as its own commit, not filed. "Small" is a reason to fix it,
   never to file it. A defect outside that class stays out of this change: file
   it under rule 4 or drop it under rule 5.
3. **A guard ships only with the input that reaches it.** A bounds check,
   clamp, timeout, retry, fallback or `NULL` test names the caller at
   `file:line`, the value, and the wrong result without it. That value comes
   from an **observed failure**, an **external contract** (a CRT, D64, SID or
   PRG file, a USB device, a C64-side program, a host app speaking the
   [remote protocol](docs/ControlComms.md)) or a **trust boundary** (the
   network, an SD card, an NFC tag). A value this code produced is not an
   input. On the firmware a guard is not free: it costs RAM1, and on the PHI2
   path it costs nanoseconds, so one with no input is a defect, not caution.
   Validate once, where the value enters.
4. **File it** only when it cannot be fixed here *and* it names the caller at
   `file:line`, the input that reaches it, and the wrong result. "Cannot be
   fixed here" means it is outside the class this change fixes, needs a design
   decision that is the maintainer's, needs hardware nobody has authorized,
   touches files another open PR is changing, or would make this change
   materially bigger or riskier. One issue per item, linked to the PR, never a
   list in a PR description. A deferred finding with a designed fix may go in
   [Known-Issues.md](docs/Architecture/Known-Issues.md) instead.
5. **Drop it — everything else.** Suspicions no input reaches, latent traps,
   guards for failures never seen, design opinions, behavior that matches its
   docs. Each gets one line under "Declined" in the report to the user; none is
   committed or filed.

### Reviews

One review per change, at an effort its size and risk call for. Count changed
code lines with `git diff --numstat <base> <tip> -- ':!*.md'`; don't estimate.

| Change | Review |
|---|---|
| Prose-only: `git diff <base> <tip> -- ':!*.md'` is empty | none |
| Trivial: ≤10 changed code lines, mechanical (rename, typo, dictated line), no pinned path | none |
| ≤50 changed code lines, a merge or conflict resolution, or a reviewer's fix commit | `low` |
| Anything else | `medium` |
| Touches a pinned path (below) | `high` |
| 50+ changed code lines that cross a trust boundary, change a protocol or persisted layout, or change timing on a pinned path; or an unsplittable 400+ | optionally `/panel-review` *instead of* the reviewer at `high` |

Reviews run through the agents in [.claude/agents/](.claude/agents/): the
`reviewer` for every change, and the four lanes (`auditor`, `adversary`,
`steward`, `pragmatist`) that `/panel-review` runs when a panel is chosen. A
reviewer fixes what it finds as its own commit; that commit gets one `low`
review, and the chain ends there.

**Pinned paths.** A change touching any of these is reviewed at `high` whatever
its size, because a one-line change there does the most damage:

- **The PHI2 path:** `Source/Teensy/MinimalBoot/Common/ISRs.c`, and the
  cycle-called handlers in every `IO_Handlers/IOH_*.c`.
- **Bus timing and DMA:** `Common_Defs.h` (the `nS_*` constants, `fBusSnoop`),
  `DMAControl.c`, `MachineDetect.h`.
- **Shared and persisted layouts:** `Menu_Regs.h` (the register map the C64
  side and downstream apps read), the `eepAd*` EEPROM map in `Common_Defs.h`
  (settings survive a flash, so a moved field misreads on upgrade and
  downgrade), and the `enumIOHandlers` numbering stored in those settings.
- **Self-flashing and boot:** `FlashUpdate.ino`, `Flash/`, `StartupHooks.c`,
  `tools/BootLinkerFiles/`, and the image layout in `tools/lib/hex.mjs`. A
  defect here can leave a board that needs the program button to recover.
- **The remote protocol:** `SerUSBIO.ino`, `Min_SerUSBIO.ino`, `ServiceTCP.ino`,
  `Min_ServiceTCP.ino`, `RemoteControl.ino`. It is a network-facing trust
  boundary, and TeensyROM-Web, TeensyROM-CLI and c64cast depend on its format.
- **`.github/workflows/`:** the jobs that decide whether a PR may merge and
  that build release images.

### Hardware

The tests and the host harnesses cover logic; timing, DMA and what the C64
shows need a board. **Flashing or driving a board needs the person's go-ahead
in this session**: say which board gets which image and wait. Once a run has
that authorization:

- Use the committed tools: `npm run flash` (it identifies every board before
  writing), and the bench scripts in [tools/bench/](tools/bench/README.md) for
  reset, launch, peek, poke and screen reads. Improve them rather than writing
  throwaway scripts.
- Never write a port name, host, IP address or serial number into a committed
  file; `$TR_PORT` and the prompt carry them.
- Say what the run covered: board (TR or TR+), machine (C64, C128, Ultimate),
  PAL or NTSC, image (main, MinimalBoot, extension), and commit. A test on one
  axis says nothing about the others.
- A check that needs someone's eyes or ears goes back as an open question.

Without that go-ahead, a claim only hardware can settle goes back as an open
question, and the PR says "host build only, not hardware tested".

## Public text

PR descriptions, issues, commit messages and docs are public. They never carry
a contributor's private notes, local paths, machine names, todo-list numbers
or unpublished investigations. Cite a commit, a PR or a doc section instead,
or state the fact plainly, and reread the text for each of these before it
goes out.

## Spelling

American English in code, comments, identifiers, docs and commit messages:
`color`, `behavior`, `initialize`, `recognize`. A British spelling in this tree
is an import, not house style; vendored libraries under `Source/Teensy/src/`
keep their own.
