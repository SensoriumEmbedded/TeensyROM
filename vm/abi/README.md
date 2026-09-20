# TeensyROM extension ABI

An **extension** is natively-compiled ARM code, shipped on the SD card, that
TeensyROM loads and runs on its own processor while the C64 handles display,
sound and input. The firmware in this repository is the **loader**: it finds
packages, validates them, reserves the memory, and carries bytes between the
module and its C64 client. It knows nothing about what any extension does.

This document is the whole contract. A module needs
[`VMABI.h`](../../Source/Teensy/MinimalBoot/Common/VMABI.h) and nothing else
from this repository to build.

> **Status.** The formats and the base profile described here are frozen; the
> version marker is `VM_ABI = 2`. The launch path, the module loader and the
> client link have run on a TeensyROM+ with the reference extension: it appears
> on the C64 screen. [§10](#10-what-has-run-on-hardware) says exactly which parts
> that covers and which it does not. The rest is verified on a development
> machine by `npm run verify:extensions`, which builds real packages and reads
> them back with the firmware's own parsers.

## 1. The base profile, and why the loader stops there

`VmHost` is the table of services the loader lends a module. It grows only at
its tail, and `host->bytes` says how far this host's copy actually runs. The
loader implements the first 76 bytes of it — files, clock, packets, write and
guest RAM — and stops.

That stopping point is not arbitrary. It is the same offset that a host with
video callbacks appended has at `offsetof(VmHost, video_present)`, so **a module
built against either header is binary compatible at the base**. A module asks
for exactly what it needs and gets a straight answer:

```c
if (!h || h->abi != VM_ABI || h->bytes < VM_HOST_BASE_BYTES) return nullptr;
```

Check `bytes >= VM_HOST_BASE_BYTES`, never `bytes == sizeof(VmHost)`. That one
line is what lets a single module binary run on this loader *and* on a host that
has appended callbacks of its own.

The loader stops here so that extensions can move independently of TeensyROM
releases. If a capability lives in the loader, adding to it means a firmware
release; if it lives behind a service bit, it does not.

## 2. Asking for more than the base

A module declares what it requires in its image header, in `required_services`.
The loader compares that against what it provides and **refuses the image
outright** if anything is missing. It never loads a module with a requested
service quietly absent.

That refusal is the negotiation. Build a capability behind its own bit, and on a
host that lacks it, fall back and retry without it. One binary, two hosts.

| Bit | Name | Provided by the loader |
|----:|------|------------------------|
| 1 | `VM_SERVICE_FILES` | yes |
| 2 | `VM_SERVICE_CLOCK` | yes |
| 4 | `VM_SERVICE_PACKETS` | yes |
| 8 | `VM_SERVICE_WRITE` | yes |
| 16 | `VM_SERVICE_GUEST_RAM` | yes |
| 128 | `VM_SERVICE_RAM2_RO` | yes (memory profile 1) |
| 32 | video transport | **reserved** |
| 64 | indexed video | **reserved** |
| 256 | indexed raster | **reserved** |
| 512 | RAM1 auxiliary spans | **reserved** |

The reserved bits are assigned to known out-of-tree extensions. The loader
refuses them, but **no future loader release will reuse those numbers for
anything else** — so an extension can define its own host tail and its own
service bits without ever colliding with a TeensyROM change.

Two rules keep that promise workable:

1. **`VmHost` only ever grows at the tail.** Never insert, never reorder.
2. **Capability, then fallback.** If a host rejects a bit, retry without it.

Memory profile `2` is likewise reserved and refused; profiles `0` and `1` load.
Profile `0` lends the whole 496 KiB RAM2 arena; profile `1` keeps its upper
80 KiB as write-protected constants loaded from the image, leaving 416 KiB.

## 3. The package

A package is a directory on the SD card holding exactly three files:

```
/VMS/<id>/manifest.vmi     six ASCII lines
/VMS/<id>/engine.mvm       the module image
/VMS/<id>/client.crt       the C64 client cartridge
```

`<id>` is 1–23 characters of `A–Z a–z 0–9 _ - .` and must match line 2 of the
manifest. The loader scans at most **32** directories under `/VMS`; more than
that, or two packages claiming the same extension, refuses the launch rather
than guessing.

### manifest.vmi

Six newline-terminated lines, nothing else:

```
VM1
HELLO
hi
engine.mvm
client.crt
END
```

Line 3 is a comma-separated list of file extensions this package claims, at most
**7 characters in total** (`gb,gbc` fits). Extensions the stock menu owns are
refused: `prg crt hex p00 sid kla koa ocp pic art aas hpi txt nfo md seq d64 d71
d81 reu`.

### engine.mvm — the MVM1 image

A 64-byte little-endian header, then `.text`, then `.data`, then (profile 1
only) the RAM2 constants. `.bss` is not stored; the loader zeroes it.

| Offset | Field | Notes |
|-------:|-------|-------|
| 0 | `magic` | `0x314d564d` (`MVM1`) |
| 4 | `abi` | 2 |
| 8 | `header_bytes` | 64 |
| 12 | `code_bytes` | ≤ 96 KiB |
| 16 | `data_bytes` | |
| 20 | `bss_bytes` | `data_bytes + bss_bytes` ≤ 192 KiB |
| 24 | `entry` | Thumb bit set; inside the code window |
| 28 | `code_base` | `0x00018000` |
| 32 | `ram_base` | `0x20014000` |
| 36 | `required_services` | see §2 |
| 40 | `payload_crc` | CRC32 of everything after the header |
| 44 | `header_crc` | CRC32 of the header with this field zeroed |
| 48 | `reserved[0]` | memory profile |
| 52 | `reserved[1]` | RAM2 constant bytes (profile 1, 1..80 KiB) |
| 56 | `reserved[2..3]` | zero |

CRC32 throughout is the reflected `0xedb88320` polynomial.

### client.crt — the C64 client

An ordinary 16 KiB EasyFlash cartridge (`.crt` type 32) with a third CHIP
appended. **Exactly `0x6070` bytes**; any other size is refused.

| Offset | Contents |
|-------:|----------|
| `0x0000` | 64-byte CRT header, `C64 CARTRIDGE   `, type 32 |
| `0x0040` | CHIP header — bank 0, `$8000`, 8192 bytes |
| `0x0050` | LOROM bank, 8192 bytes |
| `0x2050` | CHIP header — bank 0, `$A000`, 8192 bytes |
| `0x2060` | HIROM bank, 8192 bytes |
| `0x4060` | CHIP header — bank 1, `$8000`, 8192 bytes |
| `0x4070` | **128-byte `VMH1` descriptor** |
| `0x40F0` | padding to `0x6070` |

The descriptor:

| Offset | Field |
|-------:|-------|
| 0 | `VMH1` |
| 4 | ABI (2) |
| 8 | CRC32 of the two 8 KiB banks, concatenated |
| 16 | package id, NUL terminated, within 24 bytes |
| 124 | CRC32 of the preceding 124 bytes |

The descriptor is how a `.crt` on the card is recognised as a client rather than
an ordinary cartridge. A cartridge without a valid `VMH1` at `0x4070` is parsed
by the stock cartridge path exactly as before, whatever its size.

Build all three with [`tools/lib/extension.mjs`](../../tools/lib/extension.mjs)
rather than by hand; it enforces every rule above at write time.

## 4. Launching

The menu calls `VmLaunch::tryFile` for each SD selection. It returns `false` —
meaning "not mine, carry on" — for every source that is not the SD card, every
path inside a mounted disk image, and every extension the stock menu owns. Only
then does it look at the card.

A launch begins in one of two ways:

- the user picks a **client cartridge**, identified by its `VMH1` descriptor; or
- the user picks a **content file** whose extension a manifest claims, in which
  case its full path is handed to the module as `content_path`.

Which extensions are claimed is answered from a table the firmware rebuilds when
it loads an SD listing, so a package copied onto the card while a listing is
already on screen is picked up on the next listing. Over the 32-package limit, or
on a read error, the table stands aside and the launch path scans `/VMS` itself,
which refuses on screen rather than silently handing the file to the stock menu.

Then:

1. `preflight` re-validates the module image and the client cartridge in full,
   including both CRCs. A failure reports on screen and does **not** reboot.
2. A `launch.vml` record is written to `/VMS` and read back to confirm.
3. A one-shot EEPROM flag is set and the machine resets.
4. The boot image sees the flag and jumps to the extension image, which reserves
   the module's memory before anything else claims it.
5. The extension image loads the client cartridge into RAM as EasyFlash banks,
   loads the module, and calls its entry point.

The extension image lives in its own flash slot at `0x60280000..0x602e0000`, so
the main and minimal firmware images keep their own addresses.

## 5. The runtime

### Memory

| Region | Address | Size | Owner |
|--------|---------|-----:|-------|
| ITCM | `0x00000000` | 96 KiB | host code |
| ITCM | `0x00018000` | 96 KiB | **module code** |
| DTCM | below `0x20014000` | — | host state and heap |
| DTCM | `0x20014000` | 192 KiB | **module `.data`, `.bss`, then workspace** |
| DTCM | `0x20044000` | 48 KiB | shared stack |
| RAM2 | `0x20200000` | 496 KiB | **guest arena** (416 KiB on profile 1) |
| RAM2 | `0x2027c000` | 16 KiB | firmware-reserved — see below |

`workspace` is whatever is left of the 192 KiB DTCM window after the module's
own `.data` and `.bss`, aligned up to 32 bytes. Size it by shrinking your static
footprint, not by asking for more.

RAM2 is 512 KiB, but the top 16 KiB are not yours. Teensy's core keeps its
`CrashReport` in the last 128 bytes of it, and the loader leaves the reason a
launch failed just below the arena, where the main firmware image reads it after
the reset. Take the size from `host->guest_ram_bytes`, never from the physical
end of RAM2: a module that writes to `0x20280000 - 1` destroys the crash report
that would have explained why it later died.

While a module is being loaded the host makes its code window writable and
non-executable, then restores it to read-only and executable before the entry
point is called. Modules are trusted local code — this is a guard against
mistakes, not a security boundary.

### Entry point

```c
VM_MODULE_ENTRY const VmModule *vm_entry(const VmHost *host);
```

Exactly one per module, placed in `.entry` by the `VM_MODULE_ENTRY` macro and
linked first by [`module.ld`](module.ld). Return a pointer to a `VmModule`
living in module code or data, or `nullptr` to refuse the host. The loader
re-validates every pointer in the returned table before it calls any of them.

### The four callbacks

| Callback | Called when |
|----------|-------------|
| `input` | the client has sent an input record |
| `pump`  | once per scheduler turn, to advance your work in a bounded slice |
| `packet`| to collect the next packet, or `false` for "nothing to say" |
| `ack`   | the client has acknowledged the published packet |

The loop is: `input` if any, then `pump`, then `packet`. A published packet is
**frozen** until `ack` — `pump` may keep running but must not alter it, and
`packet` will not be called again for a new one. Returning `false` from `packet`
is normal and costs nothing; do not invent filler packets to keep the link busy.

Call `host->should_yield()` inside long work and return promptly when it is
true. Nothing preempts you: a module that does not yield makes the machine
unresponsive.

### Packets

```c
struct VmPacket { uint8_t type, flags, length, reserved; uint8_t payload[228]; };
```

`type` must be non-zero, `length` at most 228, `reserved` zero. **The host never
looks inside `payload`.** It frames the packet, hands it to the client, and
tells you when the client acknowledged it. What the bytes mean is entirely
between a module and its own client, so a new protocol needs no firmware change.

[`../hello/hello.cpp`](../hello/hello.cpp) defines a two-field text protocol in
about ten lines, which is the intended pattern rather than a shortcut.

## 6. The C64 client

The client is a 16 KiB EasyFlash cartridge that TeensyROM presents to the
machine like any other. Everything a client needs is in the IO2 window at
`$DF00`. Selecting **EasyFlash bank 58** (`$DE00` = 58) is what opens it; the
firmware maps every bank to the same 16 KiB, so the write is a signal, not a
bank switch. Until then the window is an ordinary EasyFlash register area.

| Address | Direction | Meaning |
|--------:|:---------:|---------|
| `$DF00..$DFEF` | read | the published packet |
| `$DFF4` | write | command: `1` run, `3` input ready, `4` quiet |
| `$DFF5` | read | status: `2` running, `$12` quiet, `$E0` failed |
| `$DFF6` | write | acknowledge: the sequence number you consumed |
| `$DFF7` | read | sequence of the packet now published, `0` for none |
| `$DFF8..$DFFA` | write | input: buttons, display, overflow |
| `$DFFB` | read | failure code, when status is `$E0` |
| `$DFFD..$DFFF` | write | input: protocol, token, checksum |

**Start.** After the cold start, select bank 58 and write `1` to `$DFF4`. The
module does not run until it sees that. Poll `$DFF5`: `$E0` means the host or the
module failed and `$DFFB` says why; the reference client prints
`extension failed` and stops.

`1` means *run*, and writing it is idempotent: before the start it starts the
module, while the module is quiet it resumes it, and while the module is already
running it does nothing. Write `1` whenever you want the module running, without
tracking which of the three states the host is in.

**Reading a packet.** When `$DFF7` is non-zero and differs from the last sequence
you consumed, copy the window, then validate the copy — the window is live, and
checking bytes that can change underneath you proves nothing. The frame is:

```
$DF00  'M' '3'   magic
$DF02  01        framing version
$DF03  type      module-defined, never 0
$DF04  sequence  1..255, wraps 255 -> 1
$DF05  flags     module-defined
$DF06  length    0..228
$DF07  00        reserved
$DF08  payload   length bytes
       crc16     little-endian, over everything above it
```

The CRC is CRC-16/CCITT-FALSE: polynomial `0x1021`, initial value `0xFFFF`, no
reflection, no final XOR. After consuming a valid packet, write its sequence to
`$DFF6`. Until you do, the module holds that packet and asks for no other. A bad
frame is simply not acknowledged. Unknown types should be acknowledged and
ignored, so a module can add one without breaking older clients.

**Going quiet.** Write `4` to `$DFF4` to stop the module running; `$DFF5` reads
`$12` while it is stopped. Acknowledging the outstanding packet lifts it, and so
does writing `1`, which is the only way back if nothing was outstanding when you
asked.

**Sending input.** Build the whole record first, then raise the command: put the
values in `$DFF8..$DFFA` and `$DFFD`, a token in `$DFFE`, the checksum in
`$DFFF`, and write `3` to `$DFF4`. The checksum is `$A5` XOR each of `$DFF8`,
`$DFF9`, `$DFFA`, `$DFFD`, `$DFFE`. The token must be non-zero and differ from
the last one, or the host ignores the record. What the four bytes mean is
between a module and its client.

**The cartridge starts in Ultimax.** The firmware presents every client with
`GAME` asserted and `EXROM` deasserted, as its EasyFlash emulation does, so
your ROMH bank answers at `$E000` and its reset vector at `$FFFC` is the entry
point. A client normally wants to run as a 16 KiB cartridge instead. Switch by
writing `$87` to `$DE02` — but the switch takes effect at once, ROMH leaves
`$E000` and the KERNAL appears there, so **the next instruction cannot be
fetched from your stub**. Copy the last three instructions
(`lda #$87 / sta $DE02 / jmp $FCE2`) to RAM and run them there. This is how
EasyFlash's own startup code does it, and forgetting it leaves the C64 running
KERNAL code from the middle of a routine with a freshly reset stack: no screen
clear, no message. `Source/C64/VMHello/vmhello.a` is a working example.

## 7. When a launch fails

The extension image has no working USB, so it cannot explain itself. Instead it
leaves a 32-byte record in the reserved top of RAM2 and resets; the main image
collects it before anything can overwrite it and the menu prints it once the
C64 is waiting to read messages. If three attempts go unread — the C64 is not in
a wait loop and each one times out — the record is dropped and the reason is left
on the Teensy's USB serial only. A successful hand-off is recorded too, as
`$00`, so a client that then fails to draw is distinguishable from a host that
never started.

| Code | Meaning |
|-----:|---------|
| `$00` | the client is up |
| `$01` | minimal jumped to the extension image and it did not start |
| `$02` | the top flash slot holds no valid image |
| `$10` | SD card would not initialise (detail: attempts) |
| `$11` | `launch.vml` missing, short or corrupt |
| `$12` | manifest unreadable or malformed |
| `$13` | manifest changed since preflight |
| `$14` | client cartridge would not open |
| `$15` | not a 16 KiB C64 EasyFlash cartridge |
| `$16` | CHIP header or bank payload bad (detail: bank) |
| `$17` | third CHIP is not a valid `VMH1` descriptor |
| `$18` | client banks do not match the descriptor CRC |
| `$20` | module image refused (detail: the host's failure code) |

The failure code a client reads from `$DFFB` is separate, and is the same value
the record carries as its detail for `$20`: `$11` the image would not open, has
a bad header, or requires a service this host does not provide; `$14` the
returned `VmModule` table failed validation; `$15` the module published a
malformed packet; `$16` the module reported an error without giving a code.
Other values come from the image loader's own bounds and CRC checks.

## 8. Building a module

The packager does the whole job — compile, link, measure, package:

```
node tools/build-extension.mjs --id HELLO --extensions hi \
     --source vm/hello/hello.cpp --client build/c64/vmhello.bin
```

It reports the code, data and bss it measured and how much workspace is left,
and reads its own output back through the firmware's validation before writing
it. It finds the ARM toolchain from `--toolchain`, then `PATH`, then the Teensy
core's own bundled copy — the same compiler the firmware is built with.

Building by hand, if you need to:

```
arm-none-eabi-g++ -std=c++17 -Os -mcpu=cortex-m7 -mthumb \
    -mfloat-abi=hard -mfpu=fpv5-d16 \
    -ffreestanding -fno-exceptions -fno-rtti -fno-threadsafe-statics \
    -ffunction-sections -fdata-sections \
    -c hello.cpp -o hello.o
arm-none-eabi-g++ -nostdlib -Wl,--gc-sections -T vm/abi/module.ld \
    hello.o vm_runtime.o -o hello.elf
```

The float ABI must match: Cortex-M7 with the double-precision FPU. A module
built for a different one links cleanly and then faults on target.

`module.ld` asserts the 96 KiB code limit and the 192 KiB data limit at link
time, so an overflow is a linker error rather than a boot failure.

There is no libc, no heap and no static constructors. Keep state in `.data` and
`.bss` and initialise it in `vm_entry`.

One wrinkle worth knowing: GCC emits calls to `memset` and `memcpy` from
ordinary C++ that never mentions them — zeroing a struct is enough — and there
is no libc to resolve them against. [`vm_runtime.c`](vm_runtime.c) supplies
those and a few neighbours as **weak** definitions, and the packager links it
into every module. Define your own if you want a faster one; yours wins with no
build changes.

For reference, the module in `vm/hello` builds to 776 bytes of code, 32 bytes of
data and 192 bytes of bss, leaving 196,384 bytes of workspace.

## 9. Testing without hardware

[`../tests/native_host.h`](../tests/native_host.h) is a complete base-profile
host over the real filesystem — same 24-handle limit, same path validation, same
refusals as the target. Compile your module for the development machine and run
it against that host to debug it, then cross-compile the identical source.

`npm run verify:extensions` runs the whole loader suite this way in a few
seconds: package format, file services, image validation, registry and
preflight, menu-hook fall-through, failure reporting, and the reference module
end to end.

A pass there says the formats and the contract hold. It says nothing about
timing, the bus, or the C64 side — that needs the hardware.

## 10. What has run on hardware

On a TeensyROM+ with the reference extension, from the SD card through to the
C64 screen:

| Verified on hardware | |
|----------------------|:-:|
| Launch record, manifest and client validation in the extension image | yes |
| Module loaded into ITCM/DTCM, entry point called, `VmModule` table accepted | yes |
| Client cartridge cold start from the Ultimax reset vector | yes |
| Bank 58 opens the IO2 window; `start` handshake | yes |
| Four packets published, framed, CRC-checked by the client and acknowledged | yes |
| Guest arena size (`507904` bytes reported by the module) | yes |
| Failure record written by the host and read back on the menu | yes |
| Input records (`$DFF4` = 3): joystick fire in the reference client reaches the module, which recolours its text | yes |
| `quiet` and resume (`$DFF4` = 4 / 1) | **no** |
| The client-side `extension failed` path | no |
| Memory profile 1 (write-protected constants) | no |
| PAL timing | no |

Nothing in the verified rows depends on timing beyond the ordinary EasyFlash bus
handling, since the base profile never becomes bus master. Treat the rows marked
**no** as untested rather than as working.

The board is returned to the menu by the reset button. The alternate button is
not serviced while an extension runs.
