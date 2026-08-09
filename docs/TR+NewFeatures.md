
# TeensyROM+ New Features Overview

The TeensyROM+ represents the next generation of TeensyROM hardware (PCB v0.4) and the capabilities that come with it. It runs 100% of the standard TeensyROM feature set from the exact same codebase — nothing is removed or downgraded — and then builds a genuinely new tier of capability on top.

## The Foundation: Real Bus-Mastering DMA

The biggest hardware upgrade in TR+ unlocks several of its most significant new capabilities. The original TR can *assert* the DMA line to pause the C64's CPU — that's how it pulls off tricks like swapping in banks for very large CRT files — but it never actually takes control of the address/data bus itself.

TR+ adds the extra bus-buffering hardware needed to do the real thing: full bi-directional control of the address bus, data bus, and R/*W line, so it can act as a genuine second bus master. That one addition is what unlocks live KERNAL replacement, a real 512KB REU, and remote read/write of C64 memory — none of which are possible on the original hardware.

A handful of other new hardware additions round out the TR+ feature set.

## TR+ Specific Additions

 * **Kernal Replacement** — load a custom KERNAL ROM image, with no other hardware additions or modification.
   * Select any KERNAL image right from the SD/USB file browser (`<Shift-K>`); your choice is remembered in EEPROM
   * Once enabled in Special IO, it takes effect through the normal "Exit to BASIC" (F2) path or when launching a PRG
 * **512KB REU (RAM Expansion Unit)** — a real REU your C64 software can detect and use
   * Pre-load an REU image from a binary file on SD/USB (`<Shift-R>` for any file, or just select it directly if it has an `.reu` extension)
   * While REU is running as the active Special IO, save its current contents with a single button press — never overwrites, each save gets its own auto-numbered filename
   * Validated against real hardware compatibility tools (REU-Checker, CMD 1750/1750XL REU Test) included right in the Test+Diags menu
 * **Freezer Cartridge Support** — Action Replay and Super Snapshot V5 (PAL and NTSC) emulated natively
   * Enter freezer on demand via the new 'Alt' button
 * **Remote DMA Memory Access** — `WriteC64Mem` and `ReadC64Mem` give an external app direct, real-time read/write access to C64 memory over Serial or Ethernet
   * This is the TR+ engine behind [c64cast](https://github.com/kfox/c64cast) — streaming video, and a lot more, straight through your C64
 * **Battery-Backed Real-Time Clock** — a built-in CR1225 coin-cell holder means TR+ remembers/displays the correct time through power cycles, no soldering required
   * Keep BASIC's `TI`/`TI$` variables and any RTC-aware program (like the included Segment/Dot Matrix Clock) accurate from the moment you power on — no Ethernet re-sync needed once it's set
   * The original TR can get there too, but only via a DIY battery mod.
 * **External Reset Detect** — press an external reset (C64U button, User port,  WOPR, or any other custom reset wiring) and TR+ responds exactly as if you'd pressed the TR menu button, dropping you right back into the menu
 * **Programmable Alternate Button** — the "Alt" button (to the left of the menu button) is assignable to whatever's most useful when it isn't busy running a freezer cart or REU:
   * **AutoLaunch** (default) — jump straight into your favorite game or utility with one press
   * **Pause/Unpause** — freeze the C64/128 mid-session
     * The button LED flashes while frozen so you always know the state
   * Or: jump to the TR Menu, Reboot TR, or disable it entirely
 * **TR+ Expansion Port Test** — a full hardware validation suite built for the real cartridge port, only possible because of TR+'s true bus-mastering DMA
   * Exercises every expansion port signal individually: address/data bus, DMA, BA, R/*W, IO1/IO2, IRQ, NMI, and all cart control lines (ROMH/ROML/GAME/EXROM)
   * Walking Ones and Cascading Ones bus tests, with a looping mode for extended burn-in testing

## Using TR+ with a Commodore 64 Ultimate / Ultimate64

If you're running TR+ on a C64 Ultimate or Ultimate64, we recommend setting **`Bus Operation Mode` to `Writes`** in the C64U's settings. This is needed for proper compatibility with TR+'s DMA-based features (Freezer Cartridge Support, REU, KERNAL Replacement, and more) — it's safe to leave on, so there's no downside to setting it even if you're not using those specific capabilities yet.

## Q&A

**What does this mean for support of the current TR design?**
Both designs will live on in parallel and continue to receive future firmware updates. However, some new features will only be available on the TR+ hardware.

**Are all the existing TeensyROM features available in the TR+?**
Absolutely! The new capabilities add to the existing ones, but **none** are lost.

**Can I update my TR to a TR+?**
Technically yes, but not easily. Updating an existing TR requires multiple trace cuts, pin lifts, wires, and kludged-on components. Contact me directly if you are interested in pursuing this further.
