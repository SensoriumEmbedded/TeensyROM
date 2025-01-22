
# TeensyROM ASID Player
The ASID Player allows you to stream SID sound information real-time from a modern computer/phone directly to the SID chip of your C64/128, turning it into a modular synthesizer device. The ASID player is highly optomized for the TeensyROM and allows fast updates and smooth streaming of SID music/sounds.

![ASID Player](/media/Screen%20captures/ASID%20Player.png)

## ASID Sources
Here are a few ASID sources which can be used to stream to your C64/SID. See detailed setup instructions at the bottom of this document.
|Project/Link|Platform|About|
|:--:|:--:|:--|
|**[DeepSID](https://deepsid.chordian.net/)**|Web based|Access/stream any existing SID file, directly from the web|
|**[ChipSynth C64](https://www.plogue.com/products/chipsynth-c64.html)**|Win/Mac|Turn your C64 into a programmable synth module|
|**[SID Factory II](https://blog.chordian.net/sf2/)**|Win/Mac/Linux|Cross-platform SID music editor with ASID output ([beta branch](https://github.com/Chordian/sidfactory2/tree/asid-support))|
|**[ASIDXP](https://elektron.se/support-downloads/sidstation#resources)**|Win/Mac|Stream SID files directly from your local hard drive|
|**[SIDPlay](http://www.sidmusic.org/sidplay/mac/)**|Mac|Stream from your local hard drive with visualizations, etc|

## USB/TeensyROM Setup
* Connect USB cable from a MIDI Host/computer to the USB Type B Micro Device port on the Teensy module.
 * Recommend direct connection between PC/Mac and TeensyROM, USB hubs can add additional timing jitter.
* Power up C64/128 to the TeensyROM main menu.
* Select "TeensyROM ASID Player", or press the number '4' for fast hotkey access.
* Program starts ready to receive/play MIDI ASID data

## TeensyROM ASID Player Usage/commands (FW 0.6.2 or higher)
### C64 Keyboard commands
|Key|Function|Description|
|:--:|:--:|:--|
|`v`|Clear Voices|Initialize/clear all voices on all SID chips|
|`s`|Screen Toggle|Turn on/off C64 screen blanking, can help with audio noise reduction|
|`m`|Mute All Toggle|Clears all voices and mutes incomming stream. "Mute" displayed when active|
|`1`/`2`/`3`|Voice # Toggle|Enable/Disable voice # 1/2/3 individually.<BR>Voice # group red when active, applies to all active SIDs
|`?`|Help List|Displays keyboard commands list|
|`d`|Indicator Decoder|Displays info about indicators at top of screen (see below)|
|`c`|Clear Screen|Clears text from display, just leaves indicators|
|`x`|Exit|Exit the application, back to the TeensyROM menu|
|`F1`/`F2`|First  SID address|Inc/Dec address of the *primary* SID, typically $d400|
|`F3`/`F4`|Second SID address|Inc/Dec address of SID #2 for multi-SID playback|
|`F5`/`F6`|Third  SID address|Inc/Dec address of SID #3 for multi-SID playback|
|`t`|Frame Timer (beta)|Turns On/Off the frame-retimer (see below)|
|`B`/`b`|Buffer Size|Inc/Dec the size of the Frame Timer buffer, if used (see below)|

### Register indicators
The top of the ASID Player screen displays real-time playback information. This information can be divided into three groups:
* **Spinner indicators** update to the next PETSCII character each time an event takes place
    * `3`,`2`,`1` These spinners update each time SID#1, 2, or 3 is written to
    * `R` Read error: Unexpected reg type or skip message received by C64 from Teensy
    * `P` Packet error: Detected by Teensy, usually from ASID source: 
        * More regs expected in ASID SysEx packet than provided
        * SysEx packet format incorrect
        * Unexpected ASID msg type
        * Data requested from C64 when queue is empty 
* **SID #1 Register Access "Lights"**
    * 25 indicators to represent an access to each SID(#1) register
    * Lights turn white on each register write, then turn grey if not accessed again withing 100mS, and turn off after another 100mS of non-access.
    * Use the `d` command to see the decoder list of registers
* **Frame Timer buffer usage bar graph**
    * When the Frame Timer is enabled, indicates how full the buffer currently is.
    * Too full or empty indicates over/underflow and may result in an audible anomaly.

### Frame timer (beta)
Timing imperfections can be introduced by control PC workload, USB packetization, drivers, and other factors. This can be compensated for by re-timing the ouput to the C64/SID to a repeatably accurate cadence.

The first step is to enable the re-timer on and select 50Hz or Auto timing of playback using the `t` command. 
* `Off` (default) simply plays each ASID packet as soon as it arrives via USB/MIDI
* `On-50Hz` is for most standard 1x speed SIDs, and can insure accurate starting speeds for SIDs of that type.
* `On-Auto` measures the initial packets for the starting time constant. Timing can start slightly off if initial packets aren't received well-timed.
* Eventually, the ASID protocol will include "recipe" information from the source to automatically configure this speed, but this capability is not yet available.
    * For this reason, variable rate and per-note sids are not currently compatible with the frame timer feature.

Once the Frame Timer is turned on, the buffer size can be selected with `b` (smaller) or `B` (larger). 
* Larger buffers afford more space for speed corrections and reduce likelyhoo of audible glitches, but also add noticable buffer time between the source and the SID output. 
* Smaller buffers are usefull for faster startup and synch with the source, but risk over/under runs for jittery sources
* Here are some recommendations:
    |Usage|Timer|Buffer Size|
    |:--:|:--:|:--:|
    |DeepSID 50Hz 1xSID|`On-50Hz`|`Tiny`-`Medium`|
    |DeepSID 50Hz 1xSID|`On-Auto`|`Small`-`Large`|
    |DeepSID 50Hz 2xSID|`On-Auto`|`Medium`-`XLarge`|
    |DeepSID 50Hz 4xSID|`On-Auto`|`Medium`-`XXLarge`|
    |SIDFactory II 50Hz 1x|`On-50Hz`|`Tiny`-`Small`|
    |ASID XP 50Hz 1xSID|`On-50Hz`|`Tiny`-`Small`|
    |ChipSynth C64|`Off`|N/A (Asynchronous)|

## ASID Source setup info
* **DeepSID** to stream .sid files from the internet to your C64
    * In your computer/phone browser, navigate to https://deepsid.chordian.net/
    * Select "ASID (MIDI)" from the drop-down in the upper left corner
    * Select "TeensyROM" from the "MIDI port for ASID" drop-down
    * Select your SID from the vast library and play it
    * The playback should be eminating from your C64/128!
* **ChipSynth C64** to control your SID chip directly as a synthesizer
    * Go to https://www.plogue.com/products/chipsynth-c64.html and download/install/launch "chipsynth C64"
    * Select the "EMU" tab, in the ASID box, enable output of "Synth V1" and "TeensyROM" as the destination
    * There are many things you can do with this great program, purchasing will unlock the 10 minute limit per use.
    * You can use the [TeensyROM CLI tool](https://github.com/MetalHexx/TeensyROM-CLI) to tweak the Chipsynth presets to work better with ASID.
    * Here's a [demo video](https://www.youtube.com/watch?v=-Xs3h59-dOU) showing some of the capabilities
* **SID Factory II** for cross-platform SID music editing/playing
    * As of this writing, ASID support is only available via [this GitHub Branch](https://github.com/Chordian/sidfactory2/tree/asid-support) and must be compiled. Let [me](mailto:travis@sensoriumembedded.com) know if you need help with this.
        * Hoping this will be fully released [here](https://blog.chordian.net/sf2/) soon.
    * Launch the SFII application
    * Press F1 on the splash page to select the TeensyROM, and Enter to continue
    * Press F10 once you're in the editor to load a song project
        * Browse to the "music" folder for some example projects
    * Click the "Output: RESID" button on top and switch it to ASID
    * Press F2 to play the song, then CTRL-P to enable "follow mode"
* **ASID XP** to stream .sid files from your PC hard drive
    * Go to https://www.elektron.se/us/download-support-sidstation to download "ASID for Sidstation" and install/launch
    * Click the "Conf" button and highlight "TeensyROM", click OK
    * Either Click "Load" or drag/drop a .sid file into the window.
    * Click "Play"
    * The playback should be eminating from your C64/128!
    * You can play with the frequency to adjust the playback tempo if needed.
    
<br>

[Back to main ReadMe](/README.md)