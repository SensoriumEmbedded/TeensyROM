
# TeensyROM CRT file support (ROM emulation) implementation details

   I wanted to document this implementation as it's been an interesting evolution and I believe it is fairly unique as compared to other cartridges.  There's been a lot of time spent exploring options, trying experiments, and learning the finer nuances of the ARM iMXRT1062 chip implemented on the Teensy 4.1.  I'm sure this story isn't finished yet.  ;)

   All the scenarios below are seemless to the user, they're all happenning "behind the scenes" and should not be noticable in typical use.

## Direct ROM emulation from Teensy RAM
 * For CRT files <~140k, ROM banks (aka "CHIPS") are stores in the RAM1 primary buffer used for general purposes such as this.
 * When the RAM1 buffer is filled, banks are dynamically stored in the RAM2 area of the microcontroller.  This gets us up to about 650K of RAM storage.
 * Files larger than 650K, a secondary stripped-down program is executed in the Teensy. This minimal-boot image has extra RAM space to accomodate CRTs up to ~850KB in size.
   * USB support is removed from this image to save RAM space, so files in this range must load from a micro-SD card.
 * This allows ROM emulation of CRTs up to 850KB directly from the Teensy's RAM that execute 100% as they would on a traditional stand-alone game cartridge.
 * The Teensy's Flash memory and external add-on RAM locations are not quite fast enough to serve the C64. This is beaause they are serial devives and take about 400nS for a random read.  We need about twice that fast to meet the 1MHz C64 bus timing. There are some discussions/attempts to increase speed documented [here](https://forum.pjrc.com/index.php?threads/faster-way-to-read-a-single-byte-from-flash-or-ext-psram.73428/)
 * That left us stuck at an 850KB CRT limit, until...

## Very Large (>850KB) CRT file support:
 * Beta starting in FW v0.6.7
 * Example files in this category "A Pig Quest", "Eye of the Beholder" and "SNK vs Capcom"
 * CRT Files >850Kb employ a bank swapping from SD card (only) mechanism
   * First 850Kb stored in TR RAM as usual, remainder are marked for swapping
   * Uses the DMA signal to halt the CPU for ~3mS during an un-cached bank swap.
     * No actual DMA (bus masterring) takes place
   * Swaps are fast and typically only take place during "scene changes" in games.
     * Should be imperceptible to user experience.
 * 8ea 8K bank RAM cache with lookup to re-use already cached banks without pausing
 * Uses "old school" REU type of DMA assertion for fast pausing and no additional CPU execution
   * Not reliable on some systems (Most C128s and a low percentage of NTSC systems)
   * DMA Pause check utility included in Test+Diags dir to test specific system DMA reliability
     * Written in BASIC with test hooks in TR BASIC commands IO Handler
     * If the check passes, the resulting screen will look like this:
     * ![DMA Check Pass](/media/Screen%20captures/DMA_Check_Pass.png)
 * Many large CRT files have been tested with this scheme, all are working smoothly (as long as host C64 passes DMA check)
 * File sizes of <850KB continue to work as they do today, all served directly out of RAM.
 * Thank you @Boris Schneider-Johne for the general idea behind this capability, very much appreciate the brainstorming!

