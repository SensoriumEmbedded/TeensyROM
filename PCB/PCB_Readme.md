# Teensy connection to TeensyROM
**There are 3 potential methods to attach the Teensy 4.1 to the TeensyROM PCB**
The first 3 columns of the [TeensyROM BOM](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/PCB/v0.2%20archive/TeensyROM%20v0.2%20BOM.xlsx) show which parts are required for each configuration/method
| Method | Advantages | Disadvantages |
|--|--|--|
| Direct attach: Solder Teensy directly to TeensyROM | Least number of sockets needed, lower profile | Can't re-use Teensy in other projects |
| Sockets for all pins | No USB Host or Ethernet dongles required, use on-board connectors (v0.2 only) | Teensy center pins pointing downward may interfere with other projects |
| Sockets for I/O only | No Teensy center pins pointing downward, better for proto-board & other project compatibility | USB Host and Ethernet dongles required for those functions, can't use on-board connectors|

**Sockets for all -or- Direct attach Teensy pin config pics:**
(USB Host/Ethernet pins on bottom)
|  | ![Bottom pins](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Teensy/Teensy%20Bot%20Pins.jpg) | ![Direct Connect](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2/20230307_163653.jpg) |
|--|--|--|

**IO only/Dongle Teensy pics:**
(USB Host/Ethernet pins on top)
| |![enter image description here](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Teensy/Teensy%20Top%20Pins.jpg) | ![Direct Connect](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.1/With_Dongles.jpg) |
|--|--|--|

# TeensyROM Assembly Instructions:
**In recommended build order...**
## TeensyROM PCB SMT Assembly
- **BOM item # 1-5**
- Observe pin 1 markings on the 5ea ICs
## TeensyROM PCB THM Assembly
- **BOM item # 6-8**
- Check LED polarity (Sq hole=short lead=flat side=Cathode)
- Check THM capacitor polarity ('+'  mark on PCA opposite '-' mark on caps)
## USB/Ethernet connector Assembly
- **BOM item # 13-15**  For full socketed or direct connect Teensy only
## Teensy Socket connector Assembly
- **BOM item # 16**  For full socketed Teensy or IO only configs
- **BOM item # 17-18**  For full socketed Teensy only
- Hold/tape sockets straight, or insert male headers to hold with a proto board.
## Teensy power prep
**This is an important step!**  Regardless of the attach method chosen, the 5v/USB connection must be cut so that the C64 won't be back-fed power from USB.  There is a small jumper trace between two pads on the back side of the Teensy that must be cut, see picture below.   You can use an ohm meter to be sure the connection between the two pads is severed.  This can be un-done later for other project either with a solder blob, or a jumper between the 5v pin and Vin.
![enter image description here](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/Teensy/Teensy%20Power.jpg)

## Teensy header placement
- **BOM item # 9a-12**
- Place 1x24 headers on bottom of Teensy, solder from top.  Use a solderless proto board or completed TeensyROM to hold the pins in position while soldering.
- Based on USB/Ethernet connection type you choose, mount the 2x3 Ethernet and 1x5 USB header on the top or bottom of Teensy.  Use either tape to hold them straight (top mount) or use completed TeensyROM to hold the pins while soldering (bottom mount).
- Remove adhesive cover and attach heatsink to Teensy CPU (center).  Be sure heatsink is not contacting large capacitor near it.
## TeensyROM/Teensy Final Assembly
- Full socket config: insert Teensy into sockets, use on-board USB/Ethernet conns
- Direct attach: insert Teensy into TeensyROM and solder directly, use on-board USB/Ethernet conns- IO only config: insert Teensy into sockets, use USB/Ethernet dongles
> **Assembly complete, next step is to load the Firmware!**

# PCB/Design History:
## **v0.2: Released Feb 23, 2023**
![TeensyROM v0.2](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.2/v0.2%20top.jpg)
### **Link to design at OSH Park :**    <a href="https://oshpark.com/shared_projects/zJfB98zq"><img src="https://oshpark.com/packs/media/images/badge-5f4e3bf4bf68f72ff88bd92e0089e9cf.png" alt="Order from OSH Park"></img></a>

### **Changes from 0.1 PCB:**
  * New features:
    * Ethernet port connector
    * USB host port connector
  * Other Improvements/Notes:
    * Corrected: Data bits 2 and 1 swapped in v0.1 (worked around in SW)
    * shield connections:  USB is GND, Eth shorted to gnd via JP1
    * 0805 shapes:  left them the same, R's accomodate 0603s, prob not caps
    * Teensy symbol: Removed unused pins, made LAN header holes larger
    * Extended 74lvc245 pads inward to support narrow packages if needed, wide still prefered
    * Silk screen "Top" marking, project URL
    * pin 1 dots outside package outlines
    * Turned hex inverter 180 deg, gate swaps.  All ICs oriented same dir now
    * mitered all PCB corners: 0.0325"
    * 90 approved DRC errors:  88 due to gold finger proximity to edge, 2 due to shorting symbol (JP1)
    * Trace length matching: "run length eth_t*", "run length eth_r*", "run length dm dp"    or "run length-freq-ri"
    * BOM/REF des updated
    * Text size/vectors same for ref-des

## **v0.1: Released January 29, 2023**
![TeensyROM v0.1](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.1/v0.1.jpg)   First PCB prototype, fully tested and working with released code using "#define HWv0_1_PCB" build option
   No USB Host or Ethernet connectors, dongles directly from Teensy needed for these.  This makes this board slightly smaller.

## **v0.0: Early January 23**
![TeensyROM v0.0](https://github.com/SensoriumEmbedded/TeensyROM/raw/main/pics-info/v0.0/v0.0.jpg)
Protoboard proof of concept.  It worked in proving the basic concept and allowed PCB design with confidence. 
