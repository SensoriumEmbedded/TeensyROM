# C64-Side Software

6502/6510 assembly programs that run ON the Commodore 64/128 itself — the on-screen menu, settings UI, and bundled utilities. Distinct from the Teensy firmware in [Teensy-Firmware.md](Teensy-Firmware.md), but the two are built in sequence — see [Overview.md](Overview.md#the-two-toolchains-and-how-they-connect).

Canonical build reference: [Source/C64/README.md](/Source/C64/README.md) (verified accurate — prefer it over this doc for exact tool versions/paths).

## Layout

`Source/C64/` has one directory per sub-project, each `<Project>/source/` (sources) + `<Project>/build/` (output, cleaned each build) + a `build*.bat` at the project root. `SetToolPaths.bat` centralizes tool locations for every build script; `BuildAllC64.bat` runs all sub-projects in dependency order and halts on first error.

Toolchain: **ACME cross-assembler 0.97** for all `.asm`/`.s` files, except **TRCustomBasicCommands**, which uses **KickAssembler** + Java. `bin2header.py` (Python 3) converts each compiled `.prg`/`.bin` into a C header and copies it into `Source/Teensy/TRMenuFiles/ROMs/` for the firmware build.

| Sub-project | Build script | What it is |
|---|---|---|
| `MainMenuCRT` | `build8000CartBin.bat` | The cartridge boot menu itself — see below |
| `SettingsMenu` | `buildSettingsMenu.bat` | F8-triggered 9-page settings/config menu — see below |
| `TRHelpScreens` | `buildTRHelpScreens.bat` | In-menu help screens |
| `TRExtPortCheck` | `buildTRExtPortCheck.bat` | External port check utility |
| `ExpansionPortTest` | `buildExpansionPortTest.bat` | Expansion port test (TR+ only) |
| `ASIDPlayer` | `buildASIDPlayer.bat` | ASID (MIDI SID) player app |
| `MIDI2SID` | `buildMIDI2SID.bat` | MIDI-to-SID synth app (IO1 register block reuses raw SID register offsets — `Menu_Regs.i:78-108` flags this as needing to stay in sync or be split out) |
| `SimpSwiftTerm` | `buildSimpSwiftTerm.bat` | Simple SwiftLink terminal program |
| `TODCheck` | `buildTODCheck.bat` | CIA Time-of-Day clock check utility |
| `TRCustomBasicCommands` | `buildTRCustomBasicCommands.bat` | Custom BASIC commands — vendored, see below |
| `BASIC` | `bin2header.bat` | Grab-bag of pre-built standalone `.prg` utilities, individually header-converted |
| `v1541Wrapper` | `buildv1541Wrapper.bat` | Virtual 1541 wrapper — **deferred/planned feature**, not currently wired into the full build; waiting on further developer capability before inclusion. Also intended as a general-purpose wrapper for assembly-code PRGs. |

## MainMenuCRT — the cartridge boot menu

Files in `MainMenuCRT/source/`: `TeensyROMC64.asm` (171 lines), `MainMenu.asm` (1432 lines), `PRGLoadStartReloc.s` (140), `SIDRelated.s` (522), `StringFunctions.s`, `StringsMsgs.s`, `CommonDefs.i`, `Menu_Regs.i` (register map, see [Comms-Protocol.md](Comms-Protocol.md)), `c64defs.i`.

`Menu_Regs.i` physically lives here but is shared well beyond MainMenuCRT — it's `!src`-included by relative path from 6 sub-projects' top-level `.asm` files: both `MainMenuCRT` sources (`TeensyROMC64.asm`, `MainMenu.asm`), `SettingsMenu`, `TRHelpScreens`, `TRExtPortCheck`, `MIDI2SID`, and `ExpansionPortTest`. It's one physical file (not duplicated per sub-project), so any future fix to the [Menu_Regs sync problem](Known-Issues.md) only needs to regenerate this one file, not touch the 6 consumers. `ASIDPlayer.asm:7` has the same include line present but **commented out** — unclear if intentional (ASID doesn't need those registers) or a leftover; worth a quick check before relying on that assumption.

- **`TeensyROMC64.asm`** is the actual 8K cartridge ROM image (`* = $8000`, `Coldstart`/`Warmstart` vectors, `CBM8O` autostart key). Does minimal hardware init (VIC/CIA/SID reset), prints the banner, then copies the separately-built `MainMenu.bin` (`!binary`-included) from cart ROM into C64 RAM at `MainCodeRAMStart` (`$6000`, per `CommonDefs.i`) and jumps there.
- **`MainMenu.asm`** is the menu program proper, running from RAM: file browser/menu UI (`ListMenuItems`, `SelectItem`, `RunSelected`, `XferCopyRun`), cursor/page navigation, keyboard handling, NFC tag writing, RTC/time display.
- **`PRGLoadStartReloc.s`**: relocated into the cassette-buffer zero page (`$033c`, per `CommonDefs.i`) and executed from there while a `.PRG` streams in. Polls `rRegStrAvailable`/`rRegStreamData` (IO1 registers) to pull bytes from the Teensy, sets BASIC's end-of-program/variables pointers, signals the Teensy via `wRegControl = rCtlRunningPRG`, then re-enters BASIC warm-start (`jmp $a7ae`). Comments document a hardcoded startup delay to avoid a race condition, and address wrap-around handling during load.
- Build order: `build8000CartBin.bat` compiles `MainMenu.asm` to `MainMenu.bin` first, then `TeensyROMC64.asm` (which embeds that binary) to produce the final headerless cartridge image — the one C64 build output that intentionally has **no PROGMEM header** (must land in RAM for ROM emulation, per `Source/C64/README.md`).

## SettingsMenu — the 9-page settings framework

`SettingsMenu.asm` (82 lines) is the driver: defines `NumPages = 9`, a jump table `tblSettingsPages`, and `!src`-includes the 9 page files in order. Each page is its own `.asm` with a `<Name>Menu:` entry label, its own init/key-wait loop, and message text. `_SettingsPageTemplate.asm` is the copy-paste starting point for a new page (currently still has leftover "Ethernet" naming from whatever page it was cloned from — a trap for anyone copying it without cleaning up). Shared logic (`CommonInit`, `CheckCommonKeys` — the F8→number page-dispatch — `DisplayTime`, `GetIn`, string helpers) lives once in `SupportFunctions.asm` and `StringFunctions.asm`, used by every page.

Page files (index → content, per the FW 0.8 layout): `Pg_Index.asm`, `Pg_TRSettings.asm` (TeensyROM General), `Pg_StartupOptions.asm`, `Pg_ColorConfig.asm` (Menu Colors), `Pg_MIDISettings.asm` (MIDI Message Filters), `Pg_EthernetSettings.asm`, `Pg_TimeRTCSettings.asm`, `Pg_InfoOther.asm` (Info: General), `Pg_InfoHotKey.asm` (Info: HotKeys).

**When adding/moving a settings page, or documenting Settings Menu key sequences, verify the current page-index and in-page key bindings against these files directly** — this menu was rewritten once already (single-screen → 9-page) and stale key references in docs have been a recurring problem.

## TRCustomBasicCommands — vendored third-party

`Source/C64/TRCustomBasicCommands/source/README.md` is unmodified upstream documentation crediting **Barry Walker (2023)**, MIT licensed, built with KickAssembler at `$c000` — treat that subtree's generic framework code (`main.asm`, `memory.asm`, `sprites.asm`, `include/`) as vendored, not TeensyROM-authored. Adds custom BASIC commands/functions (`BACKGROUND`, `BORDER`, `WOKE`, `MEMCOPY`, `STASH`/`FETCH` for REU, `MEMLOAD`/`MEMSAVE`, sprite commands, `DIR`). TeensyROM-specific additions are layered in separate files alongside the vendored skeleton: `teensyrom.asm` and `reu.asm` — this is where `TISET` and any other TR-specific BASIC command lives.

## Build pipeline (per sub-project)

Each `build*.bat` sources `SetToolPaths.bat`, cleans its local `build/`, invokes `acme.exe` (`-r <BuildReport> --vicelabels <Symbols> --msvc --color --format plain -v3 --outfile`), then runs `bin2header.py` to emit a header and copy it into `Source/Teensy/TRMenuFiles/ROMs/`. Full details, prerequisites, and tool versions: [Source/BuildInfo.md](/Source/BuildInfo.md) and [Source/C64/README.md](/Source/C64/README.md).

<br>

[Back to Architecture Overview](Overview.md)
