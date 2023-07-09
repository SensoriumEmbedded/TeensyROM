# TeensyROM Assembly Instructions:

The TeensyROM was designed with hand assembly in mind. While surface mount packages are used, they are the larger types (SOIC IC packages and 0805 passives).  If you feel this is beyond your solder capabilities, assembled units are usually available at a fair price. If there are additional questions, feel free to [contact me](mailto:travis@sensoriumembedded.com).

| ![Top View](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2b/v0.2b_top.jpg) | ![Bot View](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2b/v0.2b_Bot.jpg) |
|--|--|

- Tools/materials needed: 
  - Soldering iron/Solder (lead-free or otherwise)
  - Wire cutters (aka Side Cutters, Dykes)
  - Tweezers
  - Vice to hold work
  - Small knife (ie Exacto)
  - Workspace with good lighting and magnification
  - Parts listed in the **[TeensyROM BOM](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/PCB/v0.2%20archive/TeensyROM%20v0.2b%20BOM.xlsx)**
  - [Teensyduino app](https://www.pjrc.com/teensy/td_download.html)
  - [Arduino app](https://www.arduino.cc/en/software) (only if self compiling/customizing)

## Assembly steps, in recommended order:
### TeensyROM Surface Mount Assembly
- **BOM item # 1-6**
- Observe pin 1 marking/orientation on the 5ea ICs
- Recommend tinning a single pad of each device and attaching first.
  - Once placement looks good with single pin, solder remainig pin(s)  

### Teensy prep: *Important!*  
- **BOM item # 7**
- Load the initial firmware to be used into the Teensy module. 
  - This should be done **before** assembling and connecting to a C64/128 for the first time so that IO is driven correctly.
  - The easiest method is to use the latest .hex file from [Here](https://github.com/SensoriumEmbedded/TeensyROM/tree/main/Source/TeensyROM/build/teensy.avr.teensy41) with TeensyDuino to directly load via USB.
  - Alternately, the code can be compiled/customized and programmed via the Arduino and Teensyduino apps.
- Disconnect the Teensy module from USB
- The 5v/USB connection on the Teensy module must be cut so that the C64 won't be back-fed power from USB.
  - Find the small jumper trace between two pads on the back side of the Teensy (see pic below)
  - After cutting, plug the module back in to USB to be sure the LED does ***not*** come on.
    - The module will be supplied power from the C64/128 when fully assembled.
![Pwr_cut_view](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Teensy/T41_pwr_cut.jpg)

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
