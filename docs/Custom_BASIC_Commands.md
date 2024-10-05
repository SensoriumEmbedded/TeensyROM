
# Custom BASIC Commands

## Communicate with and through your TeensyROM from C64 BASIC  
* See the brief demo video [available here.](https://youtu.be/5qShZjLOG5s)
* Instantly load and Save BASIC programs to/from your TR
  ```
  TLOAD "MYPROG.PRG"
  TSAVE "SD:MYDIR/TERM.PRG
  TDIR
  ```
* Communicate via USB serial to/from another computer.
  * Write BASIC programs that can receive/display externally provided data
  * Send information or user input to another computer.
  * Example simple terminal program:
    ```
    10 GET A$ : TPUT A$
    20 TGET B$ : PRINT B$;
    30 GOTO 10
    ```

Implementation is built upon **[Custom Basic Commands](https://github.com/barryw/CustomBasicCommands)** Copyright 2023 by [Barry Walker](https://github.com/barryw)

To begin, select `BASIC w/ Custom Commands +TR BAS` from the main TR menu. (as of FW 0.6.2)

## TeensyROM specific commands available:
|Command|Description|
|:--|:--|
|`TSAVE <media><path><filename>`|Super fast Saves the current BASIC file to SD or USB on the TR|
|`TLOAD <media><path><filename>`|Super fast Loads a BASIC file from SD or USB on the TR|
|`TDIR <media><path>`|Display directory contents of TR media|
|`TPUT <string out>`|Send the full contents of <string out> out the TR USB Serial port|
|`TGET <string char in>`|Read a singe character from the TR USB Serial port into <string char in>|

### Command argument details:
|Argument|Description|Requirement|
|:--|:--|:--|
|`<media>`|Either `USB:` or `SD:` to specify media type|**optional**, USB is default|
|`<path>` |Path to file from root|**optional**, root is default|
|`<filename>`|Name of the program file|**required**, use `.prg` extension|

<BR>

## Capabilities included from the original implementation:

**BASIC will now accept integers as either HEX or binary, in addition to decimal.** 
<BR>For example: `POKE $0400, $0a` or `POKE $d020, %00001111`. 
<BR>You're not limited to 16-bit values, so `PRINT $d00000` is perfectly valid.
<BR>
<BR>

### Additional Commands

Note that some of these require additional HW such as an REU, or can conflict with memory space used by these commands themselves.

|Command|Description|
|:--|:--|
|`BACKGROUND` |Sets the background color: Example: `BACKGROUND 2` would set it to red|
|`BORDER` |Sets the border color. Works the same as BACKGROUND|
|`CLS` |Clears the screen. Nice and simple.|
|`WOKE` |A 16-bit version of the POKE command. Example: `WOKE $61, $0400` would put $00 into $61 and $04 into $62.|
|`MEMCOPY` |A fast way to move bytes around in memory. Example: `MEMCOPY $0428, $0400, $28` would copy the 2nd line of text to the first. This could be used to quickly scroll portions of the screen.|
|`MEMFILL` |No more slow FOR/NEXT loops to POKE values into memory. Example: `MEMFILL $0400, 1000, 32` would clear the default screen.|
|`BANK` |Select the current VIC bank. Must be between 0-3 where 0 starts at $0000 and 3 starts at $c000. This affects all graphics on the machine including screen, chars and sprites|
|`SCREEN` |Select the 1k offset for the current video screen. The default is 1 ($0400) in bank 0. Each bank can hold 16 screens (not all can be used for screen RAM), so the value here has to be between 0 and 15.|
|`STASH` |Copy bytes from the C64 to an attached REU. Example `STASH $0400, $0, 1000, 0` would copy the default screen to REU address $0 in bank $0.|
|`FETCH` |Copy bytes from an attached REU to the C64. Example `FETCH $0400, $0, 1000, 0` would copy bytes from REU address $0, bank $0 to the C64's default screen $0400|
|`MEMLOAD` |Loads binary data from disk into memory. Example `MEMLOAD "SHAPES.SPR",8,$8000` would load shape data from the file `SHAPES.SPR` to $8000, which is the start of VIC bank 2|
|`MEMSAVE` |Saves binary data from memory to disk. Example `MEMSAVE "@:SHAPES.SPR,P,W",$8000,$8040` would save the 64 bytes between $8000 and $8040 to a file named `SHAPES.SPR`|
|`SPRSET` |Turn sprites on/off as well as set their pointer to their shape data.|
|`SPRPOS` |Set a sprite's X and Y positions. X can be 0-511 and Y can be 0-255|
|`SPRCOLOR` |Set a sprite's color|
|`DIR` |Display a disk directory listing. Example `DIR 8` would display the disk directory of a disk in device 8.|

### Additional functions
|Function|Description|
|:--|:--|
|`WEEK` |A version of the PEEK function which returns a 16-bit word instead of an 8-bit byte. Example: `PRINT WEEK($61)`|
|`SCRLOC` |Returns the absolute address for the start of screen RAM. At startup `PRINT SCRLOC(0)` would return `1024`. You can use the `BANK` and `SCREEN` commands to relocate the screen.|
|`REU` |Detect the type of attached REU, if any. Currently this is broken and needs some TLC.|

<br>

[Back to main ReadMe](/README.md)