cls
:: $2400 main menu code
:: $8000 compile for Lower ROM copy code (w/ main menu code and SID BINs embedded for copy)
:: No crunching
:: Compiled/Saved as BIN w/ no header ("plain" format) for ROM image
:: bin2header to convert/copy .bin file to C header file for ROM emulation
::        copy it yo main proj ROM dir
:: cartconv to convert/copy .bin file to .crt file for PC emulation

@echo off

set CartFilename=TeensyROMC64
set MainFilename=MainMenu
set toolPath="D:\MyData\Geek Stuff\Projects\Commodore 64\Software\PC Utils-SW"

setlocal EnableDelayedExpansion
SET clean=del
SET cleanArgs=/F /Q 
SET buildPath=build
SET sourcePath=source
SET compilerPath=%toolPath%\C64-devkit\compiler\win32
SET compiler=acme.exe

SET CartBuild=%CartFilename%.bin
SET CartCompilerArgs=-r %buildPath%\CartBuildReport --vicelabels %buildPath%\CartSymbols --msvc --color --format plain -v3 --outfile
rem --format plain leaves off the 2 byte address from the start of the file.  "cbm" includes it

SET MainBuild=%MainFilename%.bin
SET MainCompilerArgs=-r %buildPath%\MainBuildReport --vicelabels %buildPath%\MainSymbols --msvc --color --format plain -v3 --outfile

:: SET cruncherPath=%toolPath%\C64-devkit\cruncher\win32
:: SET cruncher=pucrunch.exe
:: SET cruncherArgs=-x$2400 -c64 -g55 -fshort
:: rem SET cruncherArgs=-x$0801 -c64 -g55 -fshort

SET bin2headerPath=%toolPath%\bin2header
SET bin2header=bin2header.exe
rem SET bin2headerArgs=%toolPath%
SET bin2headerROMPath=..\TeensyROM\ROMs

SET cartconvPath=%toolPath%\Emulation\GTK3VICE-3.6.1-win64\bin
SET cartconv=cartconv.exe
SET cartconvFilename=%CartFilename%.crt
SET cartconvArgs=-v -t normal -i %buildPath%\%CartBuild% -o %buildPath%\%cartconvFilename%

SET emulatorPath=%toolPath%\Emulation\GTK3VICE-3.6.1-win64\bin
SET emulator=x64sc.exe
SET emulatorArgs=-autostart

::***************************************************************************************************************

@echo on
echo ***Start...
%clean% %cleanArgs% %buildPath%\*.*

echo ***Compile Main...
%compilerPath%\%compiler% %MainCompilerArgs% %buildPath%\%MainBuild% %sourcePath%\%MainFilename%.asm
if NOT %ERRORLEVEL% == 0 exit /b

echo ***Compile Cart...
%compilerPath%\%compiler% %CartCompilerArgs% %buildPath%\%CartBuild% %sourcePath%\%CartFilename%.asm
if NOT %ERRORLEVEL% == 0 exit /b

echo ***bin2header
%bin2headerPath%\%bin2header% %buildPath%\%CartBuild%
copy %buildPath%\%CartBuild%.h %bin2headerROMPath%\%CartFilename%.h

exit /b

echo ***CartConvert...
%cartconvPath%\%cartconv% %cartconvArgs%
echo ***CartConvert Read Info...
%cartconvPath%\%cartconv% -f %buildPath%\%cartconvFilename%

echo ***Emulate...
start "" %emulatorPath%\%emulator% %emulatorArgs% %buildPath%\%cartconvFilename%

::pause
