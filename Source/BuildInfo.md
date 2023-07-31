
# TeensyROM software/firmware build instructions

## Main TeensyROM C/C++ aplication
### Software tools needed
  * [Arduino IDE 2.x or 1.x](https://www.arduino.cc/en/software)
  * [Teenyduino app](https://www.pjrc.com/teensy/td_download.html)
  * Instal as directed in links above

### Build parameters/instructions
  *  In the Arduino IDE
     * Load the Teensy.ino file from the /Source/Teensy directory
     * Tools menu item settings:
       * Board: "Teensy 4.1"
       * Port: Select target Teensy
       * USB Type: "Serial + MIDI"
       * Default settings are fine for all others
     * Build the project and download directly to TeensyROM
       * TeensyROM needs to be powered by a C64/128 for programming since the Teensy USB power trace should be severed during assembly.
     * Alternately, you can generate a .hex file and put it on a SD/USB drive
       * See FW update section of the [General Usage doc](SensoriumEmbedded/TeensyROM/docs/General_Usage.md)

### Latest Support tool/lib versions as of v0.4 on 7/31/23
   * Arduino IDE 2.1.1
   * Teensyduino 1.58
   * Included libraries
     * SD at version 2.0.0            
     * SdFat at version 2.1.2         
     * SPI at version 1.0             
     * USBHost_t36 at version 0.2     
     * NativeEthernet at version 1.0.5
     * FNET at version 0.1.3          
     * EEPROM at version 2.0       

## C64/128 6502 Assembly code
These steps are only needed if modifying the aplication menu assembly code running on the C64/128.
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
  * build info displayed and files created
  * Main TeensyROM aplication must be recompiled to incorporate header/code and load to Teensy module for execution
   
## Windows C# Utility
These steps only needed if modifying the Windows C# app used to directly pass .crt or .prg files from a PC to the TeensyROM
### Software tools needed
  * [Microsoft Visual Studio](https://visualstudio.microsoft.com/downloads/) (Community or higher)
    * Include C# tools

### Edit and build in the Visual Studio Environment

<br>

[Back to main ReadMe](SensoriumEmbedded/TeensyROM/README.md)
