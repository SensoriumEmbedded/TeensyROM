# TeensyROM
**A ROM emulator, super fast loader cartridge, MIDI and Ethernet interface for the Commodore 64, based on the Teensy 4.1**
*Design by Travis Smith/Sensorium ([e-mail](mailto:travis@sensoriumembedded.com))* 

Although there are several emulators/loaders out there, I really wanted to design one around the Teensy 4.1 to take advantage of all its interface capabilities.  I also wanted to take advantage of its many pins to do "direct" interfacing so it can be largely software defined.  Oh, and it's also just a lot of fun!

I'm planning to continue to publish all PCB design files, Source code, etc here for anyone else who is interested.   If you have any input on the project, features you'd like to see, or are interested in testing it out, please send me a note.

## Features currently implemented
* Immediate Load/run/emulate files from:
  * USB thumb Drive
  * SD card
  * Teensy Internal Flash
  * Transfer directly from PC (C# Windows app included)
* Files Supported:
  * .crt files (8khi/lo/16k):  Emulates "Normal" type ROM carts
  * .prg files: Super fast-loads any PRG into RAM and executes
* Sets C64 system time from internet (via Ethernet)
* MIDI USB in -> SID Polyphonic player with waveform/envelope controls

## Hardware/PCB Design
v0.2 PCB is completed, built and tested!

Ethernet and USB host connectors added to eliminate need for dongles.
   
Compatibility: Has been tested on NTSC C64 and C64C machines to this point, waiting for VIC-II Kawari availability to implement PAL timing  :) 

## Potential future SW development/features:
* Special cartridge HW emulation
* .d64, .tap file support
  * drive emulation via custom Kernal
* REU support using PSRAM
* Other HW Support: PAL, 50Hz
* Modem emulation via Ethernet connection
* MIDI interface enhancements:
  * Single note, multi-voice/filter/ring
  * Simulate other released MIDI interfaces

## Pictures/screen captures:

![TeensyROM pic1](/pics-info/v0.2/v0.2 ang.jpg)
![Screen capture](https://www.youtube.com/watch?v=rVvf3Ve9BGU)
See /pics-info folder for more.
