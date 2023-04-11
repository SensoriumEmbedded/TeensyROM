# TeensyROM
**A ROM emulator, super fast loader cartridge, MIDI and Ethernet interface for the Commodore 64, based on the Teensy 4.1**

*Design by Travis Smith/Sensorium ([e-mail](mailto:travis@sensoriumembedded.com))* 

Although there are now many emulators/loaders out there, I really wanted to design one around the Teensy 4.1 to take advantage of all its interface capabilities.  I also wanted to use its many pins to do "direct" interfacing so it can be largely software defined.  Oh, and it's also just a lot of fun!

I'm planning to continue to publish all PCB design files and source code here for anyone else who is interested.   If you have any input on the project, features you'd like to see, or are interested in testing it out, please send me a note.    I'm also interested in any feedback/contributions from other engineers/developers out there.

## Features currently implemented
* Immediate Emulation or Load from:
  * USB thumb Drive
  * SD card
  * Teensy Internal Flash
  * Transfer directly from PC (C# Windows app included)
* Files Supported:
  * .crt files (8khi/lo/16k):  Emulates "Normal" type ROM carts
    * Includes VIC direct reads used on some HiROM carts
  * .prg files: Super fast-loads any PRG into RAM and executes
* Sets C64 system time from internet (via Ethernet)
* Adjustable startup parameters stored in Teensy internal EEPROM
* MIDI USB in -> SID
  * 3 voice Polyphonic player with waveform/envelope controls

## Hardware/PCB Design
**v0.2 PCB is completed, built and tested!**
The primary change is the addition of Ethernet and USB host connectors to eliminate the need for dongles.  A full list of changes is available in the [PCB Readme](https://github.com/SensoriumEmbedded/TeensyROM/blob/main/PCB/PCB_Readme.md) as well as a link to purchase bare PCBs.    Software is fully compatible for both PCB versions, just have to set one #define at build time to differentiate.
![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2/v0.2%20ang.jpg)
Component selection was done using parts large enough (SOIC and 0805s at the smallest) that any soldering enthusiast should be able to assemble themselves.   Since high volume production isn't necessarily the vision for this device, 2 sided SMT was used to reduce the PCB size while still accommodating larger IC packages.
   
## Compatibility
TeensyROM been tested on about a dozen NTSC C64 and C64C machines to this point.  Waiting for VIC-II Kawari availability to implement PAL timing  :) 

## Future/potential SW development/features:
* REU support using PSRAM
* Special cartridge HW emulation
* .d64, .tap file support
  * drive emulation via custom Kernal
* Other HW Support: PAL, 50Hz, C128
* Modem emulation via Ethernet connection
* Method to save from USB/PC to SD or USB Drive
* MIDI interface enhancements:
  * Dual SID, 6 voice poly support
  * Single note synth, multi-voice/filter/ring
  * Simulate other released MIDI interfaces

## Demo Videos:
*  [TeensyROM real-time video/audio capture](https://www.youtube.com/watch?v=RyowR9huh0A) of menu navigation and loading/running/emulating various programs/cartridges
* [MIDI2SID Demo ](https://www.youtube.com/watch?v=3BsX_jxIYKY) using MIDI keyboard => TeensyROM => C64/SID
## Pictures/screen captures:
| ![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2/v0.2%20top.jpg) | ![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2/20230307_164007.jpg) | 
|--|--|
![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2/20230307_163653.jpg) |![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2/20230308_121851%20edit.jpg)  |
|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Screen%20captures/Main%20Menu.png)|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Screen%20captures/MIDI%20to%20SID.png)|
![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Screen%20captures/WinPC%20x-fer%20app.png)|
See [pics-info](https://github.com/SensoriumEmbedded/TeensyROM/tree/main/pics-info) folder for more, including some oscilloscope shots showing VIC cycle timing.

