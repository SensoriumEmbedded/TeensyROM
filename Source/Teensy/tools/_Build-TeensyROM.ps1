
& "$PSScriptRoot\Build-DualBoot.ps1"

# Pause for any key so the console stays open
Write-Host ""
Write-Host "TeensyROM (Fab 0.2/0.3) .hex file generated" -ForegroundColor Yellow
Write-Host "   Press any key to continue . . ." -ForegroundColor Yellow
[void][System.Console]::ReadKey($true)

