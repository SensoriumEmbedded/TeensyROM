
# TeensyROM software/firmware build instructions

## Main TeensyROM C/C++ aplication
### Software tools & lib needed
  * [Arduino IDE 2.x or 1.x](https://www.arduino.cc/en/software)
  * [Teenyduino app](https://www.pjrc.com/teensy/td_download.html)
  * Install as directed in links above
  * The PN532 NFC library can be found [here](https://github.com/elechouse/PN532)

### Build parameters/instructions
  * In the Arduino IDE
     * Load the Teensy.ino file from the /Source/Teensy directory
     * Tools menu item settings:
       * Board: "Teensy 4.1"
       * Port: Select target Teensy
       * Optimize: "Faster"
       * CPU Speed: "600 MHz"
       * USB Type: "Serial + MIDI"
     * Build the project and download directly to TeensyROM
       * TeensyROM needs to be powered by a C64/128 for programming since the Teensy USB power trace should be severed during assembly.
     * Alternately, you can generate a .hex file and put it on a SD/USB drive
       * See FW update section of the [General Usage doc](/docs/General_Usage.md)
   * Note: To do a full build including the minimal image for large CRT files, see [this doc](Teensy/tools/_Dual Boot Build.txt).
   
### Latest Support tool/lib versions as of v0.6.7 on 6/14/2025
   * Arduino IDE 2.3.2
   * Teensyduino 1.59
   * Included libraries
     * SD at version 2.0.0
     * SdFat at version 2.1.2
     * SPI at version 1.0
     * USBHost_t36 at version 0.2
     * NativeEthernet at version 1.0.5
     * FNET at version 0.1.3
     * EEPROM at version 2.0
     * PN532 (link above) last updated 9/12/2018

## C64/128 6502 Assembly code
These steps are only needed if modifying the application menu assembly code running on the C64/128.
### Software tools needed
  * [ACME Cross-Compiler](https://sourceforge.net/projects/acme-crossass/)
  * [bin2header util](https://github.com/AntumDeluge/bin2header)

### Build instructions
  * Edit the "build8000CartBin.bat" file in the C64 directory
    * Set "toolPath" to an absolute path of the SW tools
    * Edit the following 2 variables to point to the associated tool directory
      * Relative, based on toolPath: "compilerPath", "bin2headerPath"
  * Execute the batch file to complete the following
    * Compile the main TeensyROM Code
    * Compile the Cartridge loader executed on startup
    * Convert the final binary into a header file for the Teensy code
    * Copy the updated header file to the Teensy directory
  * Any compile errors will cause early exit
  * Build information is displayed and files are created
  * Main TeensyROM application must be recompiled to incorporate header/code and load to Teensy module for execution
   
<br>

[Back to main ReadMe](/README.md)
