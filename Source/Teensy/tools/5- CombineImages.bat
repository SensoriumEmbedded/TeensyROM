
@echo off
cls

set MinimalHex=..\MinimalBoot\build\teensy.avr.teensy41\MinimalBoot.ino.hex
set MainHex=..\build\teensy.avr.teensy41\Teensy.ino.hex
set CombinedHex=build\TeensyROM_full.hex

::if exist %CombinedHex% del %CombinedHex%

HexCombineUtil\HexCombine.exe %MinimalHex% %MainHex% %CombinedHex%

@echo .
@echo    *** Verify file dates and memory locations!
@echo .

pause
