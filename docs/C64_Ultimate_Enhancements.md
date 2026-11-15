# Commodore C64 Ultimate Enhancements

Even the feature-rich C64 Ultimate benefits from TeensyROM's enhanced capabilities. Here's what TeensyROM brings to the table:

## MIDI Support

Connect a MIDI controller for an engaging musical experience—no musical background required. Whether you're casually exploring sounds with friends and family or diving into serious music production, TeensyROM makes it accessible and fun.  For professional music makers, TeensyROM stands out as a premier cartridge solution. 
>_"TeensyROM lets you make your C64 sing through a modern DAW or stream chiptunes directly from external sources. It's especially great for live performance, but just as useful in a studio setup. I can confidently say there’s nothing else on the market that’s as capable or as easy to use" **-LukHash**_

For more information: [MIDI Usage Guide](/docs/MIDI_Usage.md)

## ASID MIDI Support

Much like the hardware synthesizer TherapSID, TeensyROM implements the ASID protocol enabling real-time SID control from external applications:

- Stream SID tunes from [DeepSID](https://deepsid.chordian.net/) with multi-SID support
- Control the SID chip as a synthesizer using [ChipSynth C64](https://plogue.com/products/chipsynth-c64.html) from Plogue.
- Play and mix SID tunes live on one or two C64s with the [DJ Mixer](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/dj-mixer.md) in Trom UI

For more information: [ASID Player Guide](/docs/ASID_Player.md)

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

For more information: [NFC Loader Guide](/docs/NFC_Loader.md)

## Modern UI Control

[TeensyROM Suite](https://github.com/MetalHexx/TeensyROM-Suite) is a mashup of modern and retro. By leveraging modern computing power and memory, it unlocks instant search, indexing, and random selection across massive file collections—capabilities impossible on vintage hardware alone:

- **Your Library, Anywhere**: Browse, search and launch every game, demo, SID and image from any computer, phone or tablet on your network, with your TeensyROM on USB or Ethernet
- **Stream It**: Shuffle and auto-play hands-free across all your storage, with favorites, play history and [audio on your phone](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/trom-ui.md#mobile-audio-streaming)
- **Live Video Integration**: Watch your C64 in the browser with a large array of authentic [CRT effects](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/trom-ui.md#video-and-crt-emulation)
- **[DJ Mixer](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/dj-mixer.md)**: One or two C64s become decks: beat-match, loop, cue and scratch SID tunes live, with every note from a real SID chip. Watch the [Mix Preview](https://youtu.be/PjhqYkmsScg) and [Scratch / Beat Juggle](https://youtu.be/xoJBOEPB5IY)
- **[Copy Files](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/trom-ui.md#copy-files-to-your-cart) and [Watch Folders](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/trom-ui.md#watch-folders)**: Drag and drop files, folders and archives onto several TeensyROMs at once, or let a folder on your computer send whatever lands in it
- **[Themes](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/trom-ui.md#themes)**: Recolor the TeensyROM's own menu, on one TeensyROM or all
- **AI Companion**: [Trom Agent](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/trom-agent.md) helps you explore your collection, play mixes, and troubleshoot your setup with [Virtual Travis](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/trom-agent.md#the-setup-doctor)
- **Cross-Platform Support**: Windows, macOS, Linux, and Raspberry Pi
- **Featured Apps**:
  - [Trom UI](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/trom-ui.md): the web app, home of the DJ Mixer
  - [Trom Terminal](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/trom-terminal.md): a BBS-style menu plus one-line commands for your scripts
  - [Trom Agent](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/trom-agent.md): your AI companion, as a plugin for your AI coding agent
  - [Trom API](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/trom-api.md): the engine behind them all, open to your own tools
  - [TR Transfer/Control App](https://github.com/SensoriumEmbedded/TRWinApp) (Windows Only)

## Serial / TCP API

TeensyROM exposes a comprehensive command protocol over USB Serial or TCP/Ethernet connections, enabling developers to build custom integrations and automation tools:

- **Microcontroller Integration**: Control TeensyROM directly from other microcontrollers via USB serial interface
- **File Operations**: Upload, download, copy, delete, and directory listing capabilities across SD and USB storage
- **Playback Control**: Remote file launching, SID player control (pause, speed, voice muting, sub-tune selection)
- **Device Management**: System commands including reset, ping, version info, firmware checks, and DMA control
- **HTTP API**: [Trom API](https://github.com/MetalHexx/TeensyROM-Suite/blob/main/docs/trom-api.md#build-your-own-tools) wraps the protocol in an HTTP API with an OpenAPI spec, so your tools can launch files, search your library, copy files and set themes from any language

## Image Launcher Support

Display custom artwork and graphics on your C64:

- Direct launching of PETSCII and high-definition image formats
- Convert your own images using the [RetroPixels](https://www.micheldebree.nl/retropixels/) website

## Auto-Launch Capabilities

Streamline your workflow with customizable startup and quick-access features:

- Configure automatic startup to your preferred application
- Program custom hotkeys for instant access to specific programs

For more information: [autolaunch.txt](/docs/autolaunch.txt)

## Web Browsing

Explore the internet from your C64 with optimized web access:

- Built-in support for browsing retro-compatible websites directly on your C64
- Check out [FrogFind](http://frogfind.de/) for vintage computer-ready web content

For more information: [Browser Usage Guide](/docs/Browser_Usage.md)

## Host Extensions (TR+)

Go beyond the C64 software library with TeensyROM+ Host Extensions: add-on firmware that installs alongside the core TeensyROM firmware, right from the TeensyROM menu:

- **Play Games Never Released for the C64**: The [Mean Hamster Power Engine (MPE)](https://meanhamster.com/games/mpe-power-engine) runs NES, Game Boy, Doom, classic point-and-click adventures and more
- **Easy Install and Removal**: Select a `.TRH` file from SD or USB to install, and uninstall anytime from the Settings menu
- **Open to Experimenters**: Write and test your own TeensyROM firmware as an extension, without modifying the core firmware

For more information: [TR Host Extensions Guide](/docs/TR_Host_Extensions.md)

## C64 Ultimate Settings for TeensyROM

TeensyROM answers at addresses in the C64's IO1 (`$DE00`-`$DEFF`) and IO2 (`$DF00`-`$DFFF`) ranges, and several of the C64U's own devices can be set to answer there too. When both answer at the same address, the TeensyROM menu shows garbage or hangs, and MIDI, modem, REU and cartridge programs misbehave. These settings give the cartridge port to the TeensyROM and keep the C64U's devices out of its way. They apply to every TeensyROM, and TR+'s DMA-based features (Host Extensions, Freezer Cartridge Support, REU, and more) need the first two.

* **Cartridge and ROM Settings** (called *C64 and Cartridge Settings* on older C64U firmware):
  * **`Cartridge Preference`**: `External`
  * **`Bus Operation Mode`**: `Writes`
  * **`RAM Expansion Unit`**, **`Command Interface`** and **`Map Ultimate Audio $DF20-DFFF`**: `Disabled`. A TR+ has its own REU (Special IO `512KB REU`).
* **Modem Settings**: **`ACIA (6551) Mapping`**: `Off`. Use the TeensyROM's own `Swift-Turbo/Modem` Special IO instead.
* **SID Addressing**: keep **`UltiSID 1 Address`**, **`UltiSID 2 Address`**, **`SID Socket 1 Address`** and **`SID Socket 2 Address`** between `$D400` and `$D7E0`.
  * Never use a `$DExx` address: the TeensyROM menu uses that whole range.
  * A second SID at `$DF00` works while running Cynthcart or the ASID Player. Move it back before using the REU, GeoRAM, EasyFlash, Host Extensions or a freezer.

**Set them all at once:** download [TeensyROM_C64U.cfg](/docs/TeensyROM_C64U.cfg) and copy it to a USB drive in the C64U. Select it in the C64U's file browser and choose `Load Settings`. When you leave the C64U's menu, it asks whether to save the changes to flash: answer yes, then power the C64U off and on. The file puts `SID Socket 1` and `UltiSID 1` at `$D400` and `SID Socket 2` and `UltiSID 2` at `$D420`, and leaves the Range Split settings alone, so an 8-SID setup keeps working. For Cynthcart, set `UltiSID 2 Address` to `$DF00` afterwards, then load the file again to move it back.

The addresses each TeensyROM mode uses are listed under [IO addresses each mode uses](/docs/General_Usage.md#io-addresses-each-mode-uses).

For more information: [TR+ New Features Guide](/docs/TR+NewFeatures.md)

---

[Back to main ReadMe](/README.md)

