# Writing your own extension host

A TeensyROM+ extension host is a third firmware image. It lives in its own flash
slot, it is installed and removed without reflashing the board, and while it runs
it owns the machine. This repo ships one — the VM host in
`Source/Teensy/VMBoot`, which loads and runs modules — but nothing about the slot
is specific to it. A host is whatever program you put there.

This document is the host side. The module side, for code that runs *under* this
repo's VM host rather than replacing it, is [`vm/abi/README.md`](../../vm/abi/README.md).

`Source/Teensy/ExampleHost` is a complete, buildable host that does nothing
useful on purpose: it blinks the LED four times and returns to the menu. It
exists so that the four things a host actually owes can each be pointed at.

## Why a host is a separate image at all

Not for isolation, and not for features — for the memory map. A module needs
96 KiB of ITCM and 192 KiB of DTCM at *fixed* addresses, and the ordinary minimal
image cannot give it those while still holding a megabyte of cartridge. So the
build produces a third image with that map and the other two shrink to make room.
`tools/lib/extension-image.mjs` generates the linker script that does it, and
turns a layout regression into a link error rather than a hang on hardware:

| Assert | What it catches |
|---|---|
| `_etext <= 0x18000` | host code growing into the module's ITCM window |
| `_heap_end <= _vm_data_start` | host heap growing into the module's DTCM window |
| `_estack - _vm_data_end >= 49152` | the shared stack falling below 48 KiB |
| `SIZEOF(.bss.dma) == 0` | host globals landing in the guest's RAM2 arena |
| `SIZEOF(.bss.extram) == 0` | a host that needs PSRAM, which the map does not reserve |

Those hold for your host too, because your host is built with the same script.

## The four things a host owes

Everything else is your program. These four are what make it a host.

### 1. The descriptor

```c
__attribute__((used, section(".vmhostid")))
const VmHostId vmHostId = { VM_HOSTID_MAGIC, VM_ABI, 0, 0, "Example", 0 };
```

The main image reads this out of flash *without booting your host*, to name it on
the Installed Extensions page and to decide whether a module's required services
exist. The linker script places the section; nothing in your code picks the
address. `name` is 12 bytes and need not be terminated. `services` is a bitmask of
the module services you provide — `0` is legal and means a module asking for any
service is refused against your host rather than crashing inside it.

A host with no descriptor still runs. The Installed Extensions page says
`Installed, no descriptor.` rather than naming it.

### 2. The marker

Being in the slot is not authorization to run. `@VM1` at `VM_EEP_BOOTNAME_ADDR` is
what says an extension was selected rather than a cartridge. Minimal tests it
itself before it jumps — along with the boot indicator, the menu button and the
EEPROM magic — so a host checking it is taking a second look at what minimal
already agreed to. Cheap, and the right thing to check if you check anything: it
is the only value that means "entered on purpose".

**Do not check the boot indicator the way `MinimalBoot.ino` does.** Minimal has
already replaced `VM_BOOT_EXECUTE_MIN` with `VM_BOOT_FROM_MIN` by the time a host
sees it — it does that first, so a fault in your image cannot trap the cartridge
in a relaunch loop. Testing for `VM_BOOT_EXECUTE_MIN` therefore never passes, and
every launch falls through to the main app, which from the C64 looks exactly like
a host that failed.

### 3. The record

Your host is built `USB_DISABLED`, because the USB stack's buffers live in DMAMEM
and collide with the guest RAM2 arena. So it has no serial, and a failure inside
it is otherwise completely silent — from the couch it looks like "junk on screen,
then the menu came back".

Leave a `VmFailRecord` at `VM_FAIL_BASE` instead. It sits in the cache line below
Teensy's own `CrashReport` at the top of RAM2, survives the soft reset, and the
main image collects it on the way up and prints it on the C64 menu. Layout, magic
and address are published contract in `VMHostABI.h` precisely so a third-party
host can write one this image reads.

`VmFail::set()` handles the cache flush, which is load-bearing: RAM2 is cached and
the reset does not write the cache back.

Use `$50 HostReturned` when your host finishes normally and hands the machine
back. Every other code describes something this repo's host does — `Ok` is
filtered out of the menu report entirely, and `Exited` names a module you may not
have. `detail` is yours; the menu prints it and attaches no meaning to it.

### 4. The way back

`RebootTR()` is a raw MCU reset, so it leaves the boot indicator at whatever was
last written. Minimal wrote `VM_BOOT_FROM_MIN` there before jumping to you, so a
host that never touches the byte is already right. A host that writes
`VM_BOOT_SKIP_MIN` itself has replaced it — the stock host does, to clear the flag
in case power is lost while it runs — and the main image reads `VM_BOOT_SKIP_MIN`
as a cold power up and answers by re-running the user's autolaunch file, sending
the machine somewhere the user did not ask to go.

Write `VM_BOOT_FROM_MIN` to `VM_EEP_BOOTIND_ADDR` first, give the EEPROM write
time to land, then reset. That is right either way, and costs one EEPROM write.

## Building it

Your sketch directory holds exactly one `.ino` plus whatever else it needs, as a
flat directory of files. Those files are overlaid onto the `MinimalBoot` sketch —
which is how the stock host is built too — so you inherit the pin definitions,
the PHI2 ISR and the C64 bus machinery, and you replace the top-level program.
A subdirectory is refused rather than skipped: the overlay copies files and does
not descend, so a `src/` the build silently passed over would leave you holding a
host built without your own code.

```
node tools/build-firmware.mjs --target tr-plus --host-sketch Source/Teensy/ExampleHost
```

Two consequences of the overlay worth knowing before your first build:

- **The sibling `Min_*.ino` files come along and are compiled.** They reference
  globals that `MinimalBoot.ino` defines, and you replaced `MinimalBoot.ino`, so
  your `.ino` has to define them: `RAM_Image`, `BtnPressed`, `EmulateVicCycles`,
  `CurrentIOHandler`, `DriveDirMenu`, `DriveDirPath`, `LOROM_Mask`, `HIROM_Mask`,
  `CmdChannel`, and the four `EEP*` helpers. `ExampleHost.ino` defines exactly
  that set and nothing more, so it doubles as the list.
- **A prototype hoisted above its own constants will not compile.** The sketch
  preprocessor collects prototypes to the top of the concatenated file, so a
  helper whose signature names a `constexpr` defined further down fails with an
  error that blames the definition's line. `Teensy.ino:41` and `VMHost.h:32`
  document the same hazard.

`--host-sketch` is refused rather than ignored where there is no extension slot to
build into (`--target tr`, `--no-extensions`, `--skip-extension-build`): a flag
that silently did nothing there would ship the stock host under your name.

For the same reason, giving `--host-sketch` twice is refused rather than resolved.
This matters because `npm run <script> -- …` appends your argument *after* the
script's own, so `npm run build:example-host -- --host-sketch Source/Teensy/MyHost`
passes `--host-sketch` twice — and the value that used to win was the script's, not
yours. Call `node tools/build-firmware.mjs --target tr-plus --host-sketch <dir>`
directly for your own host; `npm run build:example-host` is only for this repo's
example.

## Packaging and installing it

The `.TRH` container is a 64-byte header plus the raw slot image.

```
node tools/build-host-package.mjs --hex build/firmware/TeensyROM+_<ver>_MyHost_full.hex
node tools/build-host-package.mjs --image my-host.bin --out MYHOST.TRH
```

A `--host-sketch` build is named for the sketch directory it built, so the hex is
`TeensyROM+_<ver>_MyHost_full.hex` rather than the shipping `TeensyROM+_<ver>_full.hex`.
That is deliberate: the slot holds a program this repo did not write, and under
the shipping name the release image and yours are one `ls` apart.

`--hex` lifts the extension image back out of a combined firmware's flash slot;
`--image` takes raw `objcopy -O binary` output. Every check the device applies
before it erases is applied here against the same constants, so a package that
reaches the C64 has already been refused on this side if it was going to be
refused at all. The one thing it cannot check is whether the image is the host you
meant, so it prints the descriptor it found — read that line.

Install it by copying the `.TRH` to the card and selecting it in the menu, or over
USB without touching the card:

```
python3 tools/bench/hostinstall.py MYHOST.TRH
```

Remove it from the C64 menu — `F8` for Settings, `0` for Installed
Extensions, then `u` and `y` — or over USB with
`python3 tools/bench/hostuninstall.py`. `tools/bench/hostcycle.py` runs
the whole install/remove cycle unattended.

USB removal is gated to the device port. The same `ProcessCommand` switch serves
the TCP listener on port 2112 and a USB host serial port, and an ungated
flash-erase token there is two unauthenticated bytes from anywhere on the LAN.

That gate is on the removal token alone. `LaunchFileToken` is handled in the
switch that runs *before* it, on every channel, and a launch still routes a
`.TRH` to `DoHostInstall` — which erases and programs this same slot — and a
`.hex` to `DoFlashUpdate`. So the install direction remains reachable from the
listener, and a board with `NetListenEnable` set is one where the whole command
protocol, not just this token, is exposed to the LAN.

## What you get to use

| | |
|---|---|
| Flash slot | 384 KiB at `0x60280000` |
| ITCM | 96 KiB for host code (module code takes `0x00018000` up) |
| DTCM | everything below `0x20014000`, heap capped at 16 KiB |
| Stack | 48 KiB, shared |
| RAM2 | 512 KiB — the guest arena; **your globals may not land here** |
| PSRAM | none: not reserved by the map |
| USB | none: the image is built `USB_DISABLED` |

If you are not running modules, the ITCM and DTCM windows reserved for them are
still reserved — the linker script is the same one. That is 96 KiB of ITCM and
192 KiB of DTCM your host cannot use, in exchange for a module ABI you are not
using either. A host that will never load a module could generate a different
script; nothing in the loader requires the module windows to exist, only that the
image is linked for the slot and passes `vm_host_slot_valid`.

## Running modules instead

If what you want is code running *under* the stock host rather than a host of your
own, you do not need any of the above. Write a module against
[`vm/abi/README.md`](../../vm/abi/README.md), build it with
`tools/build-extension.mjs`, and it runs on the host already installed:

```
node tools/build-extension.mjs --id HELLO --extensions hi \
  --source vm/hello/hello.cpp --client-source Source/C64/VMHello/vmhello.a
```

`vm/hello/hello.cpp` is the reference module, and `vm/tests/` builds and runs it
against a native fake host on every `npm run verify:extensions`.
