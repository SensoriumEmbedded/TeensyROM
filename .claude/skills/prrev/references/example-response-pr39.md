Thanks for the thorough write-up and the NTSC runs, much appreciated!  I reviewed this merged onto current main (49b23c4, which merges cleanly) and built both targets. RAM1 variables, free-for-locals and RAM2 are unchanged on TR and TR+. ITCM code grows 576 B (TR+ main), 448 B (TR main) and 128–192 B (minimal), all in functions that were already in RAM, so no concerns there.

There are three things I'd like you to look at before merging.  As always, feel free to push back on any that are inappropriate or would take significant memory/code to resolve.

### 1. A failed TR remote launch still leaves the menu type switched (`RemoteControl.ino:202`)

The TR branch now keeps the SD listing intact, which fixes the freed names, and your test confirms the listing stays readable. But by the time the TR lookups fail, `RemoteLaunched` is set, `IO1[rWRegCurrMenuWAIT]` is already `rmtTeensy`, and `VmRegistry` has been refreshed. The C64 still shows the SD listing, and picking anything from it goes down `HandleExecution`'s `rmtTeensy` case:

- A file does `memcpy(RAM_Image, MenuSelCpy.Code_Image, MenuSelCpy.Size)` (`DriveDirLoad.ino:311`). `Code_Image` and `Size` are never set for card items.
- `IOHandlerSelectInit` then reads an `IOHndlrAssoc` that's never set either.
- A folder goes to `SetMenu((StructMenuItem*)Code_Image, …)`.

Repro: browse SD, remote-launch `TR:/Games/NoSuchFile` (or press a hot key saved for a TR item that has since been renamed), then select any file in the listing.

Suggested fix: both `FindTRMenuItem` lookups have no side effects, so resolve the TR folder and file right after splitting the path. Return from there, before `RemoteLaunched = true`, `IO1[rWRegCurrMenuWAIT] = MenuSourceID`, `SDFullInit` and `VmRegistry::refresh`.

### 2. Paths of 256–357 characters are refused when they don't need to be, and silently (`RemoteControl.ino:193`)

On main these overflowed `DriveDirPath`. In today's builds the overflow lands in `EZFlashRAM` (the next 256 bytes, in both the TR and TR+ main images), so in practice they launched. Now they're refused. `LaunchFile` has already sent its second `AckToken` (`SerUSBIO.ino:803`), so the app sees success and nothing happens. Your 363-byte test hit `ReceiveFileName`'s own limit, so this range wasn't covered.

Only the folder part has to fit in `DriveDirPath`/`LaunchDir`. The file name can keep pointing into `FileNamePath`, and `LoadFile` builds the full path in a `MaxNamePathLength` buffer. So:

- Check the folder length (`strrchr(FileNamePath, '/') - FileNamePath`) against `MaxPathLength` instead of the whole length, and copy just the folder into `LaunchDir`.
- For anything still refused, have `LaunchFile` reply `FailToken` + "Path too long!" before the second ack, the way `ReceiveFileName` already does. The app already handles that reply.

The consumers that need the whole path in 256 bytes (the CRT boot name and `SetLatestSIDLoaded`) already refuse on their own in this PR.

### 3. Message wording (`FileParsers.ino:189`)

"Path too long to boot / this crt from" reads as though something is missing. Maybe "CRT path too long / to boot from SD"?

### Hardware testing still to do

Your NTSC runs cover the long text file, the looping D64, menu browsing and a remote PRG launch.  One of us should probably hit the following at some point before release:

- selecting on page 2+ of a big listing; the next/previous file-type keys; D64/D71/D81 browsing; the reload after a remote file change
- a large CRT from SD (it frees the listing mid-load, which now redirects to the TR menu), one big enough for the MinimalBoot reboot, and REU / Swiftlink / Retro Replay / SSv5 launched from SD, each followed by a reset back to the menu
- remote launch from SD root and a subfolder, USB, and TR root and a subfolder; hot keys; autolaunch at power-up; NFC tags (including random) if you have a reader
- after fixes 1 and 2: the failed TR launch followed by selecting an SD item, and a path of 256–357 characters
- a background SID set from SD and from the TR menu, then a power cycle
- setting a hot key, auto launch, REU file, KERNAL file, NFC tag and Meatloaf mount with normal paths
- TR BASIC TSAVE over an existing file, and TDIR of a big folder (the "Too many files" cutoff moved)
- MinimalBoot EasyFlash bank swapping (e.g. SNKvsCAP)
- PAL, and a plain TR board

Everything else I noted is either covered by your follow-ups list or fine as it is.
Thank you!
