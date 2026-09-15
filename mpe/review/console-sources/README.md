# Corresponding source for the console runtime handoff

The MPE public release downloads contain runtime packages. This developer
handoff supplies the complete matching GBVM/GGVM engine, adapter, C64 client,
build scripts and component source for those packages. Existing component
licenses apply. No private Prism+ implementation or game ROMs are included.

| Snapshot | SHA-256 |
| --- | --- |
| [GBVM 1.2.24](GBVM-1.2.24-source.zip) | `82f6b16ad01abe2b06fea596c76f3e5439bae8ec5b92bcd218e351c29c846dfb` |
| [GGVM 1.2.24](GGVM-1.2.24-source.zip) | `e46222d4a6c9a598cf81ebd96651edcf85093234d1c198f74fa972e1ce57089e` |

Extract the chosen archive. From its root, use Node.js 24 or later, Windows
PowerShell and GNU Arm Embedded 11.3.1:

```powershell
$env:MPE_ARM_PREFIX='C:/path/to/arm-none-eabi-'
node scripts/package-gbvm.mjs build/rebuilt
```

For GGVM, substitute `scripts/package-ggvm.mjs`. Use a fresh output directory.
The resulting `build/rebuilt/SD/` contains the matching engine and client.
The snapshots' package documentation predates the final download-location
clarification; compiled engine and client inputs are unchanged.

Fresh extraction and complete packaging rebuilt both engines and clients
byte for byte. Expected SHA-256 values:

| Component | SHA-256 |
| --- | --- |
| GB engine.mvm | `03962ee20dac03a42032e67f6d11f67427d3a8093f6ef8b59be7cd0c3af3ae18` |
| GB client.crt | `d2aa4c504568292fe4a5e05a440d4ad1ac6cab3eec2ef4e10c22e06c84ca9a0b` |
| GG engine.mvm | `1396ee6134d981548ff6588ee8050d0b973e545a77bb559d96e8e83603c57e18` |
| GG client.crt | `438bfcba7e0d04ad201a9d5d3e5a576602aca4d8c62af0dcfa61fec473a49f18` |

NESVM 1.2.1's corresponding engine source remains in the
[existing public NES engine tree](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine/tree/main/Source/NESEngine).
DoomVM 1.2.1's unchanged corresponding source remains in the
[Doom engine tree](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine/tree/main/Source/DoomEngine).
The text firmware's own host library and corresponding library materials
are separate from these independently loaded console modules.
