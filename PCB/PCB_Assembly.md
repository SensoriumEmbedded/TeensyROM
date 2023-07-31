# TeensyROM Assembly Instructions:

The TeensyROM was designed with hand assembly in mind. While surface mount packages are used, they are the larger types (SOIC IC packages and 0805 passives).  If you feel this is beyond your solder capabilities, assembled units are usually available at a fair price. If there are additional questions, feel free to [contact me](mailto:travis@sensoriumembedded.com).

| ![Top View](/media/v0.2b/v0.2b_top.jpg) | ![Bot View](/media/v0.2b/v0.2b_Bot.jpg) |
|--|--|

- Tools/materials needed: 
  - Soldering iron/Solder (lead-free or otherwise)
  - Wire cutters (aka Side Cutters, Dykes)
  - Tweezers
  - Vice to hold work
  - Small knife (ie Exacto)
  - Workspace with good lighting and magnification
  - Computer with USB and the [Teensyduino app](https://www.pjrc.com/teensy/td_download.html)
  - Parts listed in the [TeensyROM BOM](/PCB/v0.2%20archive/TeensyROM%20v0.2b%20BOM.xlsx)
    - Including bare PCB [Link to latest design at OSH Park](https://oshpark.com/shared_projects/m7YLgscM)

## Teensy prep: *Important: complete these in order shown!*  
- These steps need to be done **before** assembling the TeensyROM and connecting to a C64/128 for the first time.
- Load the initial firmware to be used into the Teensy 4.1 module. 
  - This first programming needs to be done using Teensyduino app
  - Process is described in the [General Usage Document](/docs/General_Usage.md)
- Disconnect the Teensy module from USB
- The 5v/USB connection on the Teensy module must be cut so that the C64 won't be back-feed power from USB, or vice-versa.
  - Find the small jumper trace between two pads on the back side of the Teensy (see pic below)
  - Crefully cut the trace with an Exacto knife
  - After cutting, plug the module back in to USB to be sure the LED does ***not*** come on.
    - This verifies that the trace is cut.
  - The module will be supplied power from the C64/128 when fully assembled.
![Pwr_cut_view](/media/Teensy/T41_pwr_cut.jpg)

## Assembly steps, in recommended order:
### TeensyROM Surface Mount Assembly
- **BOM item # 1-6**
- Observe pin 1 marking/orientation on the 5ea ICs
- Recommend tinning a single pad of each device and attaching first.
  - Once placement looks good with single pin, solder remainig pin(s)  

### Teensy/header placement
- **BOM item # 7-10**
- Place the PCB in a small vice with the top side up, being careful not to contact previously placed components
- Place 2ea 1x24 headers, 1ea 1x5 header, and 1ea 2x3 2mm header (BOM items 8-10) into the TeensyROM PCB with the longer pins facing down through the PCB.
- Place the Teensy module (prepared in previous step) on the pins making sure all 59 pins are showing through it.
- Solder all 59 pins from the top, taking care not to bridge or touch any components on the Teensy itself.
- Holding on to the Teensy assembly, turn the PCB over and solder a pin or two while holding it in place.  Then solder the all the remaining pins from the bottom.
- Cut the protruding pins from the bottom so they don't interfere, scratch, or short.

### TeensyROM Through Hole Assembly
- **BOM item # 11-15**
- **Observe propper polarity:**
  - LED: Square hole=short lead=flat side=Cathode
  - THM capacitors: '+' mark on PCA opposite '-' mark on caps
  
### Teensy Heatsink Assembly
- **BOM item # 16**
- Use a small amount of thermal glue (prefered) or thermal tape to attach heatsink to center of the microcontroller in the center of the Teensy module.
- Make sure the heatsink is not contacting any pins or other components, such as the large capacitor near it.

### **Assembly complete, ready for fun!**

<br>

[Back to main ReadMe](/README.md)
