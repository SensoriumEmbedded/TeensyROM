# Communication Protocols

There are **two distinct, unrelated protocols** in this codebase — don't conflate them.

## 1. Cartridge register protocol (C64 firmware ↔ Teensy firmware, on-board)

Low-level, memory-mapped I/O register block on the cartridge bus — how the C64-side assembly and the Teensy firmware talk to each other across the cartridge port itself. Defined identically in two places that **must be kept in sync manually**:

- C64 side: `Source/C64/MainMenuCRT/source/Menu_Regs.i` (comment: "These need to match Teensy Code: Menu_Regs.h")
- Teensy side: the corresponding `Menu_Regs.h`

Base address `IO1Port = $de00` (`c64defs.i`); everything else is an offset from that. Notable registers/enums found during the survey:

- `rRegStrAvailable` (2), `rRegStreamData` (3) — used by the PRG streaming loader (`PRGLoadStartReloc.s`)
- `wRegControl` (4) with `enum RegCtlCommands` — e.g. `rCtlRunningPRG=5`, `rCtlRebootTeensyROM=11`
- `rwRegStatus` (1) with `enum RegStatusTypes` — e.g. `rsChangeMenu`, `rsLoadSIDforXfer`, `rsMountDxxFile`
- SID play-info/control registers
- An IRQ command channel: `wRegIRQ_ACK` / `rwRegIRQ_CMD` with `enum RegIRQCommands` (e.g. `ricmdLaunch`, `ricmdSIDPause`) — this is what `RemoteControl.ino`'s `DoC64IRQ()` drives from the Teensy side
- `IO2Scratch = $7f` — used by the Expansion Port test app only

**How the two protocols connect:** an external host command (below) typically causes the Teensy to twiddle one of these registers or fire an IRQ, which the running C64 program observes and reacts to. Example: a host `LaunchFileToken` causes the Teensy to eventually drive `ricmdLaunch` / `rCtlRunningPRG` at this register layer — the C64 code never sees the external protocol directly, only its effects here.

## 2. External host protocol (PC/mobile apps ↔ TeensyROM, off-board)

Binary, token-based request/response protocol over USB Serial or Ethernet TCP (port 2112), for controlling TR from an external device — file transfer, launching, SID control, DMA memory read/write (TR+ only), directory listing, etc.

**Fully documented in [docs/ControlComms.md](/docs/ControlComms.md) — read that doc directly rather than duplicating it here.** Key structural points worth flagging up front:

- Command tokens sent big-endian to TR; TR's Ack/replies are little-endian
- Commands split into "always-available" (work even while TR is busy — reset, launch, version, DMA memory access, C64 pause) vs "conditionally-available" (return `FailToken` + `"Busy!"` if not idle — file ops, SID commands, most UI commands)
- Firmware-mode-aware: minimal-boot firmware only accepts a restricted command subset — see [Teensy-Firmware.md](Teensy-Firmware.md#minimalboot-vs-full-firmware)
- Implemented Teensy-side in `Source/Teensy/SerUSBIO.ino` (serial) and `Source/Teensy/ServiceTCP.ino` (Ethernet, routes into the same parser)
- Several independent third-party client implementations exist (TeensyROM-UI, TeensyROM-Web, TeensyROM-CLI, trterm, c64cast) — see the project table in `docs/ControlComms.md`

<br>

[Back to Architecture Overview](Overview.md)
