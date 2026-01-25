# Commodore C64 Ultimate Enhancements

Even the feature-rich C64 Ultimate benefits from TeensyROM's enhanced capabilities. Here's what TeensyROM brings to the table:

## MIDI Support

Connect a MIDI controller for an engaging musical experience—no musical background required. Whether you're casually exploring sounds with friends and family or diving into serious music production, TeensyROM makes it accessible and fun.  For professional music makers, TeensyROM stands out as a premier cartridge solution. 
>_"TeensyROM lets you make your C64 sing through a modern DAW or stream chiptunes directly from external sources. It's especially great for live performance, but just as useful in a studio setup. I can confidently say there’s nothing else on the market that’s as capable or as easy to use" **-LukHash**_

## ASID MIDI Support

Much like the hardware synthesizer TherapSID, TeensyROM implements the ASID protocol enabling real-time SID control from external applications:

- Stream SID tunes from [DeepSID](https://deepsid.chordian.net/) with multi-SID support
- Control the SID chip as a synthesizer using [ChipSynth C64](https://plogue.com/products/chipsynth-c64.html) from Plogue.


## SID Player Features

Take full control of SID music playback with interactive features:

- **Voice Muting**: Toggle individual SID voices on/off to isolate instruments and explore song composition
- **Speed Control**: Adjust playback speed in real-time to slow down or speed up music
- Perfect for learning, remixing, or just having fun with classic tunes

## NFC Card Launching

Transform your C64 into a tap-and-play gaming console with physical NFC cards. Perfect for gaming parties, showcasing your collection, or simply enjoying the tactile experience of physical media in the modern age:

- **Instant Game Launch**: Simply tap an NFC card to instantly load and launch any game, demo, SID file, or application
- **Build Your Physical Collection**: Create a tangible library of your favorite C64 software with custom-labeled NFC cards
- **Program Your Own Cards**: Write any file path directly to NFC tags using TeensyROM's built-in programming feature
- **Random Mode Support**: Create surprise cards that launch random titles from your collection for endless variety
- **Affordable Hardware**: Works with readily-available PN532-based USB NFC readers found worldwide
- **Universal Tag Support**: Compatible with NTAG and Mifare Classic NFC tags available from any retailer

## Modern UI Control

TeensyROM apps feature a mashup of modern and retro. By leveraging modern computing power and memory, they unlock instant search, indexing, and random selection across massive file collections—capabilities impossible on vintage hardware alone:

- **Modern Web Interfaces**: Browse and launch your entire game and media library from any computer via WiFi, Ethernet, or USB
- **Live Video Integration**: Watch your C64 gameplay with large array of authentic CRT emulation effects (Web only)
- **Complete Media Management**: Search, filter, transfer files, create playlists and track favorites across all storage devices
- **Advanced Playback Control**: Shuffle mode, auto-play, progress tracking, and support for multi-device DJ setups
- **Cross-Platform Support**: Apps available for Windows, macOS, and Linux
- **Featured Apps**:
  - [Web UI / HTTP API](https://github.com/MetalHexx/TeensyROM-Web) **New App!** (Cross-platform) 
  - [CLI App](https://github.com/MetalHexx/TeensyROM-CLI) (Cross-platform)
  - [Desktop App](https://github.com/MetalHexx/TeensyROM-UI) (Windows Only)
  - [TR Transfer/Control App](https://github.com/SensoriumEmbedded/TRWinApp) (Windows Only)

## Serial / TCP API

TeensyROM exposes a comprehensive command protocol over USB Serial or TCP/Ethernet connections, enabling developers to build custom integrations and automation tools:

- **Microcontroller Integration**: Control TeensyROM directly from other microcontrollers via USB serial interface
- **File Operations**: Upload, download, copy, delete, and directory listing capabilities across SD and USB storage
- **Playback Control**: Remote file launching, SID player control (pause, speed, voice muting, sub-tune selection)
- **Device Management**: System commands including reset, ping, version info, firmware checks, and DMA control

## Image Launcher Support

Display custom artwork and graphics on your C64:

- Direct launching of PETSCII and high-definition image formats
- Convert your own images using the [RetroPixels](https://www.micheldebree.nl/retropixels/) website

## Auto-Launch Capabilities

Streamline your workflow with customizable startup and quick-access features:

- Configure automatic startup to your preferred application
- Program custom hotkeys for instant access to specific programs

## Web Browsing

Explore the internet from your C64 with optimized web access:

- Built-in support for browsing retro-compatible websites directly on your C64
- Check out [FrogFind](http://www.frogfind.de/) for vintage computer-ready web content

---

[Back to main ReadMe](/README.md)

