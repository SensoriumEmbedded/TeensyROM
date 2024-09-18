cls
:: Compiles TR CustomBasicCommands via KickAssembler
:: https://github.com/barryw/CustomBasicCommands
:: https://theweb.dk/KickAssembler/KickAssembler.pdf

@echo off

set JavaExe="C:\Program Files (x86)\Java\jre1.8.0_421\bin\java.exe"
set KickAssemblerJar="D:\MyData\Geek Stuff\Projects\Commodore 64\Software\PC Utils-SW\KickAssembler\KickAss.jar"
set bin2headerExe="D:\MyData\Geek Stuff\Projects\Commodore 64\Software\PC Utils-SW\bin2header\bin2header.exe"
set emulatorExe="D:\MyData\Geek Stuff\Projects\Commodore 64\Software\PC Utils-SW\Emulation\GTK3VICE-3.6.1-win64\bin\x64sc.exe"
set MainAsm="source\main.asm"
set MainPrg=TRCBC.prg

@echo on


@echo ***Start...
@echo ***Compile Main...
%JavaExe% -jar %KickAssemblerJar% -showmem %MainAsm% -o build\%MainPrg% -odir ..\build
if NOT %ERRORLEVEL% == 0 exit /b

@echo ***bin2header
%bin2headerExe% build\%MainPrg%
copy build\%MainPrg%.h ..\..\Teensy\ROMs\%MainPrg%.h

@echo .
@echo Completed: %date% %time%
@echo ************************************************************************************
@echo *** Remember to add "PROGMEM" before "static const unsigned char XXXXXXXXX_prg[] = {"
@echo ************************************************************************************

cmd.exe /c start /b notepad++.exe ..\..\Teensy\ROMs\%MainPrg%.h

pause
::@exit /b

echo ***Emulate...
start "" %emulatorExe% -autostart build\%MainPrg%

::pause
@exit /b


