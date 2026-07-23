#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Automated Dual-Boot Hex File Builder for TeensyROM

.PARAMETER Fab04_Features
    When specified, adds -DFab04_Features to the compiler flags for both builds.
    Use in source with: #ifdef Fab04_Features ... #endif

    IMPLEMENTATION NOTE: Teensy's platform.txt does not expose {compiler.cpp.extra_flags}
    in its compile recipes, so the standard arduino-cli --build-property approach is
    silently ignored. The workaround (per the PJRC forum) is to:
      1. Query the existing build.flags.defs value via --show-properties
      2. Append the extra -D flag to that value
      3. Pass the combined string back via --build-property "build.flags.defs=..."
    Teensy's platform.txt does use {build.flags.defs} in its recipes, so this works.

.EXAMPLE
    .\Build-DualBoot.ps1                   # standard build
    .\Build-DualBoot.ps1 -Fab04_Features   # build with Fab04_Features defined
#>

param(
    [switch]$SkipTeensyBuild = $false,
    [switch]$SkipMinimalBuild = $false,
    [switch]$SkipCombine = $false,
    [switch]$Fab04_Features
)

$ErrorActionPreference = "Stop"
$ScriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path

function Test-Fab04FeaturesDefined {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return $false }
    foreach ($line in Get-Content $Path) {
        # Match an active (non-commented) #define, ignoring leading whitespace
        if ($line -match '^\s*#define\s+Fab04_Features\b') {
            return $true
        }
    }
    return $false
}

function Disable-Fab04FeaturesDefine {
    param([string]$Path)
    $content = Get-Content $Path
    $updated = $content | ForEach-Object {
        if ($_ -match '^(\s*)#define(\s+Fab04_Features\b.*)$') {
            "$($Matches[1])// #define$($Matches[2])"
        } else {
            $_
        }
    }
    Set-Content -Path $Path -Value $updated
}

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

Write-Host "=== TeensyROM/TR+ Dual-Boot Build ===" -ForegroundColor Cyan
Write-Host "Arduino: $ArduinoBasePath" -ForegroundColor Gray

if ($Fab04_Features) {
    Write-Host "Fab04_Features: ENABLED  (for TeensyROM+ Fab0.4)" -ForegroundColor Yellow
} else {
    Write-Host "Fab04_Features: DISABLED  (for TeensyROM Fab0.2/0.3)" -ForegroundColor Yellow
}

# Guard: Fab04FeatureCtl.h has a commented-out "#define Fab04_Features" that overrides
# the build scripts if uncommented. If it's active but the -Fab04_Features switch wasn't
# passed, the build would silently produce a TR+ image while claiming to be plain TR.
$Fab04FeatureCtlPath = Join-Path $ScriptPath "..\MinimalBoot\Common\Fab04FeatureCtl.h"
if (-not $Fab04_Features -and (Test-Fab04FeaturesDefined -Path $Fab04FeatureCtlPath)) {
    Write-Host "WARNING: Fab04_Features is #define'd in $Fab04FeatureCtlPath" -ForegroundColor Red
    Write-Host "  but the -Fab04_Features switch was not passed to this script." -ForegroundColor Red
    Write-Host "  This mismatch would build a TR+ image while labeled as plain TR." -ForegroundColor Red
    $Response = Read-Host "  Comment out the #define and continue with a plain TR build? (y/N)"
    if ($Response -match '^[Yy]') {
        Disable-Fab04FeaturesDefine -Path $Fab04FeatureCtlPath
        Write-Host "  Fab04_Features #define commented out in $Fab04FeatureCtlPath" -ForegroundColor Green
    } else {
        Write-Host "  ERROR: Aborting build. Comment out the #define, or re-run with -Fab04_Features." -ForegroundColor Red
        exit 1
    }
}

# Find Teensy hardware version
$TeensyPath = Get-ChildItem "$ArduinoBasePath\packages\teensy\hardware\avr" -Directory -ErrorAction SilentlyContinue |
               Sort-Object Name -Descending |
               Select-Object -First 1

if (-not $TeensyPath) {
    Write-Host "ERROR: Cannot find Teensy hardware package" -ForegroundColor Red
    exit 1
}

$TeensyCorePath = "$($TeensyPath.FullName)\cores\teensy4"
Write-Host "Teensy Core: $TeensyCorePath" -ForegroundColor Gray

if (-not (Test-Path $TeensyCorePath)) {
    Write-Host "ERROR: Teensy core path does not exist: $TeensyCorePath" -ForegroundColor Red
    exit 1
}

# Build paths
$TeensyInoPath = Join-Path $ScriptPath "..\Teensy.ino"
$MinimalBootInoPath = Join-Path $ScriptPath "..\MinimalBoot\MinimalBoot.ino"
$BootLinkerPath = Join-Path $ScriptPath "BootLinkerFiles"
$BuildPath = Join-Path $ScriptPath "build"
$HexCombinePath = Join-Path $ScriptPath "HexCombineUtil\HexCombine.exe"

$TeensyHexOutput = Join-Path $ScriptPath "..\build\Teensy.ino.hex"
$MinimalHexOutput = Join-Path $ScriptPath "..\MinimalBoot\build\MinimalBoot.ino.hex"
if ($Fab04_Features) {
    $FinalOutput = Join-Path $BuildPath "TeensyROM+_full.hex"
} else {
    $FinalOutput = Join-Path $BuildPath "TeensyROM_full.hex"
}

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
    param(
        [string]$InoPath,
        [string]$Fqbn,
        [switch]$WithFab04Features
    )

    Write-Host "  Building: $InoPath" -ForegroundColor Gray

    # Find or install arduino-cli
    $ArduinoCli = Get-Command arduino-cli -ErrorAction SilentlyContinue

    if (-not $ArduinoCli) {
        # Check if already downloaded in tools directory
        $LocalCli = Join-Path $ScriptPath "arduino-cli.exe"
        if (Test-Path $LocalCli) {
            $ArduinoCli = Get-Command $LocalCli
        } else {
            # Version pinned for supply-chain security - update intentionally through review
            $ArduinoCliVersion = "1.4.1"
            $ExpectedSHA256 = "44f506a29d134cb294898d5f729aea85e5498f5d81ff5fc63c549087c45a20a3"
            
            Write-Host "  Downloading arduino-cli v$ArduinoCliVersion..." -ForegroundColor Cyan
            $ZipPath = Join-Path $ScriptPath "arduino-cli.zip"
            $DownloadUrl = "https://github.com/arduino/arduino-cli/releases/download/v$ArduinoCliVersion/arduino-cli_${ArduinoCliVersion}_Windows_64bit.zip"
            
            Invoke-WebRequest -Uri $DownloadUrl -OutFile $ZipPath
            
            # Verify SHA256 checksum for security
            Write-Host "  Verifying checksum..." -ForegroundColor Cyan
            $ActualHash = (Get-FileHash -Path $ZipPath -Algorithm SHA256).Hash
            if ($ActualHash -ne $ExpectedSHA256) {
                Remove-Item $ZipPath -ErrorAction SilentlyContinue
                Write-Host "  ERROR: SHA256 checksum mismatch!" -ForegroundColor Red
                Write-Host "  Expected: $ExpectedSHA256" -ForegroundColor Red
                Write-Host "  Actual:   $ActualHash" -ForegroundColor Red
                exit 1
            }
            Write-Host "  Checksum verified" -ForegroundColor Green
            
            Expand-Archive -Path $ZipPath -DestinationPath $ScriptPath -Force
            Remove-Item $ZipPath
            $ArduinoCli = Get-Command $LocalCli
            Write-Host "  Downloaded to $LocalCli" -ForegroundColor Green
        }
    }

    Write-Host "  Using arduino-cli with FQBN: $Fqbn" -ForegroundColor Cyan

    $InoDir = Split-Path $InoPath

    if ($WithFab04Features) {
        # Teensy's platform.txt does not include {compiler.cpp.extra_flags} in its
        # compile recipes, so --build-property compiler.cpp.extra_flags=... is ignored.
        # The workaround is to piggyback on build.flags.defs, which Teensy's platform.txt
        # *does* use in its recipes:
        #   1. Retrieve the current build.flags.defs value via --show-properties
        #   2. Append -DFab04_Features to it
        #   3. Pass the combined value back as --build-property "build.flags.defs=..."
        Write-Host "  Querying build.flags.defs for Fab04_Features injection..." -ForegroundColor Gray

        $existingFlagsDefs = & $ArduinoCli.Path compile --fqbn $Fqbn --build-path $InoDir\build `
            --show-properties $InoPath |
            Where-Object { $_ -match '^build\.flags\.defs=' } |
            Select-Object -First 1

        if (-not $existingFlagsDefs) {
            Write-Host "  ERROR: Could not retrieve build.flags.defs from --show-properties" -ForegroundColor Red
            exit 1
        }

        $allFlags = "$existingFlagsDefs -DFab04_Features"
        Write-Host "  Injecting: $allFlags" -ForegroundColor Cyan

        & $ArduinoCli.Path compile --fqbn $Fqbn --build-path $InoDir\build `
            --build-property $allFlags `
            $InoPath
    } else {
        & $ArduinoCli.Path compile --fqbn $Fqbn --build-path $InoDir\build $InoPath
    }

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
$BuildStartTime = Get-Date
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

    Invoke-ArduinoBuild -InoPath $TeensyInoPath -Fqbn "teensy:avr:teensy41:usb=serialmidi,speed=600,opt=o2std,keys=en-us" -WithFab04Features:$Fab04_Features
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

    Invoke-ArduinoBuild -InoPath $MinimalBootInoPath -Fqbn "teensy:avr:teensy41:usb=serial,speed=600,opt=o2std,keys=en-us" -WithFab04Features:$Fab04_Features
    Test-HexFile -Path $MinimalHexOutput -Name "MinimalBoot"
}

if (-not $SkipCombine) {
    Write-Host "`n[Step 3/3] Combining..." -ForegroundColor Magenta
    Invoke-HexCombine
}

Write-Host "`n=== BUILD COMPLETE ===" -ForegroundColor Green
Write-Host "Output: $FinalOutput" -ForegroundColor White

$BuildEndTime = Get-Date
$BuildDuration = $BuildEndTime - $BuildStartTime
Write-Host ("Build time: {0:hh\:mm\:ss}" -f $BuildDuration) -ForegroundColor White
Write-Host "Finished at: $($BuildEndTime.ToString('yyyy-MM-dd HH:mm:ss'))" -ForegroundColor White

