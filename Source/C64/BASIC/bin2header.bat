cls
:: bin2header to convert/copy  to C header file

@echo off

SET sourceFullPath="D:\MyData\Geek Stuff\Projects\Commodore 64\Hardware\Expansion Port\TeensyROM\TeensyROM\Source\C64\BASIC\DMACheck.prg"

::SET sourcePath=D:\MyData\Geek Stuff\Projects\Commodore 64\Hardware\Expansion Port\TeensyROM\ref\Picture formats\pics\Favs
::SET sourceFile=Ex_Pie2.art

setlocal EnableDelayedExpansion
::set toolPath=D:\MyData\Geek Stuff\Projects\Commodore 64\Software\PC Utils-SW
::SET bin2headerPath=%toolPath%\bin2header
::SET bin2header=bin2header.exe

set bin2headerPy="d:/MyData/Geek Stuff/Projects/Commodore 64/Software/PC Utils-SW/bin2header/bin2header/src/bin2header.py"
set PythonExe="C:/Users/trav/AppData/Local/Microsoft/WindowsApps/python3.11.exe"

@echo on
echo ***Start...

echo ***bin2header
::"%bin2headerPath%\%bin2header%" %sourceFullPath%
:: Add "PROGMEM " type modifier to force to flash memory
%PythonExe% %bin2headerPy% -t "PROGMEM " %sourceFullPath%



rem exit /b

pause