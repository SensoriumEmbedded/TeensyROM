# Known Issues / Deferred Work

Concrete, scoped findings surfaced during architecture walkthroughs — real issues with a known cause and (usually) a designed fix, deliberately queued rather than acted on immediately. Distinct from [Constraints.md](Constraints.md), which documents permanent rules; this file is a to-do list and should shrink as items get resolved (move resolved items out rather than leaving them marked done).

## HIGH PRIORITY: `nfcReadTagLaunch()` can overflow its 256-byte `TagData` stack buffer reading a malformed/corrupted NFC tag

**Where:** `Source/Teensy/nfcScan.ino:183-403`, `nfcReadTagLaunch()`.

More realistic trigger than most "corrupted file" scenarios already waved off this session: physical NFC tags degrade over time, and third-party tag-writing tools (part of the broader "TapTo" ecosystem this feature supports) can write tags that don't match TeensyROM's exact expectations — no adversarial intent needed, just an imperfect or aging tag.

The page-read loop (lines 192-252):
```c
uint8_t DataStart, messageLength, TagData[MaxPathLength];  // 256 bytes
uint16_t CharNum = 0;
while (MoreData)
{
   ...
   nfc.mifareclassic_ReadDataBlock(PageNum, TagData+CharNum);  // writes 16 bytes at TagData+CharNum, unbounded
   for(uint8_t Num = 0; Num<16; Num++)
   {
      if(TagData[CharNum] == 0xfe || CharNum >= DataStart+messageLength) MoreData = false;
      else CharNum++;
   }
   ...
}
```
`messageLength` is read directly from the tag's own data (one byte, so up to 255) and `DataStart` can be up to 7 — so the loop's own stopping condition, `CharNum >= DataStart+messageLength`, can itself reach 262, already past the 256-byte buffer. A tag with a large declared message length and no `0xfe` terminator before that point causes repeated 16-byte writes past the end of `TagData` — a real stack buffer overflow.

Telling contrast: `nfcWriteTag()` (same file, line 429-436) explicitly checks `if (messageLength>254) { ...; return; }` before writing anything — the write path was clearly designed with the 256-byte limit in mind. The read path wasn't given the equivalent treatment, suggesting this is an oversight rather than an accepted risk.

A second, smaller instance of the same gap: the "random launch" handling (line 391) does `strcat((char*)pDataStart, CleanLocalDirMenu[...]->Name)`, appending an arbitrary filename (FAT32, up to 255 chars) into whatever's left in the same `TagData` buffer, also with no bound check against remaining space.

**Status:** deferred — confirmed, high priority given realistic trigger path, not yet fixed (2026-08-12).

## HIGH PRIORITY: `DriveDirPath` (256 bytes) has no length enforcement anywhere it's written, and two different normal-use paths can overflow it

**Where:** `DriveDirPath` is `char[MaxPathLength]` = **256 bytes** (`Teensy.ino:45`). Two independent, unrelated call sites write into it with no bound:

1. **`RemoteLaunch()`** — `Source/Teensy/RemoteControl.ino:198`, `strcpy(DriveDirPath, FileNamePath)`, fed by `SerUSBIO.ino`'s `LaunchFile()`/`ReceiveFileName()`. `LaunchFile()` (`SerUSBIO.ino:699`) declares `char FileNamePath[MaxNamePathLength]` (`MaxPathLength+MaxItemNameLength+2` = 358 bytes), and `ReceiveFileName()` correctly bounds its read loop to that 358-byte limit — so **a client is explicitly permitted, by the protocol's own declared limit, to send a path up to 357 characters** via the standard, documented `LaunchFileToken` command (see `docs/ControlComms.md`). `MaxNamePathLength` exists to accommodate path *plus* filename combined; `DriveDirPath` was apparently only ever sized for the path alone. Any path 257-357 characters — valid per the protocol layer — overflows `DriveDirPath` by up to 101 bytes. Triggerable by any client (third-party control apps included) sending an ordinary `LaunchFileToken`.
2. **`HandleExecution()`** — `Source/Teensy/DriveDirLoad.ino:65,74-75,105`, repeated `strcat(DriveDirPath, MenuSelCpy.Name)` as the user navigates directories on the menu. `MenuSelCpy.Name` is a real SD/USB filename/dirname (FAT32 allows up to 255 chars per component), and `DriveDirPath` **accumulates across successive directory selections** with no check at any point. This is triggerable through **completely ordinary menu browsing** on a real SD card with a few levels of descriptively-named folders — no crafted input, no external protocol, no corrupted file needed at all. Arguably the easier of the two to hit in practice.

Both point at the same underlying gap — nothing enforces `DriveDirPath`'s 256-byte limit at any write site — so a fix should address the buffer discipline generally rather than patching each call site individually.

**Status:** deferred — confirmed via two independent paths, high priority, not yet fixed (2026-08-12).

## HIGH PRIORITY: `Min_SerUSBIO.ino`'s `LaunchFile()` can overflow the `eepAdCrtBootName` EEPROM field into adjacent EEPROM fields

**Where:** `Source/Teensy/MinimalBoot/Min_SerUSBIO.ino:244-279`, `LaunchFile()`.

The persistent-storage (EEPROM) variant of the same `MaxNamePathLength`-vs-`MaxPathLength` size mismatch as the `DriveDirPath` entry above:

```c
char FileNamePath[MaxNamePathLength];  // 358 bytes, bounded correctly by ReceiveFileName()
...
EEPwriteStr(eepAdCrtBootName, DriveNames[DriveType]);
EEPwriteStr(eepAdCrtBootName+strlen(DriveNames[DriveType]), FileNamePath);
```
`eepAdCrtBootName` is allocated only **256 bytes** in the EEPROM layout (`Common_Defs.h`: `eepAdCrtBootName = 1919, // (256:MaxPathLength)`, confirmed tight against the next field `eepAdMinBootInd = 2175` = `1919+256`, no slack) — but `FileNamePath` is sized and bounded to `MaxNamePathLength` (358 bytes) by `ReceiveFileName()`, not `MaxPathLength`. A path long enough (plus the `"USB:"`/`"SD:"`/`"TR:"` prefix) overflows past the 256-byte field, corrupting `eepAdMinBootInd` (1 byte, immediately adjacent) and writing into `eepAdAutolaunchName` (the next 256-byte field). Unlike the RAM-based `DriveDirPath` overflow, this corruption is **persistent** — it survives a reboot and can silently break the boot-mode indicator and the separate autolaunch-file setting.

Same trigger class as the other two high-priority items: an ordinary `LaunchFileToken` with a long-but-protocol-valid path, sent while the device happens to be running in MinimalBoot mode.

**Status:** deferred — confirmed, high priority, not yet fixed (2026-08-12).

## HIGH PRIORITY: `nfcReadTagLaunch()`'s "random launch" path allocates a 16,000-byte array on the stack — likely root cause of the documented NFC large-directory crash

**Where:** `Source/Teensy/nfcScan.ino`, `nfcReadTagLaunch()`, the "random launch" filtering branch:
```c
StructMenuItem *CleanLocalDirMenu[MaxMenuItems];  // MaxMenuItems=4000, ×4-byte pointers = 16,000 bytes, on the stack
```
This is a plain local array, not heap/`malloc`-backed — the full 16KB is claimed from the stack for the duration of this call, on top of whatever else is already on the stack at that call depth.

This directly matches the crash scenario `TeensyROM.h` documents in its own comments (`>24000 RAM1 free for local` reliability-floor note, citing "Random(?) NFC tag with large directory, crash when tapped" as the historical test case that drove the `MaxRAM_ImageSize` 144→128KB reduction). Given current RAM1 `free for local variables` is ~40KB and this one call can claim 16KB of it in a single allocation, this looks like a strong candidate for the actual mechanism behind that crash, not just a theoretical risk — especially combined with whatever the rest of the call chain (NFC read, directory scan, menu-item filtering) has already put on the stack by the time this executes.

Possible fixes: shrink the array to a more realistic bound; move it off the stack (heap-allocate for the duration of the call); or avoid materializing the full filtered list at all — count matching items first, then re-scan for the Nth match instead of building `CleanLocalDirMenu[]` up front.

**Status:** deferred — newly identified, high priority given direct match to documented crash history, not yet fixed (2026-08-12).

## FLASHMEM audit (running list)

Functions that are safe `FLASHMEM` candidates — confirmed never called from `isrPHI2` or any per-cycle handler path (`ROMLHndlr`/`ROMHHndlr`/`IO1Hndlr`/`IO2Hndlr`/`CycleHndlr`) — but aren't currently marked, unlike their siblings that follow the same pattern correctly. Each one left in default RAM1/ITCM placement costs RAM1 space unnecessarily; moving to flash is low-risk (see [Constraints.md](Constraints.md#the-real-dividing-line-flash-backed-vs-ram-backed-not-just-the-isr-function-itself) for why this distinction is safe/unsafe in general). Expected to grow well beyond `IO_Handlers/` as more files get reviewed — check every `InitHndlr`/`PollingHndlr`/`SpecialBtn_*`/similar main-context-only function for this when reviewing a new file, not just when something else prompts it.

**This isn't just a nice-to-have.** Per the user (2026-08-12), the Fab04 (TR+) main-firmware build — the tightest configuration, since it compiles in REU/KernalReplace/Freezers on top of everything else — currently has only **~9-10KB of RAM1 "padding"** left before hitting a hard link failure (measured: `RAM1: variables:254468, code:220104, padding:9272  free for local variables:40444` bytes). RAM1's biggest single consumer, the `RAM_Image` buffer, is fixed and won't shrink — so `FLASHMEM` migrations are the main lever available to keep this build compiling as more features get added, not just an optimization. (Correcting an earlier framing here: MinimalBoot's RAM scarcity is well-documented and obvious, but RAM1 pressure in the full Fab04 build is actually the tighter, less-visible constraint.)

- [ ] `InitHndlr_SuperSnapshotV5` — `Source/Teensy/MinimalBoot/Common/IO_Handlers/IOH_SuperSnapshotV5.c:114` — sibling handlers' `InitHndlr` (REU, KernalReplace, ActionReplay, RetroReplay) all correctly have it; this one doesn't.
- [ ] `SpecialBtn_SuperSnapshotV5` — `Source/Teensy/MinimalBoot/Common/IO_Handlers/IOH_SuperSnapshotV5.c:104` — called only from `Teensy.ino:290` (`fSpecialBtnChange(...)`, inside the main-loop button-debounce check), confirmed never ISR-path. `SpecialBtn_REU` already has `FLASHMEM`; this one doesn't.
- [ ] `PollingHndlr_TR_BASIC` — `Source/Teensy/MinimalBoot/Common/IO_Handlers/IOH_TR_BASIC.c:442` — main-loop-only (`PollingHndlr`), like `PollingHndlr_REU` which already has `FLASHMEM`.
- [ ] `PollingHndlr_TeensyROM` — `Source/Teensy/MinimalBoot/Common/IO_Handlers/IOH_TeensyROM.c:1878` — same pattern, main-loop-only.
- [ ] `IOH_ASID.c` — largest single opportunity found so far. Confirmed via call-graph trace (not assumed) that only `IO1Hndlr_ASID` (ISR-called) and `InitTimedASIDQueue()` (called from within it, own comment confirms "Called from ISR, must be fast!") and `SendTimedASID()` (`IntervalTimer` hardware-timer interrupt context, already correctly `FASTRUN`) need to stay RAM-resident. Everything else is only reachable via `PollingHndlr_ASID`'s main-loop `usbHostMIDI.read()`/`usbDevMIDI.read()` calls — safe FLASHMEM candidates: `InitHndlr_ASID`, `PollingHndlr_ASID`, `AddToASIDRxQueue`, `FlushASIDRxQueue`, `SetASIDIRQ`, `PrintflnToASID`, `AddErrorToASIDRxQueue`, `DecodeSendSIDRegData`, and `ASIDOnSystemExclusive` (the ~180-line SysEx decoder, `IOH_ASID.c:389-567`) — likely the single biggest RAM1 reclaim of any file reviewed this session.
- [ ] `IOH_MIDI.c` — same shape as ASID. Only `IO1Hndlr_MIDI` is ISR-called; it doesn't call `SetMidiIRQ()` or any `HWEOn*` handler directly, so all of those are only reachable via `PollingHndlr_MIDI`'s main-loop `usbHostMIDI.read()`/`usbDevMIDI.read()` calls. Safe FLASHMEM candidates: `PollingHndlr_MIDI` (`IOH_MIDI.c:471`), `SetMidiIRQ` (`:107`), and all twelve `HWEOn*` callback functions (`HWEOnNoteOff`/`On`/`AfterTouchPoly`/`ControlChange`/`ProgramChange`/`AfterTouch`/`PitchChange`/`SystemExclusive`/`TimeCodeQuarterFrame`/`SongPosition`/`SongSelect`/`TuneRequest`/`RealTimeSystem`, lines 107-248).
- [ ] `IOH_Swiftlink.c` — `InitHndlr_SwiftLink` (line 380) and `PollingHndlr_SwiftLink` (line 513) are candidates; likely also `FreeSwiftlinkBuffs()` (line 263, called from `SetUpMainMenuROM()` — main-context). By contrast `SetBaud()` and `ResetSwiftLink()` are correctly left unmarked — both carry their own "called from Phi IRQ/IO handler, be quick!" comments and are genuinely ISR-reachable via `IO1Hndlr_SwiftLink`.
- [ ] `Swift_RxQueue.c` — the file demonstrates both patterns side-by-side: smaller helpers near the bottom (`AddIPaddrToRxQueueLN`, `AddMACToRxQueueLN`, `AddInvalidFormatToRxQueueLN`, `AddUpdatedToRxQueueLN`, `AddDHCPEnDisToRxQueueLN`, `AddDHCPTimeoutToRxQueueLN`, `AddDHCPRespTOToRxQueueLN`, `Add_BR_ToRxQueue`) are correctly `FLASHMEM`, but the foundational functions above them aren't, despite being equally main-context-only (reachable only via `PollingHndlr_SwiftLink`, never the ISR-path `IO1Hndlr_SwiftLink`/`CycleHndlr_SwiftLink`): `PullFromRxQueue`, `ReadyToSendRx`, `CheckRxNMITimeout`, `SendRxByte`, `CheckSendRxQueue`, `FlushRxQueue`, `AddRawCharToRxQueue`, `AddRawStrToRxQueue`, `AddToPETSCIIStrToRxQueue`, `AddToPETSCIIStrToRxQueueLN`, `inet_aton`.
- [ ] `Swift_ATcommands.c` — otherwise fully consistent (every `AT_*` function correctly `FLASHMEM`), except two tiny one-line wrappers: `AddVerboseToPETSCIIStrToRxQueueLN` and `AddVerboseToPETSCIIStrToRxQueue` (lines 84-92). Negligible RAM impact given their size, noted for completeness.
- [ ] `Swift_Browser.c` — confirmed via call-graph (only reachable through `PollingHndlr_SwiftLink`'s TxMsg/browser-command processing or `CheckSendRxQueue`'s HTML parsing, never the ISR-path `IO1Hndlr_SwiftLink`/`CycleHndlr_SwiftLink`): `SwiftTxBufToLcaseASSCII`, `SendPETSCIICharImmediate`, `SendASCIIStrImmediate`, `SendASCIIStrImmediateLN`, `SendASCIIErrorStrImmediate`, `DumpQueueUnPausePage`, `UnPausePage`, `ParseEntityReference`, `ParseHTMLTag`, `ParseURL`, `ReadClientLine`, `ClearClientStop`, `AddToPrevURLQueue`, `WebConnect`, `isURLFiltered`, `ModWebConnect`.
- [ ] `InitHndlr_ZaxxonSuper` — `Source/Teensy/MinimalBoot/Common/IO_Handlers/IOH_ZaxxonSuper.c:37` — main-context only (`InitHndlr`, called via `IOHandlerInit`), not currently marked.
- [ ] `InitHndlr_MagicDesk` — `IOH_MagicDesk.c:40`
- [ ] `InitHndlr_MagicDesk2` and `PollingHndlr_MagicDesk2` — `IOH_MagicDesk2.c:52` and `:117`
- [ ] `InitHndlr_Ocean1` — `IOH_Ocean1.c:35`
- [ ] `InitHndlr_SuperGames` — `IOH_SuperGames.c:37`
- [ ] `InitHndlr_EpyxFastLoad` — `IOH_EpyxFastLoad.c:44`
- [ ] `InitHndlr_GMod2` — `IOH_GMod2.c:35`
- [ ] `InitHndlr_EasyFlash`, `LoadBank`, `PollingHndlr_EasyFlash` — `IOH_EasyFlash.c:90`, `:60`, `:214` — all main-context only; `ImageCheckAssign` (`:143`) is correctly left unmarked since it's genuinely ISR-reachable via `IO1Hndlr_EasyFlash`.
- [ ] `PollingHndlr_Debug` — `IOH_Debug.c:243` — the one function in the file not covered by its own `DEBUG_MEMLOC` (`=FLASHMEM`) macro, which is otherwise applied consistently to everything else.
- [ ] `ServiceTCP` — `Source/Teensy/ServiceTCP.ino:4` — likely main-context (polling-loop TCP handling), not independently confirmed via caller trace.
- [ ] `ServiceTCP` — `Source/Teensy/MinimalBoot/Min_ServiceTCP.ino:4` — MinimalBoot's own copy of the same function/gap. `EthernetInit()` in the same file is already correctly marked.
- [ ] `InterruptC64`, `DoC64IRQ`, `EEPRemoteLaunch`, `RemoteLaunch` — `Source/Teensy/RemoteControl.ino` — all main-context only; `RemoteLaunch` is the largest of the batch.
- [ ] `RAM2BytesFree` — `Source/Teensy/SerUSBIO.ino:785` — the one function in this file not marked `FLASHMEM`; everything else (`ServiceSerial`, `ProcessCommand`, `LaunchFile`, `ReceiveFileName`, etc.) is fully consistent.
- [ ] `Source/Teensy/DriveDirLoad.ino` — **zero `FLASHMEM` coverage in the entire file**, likely the single largest opportunity of the session. Every function is menu-navigation-triggered, main-context only (reached via `HandleExecution()` from `RemoteLaunch()`/`IOHandlerSelectInit()`'s StatusFunction dispatch, never the ISR path): `HandleExecution` (~265 lines), `MenuChange`, `LoadFile`, `InitDriveDirMenu`, `SetDriveDirMenuNameType`, `LoadDirectory`, `AddDirEntry`, `FreeDriveDirMenu`, `FreeCrtChips`, `Assoc_Ext_ItemType`.
- [ ] `Source/Teensy/MinimalBoot/Min_DriveDirLoad.ino` — same pattern as its full-firmware counterpart above: zero `FLASHMEM` coverage, all main-context (`HandleExecution`, `LoadFile`, `ParseCRTHeader`, `ParseChipHeader`, `FreeCrtChips`, `PathIsRoot`, `SetTypeFromCRT`, `AssocHWID_IOH`, `SendMsgPrintfln`, `toU32`/`toU16`).
- [ ] `nfcCheck`, `RegMenuTypeFromFileName`, `FSfromSourceID`, `nfcReadTagLaunch` — `Source/Teensy/nfcScan.ino` — NFC polling is main-loop-driven, not ISR; `nfcInit`/`nfcConfigCheck`/`nfcWriteTag` in the same file already correctly have `FLASHMEM`.
- [ ] `setup()`, `SetNumItems`, `SDFullInit`, `USBFileSystemWait`, `SetRandomSeed`, `CheckLaunchSDAuto` — `Source/Teensy/Teensy.ino` — `setup()` runs exactly once at boot and is never ISR-reachable; verified `set_arm_clock()` (Teensyduino 1.61.0 core, `clockspeed.c`) only touches the ARM core clock tree, never FlexSPI/flash timing, so there's no clock-change-vs-flash-execution hazard ruling it out. The rest are ordinary main-context helpers. `SetUpMainMenuROM`, the `EEPwrite*`/`EEPread*` helpers, `SetEEPDefaults`, and all five `SpecialBtn_*` functions in the same file are already correctly marked.
- [ ] `setup()`, `EEPwriteNBuf`, `EEPwriteStr`, `EEPreadNBuf`, `EEPreadStr`, `LoadCRT` — `Source/Teensy/MinimalBoot/MinimalBoot.ino` — same reasoning as `Teensy.ino`'s equivalents (which already have `FLASHMEM`); this build's own copies were missed. `RAM2blocks()` in the same file is already correctly marked.

**Status:** open, actively growing as more files are reviewed (2026-08-12). Every item above was applied at once as an experiment on 2026-08-12 (both firmwares built clean), successfully clearing ~17KB of `padding` runway for future RAM1-resident code growth, then **reverted** anyway to avoid paying the flash access-time cost on ~115 functions continuously before it's needed — see [Constraints.md](Constraints.md#full-audit-list-flashmem-experiment-2026-08-12--applied-measured-then-reverted) for the measured RAM impact and the per-file byte-size ranking. None of these checkboxes reflect applied state; they're still open candidates for whenever clearing more `padding` runway (or crossing the next 32KB boundary for a `free for local variables` jump) is worth that tradeoff.

## Debug-only `'y'` serial command has a genuinely unbounded read into a fixed buffer

**Where:** `Source/Teensy/SerUSBIO.ino:183-192`, the `'y'` case (load REU PSRAM from file via serial), gated behind `Dbg_SerDMA` + `Fab04_FullDMACapable` + `USE_PSRAM`.

```c
char Filename[100];
uint32_t CharNum = 0;
while (SerialAvailabeTimeout())
{
   Filename[CharNum++] = CmdChannel->read();
}
```
No bound against the 100-byte `Filename[]` at all — worse than most other findings this session in that there's no size reasoning whatsoever, just an idle-timeout-gated loop. Multiply debug-gated though, so low real-world exposure.

**Status:** deferred — flagged, low priority given how narrowly gated it is (2026-08-12).

## `LoadDxxDirectory()` has no cycle detection or entry-count bound on the D64/D71/D81 track/sector chain

**Where:** `Source/Teensy/D64.ino:77-173`, inside `LoadDxxDirectory()`.

Walks a directory track/sector chain read directly from the disk-image file (`Track = NextTrack; Sector = NextSect;`, values read from file data) with no cycle detection, and writes `DriveDirMenu[NumDrvDirMenuItems]` with no check against `MaxMenuItems` before incrementing. A malformed or circular chain in a corrupted/crafted D64/D71/D81 image could grow `NumDrvDirMenuItems` indefinitely. Also no NULL-check on the per-entry `malloc(DxxFNB_Bytes)` (line 143) before the following `memcpy` into it. Same general category as other malformed-file scenarios already waved off this session — flagging in case the entry-count angle (unbounded growth, not just a single bad index) changes the calculus. `LoadDxxFile()` (the sibling that actually loads file contents) is unaffected — it's naturally self-bounding via its `Size + 254 > RAM_ImageSize` check regardless of chain behavior.

**Status:** deferred — flagged, not yet assessed (2026-08-12).

## `DownloadFile()` writes an over-length HTTP response chunk before validating it

**Where:** `Source/Teensy/MinimalBoot/Common/IO_Handlers/Swift_Browser.c:748-763`, inside `DownloadFile()`.

```c
uint32_t ChunkSize = client.available();
if (ChunkSize)
{
   if (ChunkSize > MaxChunkSize) ChunkSize = MaxChunkSize;
   client.read(DataChunk, ChunkSize);
   dataFile.write(DataChunk, ChunkSize);      // written first
   ...
   if (ChunkSize > Length)                     // checked after
   {
      dataFile.close();
      SendASCIIErrorStrImmediate("\rExtra data received");
      return;
   }
```
Not a buffer overflow — `DataChunk` is correctly capped to `MaxChunkSize` before use. But if a server sends more data than its own declared `Content-Length`, the oversized chunk is written to the destination file *before* the "Extra data received" error fires and aborts. The user sees the error, but the partially-corrupt file is left on disk rather than being caught before that last write or cleaned up (e.g. `sourceFS->remove()`) after.

**Status:** deferred — not yet fixed (2026-08-12).

## Design a consistent allocation-failure (OOM) policy across handlers

**Where:** cross-cutting — seen across `IOH_REU.c`, `IOH_RetroReplay.c`, `IOH_ActionReplay.c`, `IOH_SuperSnapshotV5.c`, `IOH_Swiftlink.c`, `BusSnoop.ino`, and likely more not yet reviewed.

Every handler that allocates RAM at init handles a failed `malloc`/`calloc` differently, with no consistent policy:
- `IOH_REU.c`'s RAM12 bank-allocation loop actively detects persistent failure and calls `REBOOT` (`IOH_REU.c:539-546`) — "no better way to fail..." per its own comment.
- `IOH_RetroReplay.c` and `IOH_ActionReplay.c` don't check at all (`IOH_ActionReplay.c:94-99` has the check written but commented out); `IOH_SuperSnapshotV5.c` has the identical commented-out check (see the earlier FLASHMEM/null-check discussion — assessed as low risk to leave, since the NULL guards elsewhere prevent a crash).
- `IOH_Swiftlink.c`'s `InitHndlr_SwiftLink` allocation loops (lines 404-428) print an "OOM ..." message and continue, leaving that specific buffer slot `NULL`. **Confirmed (not just suspected): `Swift_RxQueue.c` does not check for this.** `PullFromRxQueue()` (`Swift_RxQueue.c:26`) and `AddRawCharToRxQueue()` (`:129`) both index `RxQueue[RxQueueHead/Tail / RxQueueBlockSize][...]` directly with no NULL guard — if the circular buffer wraps into a block whose allocation failed, that's a real NULL-pointer dereference/crash, not just a theoretical risk. Good supporting evidence for the REBOOT direction below: if allocation failure reboots immediately at init, this consumer-side gap becomes unreachable rather than needing to be individually guarded.
- `BusSnoop.ino`'s `BusAnalysis()` (line 16) doesn't check `calloc()` either — a failure would NULL-deref inside `BusCount()` (assigned to `fBusSnoop`, genuinely ISR-called) on the very next C64 bus write. Narrower in practice than the others since this is a manually-triggered diagnostic tool, not a normal-use code path.

**Current thinking (2026-08-12, not finalized):** the likely direction is to standardize on **REBOOT on allocation failure**, matching the precedent already set by `IOH_REU.c`'s RAM12 path — consistent with the project's general RAM-scarcity posture (MinimalBoot's whole existence is a response to RAM exhaustion, see [Teensy-Firmware.md](Teensy-Firmware.md#minimalboot-vs-full-firmware)) and simpler to reason about than trying to gracefully degrade every handler individually. Not yet designed in detail (e.g., whether every handler gets this treatment uniformly, or whether some cases warrant a softer failure).

**Status:** deferred — direction leaning REBOOT, full design discussion not yet held (2026-08-12).

## Audit `sprintf`/`vsprintf`-into-fixed-buffer pattern across the codebase

**Where seen so far:**
- `FileParsers.ino:510-532` (`SendMsgPrintfln`/`SendMsgPrintf` → `SerialStringBuf`, 262 bytes) — confirmed reachable overflow via a malformed CRT file's unterminated "Name" field (see the dedicated entry above).
- `IOH_ASID.c:229-253` (`PrintflnToASID` → `ToSend`, 300 bytes) — lower risk, every call site is either a fixed literal, small integer formatting, or bounded by the MIDI library's SysEx null-termination/size cap (that cap itself not independently verified small enough to always fit).
- `Swift_ATcommands.c:186-187` (`AT_DT()` → local `Buf[100]`) — **confirmed reachable via completely ordinary use**, not just a malformed-input edge case: `sprintf(Buf, "Trying \"%s\"\r\n on port %d...", CmdArg, Port);` where `CmdArg` is user-typed hostname text from an `ATDT<hostname>:<port>` command, bounded only by `TxMsg`'s 128-byte limit — a normal long hostname plus the fixed format text alone can exceed the 100-byte `Buf`.
- `DriveDirLoad.ino:406-417` (full firmware) and `Min_DriveDirLoad.ino:406-417` (MinimalBoot) — each has its **own independent copy** of `SendMsgPrintfln`/`SendMsgPrintf` with the same unbounded `vsprintf` into a local `SerialStringBuf[MaxPathLength]`. Their `ParseCRTHeader()`/`LoadFile()` call chain is a **more severe variant of the FileParsers.ino case above**: `LoadFile()` declares `uint8_t lclBuf[CRT_MAIN_HDR_LEN]` (64 bytes, confirmed via `DriveDirLoad.h:170`), and the CRT "Name" field read by `SendMsgPrintfln("Name: %s", (CRT_Image+0x20))` starts at byte 32 and is a 32-byte field ending *exactly* at byte 64 — the last byte of `lclBuf` itself. An unterminated Name field (fills all 32 bytes, no padding) causes the `%s` read to run off the end of `lclBuf` immediately — a stack **out-of-bounds read at the source**, before even reaching the already-known destination-buffer overflow. Tighter than the `FileParsers.ino` instance, where the Name field sits inside a buffer holding the whole loaded file (an unterminated name there just reads into subsequent legitimate file data, not immediately out of bounds). Present in both builds since `Source/C64`-style code sharing doesn't apply here — each `LoadFile()` is its own independent implementation.

Four independent instances of the same shape (`sprintf`/`vsprintf` into a fixed local/global buffer, no length argument) found without specifically looking for it — worth a dedicated sweep (`grep -rn "sprintf\|vsprintf"`, excluding the safe `snprintf`/`vsnprintf` variants) across the rest of the codebase rather than waiting to stumble onto more of them file-by-file.

**Status:** open, not yet swept (2026-08-12).

## Unbounded writes in `IOH_TR_BASIC.c`'s ISR-path IO1 write handler

**Where:** `Source/Teensy/MinimalBoot/Common/IO_Handlers/IOH_TR_BASIC.c:429` and `:436`, inside `IO1Hndlr_TR_BASIC` (called directly from `isrPHI2`, `ISRs.c:128`).

Two writes into fixed-size buffers with no bounds check against the C64-side-controlled byte count:
- `LSFileName[FNCount++] = Data;` (line 429, `TR_BASFileNameReg` write case) — `LSFileName` is a 256-byte (`MaxPathLength`) allocation; a filename stream longer than that with no null terminator would overflow it.
- `RAM_Image[StreamOffsetAddr++] = Data;` (line 436, `TR_BASStreamDataReg` write case) — similarly unbounded against `RAM_Image`'s size.

Both depend entirely on the C64-side `TRCustomBasicCommands` assembly code behaving itself and never streaming more bytes than the Teensy side expects — nothing on the Teensy side enforces the limit. Noted separately from the `Serial.write()` finding in this same file (see below) since this is a memory-safety/buffer-overflow concern from data volume, not a timing concern — the "BASIC interpreter overhead paces this" reasoning that resolved the `Serial.write()` question doesn't obviously extend to whether the *byte count* itself could ever exceed the buffer.

**Status:** deferred — flagged, not yet assessed how to fix or whether real-world usage can trigger it (2026-08-12).

## Documented exception: `Serial.write()` inside the ISR-path `IO1Hndlr_TR_BASIC`

**Where:** `Source/Teensy/MinimalBoot/Common/IO_Handlers/IOH_TR_BASIC.c:394`, inside `IO1Hndlr_TR_BASIC` (ISR-called, `TR_BASDataReg` write case — the TPUT BASIC command's output path).

```c
Serial.write(Data); //a bit risky doing this here, but seems fast enough in testing
```
This runs in the ISR call chain, which `Constraints.md` otherwise rules out for anything with unbounded latency (Serial writes included, same class as SD/USB/EEPROM). Confirmed by the developer as a **deliberate, understood exception**: TPUT's `Serial.write()` calls are paced by BASIC interpreter overhead (tokenization/dispatch), not raw machine-code speed, so in practice this can't be called densely enough to threaten bus timing — verified in testing. Not queued for a fix; recorded here (and to be folded into `Constraints.md`) so it isn't re-flagged as a fresh violation in a future pass.

**Status:** accepted, documented exception — no action needed (2026-08-12).

## MinimalBoot's Ethernet-disable protection has a gap during VIC-cycle emulation

**Where:** `Source/Teensy/MinimalBoot/Min_DriveDirLoad.ino:53-55`

The full-firmware path disables `IRQ_ENET`/`IRQ_PIT` while `EmulateVicCycles` is active, because servicing the VIC half of the cycle extends `isrPHI2` enough that a stray Ethernet interrupt can cause a missed cycle (see [Constraints.md](Constraints.md)). MinimalBoot has the identical `EmulateVicCycles = true;` line (`Min_DriveDirLoad.ino:55`) but its matching `NVIC_DISABLE_IRQ(IRQ_ENET/IRQ_PIT)` lines just above are commented out. This protection gap is now live, not just theoretical, since MinimalBoot runs Ethernet (added deliberately for remote-launch/interrupt support, see [Teensy-Firmware.md](Teensy-Firmware.md#minimalboot-vs-full-firmware)).

**Why it hasn't caused a visible problem:** carts needing VIC-half emulation are old/small titles (e.g. Jupiter Lander, Radar Rat Race, Clowns) that always fit within full-firmware's RAM budget — the combination "needs VIC emulation" + "too large for full FW, must run from MinimalBoot" doesn't occur among real carts. This is an accidental non-collision, not a designed-safe state.

**Proposed fix:** comment out `EmulateVicCycles = true;` at `Min_DriveDirLoad.ino:55` too, so MinimalBoot never attempts VIC-cycle emulation at all — trading "a hypothetical oversized cart needing VIC emulation wouldn't run correctly from MinimalBoot" for closing the unprotected-Ethernet window.

**Status:** deferred — not ready to test yet (2026-08-11).

## Cartridge register map (`Menu_Regs.i` / `Menu_Regs.h`) is hand-duplicated across two languages

**Where:** `Source/C64/MainMenuCRT/source/Menu_Regs.i` (ACME) and the Teensy-side `Menu_Regs.h` (C) — see [Comms-Protocol.md](Comms-Protocol.md).

Two independently hand-maintained files define the same register offset/enum map, with only a comment ("These need to match Teensy Code: Menu_Regs.h") enforcing consistency — no shared source, no build-time check.

**Fix implemented:**
1. `Menu_Regs.h` stays the single hand-edited source of truth. The region between the `These need to match C64 Code` / `End C64 matching` marker comments is now mechanically translated, not hand-copied.
2. A generator script, **`Source/C64/gen_menu_regs_i.py`**, parses that region directly and emits ACME assignment lines — no C preprocessor involved. (The original plan called for `gcc -E`/`cpp`, but the synced region turned out to contain zero `#ifdef`/`#include` for a preprocessor to resolve, and a plain macro-expansion pass can't translate C `enum { A = 1, B };` block syntax into ACME's flat `A = 1` / `B = 2` assignment lines anyway — that translation is what the script does.) Handles multi-line enums, `#define` constants, auto-incrementing members with no explicit value, and comment-style translation (`//` → `;`).
3. One documented exception: `IOH_None` is pulled in from `enum enumIOHandlers`, which lives just *outside* the synced region (deliberately — the rest of that enum is Teensy-internal `IOHandler[]` indices with no meaning on the C64 side, and its values shift under `MinimumBuild`/`Fab04_*` build flags that don't apply to C64 assembly). `IOH_None` is the only member any C64 code references (`ldx #IOH_None` in `Pg_TRSettings.asm`), and it's pinned at 0 by its own "always 0" comment in `Menu_Regs.h`, so the script asserts that invariant rather than hardcoding the value blindly.
4. Found and fixed a real drift in the process: `EscMenuMiscColor`/`EscTypeColor` existed only in the hand-written `Menu_Regs.i`, with no `Menu_Regs.h` counterpart, even though they're referenced from 8 C64 ASM files. Moved them into `Menu_Regs.h`'s `enum ColorRefOffsets` as explicit aliases (`EscMenuMiscColor = EscNameColor`, `EscTypeColor = EscSourcesColor`) so they're now covered by the same sync guarantee as everything else.
5. `Menu_Regs.i` keeps its existing location/format, so its 6 consumers (`MainMenuCRT`, `SettingsMenu`, `TRHelpScreens`, `TRExtPortCheck`, `MIDI2SID`, `ExpansionPortTest`) needed no changes.
6. Regeneration is wired into **`Source/C64/SetToolPaths.bat`** (confirmed universal chokepoint — all 12 sub-project build scripts source it first, unconditionally), so every C64 build regenerates `Menu_Regs.i` from `Menu_Regs.h` before assembling.

**Verified:** assembled both the old hand-written `Menu_Regs.i` and the generated one with ACME 0.97 (`--symbollist`) — all 268 symbols matched name-for-name and value-for-value, and the resulting `.prg` output was byte-identical.

**Status:** fixed (2026-08-14).

## Large-CRT bank-swap DMA reliability claim may be stale

**Where:** [CRT_Implementation.md](/docs/CRT_Implementation.md), [Constraints.md](Constraints.md#memory-budgets-are-hard-caps-not-soft-targets)

The >850KB bank-swap mechanism (REU-style DMA-line-assert pause, not true bus-mastering DMA — TR+'s bus-mastering doesn't help here since the pause just needs to be perceptually instant, which bus-mastering doesn't improve) is documented as unreliable on most C128s and a low percentage of NTSC systems. That claim may no longer hold and is worth re-testing.

**Status:** flagged for re-test, not yet re-verified (2026-08-11).

## `ASIDPlayer.asm` has its `Menu_Regs.i` include commented out

**Where:** `Source/C64/ASIDPlayer/source/ASIDPlayer.asm:7`

Every other `Menu_Regs.i` consumer has an active `!src` line; ASIDPlayer's is present but commented out. Not yet confirmed whether this is intentional (ASID genuinely doesn't need those registers) or a leftover from an earlier split.

**Status:** unverified, low priority.

## `IOHandler[]` array and `enum enumIOHandlers` are hand-synced with no compile-time check

**Where:** `Source/Teensy/MinimalBoot/Common/IOHandlers.h:105-147` (array) and `Source/Teensy/MinimalBoot/Common/Menu_Regs.h:390-434` (enum), both explicitly commented "Synch order/qty with" the other.

Every `stcIOHandlers*` entry in `IOHandler[]` must line up positionally with the matching `enumIOHandlers` value, across every `#ifdef` gate (`MinimumBuild`, `Fab04_REU`, `Fab04_KernalReplace`, `Fab04_Freezers`) — `CurrentIOHandler` is just an integer index into the array. Hand-verified during architecture review (2026-08-11) that the two currently match exactly, entry-for-entry including `RetroReplay`'s recent insertion — but nothing enforces that going forward; adding/removing a handler in only one list would silently misindex `IOHandler[CurrentIOHandler]` at runtime (e.g. selecting one cartridge type could actually load a different one).

**Fix:** added `static_assert(sizeof(IOHandler) / sizeof(IOHandler[0]) == IOH_Num_Handlers, "IOHandler[] / enumIOHandlers count mismatch");` right after the array definition (`IOHandlers.h:105-149`). Used `static_assert` rather than `_Static_assert` since this header is included from `.ino` files and compiled as C++. Zero runtime cost, compile-time only, doesn't touch the ISR or any hot path. Note: this only catches a *count* mismatch (forgot to add/remove an entry in one list), not a pure *reordering* with the same count — still worth having since count mismatches are the more likely mistake.

**Status:** fixed (2026-08-14).

## Third `USBHIDParser` instance (`hid3`) may be unnecessary RAM cost

**Where:** `Source/Teensy/MinimalBoot/Common/IOHandlers.h:38`

Three `USBHIDParser` instances (`hid1`, `hid2`, `hid3`) are declared for the USB host stack, with the original developer's own comment `//need all 3?` left in place — an open question, not a confirmed requirement. Each instance carries its own RAM cost (endpoint buffers/state); if 3 simultaneous HID devices isn't a real-world scenario, removing `hid3` could reclaim some RAM in an area (full firmware's USB host stack) that already competes for the same RAM1/RAM2 budget as everything else.

**Status:** deferred — needs investigation into whether 3 HID devices is a real usage case before touching (2026-08-12).

## Full-firmware and MinimalBoot each maintain their own independent copy of same-named functions — a standing maintainability risk, not a single bug

**Where:** cross-cutting. `Source/Teensy/*.ino`/`.h` (full firmware) and `Source/Teensy/MinimalBoot/*.ino`/`.h` (MinimalBoot) are two separate builds that don't share translation units — despite many functions having identical names and near-identical logic, each build carries its own independent implementation. Confirmed pairs found this session: `SendMsgPrintfln`/`SendMsgPrintf` (`DriveDirLoad.ino` vs `Min_DriveDirLoad.ino`), the `EEPwrite*`/`EEPread*` helpers (`Teensy.ino` vs `MinimalBoot.ino`), the `LoadFile`/`ParseCRTHeader`/`ParseChipHeader` chain (`DriveDirLoad.ino` vs `Min_DriveDirLoad.ino`), and `ServiceTCP` (`ServiceTCP.ino` vs `Min_ServiceTCP.ino`).

Unlike the C64 side, where `Menu_Regs.i`/`Menu_Regs.h` at least document the sync requirement in a comment, these pairs carry no such marker — nothing signals that a fix in one file has a counterpart that likely needs the same fix.

**Already causing real drift, not just a theoretical risk:** MinimalBoot's `ParseChipHeader` correctly bounds-checks `NumCrtChips == MAX_CRT_CHIPS` before writing; the full-firmware `ParseChipHeader` in `DriveDirLoad.ino` does not. Same function, same name, same intended behavior, silently diverged.

**No fix proposed** — restructuring this (shared translation units, a common library, etc.) would be a significant undertaking given the two builds' different memory/feature constraints, and isn't asked for. Recorded as a standing reminder: when fixing a bug in one of these known-duplicated functions, check whether the other build's copy needs the identical fix.

**Status:** noted — not queued for a fix, kept as a reminder for future bug fixes in either build (2026-08-13).

## Unbounded `vsprintf` into a fixed global buffer — triggerable by a malformed CRT file

**Where:** `Source/Teensy/FileParsers.ino:510-532` (`SendMsgPrintfln`/`SendMsgPrintf`), writing into `SerialStringBuf` (`char[MaxPathLength+6]` = 262 bytes, declared `Source/Teensy/MinimalBoot/Common/IO_Handlers/IOH_TeensyROM.c:45`).

Both functions call `vsprintf(SerialStringBuf, Fmt, ap)` with no length limit. `SendMsgPrintfln` additionally shifts the buffer in place to prepend `\r\n` (`FileParsers.ino:518`), writing as far as `strlen(SerialStringBuf)+2` — a second unbounded-length write on top of the first.

**Concrete trigger:** `ParseCRTHeader()` (`FileParsers.ino:93`) calls `SendMsgPrintfln("Name: %s", (CRT_Image+0x20))` — `CRT_Image+0x20` is the CRT file's 32-byte "Name" field, read directly from file content with no length bound enforced before the `%s`. A corrupted or malformed CRT (from SD/USB, or posted over the external USB/Ethernet protocol) with a non-terminated name field would have `%s` read straight into the rest of the ROM image looking for a zero byte — real cartridge ROM data could go a very long way before hitting one — overflowing the 262-byte global buffer.

**Proposed fix:** swap `vsprintf` for `vsnprintf(SerialStringBuf, sizeof(SerialStringBuf), Fmt, ap)` in both functions, clamp the `\r\n`-shift loop to `sizeof(SerialStringBuf)-1`, and bound the CRT Name field read itself (e.g. `%.32s`) at the call site as defense in depth. Plain main-context code, no ISR/hot-path constraints apply.

**Status:** deferred — fix proposed, not yet implemented (2026-08-12).

## Planned refactor: split `StatusFunction[]` out of `IOH_TeensyROM.c`

**Where:** `Source/Teensy/MinimalBoot/Common/IO_Handlers/IOH_TeensyROM.c` (1891 lines, largest IO handler by far).

Plan to pull all the `StatusFunction` functions and the `StatusFunction[]` declaration/array out into a separate file, for organization given the file's size.

**Assessed during architecture review (2026-08-12), no structural problem found:** `StatusFunction[]` is only ever dispatched from `PollingHndlr_TeensyROM()` (main-loop context, not the ISR path) — zero interaction with hot-path constraints. Every `IO_Handlers/*.c` file is already `#include`d as raw source into `IOHandlers.h` rather than compiled as an independent translation unit, so adding one more file to that same `#include` chain doesn't introduce a new pattern or a real linkage boundary. The one thing to watch when actually doing the split: anything the moved `StatusFunction` bodies reference that's currently file-scoped in `IOH_TeensyROM.c` (statics, helpers without prototypes) needs to stay visible via include-order placement or get forward-declared — same discipline already used elsewhere (e.g. `IOH_REU.c`'s block of `extern` declarations for cross-file symbols).

**Status:** deferred — planned by the developer, no blocker identified (2026-08-12).

## Low priority: style/consistency observations from the code-review pass

Cosmetic, no functional impact — not queued for a dedicated pass, but worth applying prospectively (new code, or opportunistically when touching a function for another reason).

- **Naming/abbreviation is inconsistent, sometimes within the same file.** E.g. `IOHandlers.h`: `struct stcIOHandlers` (spelled out) contains members named `InitHndlr`/`IO1Hndlr` (abbreviated), immediately followed by `#include "IO_Handlers/IOH_MIDI.c"` — "Handler" spelled out in the directory name, abbreviated in the filename. Same pattern with counts: `CharNum`, `NumRegs`, `BigBufCount`, `FNCount` are four different naming shapes for the same concept. Buffer names for the recurring "build an outgoing string" pattern are especially inconsistent (`SerialStringBuf`, `Buf`, `ToSend`, `TxMsg`, `lclBuf`) — a shared naming convention there might have made the repeated `vsprintf`-into-fixed-buffer pattern (see above) easier to spot by inspection.
- **Very large files/functions are hard to review in one pass.** `IOH_TeensyROM.c` (~1,900 lines) and `HandleExecution()` (~265 lines) took noticeably longer to review than anything else this session. Worth treating size itself as a standing signal to split, beyond the one `StatusFunction[]` split already planned.
- **Commented-out code left as an unresolved decision rather than a removed/explained one.** The OOM checks in `IOH_ActionReplay.c`/`IOH_SuperSnapshotV5.c` are commented out with no note on why — ambiguous later whether that was a deliberate call or an unfinished thought. A one-line "why" comment (or deletion, since git retains history) would remove the ambiguity.
- **Open design questions sometimes live only as inline comments**, e.g. `//need all 3?` on `hid3` (`IOHandlers.h:38`) — easy to never resurface. `Known-Issues.md` is now available as a better home for these going forward.
- **A couple of "must stay in sync" relationships are enforced only by comment, not the compiler**, where a cheap compile-time check exists — `IOHandler[]`/`enumIOHandlers` (`_Static_assert` proposed above) and `Menu_Regs.i`/`Menu_Regs.h`. Worth treating "keep in sync with X" comments generally as a prompt to ask whether a build-time check could replace the comment.

**Status:** noted, low priority — not queued for a dedicated pass (2026-08-13).

<br>

[Back to Architecture Overview](Overview.md)
