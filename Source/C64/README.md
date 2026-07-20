# TeensyROM C64 Source

C64-side (6502 assembly) programs for the TeensyROM cartridge. Builds are Windows batch based.

## Tool Path Setup (`SetToolPaths.bat`)

All build scripts call `SetToolPaths.bat` to locate the required tools. Edit the paths in this file to match your machine before building:

| Variable | Purpose |
|---|---|
| `PythonExe` | Python launcher, used to run `bin2header.py` |
| `compilerPath` | Path to the ACME cross-assembler (`acme.exe`) |
| `JavaExe` / `KickAssemblerJar` | Java runtime and KickAssembler (used by TRCustomBasicCommands) |
| `emulatorPath` | VICE emulator `bin` directory, for optional test launches |
| `bin2headerPy` / `bin2headerROMPath` | Local script and destination for generated C headers (`..\..\Teensy\TRMenuFiles\ROMs`) |

## Build All (`BuildAllC64.bat`)

Runs each sub-project's build script in sequence, stopping on the first failure. Core programs/dependencies build first (MainMenuCRT, SettingsMenu, TRHelpScreens, TRExtPortCheck, ExpansionPortTest, ASIDPlayer, MIDI2SID), followed by the other programs. Outputs are converted with `bin2header.py` and copied into the Teensy firmware tree for inclusion in the ROM image.

## Prerequisites

Referenced versions from `SetToolPaths.bat` (newer versions will likely work):

- **ACME cross-assembler** 0.97 (Windows build)
- **Python 3** (for `bin2header.py`; Python 2 is rejected by the script)
- **KickAssembler** + **Java JRE 1.8** (TRCustomBasicCommands only)
- **VICE** 3.9 (GTK3, win64) — optional, for emulator test launches

## bin2header.py

A Python 3 utility (bin2header v0.3.1, MIT licensed) that converts a compiled binary into a C header containing a `static const unsigned char` array. Build scripts run it on each `.prg`/`.bin` output so the program can be embedded directly in the Teensy firmware. Most builds pass `-t "PROGMEM "` to place the array in Teensy flash memory; the MainMenuCRT cartridge image is a deliberate exception — it omits PROGMEM because it must reside in RAM for ROM emulation.

## Build Outputs

Each sub-project compiles into its local `build\` directory (cleaned at the start of every build). The generated `.h` file is then copied to `..\Teensy\TRMenuFiles\ROMs\` (set via `bin2headerROMPath`), where the Teensy firmware picks it up. After running a build, verify the header begins with `PROGMEM static const unsigned char ..._prg[] = {` (except the MainMenuCRT cartridge header, which intentionally has no `PROGMEM`).

## Sub-Projects

| Directory | Build Script | Description |
|---|---|---|
| `MainMenuCRT` | `build8000CartBin.bat` | Main TeensyROM menu; built as a headerless cartridge ROM binary at $8000 |
| `SettingsMenu` | `buildSettingsMenu.bat` | TeensyROM settings/configuration menu |
| `TRHelpScreens` | `buildTRHelpScreens.bat` | Help screens displayed from the TeensyROM menu |
| `TRExtPortCheck` | `buildTRExtPortCheck.bat` | External port check utility |
| `ExpansionPortTest` | `buildExpansionPortTest.bat` | Expansion (cartridge) port test utility (TR+ only) |
| `ASIDPlayer` | `buildASIDPlayer.bat` | ASID (MIDI SID) player application |
| `MIDI2SID` | `buildMIDI2SID.bat` | MIDI-to-SID synthesizer application |
| `SimpSwiftTerm` | `buildSimpSwiftTerm.bat` | Simple SwiftLink terminal program |
| `TODCheck` | `buildTODCheck.bat` | CIA Time-of-Day clock check utility |
| `TRCustomBasicCommands` | `buildTRCustomBasicCommands.bat` | Custom TR BASIC command extensions (built with KickAssembler) |
| `BASIC` | `bin2header.bat` | Standalone BASIC `.prg` utilities, converted individually to headers |
| `v1541Wrapper` | `buildv1541Wrapper.bat` | Virtual 1541 wrapper (not currently used in the full build) |
