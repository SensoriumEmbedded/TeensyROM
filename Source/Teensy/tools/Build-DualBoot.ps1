#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Automated Dual-Boot Hex File Builder for TeensyROM
#>

param(
    [switch]$SkipTeensyBuild = $false,
    [switch]$SkipMinimalBuild = $false,
    [switch]$SkipCombine = $false
)

$ErrorActionPreference = "Stop"
$ScriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path

# Auto-detect Arduino paths
$LocalAppData = $env:LOCALAPPDATA
$ArduinoBasePath = "$LocalAppData\Arduino15"
if (-not (Test-Path $ArduinoBasePath)) {
    $ArduinoBasePath = "$LocalAppData\.arduino15"
}

if (-not (Test-Path $ArduinoBasePath)) {
    Write-Host "ERROR: Cannot find Arduino installation at $LocalAppData" -ForegroundColor Red
    exit 1
}

Write-Host "=== TeensyROM Dual-Boot Build ===" -ForegroundColor Cyan
Write-Host "Arduino: $ArduinoBasePath" -ForegroundColor Gray

# Find Teensy hardware version
$TeensyPath = Get-ChildItem "$ArduinoBasePath\packages\teensy\hardware\avr" -Directory -ErrorAction SilentlyContinue |
               Sort-Object Name -Descending |
               Select-Object -First 1

if (-not $TeensyPath) {
    Write-Host "ERROR: Cannot find Teensy hardware package" -ForegroundColor Red
    exit 1
}

$TeensyCorePath = "$($TeensyPath.FullName)\cores\teensy4"

# Build paths
$TeensyInoPath = Join-Path $ScriptPath "..\Teensy.ino"
$MinimalBootInoPath = Join-Path $ScriptPath "..\MinimalBoot\MinimalBoot.ino"
$BootLinkerPath = Join-Path $ScriptPath "BootLinkerFiles"
$BuildPath = Join-Path $ScriptPath "build"
$HexCombinePath = Join-Path $ScriptPath "HexCombineUtil\HexCombine.exe"

$TeensyHexOutput = Join-Path $ScriptPath "..\build\Teensy.ino.hex"
$MinimalHexOutput = Join-Path $ScriptPath "..\MinimalBoot\build\MinimalBoot.ino.hex"
$FinalOutput = Join-Path $BuildPath "TeensyROM_full.hex"

# Helper Functions
function Copy-LinkerFiles {
    param([string]$Suffix)
    Copy-Item "$BootLinkerPath\bootdata.c.$Suffix" "$TeensyCorePath\bootdata.c" -Force
    Copy-Item "$BootLinkerPath\imxrt1062_t41.ld.$Suffix" "$TeensyCorePath\imxrt1062_t41.ld" -Force
}

function Remove-ArduinoCache {
    # Arduino cache can be in two locations depending on system configuration
    $TempPath = "$LocalAppData\Temp\arduino"
    if (Test-Path $TempPath\cores) { Remove-Item -Recurse -Force $TempPath\cores -ErrorAction SilentlyContinue }
    if (Test-Path $TempPath\sketches) { Remove-Item -Recurse -Force $TempPath\sketches -ErrorAction SilentlyContinue }
    
    $AltPath = "$LocalAppData\arduino"
    if (Test-Path $AltPath\cores) { Remove-Item -Recurse -Force $AltPath\cores -ErrorAction SilentlyContinue }
    if (Test-Path $AltPath\sketches) { Remove-Item -Recurse -Force $AltPath\sketches -ErrorAction SilentlyContinue }
}

function Invoke-ArduinoBuild {
    param([string]$InoPath, [string]$Fqbn)

    Write-Host "  Building: $InoPath" -ForegroundColor Gray

    # Find or install arduino-cli
    $ArduinoCli = Get-Command arduino-cli -ErrorAction SilentlyContinue

    if (-not $ArduinoCli) {
        # Check if already downloaded in tools directory
        $LocalCli = Join-Path $ScriptPath "arduino-cli.exe"
        if (Test-Path $LocalCli) {
            $ArduinoCli = Get-Command $LocalCli
        } else {
            Write-Host "  Downloading arduino-cli..." -ForegroundColor Cyan
            $ZipPath = Join-Path $ScriptPath "arduino-cli.zip"
            Invoke-WebRequest -Uri "https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip" -OutFile $ZipPath
            Expand-Archive -Path $ZipPath -DestinationPath $ScriptPath -Force
            Remove-Item $ZipPath
            $ArduinoCli = Get-Command $LocalCli
            Write-Host "  Downloaded to $LocalCli" -ForegroundColor Green
        }
    }

    Write-Host "  Using arduino-cli with FQBN: $Fqbn" -ForegroundColor Cyan

    # Determine output directory from the .ino path
    $InoDir = Split-Path $InoPath
    & $ArduinoCli.Path compile --fqbn $Fqbn --build-path $InoDir\build $InoPath

    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ERROR: Build failed" -ForegroundColor Red
        exit 1
    }
    Write-Host "  Build complete" -ForegroundColor Green
}

function Test-HexFile {
    param([string]$Path, [string]$Name)
    if (-not (Test-Path $Path)) {
        Write-Host "  ERROR: Hex file not found: $Path" -ForegroundColor Red
        exit 1
    }
    $Size = (Get-Item $Path).Length
    Write-Host "  Hex: $Path ($([math]::Round($Size/1KB, 2)) KB)" -ForegroundColor Gray
}

function Invoke-HexCombine {
    if (-not (Test-Path $HexCombinePath)) {
        Write-Host "ERROR: HexCombine.exe not found" -ForegroundColor Red
        exit 1
    }
    if (-not (Test-Path $BuildPath)) { New-Item -ItemType Directory -Path $BuildPath -Force | Out-Null }

    & $HexCombinePath $MinimalHexOutput $TeensyHexOutput $FinalOutput

    if (-not (Test-Path $FinalOutput)) {
        Write-Host "ERROR: Combined hex not created" -ForegroundColor Red
        exit 1
    }

    $FinalSize = (Get-Item $FinalOutput).Length
    Write-Host "Combined: $FinalOutput ($([math]::Round($FinalSize/1KB, 2)) KB)" -ForegroundColor Green
}

# Build Process
Write-Host "Starting build..." -ForegroundColor Cyan

if (-not $SkipTeensyBuild) {
    Write-Host "`n[Step 1/3] Building TeensyROM (Upper)" -ForegroundColor Magenta
    Copy-LinkerFiles "upper"
    Remove-ArduinoCache

    # Clean previous build
    $TeensyBuildDir = Join-Path $ScriptPath "..\build"
    if (Test-Path $TeensyBuildDir) {
        Remove-Item -Recurse -Force $TeensyBuildDir -ErrorAction SilentlyContinue
    }

    Invoke-ArduinoBuild -InoPath $TeensyInoPath -Fqbn "teensy:avr:teensy41:usb=serialmidi,speed=600,opt=o2std,keys=en-us"
    Test-HexFile -Path $TeensyHexOutput -Name "TeensyROM"
}

if (-not $SkipMinimalBuild) {
    Write-Host "`n[Step 2/3] Building MinimalBoot (Lower)" -ForegroundColor Magenta
    Copy-LinkerFiles "orig"
    Remove-ArduinoCache

    # Clean previous build
    $MinimalBuildDir = Split-Path $MinimalHexOutput
    if (Test-Path $MinimalBuildDir) {
        Remove-Item -Recurse -Force $MinimalBuildDir -ErrorAction SilentlyContinue
    }

    Invoke-ArduinoBuild -InoPath $MinimalBootInoPath -Fqbn "teensy:avr:teensy41:usb=serial,speed=600,opt=o2std,keys=en-us"
    Test-HexFile -Path $MinimalHexOutput -Name "MinimalBoot"
}

if (-not $SkipCombine) {
    Write-Host "`n[Step 3/3] Combining..." -ForegroundColor Magenta
    Invoke-HexCombine
}

Write-Host "`n=== BUILD COMPLETE ===" -ForegroundColor Green
Write-Host "Output: $FinalOutput" -ForegroundColor White
Write-Host "Flash this to your Teensy 4.1" -ForegroundColor Yellow
