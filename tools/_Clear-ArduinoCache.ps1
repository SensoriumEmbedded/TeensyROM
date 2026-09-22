#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Clears Arduino/TeensyROM build cache and output directories.

.DESCRIPTION
    Mirrors the cleanup Build-DualBoot.ps1 does before each build, so it can be
    run standalone to clear these 4 locations and simplify file searches:
      - %LOCALAPPDATA%\Temp\arduino (cores and sketches subfolders)
      - %LOCALAPPDATA%\arduino (cores and sketches subfolders)
      - Source\Teensy\build (TeensyROM build output)
      - Source\Teensy\MinimalBoot\build (MinimalBoot build output)

.EXAMPLE
    .\Clear-ArduinoCache.ps1
#>

$ErrorActionPreference = "Stop"
$ScriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
$LocalAppData = $env:LOCALAPPDATA

$Locations = @(
    @{ Name = "Arduino temp cache"; Paths = @("$LocalAppData\Temp\arduino\cores", "$LocalAppData\Temp\arduino\sketches") },
    @{ Name = "Arduino local cache"; Paths = @("$LocalAppData\arduino\cores", "$LocalAppData\arduino\sketches") },
    @{ Name = "TeensyROM build output"; Paths = @((Join-Path $ScriptPath "..\build")) },
    @{ Name = "MinimalBoot build output"; Paths = @((Join-Path $ScriptPath "..\MinimalBoot\build")) }
)

Write-Host "=== Clearing Arduino/TeensyROM Build Cache ===" -ForegroundColor Cyan

foreach ($Location in $Locations) {
    Write-Host "$($Location.Name):" -ForegroundColor Cyan
    foreach ($Path in $Location.Paths) {
        if (Test-Path $Path) {
            Remove-Item -Recurse -Force $Path -ErrorAction SilentlyContinue
            Write-Host "  Cleared: $Path" -ForegroundColor Green
        } else {
            Write-Host "  Not found (skipped): $Path" -ForegroundColor Gray
        }
    }
}

Write-Host "Done." -ForegroundColor Cyan
