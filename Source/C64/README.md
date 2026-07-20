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

## Sub-Projects

| Directory | Build Script | Description |
|---|---|---|
| `MainMenuCRT` | `build8000CartBin.bat` | Main TeensyROM menu; built as a headerless cartridge ROM binary at $8000 |
| `SettingsMenu` | `buildSettingsMenu.bat` | TeensyROM settings/configuration menu |
| `TRHelpScreens` | `buildTRHelpScreens.bat` | Help screens displayed from the TeensyROM menu |
| `TRExtPortCheck` | `buildTRExtPortCheck.bat` | External port check utility |
| `ExpansionPortTest` | `buildExpansionPortTest.bat` | Expansion (cartridge) port test utility |
| `ASIDPlayer` | `buildASIDPlayer.bat` | ASID (MIDI SID) player application |
| `MIDI2SID` | `buildMIDI2SID.bat` | MIDI-to-SID synthesizer application |
| `SimpSwiftTerm` | `buildSimpSwiftTerm.bat` | Simple SwiftLink terminal program |
| `TODCheck` | `buildTODCheck.bat` | CIA Time-of-Day clock check utility |
| `TRCustomBasicCommands` | `buildTRCustomBasicCommands.bat` | Custom BASIC command extensions (built with KickAssembler) |
| `BASIC` | `bin2header.bat` | Standalone BASIC `.prg` utilities, converted individually to headers |
| `v1541Wrapper` | `buildv1541Wrapper.bat` | Virtual 1541 wrapper (not currently used in the full build) |
