
## **v0.2: Released 2/23/22**
**Second protype, Link to designOSH Park :** 
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; <a href="https://oshpark.com/shared_projects/zJfB98zq"><img src="https://oshpark.com/packs/media/images/badge-5f4e3bf4bf68f72ff88bd92e0089e9cf.png" alt="Order from OSH Park"></img></a>
**Changes from 0.1 PCB:**
  * New features:
    * Ethernet port
    * USB host port
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

## **v0.1: Released 1/29/23**
   First prototype, fully tested and works with released code using "#define HWv0_1_PCB" build option
   No USB Host or Ethernet connectors, dongles directly from Teensy needed for these
