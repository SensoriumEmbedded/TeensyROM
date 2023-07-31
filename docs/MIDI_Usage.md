
# TeensyROM USB MIDI Usage

## USB MIDI Host port
### Hardware connection:
  * Cable from a USB MIDI Keyboard/Controller device to the USB-A Host port on the TeensyROM board.
### Sending MIDI data to/from your C64 from a MIDI/USB kayboard
  * Select "MIDI:*" Special IO HW from the settings menu prior to running a MIDI program.
    * The following MIDI cartridges can be emulated/selected:
      * Sequential
      * Datel/Siel
      * Passport/Sentech (w/o 6480 timer)
      * Namesoft (w/o 6480 timer)
    * All use DExx address space and IRQ for interrupts

  * Select CynthCart, Station64, the MIDI2SID app, etc. to receive/play the MIDI data from your controller
  * Play around and have fun!
  * MIDI out (C64 to MIDI Device) is alo implemeted so that keyboards, etc with their own sound capability can be "played" by the C64
  * Some sequencer apps work, others require the 6840 timer chip in Passport/Namesoft, which is not currently emulated.  May add later if needed/requested
    
<BR>

## USB MIDI Device port
### Hardware connection:
  * Cable from the Micro USB-B Device port on the Teensy module to a computer or sequencer with a USB-A Host port.
    
### Sending MIDI/SID data to your C64/128 from a modern computer

  * **Playing .sid files on your C64 from a computer** (FW v0.4 and higher)
    * You can use your C64/128 as a directly controlled SID chip with the use of the ASID MIDI protocol.
    * This protocol was designed by Elektron to send data to their SIDstation device, but has been adopted for general use by the community
    * **C64/128 Setup**
      * Make sure your TeensyROM is connected to the computer via USB
      * Power up C64/128 to TeensyROM main menu.
      * Select Station64+Passport MIDI to play the ASID/MIDI data
        * In Station64, select F4 to enter setup mode, then F2 for the ASID player
        * ASID player control keys:
          * F7: Screen on/off
          * F8: SID Reset
          * 1/2/3: Toggle SID Voice
          * <-(left arrow): Escape to synth/arp/perf window
    * **PC instructions to stream .sid files from the internet to your C64**
      * In your browser, navigate to https://deepsid.chordian.net/
        * Select "ASID (MIDI)" from the drop-down in the upper left corner
        * Select "TeensyROM" from the "MIDI port for ASID" drop-down
        * Select your SID from the vast library and play it
        * The playback should be eminating from your C64/128!
    * **PC instructions to stream .sid files from your PC hard drive to your C64**
      * Go to https://www.elektron.se/us/download-support-sidstation and download/instal/launch "ASID for Sidstation"
        * Click the "Conf" button and highlight "TeensyROM", click OK
        * Either Click "Load" or drag/drop a .sid file into the window.
        * Click "Play"
        * The playback should be eminating from your C64/128!
        * You can play with the frequency to adjust the playback tempo if needed.

<BR>

  * **Playing .midi/.mid files on your C64 from a computer**
    * **C64/128 Setup**
      * Make sure your TeensyROM is connected to the computer via USB
      * Power up C64/128 to TeensyROM main menu.
      * Select Cynthcart+Datel MIDI or Station64+Passport MIDI to play the MIDI data
        * Select a polyphonic voice profile to get as many independant notes as possible (3).
      * The buit-in MIDI2SID (F8 from the main menu) can be used instead of these
        * Can be helpful in seeing/hearing independant notes and seeing any voice overflows
        * No other MIDI special HW emulation required
    * **PC Instructions using Cakewalk by BandLab.**
      * Other DAWs/Players may work similarly.  If you find another good one, please let me know
      * Download/Instal from here: https://www.bandlab.com/products/cakewalk
        * No need to activate/purchase unless you otherwise desire
      * In Cakewalk, press "p" or select Edit/Preferences to enter the preferences window
        * Select MIDI/Devices
          * Check TeensyROM under the "Outputs" section, and click OK
        * Select Project/MIDI
          * Uncheck "Zero Controllers when play stops" under Other Options
        * Click OK to close Preferences window
      * Load a .mid/.midi file and play it
        * Remember that there are only 3 voices in the SID, so complex midi files with many simultaneous notes/voices won't sound right.
      * The playback should be eminating from your C64/128!

<br>

[Back to main ReadMe](/README.md)
