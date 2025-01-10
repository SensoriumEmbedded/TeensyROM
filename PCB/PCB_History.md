# PCB/Design History:
## Latest firmware will work with all PCB versions 0.2 and higher, recommend always building with the latest
### **[Link to latest design at OSH Park](https://oshpark.com/shared_projects/klnNznNJ)**

## **v0.3: Minor Update released Dec 11, 2024**
  * Fix for screen noise when using UltiMax .crt files on C128 machines
    * Data Buffer (U5) Dir controlled directly from Teensy instead of R/nW signal
      * Previous "debug" signal used for this.
      * Compatible with FW v0.6.4 and higher. 
        * Future FW will continue to be compatible with all PCB versions.
  * Removed (unused) Dot_Clock input to Teensy, now spare/debug signal to header
  * C1 & C2 (22uF bulk caps) replaced with 1210 size SMT versions
  * C9 added for Vhst (optional)

![TeensyROM v0.3](../media/v0.3/v0.3_top.png)

## **v0.2c: Minor Update released Nov 1, 2023**
  * **No electrical changes from previous version**
  * **Mounting hole added to better accomodate cartridge case**
  * Removed C9 and JP1 (both unused)
  * Added 4th pin to unused header (J4)
  * Narrowed C1/C2 pad spacing 2.5->2.0mm to better fit caps
  * Slightly increased 6ea 0805 cap SMT pad sizes for easier assy
  * Silk screen changes:
    * under Teensy: "Reminder: Program/Prep Teensy before installing"
    * serial number location and label box on back under Teensy
    * values (1k/10k) by 3ea SMT Rs

![TeensyROM v0.2c](../media/v0.2c/v0.2c_top.png)

## **v0.2b: Minor Update released July 1, 2023**
  * R3 added to ensure data buffer is off during reset/programming

## **v0.2: Released Feb 23, 2023**

![TeensyROM v0.2](/media/v0.2/v0.2%20top.jpg)

### **Changes from 0.1 PCB:**
  * New features:
    * Ethernet port connector
    * USB host port connector
  * Other Improvements/Notes:
    * Corrected: Data bits 2 and 1 swapped in v0.1
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
![TeensyROM v0.1](/media/v0.1/v0.1.jpg)   First PCB prototype, no longer supported.
   No USB Host or Ethernet connectors, dongles directly from Teensy needed for these.  This makes this board slightly smaller.

## **v0.0: Early January 23**
![TeensyROM v0.0](/media/v0.0/v0.0.jpg)
Protoboard proof of concept.  It worked well to prove out the basic architecture+FW, and allowed PCB design with confidence. 

<br>

[Back to main ReadMe](/README.md)
