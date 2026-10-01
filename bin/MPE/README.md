# MPE host installer

This is the October 1 MPE.TRH tested with stock TeensyROM, packaged HamsterOS and Doom on the owner's NTSC C64 with SIDKick Pico. It supplies the current MPE runtime, desktop, native APP, storage, Ethernet and PCM8 services. Games and desktop packages are supplied separately.

Copy MPE.TRH and the VMS directory to the SD root, preserving paths. Optionally copy MPE-DEMO.MPE for a game-free diagnostic. Select MPE.TRH in the stock SD browser and confirm installation, then fully power the C64 off and on. F8, 0 should identify MHS MPE, ABI 2, services $20. Install only MPE.TRH for MPE use: installing VMBoot.TRH afterward replaces it.

Select your Sys/HAMSTEROS.MPE to use HamsterOS, or select a game directly from the stock menu. A game launched from HamsterOS returns there after pressing and releasing the cartridge Menu/reset button. Holding Menu for two seconds returns to stock. Direct stock-menu game launches and native PRG/ordinary CRT/disk-image launches return to stock. MPE packages run from SD; desktop files and native APP content can also use USB. HamsterOS firmware flashing is unavailable in this host.

The tested stock firmware was built from upstream main 018641a1fce70eba94c49c940ae677758cde0288, after PR #45 merged. PR #43 has since been refreshed onto upstream main with merged #48; no #48 fix is proposed. The installer bytes remain the accepted build. Public descriptor bit 5 is the existing out-of-tree registration discriminator. The private MPE callback table stays inside this host and is not the public stock DMA interface.

MPE.TRH SHA-256: 3883fab8ad17461d677ab40331cbcc7b8dc8f83e152d38c415952c507b864f73

VERIFICATION.json and HARDWARE-TEST.json record software checks and the owner's "its all working" report for the supplied HamsterOS/Doom/Menu-return setup. They do not establish complete-game, PAL hardware, C128 or separately enumerated optional USB/Ethernet acceptance. No games, ROMs, saved games, private renderer source or stock firmware image are included. Copyright and license notices are retained in Notices. See ../../docs/Architecture/MPE-Packaged-Runtimes.md for integration details.
