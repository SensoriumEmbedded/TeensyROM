
# TeensyROM USB MIDI Usage

## USB MIDI Host port
### Sending MIDI data from a MIDI/USB keyboard to/from your C64
  * Connect USB cable from a MIDI Keyboard/Controller device to the USB-A Host port on the TeensyROM board.
  * Prior to running a MIDI program, go to the settings menu. Under "Special IO:", select your preferred "MIDI:*" device by cycling through the options.
    * Note: if using the built-in CynthCart, Station64, or MIDI2SID app, the Special IO is already associated and you can skip this step.
    * The following MIDI cartridges can be emulated/selected:
      * Sequential
      * Datel/Siel
      * Passport/Sentech (w/o 6480 timer)
      * Namesoft (w/o 6480 timer)
    * All use DExx address space and IRQ for interrupts
  * Select/load a MIDI capable application to receive/play the MIDI data from your controller
  * Play around and have fun!
  * MIDI out (C64 to MIDI Device) is also implemented so that keyboards, etc with their own sound capability can be "played" by the C64
  * Some sequencer apps work, others require the 6840 timer chip in Passport/Namesoft, which is not currently emulated.

<BR>

## USB MIDI Device port 
### Streaming MIDI/SID data to your C64/128 from a modern computer
  * **Controlling your C64's SID chip using the ASID MIDI protocol** (FW v0.5.15 and higher)
    * This protocol was designed by Elektron to send data to their SIDstation device, but has been adopted for general use by the community
    * **C64/128 Setup**
      * Connect USB cable from the Micro USB-B Device port on the Teensy module to a computer or sequencer with a USB-A Host port.
      * Power up C64/128 to the TeensyROM main menu.
      * Select "TeensyROM ASID Player", or press '4' for fast hotkey access.
        * Starts ready to receive single SID ASID data
        * See on-screen menu for additional commands.
    * **PC instructions to stream .sid files from the internet to your C64**
      * In your browser, navigate to https://deepsid.chordian.net/
        * Select "ASID (MIDI)" from the drop-down in the upper left corner
        * Select "TeensyROM" from the "MIDI port for ASID" drop-down
        * Select your SID from the vast library and play it
        * The playback should be eminating from your C64/128!
    * **PC instructions to stream .sid files from your PC hard drive to your C64**
      * Go to https://www.elektron.se/us/download-support-sidstation and download/install/launch "ASID for Sidstation"
        * Click the "Conf" button and highlight "TeensyROM", click OK
        * Either Click "Load" or drag/drop a .sid file into the window.
        * Click "Play"
        * The playback should be eminating from your C64/128!
        * You can play with the frequency to adjust the playback tempo if needed.
    * **Control your SID chip directly as a synthesizer**
      * Go to https://www.plogue.com/products/chipsynth-c64.html and download/install/launch "chipsynth C64"
        * Select the "EMU" tab, in the ASID box, enable output of "Synth V1" and "TeensyROM" as the destination
        * There are many things you can do with this great program, purchasing will unlock the 10 minute limit per use.
        * Here's a [demo video](https://www.youtube.com/watch?v=-Xs3h59-dOU) showing some of the capabilities

<BR>

  * **Playing .MIDI/.MID files on your C64 from a computer**
    * **C64/128 Setup**
      * Make sure your TeensyROM is connected to the computer via USB
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
