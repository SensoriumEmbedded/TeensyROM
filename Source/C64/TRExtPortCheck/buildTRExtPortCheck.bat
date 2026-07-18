cls

@echo off

set MainFilename=TRExtPortCheck

call ../SetToolPaths.bat

::old laptop:
::SET toolPath="D:\MyData\Geek Stuff\Projects\Commodore 64\Software\PC Utils-SW"
::SET compilerPath=%toolPath%\C64-devkit\compiler\win32
::SET PythonExe="C:/Users/trav/AppData/Local/Microsoft/WindowsApps/python3.11.exe"
::SET emulatorPath=%toolPath%\Emulation\GTK3VICE-3.6.1-win64\bin

::new laptop:
::SET toolPath="C:\Users\trav\AppData\Roaming"
::SET compilerPath=%toolPath%\acme0.97win\acme
::SET PythonExe="py"
::SET emulatorPath=C:\Users\trav\AppData\Local\GTK3VICE-3.9-win64\bin


setlocal EnableDelayedExpansion
SET clean=del
SET cleanArgs=/F /Q 
SET buildPath=build
SET sourcePath=source

SET compiler=acme.exe

SET MainBuild=%MainFilename%.prg
SET MainCompilerArgs=-r %buildPath%\BuildReport --vicelabels %buildPath%\Labels --msvc --color --format cbm -v3 --outfile
rem --format plain leaves off the 2 byte address from the start of the file.  "cbm" includes it

SET bin2headerROMPath=..\..\Teensy\TRMenuFiles\ROMs
:: Python script updated to allow type modifier (-t "PROGMEM ") addition
set bin2headerPy="bin2header.py"

SET emulator=x64sc.exe
SET emulatorArgs=-autostart

::***************************************************************************************************************

@echo on
echo ***Start...
md %buildPath%
%clean% %cleanArgs% %buildPath%\*.*

echo ***Compile Main...
%compilerPath%\%compiler% %MainCompilerArgs% %buildPath%\%MainBuild% %sourcePath%\%MainFilename%.asm
if NOT %ERRORLEVEL% == 0 exit /b 1

echo ***bin2header
::%bin2headerPath%\%bin2header% %buildPath%\%MainBuild%
:: Add "PROGMEM " type modifier to force to flash memory
%PythonExe% %bin2headerPy% -t "PROGMEM " %buildPath%\%MainBuild%
if NOT %ERRORLEVEL% == 0 exit /b 1

copy %buildPath%\%MainBuild%.h %bin2headerROMPath%\%MainBuild%.h
if NOT %ERRORLEVEL% == 0 exit /b 1

@echo .
@echo Completed: %date% %time%
@echo ************************************************************************************
@echo *** Verify "PROGMEM" before "static const unsigned char XXXXXXXXX_prg[] = {"
@echo ************************************************************************************

::cmd.exe /c start /b notepad++.exe %bin2headerROMPath%\%MainBuild%.h
::"C:\Program Files\Notepad++\notepad++.exe"

::pause
@exit /b 0
::only some features can be emulated without the associated TeensyROM hardware

echo ***Emulate...
start "" %emulatorPath%\%emulator% %emulatorArgs% %buildPath%\%MainBuild%
::
::pause
@exit /b