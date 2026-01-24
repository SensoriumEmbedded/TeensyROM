# TeensyROM Dual-Boot Build Script

Automates building the dual-boot TeensyROM firmware (`TeensyROM_full.hex`) containing both MinimalBoot and TeensyROM images.

## Prerequisites

- **Arduino IDE** with Teensyduino installed
- **Windows PowerShell** (Windows 10/11)
- **Teensy 4.1** board

## What It Does

1. **Downloads arduino-cli v1.4.1** (if not in PATH) to `tools\arduino-cli.exe`
   - Version is pinned for supply-chain security
   - SHA256 checksum is verified before use
   - Update version intentionally through pull request review
2. **Builds TeensyROM** (upper memory, USB: Serial+MIDI)
3. **Builds MinimalBoot** (lower memory, USB: Serial)
4. **Combines** both into single hex file

## Default Usage
Builds both images and combines them.

```powershell
.\Build-DualBoot.ps1
```
## Parameters

```powershell
.\Build-DualBoot.ps1 [-SkipTeensyBuild] [-SkipMinimalBuild] [-SkipCombine]
```

| Parameter | Purpose |
|-----------|---------|
| `-SkipTeensyBuild` | Skip TeensyROM build |
| `-SkipMinimalBuild` | Skip MinimalBoot build |
| `-SkipCombine` | Skip combining hex files |

## Output

**Final firmware:** `tools\build\TeensyROM_full.hex`

**Intermediate files:**
- `build\Teensy.ino.hex` - TeensyROM image
- `MinimalBoot\build\MinimalBoot.ino.hex` - MinimalBoot image

## Flashing

The script does **NOT** flash automatically. Use Teensy Loader:

1. Open Teensy Loader
2. Drag `TeensyROM_full.hex` onto it
3. Press button on Teensy 4.1

## Security

**Arduino CLI Version Pinning:**
- The script uses arduino-cli **v1.4.1** with SHA256 verification
- This prevents supply-chain attacks from compromised downloads
- Version updates require intentional review and checksum verification

**To update arduino-cli version:**
1. Check latest release: https://github.com/arduino/arduino-cli/releases
2. Download checksums: `https://github.com/arduino/arduino-cli/releases/download/vX.Y.Z/X.Y.Z-checksums.txt`
3. Find SHA256 for `arduino-cli_X.Y.Z_Windows_64bit.zip`
4. Update `$ArduinoCliVersion` and `$ExpectedSHA256` in `Build-DualBoot.ps1`
5. Test the build thoroughly before committing