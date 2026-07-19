:: cls
:: Compiles TR CustomBasicCommands via KickAssembler
:: https://github.com/barryw/CustomBasicCommands
:: https://theweb.dk/KickAssembler/KickAssembler.pdf

@echo off

call ../SetToolPaths.bat

SET buildPath=build

:: set JavaExe="C:\Program Files (x86)\Java\jre1.8.0_421\bin\java.exe"
:: set KickAssemblerJar="D:\MyData\Geek Stuff\Projects\Commodore 64\Software\PC Utils-SW\KickAssembler\KickAss.jar"
:: set emulatorExe="D:\MyData\Geek Stuff\Projects\Commodore 64\Software\PC Utils-SW\Emulation\GTK3VICE-3.6.1-win64\bin\x64sc.exe"
:: SET emulatorPath=%toolPath%\Emulation\GTK3VICE-3.6.1-win64\bin
SET emulator=x64sc.exe
SET emulatorArgs=-autostart

::set bin2headerExe="D:\MyData\Geek Stuff\Projects\Commodore 64\Software\PC Utils-SW\bin2header\bin2header.exe"
:: Python script updated to allow type modifier (-t "PROGMEM ") addition
:: set bin2headerPy="d:/MyData/Geek Stuff/Projects/Commodore 64/Software/PC Utils-SW/bin2header/bin2header/src/bin2header.py"
:: set PythonExe="C:/Users/trav/AppData/Local/Microsoft/WindowsApps/python3.11.exe"

::SET bin2headerROMPath=..\..\Teensy\TRMenuFiles\ROMs
::        set TRROMPath=..\..\Teensy\TRMenuFiles\ROMs
set MainAsm="source\main.asm"
set MainPrg=TRCBC.prg

@echo on


@echo ***Start...
md %buildPath%
@echo ***Compile Code...
%JavaExe% -jar %KickAssemblerJar% -showmem %MainAsm% -o %buildPath%\%MainPrg% -odir ..\%buildPath%
if NOT %ERRORLEVEL% == 0 exit /b 1

@echo ***bin2header
::%bin2headerExe% build\%MainPrg%
:: Add "PROGMEM " type modifier to force to flash memory
%PythonExe% %bin2headerPy% -t "PROGMEM " %buildPath%\%MainPrg%
if NOT %ERRORLEVEL% == 0 exit /b 1

copy %buildPath%\%MainPrg%.h %bin2headerROMPath%\%MainPrg%.h
if NOT %ERRORLEVEL% == 0 exit /b 1

@echo .
@echo Completed: %date% %time%
@echo ************************************************************************************
@echo *** Verify "PROGMEM" before "static const unsigned char XXXXXXXXX_prg[] = {"
@echo ************************************************************************************

::cmd.exe /c start /b notepad++.exe %TRROMPath%\%MainPrg%.h

::pause
@exit /b 0

echo ***Emulate...
start "" %emulatorPath%\%emulator% %emulatorArgs% %buildPath%\%MainBuild%
::start "" %emulatorExe% -autostart build\%MainPrg%

::pause
@exit /b


