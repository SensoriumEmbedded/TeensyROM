# TeensyROM
 A Commodore 64 ROM emulator and loader/interface cartidge based on the Teensy 4.1
   by Travis Smith <travis@sensoriumembedded.com> 

 I know there are plenty of Emulators/loaders out there, but I wanted to design my own for the sheer fun of it.  I'm publishing all deign files, code, etc for anyone interrested. 

 Features as of 2/9/23:
- Immediate Load/run/emulate files from:
- `USB thumb Drive`
- `SD card`
- `Teensy Internal Flash`
- `Transfer directly from PC (C# Windows app included)`
- Sets C64 system time from internet (via Ethernet)
- MIDI USB in -> SID Polyphonic player with waveform/envelope controls
- Emulates "Normal" .crt files (8khi/lo/16k)
- Loads/executes .prg files

   v0.1 PCB completed and tested!
   This will not be the "final" version though.  Next spin will add Ethernet and USB host connectors to remove the dongles currently in use, also exploring other potential improvements/features.
   
Compatability: Has only been tested on NTSC C64 machines to this point

![](/pics-vid/v0.1.jpg)
   See /pics-vids folder for more.
