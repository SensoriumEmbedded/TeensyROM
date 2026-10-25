
# TeensyROM CRT file support (ROM emulation) implementation details

   I wanted to document this implementation as it's been an interesting evolution and I believe it is fairly unique as compared to other cartridges.  There's been a lot of time spent exploring options, trying experiments, and learning the finer nuances of the ARM iMXRT1062 chip implemented on the Teensy 4.1.  I'm sure this story isn't finished yet.  ;)

   All the scenarios below are seamless to the user, they're all happening "behind the scenes" and should not be noticeable in typical use.

## Direct ROM emulation from Teensy RAM
 * For CRT files <~128k, ROM banks (aka "CHIPS") are stored in the RAM1 primary buffer used for general purposes such as this.
 * When the RAM1 buffer is filled, banks are dynamically stored in the RAM2 area of the microcontroller.  This gets us up to about 650K of RAM storage.
 * Files larger than 650K, a secondary stripped-down program is executed in the Teensy. This minimal-boot image has extra RAM space to accommodate CRTs up to roughly 850KB in size.
   * USB support is removed from this image to save RAM space, so files in this range must load from a micro-SD card.
 * This allows ROM emulation of CRTs up to 850KB directly from the Teensy's RAM that execute 100% as they would on a traditional stand-alone game cartridge.
 * The Teensy's Flash memory and external add-on RAM locations are not quite fast enough to serve the C64. This is because they are serial devices and take about 400nS for a random read.  We need about twice that fast to meet the 1MHz C64 bus timing. There are some discussions/attempts to increase speed documented [here](https://forum.pjrc.com/index.php?threads/faster-way-to-read-a-single-byte-from-flash-or-ext-psram.73428/)
 * That left us stuck at an 850KB CRT limit, until...

## Extended EasyFlash images

 * CRT hardware type 32 supports read-only images with 64, 128 or 256 banks (1, 2 or 4 MiB).
 * Each accepted CHIP contains 8 KiB: ROML at `$8000` and ROMH at `$A000` or `$E000`. In MinimalBoot, omitted halves read as erased flash (`$FF`).
 * MinimalBoot uses the highest accepted bank to select the `$DE00` mask: `$3F`, `$7F` or `$FF`. Ordinary 1 MiB images retain six-bit wrapping; the main image and extension host retain their 64-bank decoder.
 * The main image keeps its static 128-entry CHIP table. Any EasyFlash bank of 64 or above uses MinimalBoot even in a small sparse file. MinimalBoot holds up to 512 unique bank halves on the RAM2 heap; other cartridge types retain their 128-CHIP limit.
 * A later duplicate half replaces the earlier data without using another table slot. `$A000` and `$E000` address the same ROMH half. Packet-length fields are ignored, and CHIP types other than 0 and 2 are skipped using their declared ROM size.
 * Accepted EasyFlash records require a bank below 256, an 8 KiB size and one of the addresses above. Short headers/payloads, failed reads and allocation failures produce a reason. Swapped payloads must fit the reader's 24-bit file-offset range, including their last byte; longer duplicate/skipped-packet streams are refused if they require an unrepresentable swap offset. A failed runtime SD bank read returns to the main menu.
 * Extended images use the existing MinimalBoot SD bank-swap mechanism below. Launch them from SD; USB launches that need MinimalBoot are refused with a message. No host extension or external PSRAM is needed. EAPI writes remain unsupported.

### Loader regression fixtures

`node --test Source/Teensy/tests/easyflash.test.js` compiles the production MinimalBoot loader and handlers against native SD/register stand-ins. Focused main-image checks cover the static table boundary and SD-only high-bank handoff.

Generate original bank-identification data without game assets:

```
node Source/Teensy/tests/easyflash-fixtures.mjs --mib 2 --output ef2.crt
node Source/Teensy/tests/easyflash-fixtures.mjs --mib 4 --sparse --reverse --output ef4-sparse-reverse.crt
```

The generator refuses to overwrite a file. These are loader fixtures, not runnable C64 applications. Each half starts with its bank number, half number and bank XOR `$5A`, followed by a deterministic pattern and a readable bank label. `--sparse` omits selected halves; `--reverse` puts bank zero last.

## Very Large (>850KB) CRT file support:
 * Beta starting in FW v0.6.7
 * Example files in this category "A Pig Quest", "Eye of the Beholder" and "SNK vs Capcom"
 * CRT Files >850Kb employ a bank swapping from SD card (only) mechanism
   * First 850Kb stored in TR RAM as usual, remainder are marked for swapping
   * Uses the DMA signal to halt the CPU for ~3mS during an un-cached bank swap.
     * No actual DMA (bus mastering) takes place
   * Swaps are fast and typically only take place during "scene changes" in games.
     * Should be imperceptible to user experience.
 * 16ea 8K bank RAM cache with lookup to re-use already cached banks without pausing
 * Uses "old school" REU type of DMA assertion for fast pausing and no additional CPU execution
   * Not reliable on some systems (Most C128s and a low percentage of NTSC systems)
   * DMA Pause check utility included in Test+Diags dir to test specific system DMA reliability
     * Written in BASIC with test hooks in TR BASIC commands IO Handler
     * If the check passes, the resulting screen will look like this:
     * ![DMA Check Pass](/media/Screen%20captures/DMA_Check_Pass.png)
     * If the check fails ("CHECK PASSED" not displayed), we recommend trying once or twice more. Consistent fails indicate games of this size won't run reliably on this C64. They may load fine initially, but could fail any time later in the game.
 * Many large CRT files have been tested with this scheme, all are working smoothly (as long as host C64 passes DMA check)
 * File sizes of <850KB continue to work as they do today, all served directly out of RAM.
 * Thank you @Boris Schneider-Johne for the general idea behind this capability, very much appreciate the brainstorming!

