# MPE packages with their own runtime

A separately installed MPE.TRH host lets the stock TeensyROM text menu launch
self-contained MGC1 .MPE packages. Each package contains its own ARM engine,
C64 receiver and content. The host supplies loading, files, packets, input,
bounded transfer and return to the menu. The renderer stays in the package.

There is one host slot. MPE.TRH replaces stock VMBoot.TRH; both files may stay
on the card, but only the MPE host needs to be installed to run MPE games.
VMBoot is useful separately for generic VM demos and stock GRANTS diagnostics.

## Registration and ABI boundary

Copy MPE.TRH and VMS/MPE/{manifest.vmi,engine.mvm,client.crt} to the SD card.
The manifest has six lines:

```
VM1
MPE
mpe
engine.mvm
client.crt
END
```

The executable registration module requires the existing out-of-tree discriminator
bit 5 (32, $20). The tested MPE host's public descriptor advertises that bit,
ABI 2 and the 76-byte shared prefix. It does not advertise private MPE callback
meanings under the public DMA or EXIT service bits. Installing only the TRH,
without the three registration files, does not associate .MPE.

Select MPE.TRH through F3 and confirm installation. After it finishes, power the
C64 off and on before the initial game test. F8, 0 should identify MHS MPE,
ABI 2, services $20. An .MPE entry can still show the stock Unk label:
registered extension launch is checked before the ordinary unknown-file fallback.

Selecting an owned game passes its exact path through /VMS/launch.vml. The MPE
host rechecks authorization, the record and manifest CRCs, and the selected MGC1
container. It loads the package's embedded engine and receiver; the registration
engine/client are not substitutes for the game. Renaming MPE to CRT is incorrect.

Private MPE optional callbacks and image profiles differ from the stock ABI.
The public host wire contract covers installation, launch and failure reporting;
the MPE callback table stays inside the MPE host. No extra upstream renderer or
new service assignment is requested by this integration note.

## Tested memory and transport

The tested MPE host independently reserves a 64 KiB ITCM ceiling and accepts its
private 128 KiB package-code profile at 0x10000..0x30000. Its actual code floor
is 61,452 bytes. It preserves module data at 0x20014000..0x20044000, the guest
RAM arena and a minimum 48 KiB host stack. Where the descriptor supports the
code-floor field introduced by #45, the host publishes the actual linker end.

The host validates request bounds, receiver mailbox exclusions and PAL/NTSC
budgets. It holds payloads until completion, prioritizes pending border transfers
over storage work, retains finite failure deadlines, and releases the bus on
failure. This is MPE's existing bounded transport, not a claim that the private
callback table is interchangeable with public #48 c64_write/c64_status. Public
DMA is tested separately through GRANTS; #48 has not been modified by this work.

## Qualification, September 30, 2026

The test firmware combined #45 at d05609b083c8bbcb6855d9a35540c6902d90a111
and unchanged #48 at e95fda16acdedf660522fb2683cda2bb9adca8c7, which includes #46.
Actual stock ARM builds and native loader conformance passed. The maintained MPE
host passed stock preflight, corruption/refusal tests, demo content/save/input,
the emitted 6502 receiver tests, and simulated PAL/NTSC Monkey transport.

On the owner's NTSC C64 with SIDKick Pico, the demo was readable. Monkey initially
showed corrupted graphics immediately after host installation; a full power cycle
cleared it. The owner then confirmed controls, sound, F3 save/F7 restore, and
menu-button return followed by another launch all work. The installation transition
cause is not established. This accepts those scenarios for the tested artifacts,
not complete-game, PAL hardware, C128 or the rest of the MPE catalog.

The host SHA-256 is
71e6d37e1785bc8c2adf348d2c34074b62284a522e73b4812ada96d2aee121ba.
No commercial game data or private renderer implementation is included here.
