# TeensyROM
**ROM emulator, super fast loader, MIDI and Internet interface cartridge for the Commodore 64 & 128, based on the Teensy 4.1**

*Design by Travis S/Sensorium ([e-mail](mailto:travis@sensoriumembedded.com))* 

Although there other emulators/loaders out there, I really wanted to design one around the Teensy 4.1 to take advantage of all its external interface capabilities (USB Host, SD card, Ethernet).  I also wanted to use its many IO pins to do "direct" interfacing so it can be largely software defined. 

I'll continue to publish all PCB design files and source code here for anyone else who is interested.   If you have any input on the project, features you'd like to see, or are interested in trying one out, just send [me a note](mailto:travis@sensoriumembedded.com).    I'm also interested in any feedback/contributions from other engineers/developers out there.

## Features
* Immediate Emulation or Load from:
  * USB thumb Drive
  * SD card
  * Teensy Internal Flash Memory
  * Transfer directly from PC
    * C# Windows app included
* Files Supported:
  * .PRG files: Super fast-loads any PRG into RAM and executes
    * must be single PRG file, not multi-part
  * .CRT files: Emulates 8khi, 8klo, 16k, C128
    * Emulates several special HW types (Epyx, etc), and all "normal" ROM carts 
    * Includes VIC direct reads used on some HiROM carts
* Key parameters (startup, Ethernet, etc) stored in internal EEPROM for retention after power down.
* **MIDI in/out via USB Host connection:** Play your SID with a USB MIDI keyboard!
  * Use with released software such as **Cynthcart, SIDWizard,** etc.
    * Or use included MIDI2SID app
  * Supports all regular MIDI messages
    * MIDI **in and out**, can also use your C64 to play a MIDI capable device.
  * **Sequential/Datel/Siel** MIDI cartridge full emulation 
  * **Passport/Sentech/Namesoft** MIDI cartridge partial emulation
    * Not emulating 6840 timer chip, may add later if needed
  * Can use USB Hub for multiple instruments+thumb drive access
* **Internet communication via Ethernet connection**
  * Connect to your favorite C64/128/Telnet BBS!
  * Use with released software such as **CCGMS, StrikeTerm2014, DesTerm128,** etc
  * **Swiftlink** cartridge + 38.4k modem emulation
  * Send AT commands from terminal software to configure the Ethernet connection
    * See full list [here](https://github.com/SensoriumEmbedded/TeensyROM/blob/main/pics-info/AT_Commands.md)
  * Sets C64 system time from internet

![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2b/v0.2b_angle.jpg)
  
## Hardware/PCB Design
* **PCB design is fully validated/tested.** 
* Complete BOM and Assembly Instructions are available via the [PCB Readme](https://github.com/SensoriumEmbedded/TeensyROM/blob/main/PCB/PCB_Readme.md) 

Component selection was done using parts large enough (SOIC and 0805s at the smallest) that any soldering enthusiast should be able to assemble themselves.   Since high volume production isn't necessarily the vision for this device, 2 sided SMT was used to reduce the PCB size while still accommodating larger IC packages.

**A note about overclocking**
The Teensy 4.1 is slightly "overclocked" to 816MHz from FW in this design. Per the app, external cooling is not required for this speed.  However, in abundance of caution, a heatsink is specified in the BOM for this project.  In addition, the temperature can be read on the setup screen of the main TeensyROM app. The max spec is 95C, and there is a panic shutdown at 90C.  In my experience, even on a warm day running for hours with no heatsink, the temp doesn't excede 75C.

## Compatibility
* TeensyROM been tested on ~20 different NTSC C64, C64C, and C128 machines to this point. 
* Waiting for my Kawari to implement PAL timing  :)

## Future/potential SW development/features:
* More Special HW cartridge emulation
* PAL Support
* .d64, .tap file support
* Host USB Printers

## Demo Videos:
* [TeensyROM real-time video/audio capture](https://www.youtube.com/watch?v=RyowR9huh0A) of menu navigation and loading/running/emulating various programs/cartridges
* [Demo using Cynthcart and Datel MIDI emulation](https://www.youtube.com/watch?v=-LumhU60d_k) to play with a USB keyboard 
* [MIDI2SID Demo ](https://www.youtube.com/watch?v=3BsX_jxIYKY) using MIDI keyboard => TeensyROM => C64/SID

## Inspiration:
* **Heather S**: Loving wife through whom all things are possible
* **Paul D**: Thought provoker, Maker, and Beta tester extraordinaire
* **Giants with tall shoulders**: SID/SIDEKick, KungFu Flash, VICE
* **Frank Z**: Music is The Best.

## Pictures/screen captures:
|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2b/v0.2b_top.jpg) |![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2b/v0.2b_top_loaded.jpg) | 
|:--:|:--:|
|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2b/v0.2b_insitu_MIDI.jpg) |![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2b/v0.2b_insitu_USBdrive.jpg)  |
|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Screen%20captures/Main%20Menu.png)|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Screen%20captures/MIDI%20to%20SID.png)|
|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Screen%20captures/Settings%20Menu.png)|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Screen%20captures/WinPC%20x-fer%20app.png)|

See [pics-info](https://github.com/SensoriumEmbedded/TeensyROM/tree/main/pics-info) folder for more, including some oscilloscope shots showing VIC cycle timing.

