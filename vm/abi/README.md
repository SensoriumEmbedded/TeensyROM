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
> version marker is `VM_ABI = 2`. Everything below has been verified on a
> development machine by `npm run verify:extensions`, which builds real packages
> and reads them back with the firmware's own parsers. None of it has been run
> on hardware yet.

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
| 52 | `reserved[1]` | RAM2 constant bytes (profile 1) |
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
| RAM2 | `0x20200000` | 512 KiB | **guest arena** (416 KiB on profile 1) |

`workspace` is whatever is left of the 192 KiB DTCM window after the module's
own `.data` and `.bss`, aligned up to 32 bytes. Size it by shrinking your static
footprint, not by asking for more.

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

## 6. Building a module

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

## 7. Testing without hardware

[`../tests/native_host.h`](../tests/native_host.h) is a complete base-profile
host over the real filesystem — same 24-handle limit, same path validation, same
refusals as the target. Compile your module for the development machine and run
it against that host to debug it, then cross-compile the identical source.

`npm run verify:extensions` runs the whole loader suite this way in a few
seconds: package format, file services, image validation, registry and
preflight, menu-hook fall-through, and the reference module end to end.

A pass there says the formats and the contract hold. It says nothing about
timing, the bus, or the C64 side — that needs the hardware.
