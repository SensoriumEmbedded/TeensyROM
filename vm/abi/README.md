# MHS Power Engine VM ABI 2

The firmware supplies hardware, SD file handles, timing, CRC-protected packets,
video services and reset. A separately downloaded trusted native ARM module
supplies the engine and its C64 client. Compatible VM packages are distributed
separately through the [MPE project](https://github.com/ziggystar12/MHS-Teensy-Rom-Power-Engine/tree/main/vms).

See [the TeensyROM build and installation guide](../../docs/MPE-VM.md) and the
[authoritative ABI header](../../Source/Teensy/MinimalBoot/Common/VMABI.h).

Each package contains /VMS/<id>/manifest.vmi, engine.mvm, client.crt and its
support files. The six-line ASCII manifest contains VM1, package ID, associated
extension list, module filename, client filename, then END. Registry scanning
is bounded to 32 packages. Paths, duplicate associations, header/service bounds
and CRCs are checked before launch. The ABI-2 client descriptor and engine must
be paired; ABI number alone does not guarantee every optional service.

Only one module runs per reset. The stock firmware's ordinary cartridge mode
has its own image and memory configuration. The dedicated VM image initializes
192 KiB ITCM and 320 KiB DTCM:

| Region | Reservation |
| --- | --- |
| ITCM 0x00000000..0x00017fff | Generic host code, 96 KiB ceiling |
| ITCM 0x00018000..0x0002ffff | Module code/constants, 96 KiB |
| DTCM below 0x20014000 | Host state and 16 KiB heap |
| DTCM 0x20014000..0x20043fff | Module data/BSS/support, 192 KiB |
| DTCM 0x20044000..0x2004ffff | Shared stack, 48 KiB |
| RAM2 0x20200000..0x2027ffff | Guest memory, 512 KiB by default |

DoomVM uses optional profile 1: up to 96 KiB of initialized read-only constants
at 0x20268000, with 416 KiB guest memory below it. The loader validates this
profile and service bit, checks payload CRC and applies non-executable MPU
protection. Profile 0 retains the full 512 KiB guest arena. Neither requires
PSRAM, module flash writes or executable code in RAM2.

Optional profile 2 keeps all 512 KiB of RAM2 and restricts module code to
64 KiB (`0x18000..0x27fff`). The host lends a non-executable 32 KiB ITCM tail
and the first 16 KiB of its retired CRT swap arena through `VmHost::auxiliary`.
Only a validated profile-2 module with the auxiliary service bit receives these
spans. The VM image blocks legacy swap access for their lifetime; ordinary
cartridge firmware retains its cache. This supports the current DOS module's
640K layout without borrowing the execution stack or host heap.

The host also accepts the optional center and fitted full-height F5 setup
extensions. Modules lend bounded video workspace; dirty raster hints avoid
reconverting unchanged source cells. Transfers update the hidden display bank
in bounded slices, and workspace stays owned until the frame is acknowledged.
These optional services do not change the requirements of existing ABI-2 clients.

The host provides ABI 2 file read/write/directory services, clock, packets,
native cell video, indexed video, RAM2 constants and indexed-raster services.
Input and presentation policy remain in the client/module, with generic video
mode selection handled by the shared video service. Packets and pending frames
remain immutable until acknowledged. Replay is CRC checked and pauses module
work until the packet is acknowledged. All file access and module callbacks run
in foreground code; the bus handler only accepts bounded transport operations.

The stock boot router consumes the one-shot request before entering the VM
image. Reset/failure returns to the stock menu and skips autolaunch once. There
is no live module unload or live memory repartition. CRCs protect integrity;
modules are trusted native code, not sandboxed applications.
