# SPDX-License-Identifier: MIT
param(
    [ValidateSet('mpe','stock-plus','stock')][string]$Mode = 'mpe',
    [string]$Output = (Join-Path ([IO.Path]::GetTempPath()) "tr-mpe\$Mode"),
    [string]$ArduinoCli = 'arduino-cli',
    [string]$ArduinoData = '',
    [string]$ArduinoUser = ''
)
$ErrorActionPreference = 'Stop'
if ($ArduinoCli -eq 'arduino-cli' -and !(Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
    $candidates = @(
        "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe",
        "$env:ProgramFiles\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
    )
    $found = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (!$found) { throw 'Arduino CLI was not found. Pass -ArduinoCli with its full path.' }
    $ArduinoCli = $found
}
$buildArguments = @((Join-Path $PSScriptRoot 'tools\build.mjs'), '--mode', $Mode, '--out', $Output, '--arduino-cli', $ArduinoCli)
if ($ArduinoData) { $buildArguments += @('--arduino-data', $ArduinoData) }
if ($ArduinoUser) { $buildArguments += @('--arduino-user', $ArduinoUser) }
& node @buildArguments
if ($LASTEXITCODE -ne 0) { throw 'Build failed. Do not use incomplete outputs.' }
