# TeensyROM
**A ROM emulator, super fast loader cartridge, MIDI and Ethernet interface/emulator for the Commodore 64, based on the Teensy 4.1**

*Design by Travis Smith/Sensorium ([e-mail](mailto:travis@sensoriumembedded.com))* 

Although there are now many emulators/loaders out there, I really wanted to design one around the Teensy 4.1 to take advantage of all its interface capabilities.  I also wanted to use its many pins to do "direct" interfacing so it can be largely software defined.  Oh, and it's also just a lot of fun!

I'm planning to continue to publish all PCB design files and source code here for anyone else who is interested.   If you have any input on the project, features you'd like to see, or are interested in testing it out, please send me a note.    I'm also interested in any feedback/contributions from other engineers/developers out there.

## Features available
* Immediate Emulation or Load from:
  * USB thumb Drive
  * SD card
  * Teensy Internal Flash Memory
  * Transfer directly from PC
    * C# Windows app included
* Files Supported:
  * .PRG files: Super fast-loads any PRG into RAM and executes
  * .CRT files: Emulates 8khi, 8klo, 16k, C128
    * Emulates several special HW types, and all "normal" ROM carts 
    * Includes VIC direct reads used on some HiROM carts
* Startup parameters stored in Teensy internal EEPROM
* Special HW Emulation
  * **MIDI in/out via USB Host connection**
    * **Sequential/Datel/Siel** MIDI cartridge full emulation 
    * **Passport/Sentech/Namesoft** partial emulation
    * Use with **Cynthcart, SIDWizard,** etc.
    * Play your SID Synthesizer with any USB MIDI device!
    * Supports USB Hub for multiple instruments+thumb drive access
    * Built-in MIDI2SID app: 
      * Polyphonic player with waveform/envelope controls
  * **Ethernet communication via ethernet connection**
    * **Swiftlink** cartridge + 38.4k modem emulation
    * Use with **CCGMS, StrikeTerm2014, DesTerm128,** etc
    * Connect to your favorite C64/128/Telnet BBS!
    * Sets C64 system time from internet
    * General AT commands:
       | Command | Description |
       |--|--|
       AT | Ping  
       ATC | Connect Ethernet using saved parameters and display connection info
       ATDT<HostName>:<Port> | Connect to host and enter On-line mode
       AT+S | Display stored default Ethernet settings
    * These commands set the default values saved in the Teensy, use ATC or power cycle to apply settings.
       | Command | Description |
       |--|--|
       AT+DEFAULTS | Defaults Settings for all
       AT+RNDMAC | MAC address to random value
       AT+MAC=\<XX:XX:XX:XX:XX:XX>  | MAC address to provided value
       AT+DHCP=\<0:1> | DHCP On/Off
       DHCP mode only:
       AT+DHCPTIME=\<D> |  DHCP Timeout in mS
       AT+DHCPRESP=\<D> |  DHCP Response Timeout in mS
       Static mode only:
       AT+MYIP=<D.D.D.D> | Local IP address
       AT+DNSIP=<D.D.D.D> | DNS IP address
       AT+GTWYIP=<D.D.D.D> | Gateway IP address
       AT+MASKIP=<D.D.D.D> | Subnet Mask IP address
    * When in connected/on-line mode:
       | Command | Description |
       |--|--|
       +++ | Disconnect from host and enter command mode
  
## Hardware/PCB Design
**v0.2 PCB is completed and fully validated/tested**
The primary change from v0.1 was the addition of Ethernet and USB host connectors to eliminate the need for dongles.  A full list of changes is available in the [PCB Readme](https://github.com/SensoriumEmbedded/TeensyROM/blob/main/PCB/PCB_Readme.md) as well as a link to purchase bare PCBs.    Software is fully compatible for both PCB versions, just have to set one #define at build time to differentiate.
![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2/v0.2%20ang.jpg)
Component selection was done using parts large enough (SOIC and 0805s at the smallest) that any soldering enthusiast should be able to assemble themselves.   Since high volume production isn't necessarily the vision for this device, 2 sided SMT was used to reduce the PCB size while still accommodating larger IC packages.

**A note about overclocking**
The Teensy 4.1 is slightly "overclocked" to 816MHz from FW in this design. Per the app, external cooling is not required for this speed.  However, in abundance of caution, a heatsink is specified in the BOM for this project.  In addition, the temperature can be read on the setup screen of the main TeensyROM app. The max spec is 95C, and there is a panic shutdown at 90C.  In my experience, even on a warm day running for hours with no heatsink, the temp doesn't excede 75C.

## Compatibility
TeensyROM been tested on a couple dozen NTSC C64, C64C, and C128 machines to this point.  Waiting for VIC-II Kawari availability to implement PAL timing  :) 

## Future/potential SW development/features:
* Host USB Printers
* REU support using PSRAM
* Other Special HW cartridge emulation
* .d64, .tap file support
  * drive emulation via custom Kernal
* PAL Support
* Method to save from USB/PC to SD or USB Drive

## Demo Videos:
*  [TeensyROM real-time video/audio capture](https://www.youtube.com/watch?v=RyowR9huh0A) of menu navigation and loading/running/emulating various programs/cartridges
* [MIDI2SID Demo ](https://www.youtube.com/watch?v=3BsX_jxIYKY) using MIDI keyboard => TeensyROM => C64/SID
## Pictures/screen captures:
| ![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2/v0.2%20top.jpg) | ![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2/20230307_164007.jpg) | 
|:--:|:--:|
|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2/20230307_163653.jpg) |![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2/20230308_121851%20edit.jpg)  |
|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Screen%20captures/Main%20Menu.png)|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Screen%20captures/MIDI%20to%20SID.png)|
|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Screen%20captures/Settings%20Menu.png)|![TeensyROM pic1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Screen%20captures/WinPC%20x-fer%20app.png)|

See [pics-info](https://github.com/SensoriumEmbedded/TeensyROM/tree/main/pics-info) folder for more, including some oscilloscope shots showing VIC cycle timing.

