Thanks for the write-up and the before/after runs over USB, much appreciated!  I reviewed this merged onto current main (49b23c4, which merges cleanly) and built both targets. RAM1 and RAM2 are unchanged on TR and TR+, and flash code grows 88 B (TR+) and 128 B (TR), all in FLASHMEM functions, so no concerns there.

There are three things I'd like you to look at before merging.  As always, feel free to push back on any that are inappropriate or would take significant memory/code to resolve.

### 1. A failed upload leaves a truncated file that blocks the retry (`FileTransfer.ino:119`)

When `ReceiveFileData` times out, it closes the partial file and returns. The checksum-failure path (`FileTransfer.ino:130`) does the same with a full-length but bad file. `GetFileStream` refuses a file that already exists (`FileTransfer.ino:54`), so once the drain has cleared the stream, the client's retry gets "File already exists." The truncated file also stays on the card, where it can be launched as a broken cart or program.

Repro: post a file, stall partway through until the timeout, then post it again.

Suggested fix: in `PostFileCommand`, remove the file when `ReceiveFileData` returns false. That covers both failure paths in one place. Since `GetFileStream` only ever creates a new file, it can't delete anything the user already had.

### 2. The drain's ceiling counts from the start of the command (`FileTransfer.ino:120`, `SerUSBIO.ino:1255`)

`DiscardCmdInput` stops at `SpansCeilingmS(Bytes)` + 2 s, measured from `Began`. Both callers set `Began` at the start of the command and pass the whole length as `Bytes`, so the ceiling is 7 s + len/2 mS from the start. A transfer that averaged under 2 bytes/mS and fails late is already past it. The drain then returns at once and the rest of the payload is parsed as commands, which is the case this PR fixes. It needs a slow link or many pauses just under 500 mS, so it's an edge case, but the fix is small.

Suggested fix: at both call sites, pass `millis()` as `Began` and the bytes still owed as `Bytes`. The ceiling then becomes the remainder's time at 2 bytes/mS plus the slack, counted from the failure.

While you're there: the comments over `SpansCeilingmS` and `DiscardCmdInput` (`SerUSBIO.ino:983-989`) still talk about "the command's deadline", which PostFile and WriteC64Mem no longer have.

### 3. The title and description still describe the deadline

The title is "Put a deadline on remote file and memory transfers", but the deadline was dropped in a84de2e, and the title ends up in the merge commit.

The description also says the flushes are what hang `loop()` when a peer disappears. They were one way, but the writes themselves can block too:
- `EthernetClass::socketSend` waits with no deadline for room in the send buffer.
- `USBSerialBase::write` in USBHost_t36 does the same.

So a reply bigger than the free buffer can still hang with a vanished peer. That seems a better fit for your overall-deadline follow-up than for this PR.

Suggested fix: retitle to something like "Discard what a failed remote transfer still sends, and don't block on flush", and move the write-blocking case into the follow-ups.

### Hardware testing still to do

Your USB runs on a TR+ cover the drain for both commands, with a before/after against a build without this PR.  One of us should probably hit the following at some point before release:

- the same upload stall test on a plain TR, which gets `DiscardCmdInput` for the first time with this PR
- both stall tests over TCP
- a large GetFile over TCP that completes with a matching checksum, since `SendFileData` no longer flushes there
- pulling the Ethernet cable in the middle of a GetFile, to check that the menu recovers
- an Ultimax 8k launch while a TCP client is still connected, on main and on this PR. I suspect this PR fixes a hang on main here, and if so it's worth adding to the description:
  - The launch disables `IRQ_ENET` and `IRQ_PIT` (`DriveDirLoad.ino:386`).
  - The old `CmdChannel->flush()` before the reset (now `Teensy.ino:302`) then waits for an ACK that FNET can't process.
  - So I think main hangs there with the C64 held in reset.
- normal large uploads and downloads with the TeensyROM app over USB and TCP, plus a failed upload followed by the app's retry. MetalHexx offered to test once this was rebased, and his app is the best check that nothing it sends right after a failure gets eaten by the drain.
- after fixes 1 and 2: a failed upload leaves no file and the retry succeeds, and a sender paced under 2 bytes/mS that stalls late is still drained

Everything else I noted is either covered by your follow-ups list or fine as it is.
If any of these would be easier for me to take on, or you'd like a hand with the testing, just let me know.
Thank you!
