cls
:: Compiles TR CustomBasicCommands via KickAssembler
:: https://github.com/barryw/CustomBasicCommands
:: https://theweb.dk/KickAssembler/KickAssembler.pdf

@echo off

set JavaExe="C:\Program Files (x86)\Java\jre1.8.0_421\bin\java.exe"
set KickAssemblerJar="D:\MyData\Geek Stuff\Projects\Commodore 64\Software\PC Utils-SW\KickAssembler\KickAss.jar"
set emulatorExe="D:\MyData\Geek Stuff\Projects\Commodore 64\Software\PC Utils-SW\Emulation\GTK3VICE-3.6.1-win64\bin\x64sc.exe"

::set bin2headerExe="D:\MyData\Geek Stuff\Projects\Commodore 64\Software\PC Utils-SW\bin2header\bin2header.exe"
:: Python script updated to allow type modifier (-t "PROGMEM ") addition
set bin2headerPy="d:/MyData/Geek Stuff/Projects/Commodore 64/Software/PC Utils-SW/bin2header/bin2header/src/bin2header.py"
set PythonExe="C:/Users/trav/AppData/Local/Microsoft/WindowsApps/python3.11.exe"

set TRROMPath=..\..\Teensy\ROMs
set MainAsm="source\main.asm"
set MainPrg=TRCBC.prg

@echo on


@echo ***Start...
@echo ***Compile Code...
%JavaExe% -jar %KickAssemblerJar% -showmem %MainAsm% -o build\%MainPrg% -odir ..\build
if NOT %ERRORLEVEL% == 0 exit /b

@echo ***bin2header
::%bin2headerExe% build\%MainPrg%
:: Add "PROGMEM " type modifier to force to flash memory
%PythonExe% %bin2headerPy% -t "PROGMEM " build\%MainPrg%

copy build\%MainPrg%.h %TRROMPath%\%MainPrg%.h

@echo .
@echo Completed: %date% %time%
@echo ************************************************************************************
@echo *** Verify "PROGMEM" before "static const unsigned char XXXXXXXXX_prg[] = {"
@echo ************************************************************************************

::cmd.exe /c start /b notepad++.exe %TRROMPath%\%MainPrg%.h

::pause
@exit /b

echo ***Emulate...
start "" %emulatorExe% -autostart build\%MainPrg%

::pause
@exit /b


