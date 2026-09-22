# TeensyROM C64 Source

C64-side (6502 assembly) programs for the TeensyROM cartridge. One Node command builds all of them.

## Building

```
npm run build:c64
```

That regenerates `Menu_Regs.i`, assembles every sub-project, and writes the results as C headers into `Source/Teensy/TRMenuFiles/ROMs/`, where the Teensy firmware build picks them up. It works the same on Windows, macOS and Linux, and it never asks you to edit a tracked file.

Useful options (after `--` when going through npm):

| Option | Effect |
|---|---|
| `--project SettingsMenu,TODCheck` | Build only those projects (names are case-insensitive) |
| `--list` | List the projects and which assembler each needs |
| `--verbose` | Show the assemblers' full output instead of a one-line summary per step |
| `--rom-dir <dir>` | Write the headers there instead of the firmware tree, for example to compare against what is committed |

The build stops at the first failure and names the project and file. Each project builds in its own `build/` directory, which is emptied first and also holds the assembly report and VICE label file (`Labels`; `MainSymbols` and `CartSymbols` for MainMenuCRT).

**Rebuild the C64 side after any change to a `.asm`, `.s` or `.i` file, before building the Teensy firmware,** or the firmware embeds the old code. CI catches it for every project but `TRCustomBasicCommands`, which KickAssembler builds and CI installs no JRE for: `tools/build-c64.test.mjs` reassembles the rest and byte-compares against what is committed.

## Prerequisites

Node (see `.nvmrc`), and nothing else for most work. The assemblers are found like this, in order:

| Tool | Used by | Environment override | Otherwise |
|---|---|---|---|
| **ACME** 0.97 | every project except TRCustomBasicCommands | `ACME` (path to the executable) | `acme` on `PATH`, then a checksummed download on Windows and macOS. On Linux, install it (`apt install acme`). |
| **KickAssembler** 5.25 | TRCustomBasicCommands only | `KICKASS_JAR` (path to `KickAss.jar`) | A checksummed download |
| **Java** 8 or newer | KickAssembler | `JAVA_HOME` | `java` on `PATH`. Never downloaded. |

Downloads go to `tools/.cache/` (ignored by git) and are checked against a pinned SHA-256; a mismatch is an error. A tool is only looked for when a project you are building needs it, so building an ACME program never asks for Java. The macOS ACME download is an x86_64 binary, so Apple silicon runs it under Rosetta; a Homebrew `acme` on `PATH` is used first if you have one.

Optional: **VICE** (`x64sc`), if you want to try a `.prg` in an emulator. Only some features work without the cartridge hardware.

## The project list

What gets built, and how, is `tools/c64-projects.json`; the format is described at the top of `tools/lib/c64-projects.mjs`. To add a program, add an entry there.

| Directory | Description |
|---|---|
| `MainMenuCRT` | Main TeensyROM menu, built as a headerless cartridge ROM binary at $8000 |
| `SettingsMenu` | TeensyROM settings/configuration menu |
| `TRHelpScreens` | Help screens displayed from the TeensyROM menu |
| `TRExtPortCheck` | External port check utility |
| `ExpansionPortTest` | Expansion (cartridge) port test utility (TR+ only) |
| `ASIDPlayer` | ASID (MIDI SID) player application |
| `MIDI2SID` | MIDI-to-SID synthesizer application |
| `SimpSwiftTerm` | Simple SwiftLink terminal program |
| `TODCheck` | CIA Time-of-Day clock check utility |
| `TRCustomBasicCommands` | Custom TR BASIC command extensions (built with KickAssembler) |
| `v1541Wrapper` | Virtual 1541 wrapper (not currently used by the firmware; its header is still kept current) |
| `BASIC` | Standalone BASIC `.prg` utilities: nothing to assemble, each is converted to a header |

## Headers

Each program is written as a C header holding a `static const unsigned char` array (the format of the [bin2header](https://github.com/AntumDeluge/bin2header) utility, which `tools/lib/bin2header.mjs` ports). Most are declared `PROGMEM` so they stay in Teensy flash; the MainMenuCRT cartridge image is a deliberate exception, because it must sit in RAM for ROM emulation.

To add a program you didn't assemble here (a third-party `.prg`, `.crt`, `.sid`...), convert it with the command-line wrapper:

```
node tools/bin2header.mjs -t PROGMEM "path/to/file.prg"
```

Options: `-n <array name>` (use it when the file name starts with a digit), `-o <output file>`. See `tools/bin2header.mjs`.

## Menu_Regs.i is generated

`MainMenuCRT/source/Menu_Regs.i` is generated from `Source/Teensy/MinimalBoot/Common/Menu_Regs.h` at the start of every build, by `tools/lib/menu-regs.mjs`. Edit `Menu_Regs.h`, never `Menu_Regs.i`: a change to the `.i` file is overwritten on the next build.
