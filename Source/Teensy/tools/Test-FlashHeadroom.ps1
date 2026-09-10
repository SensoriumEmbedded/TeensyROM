#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Checks a combined TeensyROM "_full.hex" image against the self-update flash
    headroom ceiling before it grows too large to update itself over serial/SD.

.DESCRIPTION
    FlashUpdate.ino / FlashTxx.c update the running firmware by copying the new
    image into a scratch buffer carved out of whatever flash is free above the
    currently-installed firmware, verifying it, then copying it over the live
    code (see FlashTxx.c firmware_buffer_init()). Because that scratch buffer
    always has to hold one full spare copy of the firmware next to the copy
    already running, the long-term-safe ceiling -- assuming future builds stay
    roughly the size of the one currently installed -- is half of the flash left
    after FLASH_RESERVE is set aside at the top:

        usable            = FLASH_SIZE - FLASH_RESERVE
        steady-state limit = usable / 2

    FLASH_SIZE and FLASH_RESERVE are read directly from Flash/FlashTxx.h and
    FlashUpdate.ino so this check tracks those constants automatically if they
    ever change. The hex file itself is parsed the same way FXUtil.cpp's
    process_hex_record() does (tracking type 02/04 base-address records) to get
    the true flashed-image span, not just the on-disk text size of the .hex file.

.PARAMETER HexPath
    Path to the combined "_full.hex" file to check (the output of HexCombine.exe).

.EXAMPLE
    .\Test-FlashHeadroom.ps1 -HexPath .\build\TeensyROM+_0.8.0.5_full.hex
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$HexPath
)

$ErrorActionPreference = "Stop"
$ScriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path

function Get-DefineValue {
    param([string]$Path, [string]$Name, [uint64]$Default)
    if (-not (Test-Path $Path)) { return $Default }
    foreach ($line in Get-Content $Path) {
        if ($line -match "^\s*#define\s+$Name\s+\(?(0x[0-9A-Fa-f]+|\d+)\)?") {
            $raw = $Matches[1]
            if ($raw -match '^0x') {
                return [Convert]::ToUInt64($raw.Substring(2), 16)
            }
            return [Convert]::ToUInt64($raw, 10)
        }
    }
    return $Default
}

function Get-HexImageExtent {
    param([string]$Path)

    $base    = [uint64]0
    $minAddr = [uint64]0xFFFFFFFFL
    $maxAddr = [uint64]0
    $lines   = 0

    :scan foreach ($line in [System.IO.File]::ReadLines($Path)) {
        $line = $line.Trim()
        if (-not $line.StartsWith(':')) { continue }

        $byteCount = [Convert]::ToInt32($line.Substring(1,2), 16)
        $addr      = [Convert]::ToInt32($line.Substring(3,4), 16)
        $recType   = [Convert]::ToInt32($line.Substring(7,2), 16)
        $data      = $line.Substring(9, $byteCount * 2)
        $lines++

        switch ($recType) {
            0 { # data record
                $a = $base + $addr
                if ($a -lt $minAddr) { $minAddr = $a }
                if (($a + $byteCount) -gt $maxAddr) { $maxAddr = $a + $byteCount }
            }
            1 { break scan }                                    # EOF record
            2 { $base = [Convert]::ToUInt64($data,16) -shl 4 }   # extended segment address
            4 { $base = [Convert]::ToUInt64($data,16) -shl 16 }  # extended linear address
        }
    }

    [PSCustomObject]@{
        Lines = $lines
        Min   = $minAddr
        Max   = $maxAddr
        Size  = $maxAddr - $minAddr
    }
}

if (-not (Test-Path $HexPath)) {
    Write-Host "ERROR: Hex file not found: $HexPath" -ForegroundColor Red
    exit 1
}

$FlashTxxH      = Join-Path $ScriptPath "..\Flash\FlashTxx.h"
$FlashUpdateIno = Join-Path $ScriptPath "..\FlashUpdate.ino"

$FlashSize    = Get-DefineValue -Path $FlashTxxH      -Name "FLASH_SIZE"    -Default 0x800000
$FlashReserve = Get-DefineValue -Path $FlashUpdateIno -Name "FLASH_RESERVE" -Default 0x40000

$Usable         = $FlashSize - $FlashReserve
$SteadyStateMax = [math]::Floor($Usable / 2)

$Extent    = Get-HexImageExtent -Path $HexPath
$SizeK     = [math]::Round($Extent.Size / 1KB, 1)
$LimitK    = [math]::Round($SteadyStateMax / 1KB, 1)
$HeadroomK = [math]::Round($LimitK - $SizeK, 1)

Write-Host ""
Write-Host "=== Flash self-update headroom check ===" -ForegroundColor Cyan
Write-Host "  File:  $HexPath" -ForegroundColor Gray
Write-Host ("  Image: {0:X8} - {1:X8}  ({2} K, {3} lines)" -f $Extent.Min, $Extent.Max, $SizeK, $Extent.Lines) -ForegroundColor Gray
Write-Host ("  Flash: {0} K total, {1} K reserved -> {2} K usable" -f ($FlashSize/1KB), ($FlashReserve/1KB), ($Usable/1KB)) -ForegroundColor Gray
Write-Host ("  Steady-state self-update ceiling: {0} K (usable / 2)" -f $LimitK) -ForegroundColor Gray

if ($Extent.Size -gt $SteadyStateMax) {
    Write-Host ("  FAIL: image is {0} K OVER the steady-state ceiling ({1} K vs {2} K limit)" -f (-$HeadroomK), $SizeK, $LimitK) -ForegroundColor Red
    Write-Host "        FlashUpdate.ino's scratch buffer may no longer fit next to the running firmware." -ForegroundColor Red
    exit 1
}
elseif ($HeadroomK -lt ($LimitK * 0.1)) {
    Write-Host ("  WARN: only {0} K of headroom left before the self-update ceiling" -f $HeadroomK) -ForegroundColor Yellow
    exit 0
}
else {
    Write-Host ("  OK: {0} K of headroom left before the self-update ceiling" -f $HeadroomK) -ForegroundColor Green
    exit 0
}
