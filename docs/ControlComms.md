# TeensyROM Serial/USB Command Protocol
 
## Overview

This document describes the **command protocol** used to remotely control a TeensyROM (or TeensyROM+) unit from an external host — a PC, single-board computer, or microcontroller — over USB Serial or Ethernet TCP. The protocol is a binary, token-based request/response scheme: each command begins with a 16-bit token identifying the operation, followed by any required parameter bytes, and the TR replies with an `AckToken`/`FailToken` and any associated response data. It covers everything from basic actions (launching files, resetting the C64, checking firmware version) to more advanced operations such as direct C64 memory read/write via DMA (TR+ only), SID playback control, file transfer (post/get/copy/delete), and directory listing.

The sections below detail the physical/transport connection options, advanced behavior (firmware modes, timing, and robustness considerations), and the full token and command reference.

## Connection options/details

### Ethernet TCP
  * TR Setup: Set "Enable TCP Listener" to "On"
  * Ethernet connection to port 2112 of TR IP address.
    * IP address can be set static or DHCP, see Ethernet documentation.
  * TCP connection can be persistent or re-established for each transaction.

### USB Serial
  * Connect Host computer USB-A to TR USB-B micro.
    * No additional TR Setup required
    * Host USB Serial connection can be made at any speed as the TR device port uses true USB communication rates (480 Mbps). Most projects set the host to 2000000 baud, 8/N/1.
  * Optionally, microcontroller USB device control via connection to TR USB-A host port.
    * TR Setup: "USB Host Serial Dev" set to "TRCont"
    * USB Device Serial connection can be made to TR Host at 115200 baud, 8N1.

### General
  * Command tokens/fields are sent in big-endian to TR.
  * Ack/replies from TR are sent little-endian.
  * Excessive USB/Ethernet traffic can interfere with emulation, so only specific commands are always available. When other commands try to communicate during these operations, a "Busy!" message is sent back. Issuing a reset will return the TR/C64 to its main menu and allow all available commands.

The protocol is also currently implemented in several control projects, for reference:
|Project name/Link|Developer|Description|
|:--|:--:|:--|
|**[TeensyROM-UI](https://github.com/MetalHexx/TeensyROM-UI)** |[MetalHexx](https://github.com/MetalHexx)|Windows based UI with media exploration, uses all available commands|
|**[TeensyROM-Web](https://github.com/MetalHexx/TeensyROM-Web)**  |[MetalHexx](https://github.com/MetalHexx)|Cross-platform web based version with stand-alone API server|
|**[TeensyROM-CLI](https://github.com/MetalHexx/TeensyROM-CLI)** |[MetalHexx](https://github.com/MetalHexx)|Cross-platform CLI version|
|**[c64cast](https://github.com/kfox/c64cast/)**  |[Kelly Fox](https://github.com/kfox) |Python project to stream video/audio to the C64 via the TR+|
|**[TRWinApp](https://github.com/SensoriumEmbedded/TRWinApp)** |[SensoriumEmbedded](https://github.com/SensoriumEmbedded)|Very basic command implementation, used for test/development|
|**[TeensyROMControl](https://github.com/SensoriumEmbedded/TeensyROMControl)** |[SensoriumEmbedded](https://github.com/SensoriumEmbedded)|Library for TeensyROM control from a microcontroller|

---

## Advanced Connection Details

### Command Availability During Operations

The TeensyROM divides commands into two availability tiers:

#### Always-Available Commands
These commands are processed regardless of what handler is active, even if the TeensyROM is "busy":
  * Core control (LaunchFile, ResetC64, VersionInfo, FWCheck)
  * DMA memory access (WriteC64Mem, ReadC64Mem) — TR+ only
  * C64 pause control (C64PauseOn, C64PauseOff)

#### Conditionally-Available Commands
These commands return `FailToken` (`0x9B7F`) followed by `"Busy!\n"` if the TeensyROM is not idle:
  * All file operations (PostFile/SendFile, GetFile, GetDirectory, CopyFile, DeleteFile)
  * All SID-related commands (PauseSID, SetSIDSong, SetSIDSpeed*, SetSIDVoiceMute)
  * Other menu/UI commands (SetColor, Ping, Debug)

---

### Minimal vs. Full Firmware Modes

TeensyROM uses a dual-firmware system for large cartridge support:

#### Minimal Firmware Boot
  * **Purpose**: Lightweight loader for large CRT files; reduces memory footprint to maximize cartridge size
  * **Restricted Command Set**: Only these commands are available in minimal mode:
    * `ResetC64Token` (Includes Reset to Full FW)
    * `LaunchFileToken` (Includes Reset to Full FW)
    * `VersionInfoToken`
    * `FWCheckToken` (responds with `FWMinimalToken` `0x64E1`)
  * **All other commands**: Return `FailToken` + `"Busy!\n"`
  * **Trigger**: Automatic when launching large CRTs (if FW mode switch is needed)
  * **Detection**: Query firmware type via `FWCheckToken` (`0x64E0`):
    * Response `0x64E1` = Minimal firmware
    * Response `0x64E2` = Full firmware

#### Full Firmware
  * **All commands available** (when not busy with active handler)
  * **Larger memory footprint** limits maximum cartridge size (~740–800 KB before switch to Minimal FW)

---
 
## Tokens

### Command Tokens
|Token Name|2 byte value|Description|
|:--|:--|:--|
|*ResetC64Token     | 0x64EE  | Reset C64 |
|*LaunchFileToken   | 0x6444  | Launch/execute a file on C64 |
|*VersionInfoToken  | 0x6476  | Get firmware version and build info |
|*FWCheckToken      | 0x64E0  | Query firmware type |
|PingToken         | 0x6455  | Check device connectivity and status |
|SendFileToken     | 0x64AA  | File transfer (PC → TR) - legacy, now identical to PostFileToken |
|PostFileToken     | 0x64BB  | File transfer (PC → TR) - v2 |
|GetFileToken      | 0x64B0  | Retrieve file from TR storage |
|CopyFileToken     | 0x64FF  | Copy file between directories |
|DeleteFileToken   | 0x64CF  | Delete file from storage device |
|GetDirectoryToken | 0x64DD  | List directory contents (JSON format - deprecated) |
|GetDirNDJSONToken | 0x64DE  | List directory contents (NDJSON format) |
|SetColorToken     | 0x6422  | Set a TR UI color value |
|PauseSIDToken     | 0x6466  | Pause SID playback |
|SetSIDSongToken   | 0x6488  | Set sub-song number of currently loaded SID |
|SIDSpeedLinToken  | 0x6499  | Set SID playback speed (linear equation) |
|SIDSpeedLogToken  | 0x649A  | Set SID playback speed (logarithmic equation) |
|SIDVoiceMuteToken | 0x6433  | Set individual SID voice muting |
|*C64PauseOnToken   | 0x6431  | Pause C64 via DMA |
|*C64PauseOffToken  | 0x6430  | Resume C64 (un-pause) |
|*WriteC64MemToken  | 0x64FB  | Write sequential C64 memory segment via DMA (TR+ Only)|
|*ReadC64MemToken   | 0x64FD  | Read sequential C64 memory segment via DMA (TR+ Only)|
|DebugToken        | 0x6467  | Internal debug use only |

*- Always available with full FW (others return "Busy!" if not ready for commands)

### Response Tokens
|Token Name|2 byte value|Description|
|:--|:--|:--|
|AckToken          | 0x64CC  | Acknowledgment response |
|FailToken         | 0x9B7F  | Failure response |
|RetryToken        | 0x9B7E  | Retry command (reserved — not currently sent by any implemented workflow) |
|BadSIDToken       | 0x9B80  | SID header parsing failed (sent asynchronously during file load) |
|GoodSIDToken      | 0x9B81  | SID header parsed successfully (sent asynchronously during file load) |
|FWMinimalToken    | 0x64E1  | Minimal firmware response |
|FWFullToken       | 0x64E2  | Full firmware response |
|StartDirectoryListToken | 0x5A5A | Directory listing start marker |
|EndDirectoryListToken   | 0xA5A5 | Directory listing end marker |

## Command descriptions
 
Workflow tables are written from the TeensyROM's point of view: "Receive" means the TR receives data from the host, and "Send" means the TR sends data back to the host.

---

### Reset C64
Resets the C64/128, returning it to the TeensyROM main menu (and full FW if in minimal). All commands become available again after a reset, regardless of prior busy state.

**Workflow:**
| Direction | Data |
|---|---|
| Receive | `ResetC64Token` — `0x64EE` |
| Send | `"Reset cmd received\n"` |

**Handler:** Device resets the C64 and responds with a confirmation message; no separate `AckToken` response.

---

### Launch File
Launches/executes a file (PRG, CRT, SID, etc.) from the specified storage device on TeensyROM.

**Workflow:**
| Direction | Data |
|---|---|
| Receive | `LaunchFileToken` — `0x6444` |
| Send | `AckToken 0x64CC` |
| Receive | DriveType (1), Path (MaxNameLength, null-terminated)<br>DriveTypes (`RegMenuTypes`): `USBDrive = 0`, `SD = 1`, `Teensy = 2` |
| Send | `AckToken 0x64CC` on pass, `FailToken 0x9B7F` on fail |

**Note:** If the launched file is a SID, `BadSIDToken` (`0x9B80`) or `GoodSIDToken` (`0x9B81`) may also be sent asynchronously during header parsing, separately from the Ack/Fail pair above.

**Handler:** `LaunchFile()`

---

### Version Info
Retrieves firmware version and build information from the TeensyROM.

**Workflow:**
| Direction | Data |
|---|---|
| Receive | `VersionInfoToken` — `0x6476` |
| Send | `AckToken 0x64CC` |
| Send | Version/build info string |

**Example output:**
```
"TeensyROM+ v0.7.2"
"   FW: Jul 11 2026, 14:13:30"
"      Teensy: 816MHz  43.8C"
```

**Handler:** Device acknowledges the token, then returns firmware version, build date, and Teensy clock speed/temperature as a formatted string.

---

### Firmware Check
Queries which firmware mode (Minimal or Full) is currently loaded. See [Minimal vs. Full Firmware Modes](#minimal-vs-full-firmware-modes) for background on why this matters before executing file operations or SID commands.

**Workflow:**
| Direction | Data |
|---|---|
| Receive | `FWCheckToken` — `0x64E0` |
| Send | `FWMinimalToken` (`0x64E1`) **or** `FWFullToken` (`0x64E2`) |

**Handler:** Always succeeds; no `FailToken` response case.

---

### Ping
Verifies TeensyROM connectivity and responsiveness. Useful as a lightweight heartbeat to detect device hang or disconnection without triggering state changes.

**Workflow:**
| Direction | Data |
|---|---|
| Receive | `PingToken` — `0x6455` |
| Send | `{strVersionNumber} ready!\n` (e.g., `"TeensyROM v0.7.2.8 ready!\n"`) |

**Handler:** Device responds immediately with version string and ready status; no separate `AckToken` response.

---

### Post File
Posts a file to a target directory and storage type on TeensyROM. Automatically creates the target directory if missing.
 
**Workflow:**
| Direction | Data |
|---|---|
| Receive | `PostFileToken` — `0x64BB` |
| Send | `AckToken 0x64CC` |
| Receive | Length (4), Checksum (2), SD_nUSB (1), Destination Path (MaxNameLength, null-terminated) |
| Send | `AckToken 0x64CC` on pass, `FailToken 0x9B7F` on fail |
| Receive | File (Length bytes) |
| Send | `AckToken 0x64CC` on pass, `FailToken 0x9B7F` on fail |
 
**Note:** Once the Post File Token is received, all subsequent responses are 2 bytes in length.

**Note:** Checksum is a simple sum of all file bytes, masked to 16 bits (`sum & 0xFFFF`).

**Note:** The legacy `SendFileToken` (`0x64AA`) follows this identical workflow — the two tokens have converged and can be used interchangeably.

**Handler:** `PostFileCommand()`
 
---
 
### Get File
Retrieves a file from the specified directory and storage type on TeensyROM.
 
**Workflow:**
| Direction | Data |
|---|---|
| Receive | `GetFileToken` — `0x64B0` |
| Send | `AckToken 0x64CC` |
| Receive | SD_nUSB (1), Source Path (MaxNameLength, null-terminated) |
| Send | `AckToken 0x64CC` on successful check of storage availability, `FailToken 0x9B7F` on fail |
| — | TeensyROM reads the file from storage |
| Send | File Length (4), Checksum (4, but value is a 16-bit checksum sent zero-extended — upper 2 bytes always `0x0000`) |
| Send | File data, in chunks |
| Send | `AckToken 0x64CC` on successful completion, `FailToken 0x9B7F` on fail |
 
**Note:** Checksum is a simple sum of all file bytes, masked to 16 bits (`sum & 0xFFFF`).
 
**Handler:** `GetFileCommand()`

---

### Copy File
Copies a file from one folder to another in USB/SD storage. If a file with the same name already exists at the destination, it is overwritten.
 
**Workflow:**
| Direction | Data |
|---|---|
| Receive | `CopyFileToken` — `0x64FF` |
| Send | `AckToken 0x64CC` |
| Receive | Source Path (MaxNameLength, null-terminated), Destination Path (MaxNameLength, null-terminated) |
| Send | `AckToken 0x64CC` on pass, `FailToken 0x9B7F` on fail |
 
**Note:** Once the Copy File Token is received, all responses are 2 bytes in length.
 
**Handler:** `CopyFileCommand()`
 
---
 
### Delete File
Deletes a file from the specified storage device on TeensyROM.
 
**Workflow:**
| Direction | Data |
|---|---|
| Receive | `DeleteFileToken` — `0x64CF` |
| Send | `AckToken 0x64CC` |
| Receive | SD_nUSB (1), File Path (MaxNameLength, null-terminated) |
| Send | `AckToken 0x64CC` on pass, `FailToken 0x9B7F` on fail |
 
**Note:** Once the Delete File Token is received, all responses are 2 bytes in length.
 
**Handler:** `DeleteFileCommand()`
 
---

### List Directory
Lists directory contents on TeensyROM given a `skip`/`take` pair, to facilitate batch processing.
 
**Workflow:**
| Direction | Data |
|---|---|
| Receive | `GetDirectoryToken` — `0x64DD` **or** `GetDirNDJSONToken` — `0x64DE` |
| Send | `AckToken 0x64CC` |
| Receive | Storage Type (1), Skip (2), Take (2), Destination Path (MaxNameLength, null-terminated)<br>Storage types (`RegMenuTypes`): `USBDrive = 0`, `SD = 1`, `Teensy = 2` |
| Send | `AckToken 0x64CC` on successful check of directory existence, `FailToken 0x9B7F` on fail |
| Send | `StartDirectoryListToken 0x5A5A` or `FailToken 0x9B7F` |
| Send | Content written as JSON or NDJSON, depending on which token was used |
| Send | `EndDirectoryListToken 0xA5A5`, `FailToken 0x9B7F` on fail |
 
**Handler:** `GetDirectoryCommand(bool SetNDJSONformat)`
 
---
  
### Set Color Reference
Sets one of the colors in the TR UI. Color is stored in EEPROM and IO space; the TR menu must be reset for the change to take visual effect.
 
**Workflow:**
| Direction | Data |
|---|---|
| Receive | `SetColorToken` — `0x6422` |
| Receive | Color reference to set (range `0` to `NumColorRefs-1`, see enum `ColorRefOffsets`) |
| Receive | Color to set (range `0`–`15`, see enum `PokeColors`) |
| Send | `AckToken 0x64CC` or `FailToken 0x9B7F` |
 
**Handler:** `SetColorRef()`
 
---

### Pause SID
Pauses/Resumes SID playback by sending an interrupt request to the C64.

**Workflow:**
| Direction | Data |
|---|---|
| Receive | `PauseSIDToken` — `0x6466` |
| Send | `AckToken 0x64CC` on success, `FailToken 0x9B7F` on fail |

**Handler:** `RemotePauseSID()`

---
 
### Set SID Song
Sets the sub-song number of the currently loaded SID.
 
**Workflow:**
| Direction | Data |
|---|---|
| Receive | `SetSIDSongToken` — `0x6488` |
| Receive | Song number to set (1 byte, zero-based; song 1 = `0`) |
| Send | `AckToken 0x64CC` or `FailToken 0x9B7F` |
 
**Handler:** `SetSIDSong()`
 
---
 
### Set SID Playback Speed
Sets the SID playback speed of the currently loaded SID.
 
**Workflow:**
| Direction | Data |
|---|---|
| Receive | `SIDSpeedLinToken` — `0x6499` **or** `SIDSpeedLogToken` — `0x649A` |
| Receive | Playback rate (16-bit signed int, 2 bytes: Hi, Low)<br>• Linear range: `-68(*256)` to `<128(*256)` — argument is speed change percent from nominal<br>• Logarithmic range: `-127(*256)` to `99(*256)` — argument corresponds to percentage shown in "SID playback speed-log.txt" |
| Send | `AckToken 0x64CC` or `FailToken 0x9B7F` |
 
**Examples:**
- `0x64 0x99 0xF0 0x40` → set to −15.75% via linear equation
- `0x64 0x9A 0x20 0x40` → set to +32.25% via logarithmic equation

**Handler:** `RemoteSetSIDSpeed(bool LogConv)`
 
---
 
### Set SID Voice Mute
Sets individual SID voice muting.
 
**Workflow:**
| Direction | Data |
|---|---|
| Receive | `SIDVoiceMuteToken` — `0x6433` |
| Receive | Voice mute info (1 byte):<br>• bit 0 = Voice 1 (`0`=on, `1`=mute)<br>• bit 1 = Voice 2 (`0`=on, `1`=mute)<br>• bit 2 = Voice 3 (`0`=on, `1`=mute)<br>• bits 7:3 = zero |
| Send | `AckToken 0x64CC` or `FailToken 0x9B7F` |
 
**Handler:** `RemoteSetSIDVoiceMute()`
 
---

### C64 Pause / Resume
Pauses or resumes the C64 via DMA, halting/resuming CPU execution. 

**Workflow:**
| Direction | Data |
|---|---|
| Receive | `C64PauseOnToken` — `0x6431` **or** `C64PauseOffToken` — `0x6430` |
| Send | `AckToken 0x64CC` |

**Handler:** Always succeeds; no `FailToken` response case.

---

### Write C64 Memory (TR+ Only)
Writes a sequential C64 memory segment with supplied data.
 
**Workflow:**
| Direction | Data |
|---|---|
| Receive | `WriteC64MemToken` — `0x64FB` |
| Receive | C64 address (Hi, Low) |
| Receive | Data length (Hi, Low) |
| Receive | Data bytes (per Data Length) |
| — | TR+ performs DMA write |
| Send | `AckToken 0x64CC` on success, `FailToken 0x9B7F` on fail |
 
**Handler:** `WriteC64MemCommand()`
 
---
 
### Read C64 Memory (TR+ Only)
Reads a sequential C64 memory segment and sends it to the host.
 
**Workflow:**
| Direction | Data |
|---|---|
| Receive | `ReadC64MemToken` — `0x64FD` |
| Receive | C64 address (Hi, Low) |
| Receive | Data length (Hi, Low) |
| — | TR+ performs DMA read |
| Send | `AckToken 0x64CC` on success, `FailToken 0x9B7F` on fail |
| Send | Data bytes (per Data Length) |
 
**Handler:** `ReadC64MemCommand()`
