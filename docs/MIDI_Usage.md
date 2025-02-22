
# TeensyROM USB MIDI Usage

MIDI/ASID control options:
![Control Options](/media/MIDI-ASID%20paths.png)
Diagram by [**MetalHexx**](https://github.com/MetalHexx)
<BR>
<BR>

### Sending MIDI from a MIDI/USB controller such as a keyboard, drum pad, [Sax](https://www.akaipro.com/ewi-usb), or [M8 tracker/sequencer](https://dirtywave.com/) to your C64
  * Connect USB cable from a MIDI Keyboard/Controller device to the USB Type A Host port on the TeensyROM board.
    * Note: If your MIDI controller has a 5-pin DIN connector instead of USB, you can use an adapter such as [this link](https://www.amazon.ca/USB-OUT-MIDI-Cable-Converter/dp/B077X7R74Y?th=1).
  * Select MIDI device to emulate
    * **Note: if using the ***built-in*** CynthCart, Station64, or MIDI2SID app**, the correct IO is already associated, **you can skip this step.**
    * Prior to running a MIDI program, go to the settings menu (F8) 
    * Under "Special IO:", select your preferred "MIDI:*" device by cycling through the options.
    * The following MIDI cartridges can be emulated/selected:
      * Sequential, Datel/Siel, Passport/Sentech, Namesoft
    * All use $DExx address space and IRQ for interrupts
  * Select/load a MIDI capable application on your C64 to receive the MIDI data from your controller
  * Play around and have fun!
  * MIDI out (C64 to MIDI Device) is also implemented so that keyboards, etc with their own sound capability can be "played" by the C64
  * Some C64 sequencer apps require the 6840 timer chip in Passport/Namesoft, which is not currently emulated.

### Streaming MIDI/SID data/sounds/music directly to your C64/128 from a modern computer
  **Full control of your C64's SID chip remotely using the ASID MIDI protocol**
  * See the **[ASID Player document](/docs/ASID_Player.md)**

  **Playing .MIDI/.MID files on your C64 from a computer**
  * **C64/128 Setup**
    * Connect USB cable from a MIDI Host/computer to the USB Type B Micro Device port on the Teensy module.
    * Power up C64/128 to TeensyROM main menu.
    * Select Cynthcart+Datel MIDI or Station64+Passport MIDI to play the MIDI data
      * Select a polyphonic voice profile to get as many independent notes as possible (3).
    * The buit-in MIDI2SID (F8 from the main menu) can be used instead of these
      * Can be helpful in seeing/hearing independent notes and seeing any voice overflows
      * No other MIDI special HW emulation required
  * **PC Instructions using Cakewalk by BandLab.**
    * Other DAWs/Players may work similarly.  If you find another good one, please let me know
    * Download/Install from here: https://www.bandlab.com/products/cakewalk
      * No need to activate/purchase unless you otherwise desire
    * In Cakewalk, press "p" or select Edit/Preferences to enter the preferences window
      * Select MIDI/Devices
        * Check TeensyROM under the "Outputs" section, and click OK
      * Select Project/MIDI
        * Uncheck "Zero Controllers when play stops" under Other Options
      * Click OK to close Preferences window
    * Load a .mid/.midi file and play it
      * Remember that there are only 3 voices in the SID, so complex midi files with more simultaneous notes/voices won't sound right.
    * The playback should be eminating from your C64/128!

<br>

[Back to main ReadMe](/README.md)
