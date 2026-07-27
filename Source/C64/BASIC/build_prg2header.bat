cls
:: bin2header to convert/copy  to C header file

@echo off

:: SET sourceFullPath="C:\Users\trav\OneDrive\Desktop\TeensyROM\TeensyROM\Source\C64\BASIC\CLOCK0726A.PRG"
SET sourceFullPath=CLOCK0726A_TR.PRG

call ../SetToolPaths.bat

setlocal EnableDelayedExpansion

@echo on
echo ***Start...

echo ***bin2header
:: Add "PROGMEM " type modifier to force to flash memory
%PythonExe% %bin2headerPy% -t "PROGMEM " %sourceFullPath%
if NOT %ERRORLEVEL% == 0 exit /b 1

copy %sourceFullPath%.h %bin2headerROMPath%\%sourceFullPath%.h
if NOT %ERRORLEVEL% == 0 exit /b 1

@echo .
@echo Completed: %date% %time%
@echo *** Verify "PROGMEM" before "static const unsigned char XXXXXXXXX_prg[] = {"

rem exit /b 0
pause