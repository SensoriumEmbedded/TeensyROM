# TeensyROM Multi-capable Cartridge for C64/128

***Connect your Commodore to the 21st century***

**Features include:**
* **ROM emulator**: The perfect way to play CRT files, such as the highly recommend [OneLoad64](https://www.youtube.com/watch?v=lz0CJbkplj0) collection.
* **Instant loader**: [Immediately load/run](docs/General_Usage.md) program (PRG) files
* **MIDI USB Host & Device, ASID Player**: Use a [MIDI keyboard, DAW](docs/MIDI_Usage.md), or [ASID source](docs/ASID_Player.md) to make your SID chip sing!
* **Internet interface**: Connect to a [Telnet BBS](docs/Ethernet_Usage.md) or use the integrated [web browser](docs/Browser_Usage.md) to surf/search/download
* **Remote Control** your C64 using 
  * [NFC Launch System](docs/NFC_Loader.md): The feel of old school cartridges combined with [instant loading](https://www.youtube.com/watch?v=iNfQx2gx0hA)
  * The feature rich [TeensyROM UI](https://github.com/MetalHexx/TeensyROM-UI) and Cross-platform [Command Line Interface](https://github.com/MetalHexx/TeensyROM-CLI)
* Picture viewer, SID Player, Custom BASIC Commands, Autolaunch, *and more* 
* Lots of games, utilities, pics, and music built-in: no external media required to get started!
* [**Multiple Hardware Interfaces:**](media/TR_Connections.png) SD card, USB Drive, USB Device and host Ports, Ethernet Port

*Design by Travis S/Sensorium ([e-mail](mailto:travis@sensoriumembedded.com))* 

![TeensyROM pic1](media/v0.3/v0.3_angle.png)

|![TeensyROM pic1](media/case/case-front-corner.png)|![TeensyROM case](media/case/case-rear-corner.png)| 
|:--:|:--:|

Makers can build their own TeensyROM, the HW was designed with mid-level solder skills in mind. See these [assembly instructions](PCB/PCB_Assembly.md).
<BR>If you prefer, **Fully assembled/tested units** are available via my [Tindie shop](https://www.tindie.com/products/travissmith/teensyrom-cartridge-for-c64128/) (prefered) and [eBay](https://www.ebay.com/usr/travster1).
<BR><a href="https://www.tindie.com/products/travissmith/teensyrom-cartridge-for-c64128/"><img src="media/Other/tindie-mediums.png" alt="Tindie Logo Link" width="150" height="78"></a>

Case/enclosures are avaible to [download](3D_Print_Case/) and print at home, for sale with a new unit (Tindie/eBay links above), or direct from [sMs Retro Electronics](https://ko-fi.com/smsretroelectronics)

Please consider joining us in the [TeensyROM Discord Server](https://discord.gg/ubSAb74S5U) to meet other TeensyROM users, ask questions, provide thoughts/input/feedback, etc.

## Table of contents
  * [TeensyROM Feature details](#teensyrom-feature-details)
  * [Links to detailed documentation](#links-to-detailed-documentation)
  * [Demo Videos](#demo-videos)
  * [Hardware/PCB Design](#hardware-pcb-design)
  * [Compatibility](#compatibility)
  * [Inspiration](#inspiration-and-thank-yous)
  * [Pictures/screen captures](#pictures-screen-captures)

<BR>

## TeensyROM Feature Details
### Compatable with C64 and C128 machines/variants, NTSC and PAL supported
### **Super fast Loading (.PRG/P00) or ROM emulation (.CRT)** directly from:
  * USB thumb Drive
  * SD card
  * Teensy Internal Flash Memory
  * Transfer directly from PC using the [TeensyROM UI](https://github.com/MetalHexx/TeensyROM-UI)
  * See supported file details [here](https://github.com/SensoriumEmbedded/TeensyROM/blob/main/docs/General_Usage.md#loading-programs-and-emulating-roms)
  * [NFC Loading system](docs/NFC_Loader.md) available to quickly select/load with NFC tags.
### **MIDI in/out via USB Host connection:** 
  * Play your SID with a USB MIDI keyboard!
  * Use with popular software such as **Cynthcart, Station64** etc, or the included MIDI2SID app
  * Supports all regular MIDI messages **in and out**
    * Can use your C64 to play a MIDI sound capable device.
  * **Sequential, Datel/Siel, Passport/Sentech, and Namesoft** MIDI cartridges emulated 
  * Use a USB Hub for multiple instruments+thumb drive access
### **MIDI in via USB Device connection:** 
  * Stream .SID or .MIDI files from a modern computer directly to your Commodore machine SID chip!
  * Play MIDI files out of your PC into C64 apps such as Cynthcart or the MIDI2SID app
  * Stream .SID files out of your PC using the ASID MIDI protocol to hear any SID file on original hardware.
### **Internet communication via Ethernet connection**
  * Connect to your favorite C64/128 Telnet BBS!
  * Use with released software such as **CCGMS, StrikeTerm2014, DesTerm128,** etc
  * **Swiftlink** cartridge + 38.4k modem emulation
  * Send AT commands from terminal software to configure the Ethernet connection
  * Sets C64 system time from internet
### **Firmware updates directly from SD card or USB thumb drive**
  * Just drop the .hex file on an SD card or USB drive, no need for extra software to update.
### Key parameters stored in internal EEPROM
  * Startup, Ethernet, timezone, etc retained after power down.

## Links to detailed documentation
  * **Usage Documents**
    * **[General Usage](docs/General_Usage.md)**
    * **[MIDI Usage](docs/MIDI_Usage.md)**
    * **[ASID Player](docs/ASID_Player.md)**
    * **[Ethernet Usage](docs/Ethernet_Usage.md)**
    * **[NFC Loading System](docs/NFC_Loader.md)**
    * **[TeensyROM Web Browser](docs/Browser_Usage.md)**
    * **[Custom BASIC Commands](docs/Custom_BASIC_Commands.md)**
  * **SW Release notes/developnment**
    * **[Firmware Release history](bin/TeensyROM/FW_Release_History.md)**
    * **[Win App Release History](bin/WinApp/WinApp_Release_History.md)**
    * **[Software Build Instructions](Source/BuildInfo.md)**
  * **Hardware & PCB Related**
    * **[3D printed case files/document](3D_Print_Case/3D-Printed-Case-ReadMe.md)**
    * **[TeensyROM Assembly Instructions](PCB/PCB_Assembly.md)**
    * **[PCB Design History](PCB/PCB_History.md)**
    * **[Bill of materials with cost info](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/PCB/v0.2c/TeensyROM%20v0.2c%20BOM.xlsx)**
    * **[PDF Schematic](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/PCB/v0.2c/TeensyROM_v0.2c_Schem.pdf)**


## Demo Videos:
  * **[This YouTube Playlist](https://www.youtube.com/playlist?list=PL3fTdu8e_1iChAsRr9KjWtC3A8Ql8IaDn)** contains all the latest TeensyROM demo videos, such as: 
    * Real-time video/audio capture of menu navigation and loading/running/emulating various programs/cartridges
    * Demo using Cynthcart and Datel MIDI emulation to play with a USB keyboard 
    * MIDI ASID Demo: Stream .SID & .MIDI files directly to your C64/SID
    * Web Browser and internet file download demo 
    * SID Player and Picture viewer demo 

## Compatibility
* TeensyROM compatability has been fully validated on **many** different NTSC **and** PAL machines: C64, C64C, SX-64, and C128 as well as modern reproductions such as EVO64, Mega65 (r5 and higher), Ultimate 64, and Reloaded MKII
* The early "prototype" version of breadbin (PCA 36298 w/ 5 pin video) has a known issue with the reset circuit that must be corrected to be compatible with the TeensyROM and other fastload cartridges. See [this post](https://www.lemon64.com/forum/viewtopic.php?t=74222) or [this video](https://youtu.be/agDFLPP9yIw?t=813) for instructions on how to correct this issue.

## Hardware-PCB Design
Component selection was done using parts large enough (SOIC and 0805s at the smallest) that any soldering enthusiast should be able to assemble themselves.   Since high volume production isn't necessarily the vision for this device, 2 sided SMT was used to reduce the PCB size while still accommodating larger IC packages.

**A note about overclocking**
The Teensy 4.1 is slightly "overclocked" to 816MHz from FW in this design. Per the app, external cooling is not required for this speed.  However, in abundance of caution, a heatsink is specified in the BOM for this project.  In addition, the temperature can be read on the setup screen of the main TeensyROM app. The max spec is 95C, and there is a panic shutdown at 90C.  In my experience, even on a warm day running for hours with no heatsink, the temp doesn't excede 75C.

## Inspiration and Thank-Yous:
* [**Heather S**](https://www.instagram.com/dalliancecreations/): Loving wife, continuous encourager, saintly patience
* [**MetalHexx**](https://github.com/MetalHexx): Big picture ideas, [TeensyROM UI](https://github.com/MetalHexx/TeensyROM-UI) and [CLI](https://github.com/MetalHexx/TeensyROM-CLI), testing, friendship
* [**Avrilcadabra**](https://www.youtube.com/@avrilcadabra): Musician, experimenter, provider of ideas and feedback 
* [**Paul D aka Digitalman**](https://www.youtube.com/@digitalman4404): Thought provoker, promoter, Maker, and tester extraordinaire
* [**Stefan Wessels**](https://github.com/StewBC): Cartridge case design
* [**StatMat**](https://github.com/Stat-Mat): NFC Scanner idea, Fast boot code, OneLoad64 creation
* **Giants with tall shoulders**: SID/SIDEKick, KungFu Flash, VICE Team

## Pictures-screen captures:
|![TeensyROM pic1](media/v0.3/v0.3_top.png)|![TeensyROM pic1](media/v0.2b/v0.2b_insitu_MIDI.jpg)|
|:--:|:--:|
|![TeensyROM pic1](media/Screen%20captures/Main%20Menu.png)|![TeensyROM pic1](media/Screen%20captures/USB%20Menu.png)|
|![TeensyROM pic1](media/Screen%20captures/Settings%20Menu.png)|![TeensyROM help](media/Screen%20captures/Help%20Menu.png)|

See the [media](media/) folder for more pics, videos, and oscilloscope shots.

