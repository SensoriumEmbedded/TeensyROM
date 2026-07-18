
cls

::Core programs/dependancies:
CD MainMenuCRT
call build8000CartBin.bat
if NOT %ERRORLEVEL% == 0 exit /b 1

CD ../SettingsMenu
call buildSettingsMenu.bat
if NOT %ERRORLEVEL% == 0 exit /b 1

CD ../TRHelpScreens
call buildTRHelpScreens.bat
if NOT %ERRORLEVEL% == 0 exit /b 1

CD ../TRExtPortCheck
call buildTRExtPortCheck.bat
if NOT %ERRORLEVEL% == 0 exit /b 1

CD ../ExpansionPortTest
call buildExpansionPortTest.bat
if NOT %ERRORLEVEL% == 0 exit /b 1

CD ../ASIDPlayer
call buildASIDPlayer.bat
if NOT %ERRORLEVEL% == 0 exit /b 1

CD ../MIDI2SID
call buildMIDI2SID.bat
if NOT %ERRORLEVEL% == 0 exit /b 1


::Other programs:
CD ../SimpSwiftTerm
call buildSimpSwiftTerm.bat
if NOT %ERRORLEVEL% == 0 exit /b 1

CD ../TODCheck
call buildTODCheck.bat
if NOT %ERRORLEVEL% == 0 exit /b 1

CD ../TRCustomBasicCommands
call buildTRCustomBasicCommands.bat
if NOT %ERRORLEVEL% == 0 exit /b 1


::Adjust for individual prgs in dir:
:: "BASIC\bin2header.bat"

:: Not used:
:: "v1541Wrapper\buildv1541Wrapper.bat"

CD ..

@echo .
@echo .
@echo .
@echo "*** All programs built/copied! ***
@pause

@exit /b 0

