<#
.SYNOPSIS
  Build air-alarm Feather firmware and deploy to HA web server for OTA.
.NOTES
  Run from anywhere -- script uses its own location to resolve paths.
#>

# Resolve paths relative to this script's directory (config\)
$ScriptDir  = $PSScriptRoot
$ConfigFile = Join-Path $ScriptDir "air-alarm.yaml"
$BuildRoot  = Join-Path $ScriptDir ".esphome\build\air-alarm"
$DestDir    = "\\192.168.4.2\config\www\esphome"
$DestBin    = "$DestDir\air-alarm.bin"
$DestMd5    = "$DestDir\air-alarm.md5"
$DestVer    = "$DestDir\air-alarm-version.txt"

# Version from git describe
# git describe --tags --long --dirty produces: v1.2.3-4-gabcdef[-dirty]
# Replace hyphens with underscores: v1.2.3_4_gabcdef
Push-Location $ScriptDir
$RawVersion = git describe --tags --long --dirty 2>$null
$GitExit = $LASTEXITCODE
Pop-Location

if ($GitExit -ne 0 -or -not $RawVersion) {
    Write-Warning "git describe failed -- version will be 'unknown'"
    $Version = "unknown"
} else {
    $Version = $RawVersion.Trim() -replace '-', '_'
}
Write-Host "Version: $Version" -ForegroundColor Cyan

# Compile
Write-Host "Compiling $ConfigFile ..." -ForegroundColor Cyan
Push-Location $ScriptDir
esphome -s firmware_version $Version compile air-alarm.yaml
$CompileExit = $LASTEXITCODE
Pop-Location

if ($CompileExit -ne 0) {
    Write-Error "Compile failed."
    exit 1
}

# Locate binary
$Binary = Get-ChildItem -Path $BuildRoot -Recurse -Filter "firmware.bin" |
          Where-Object { $_.FullName -notmatch '\\.pio\\' } |
          Sort-Object LastWriteTime -Descending |
          Select-Object -First 1

if (-not $Binary) {
    Write-Error "firmware.bin not found under $BuildRoot"
    exit 1
}
Write-Host "Binary: $($Binary.FullName)" -ForegroundColor Gray

# Compute MD5
$Md5Hash = (Get-FileHash $Binary.FullName -Algorithm MD5).Hash.ToLower()
Write-Host "MD5: $Md5Hash" -ForegroundColor Gray

# Deploy
if (-not (Test-Path $DestDir)) {
    New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
}
Copy-Item $Binary.FullName $DestBin -Force
$Md5Hash | Out-File -FilePath $DestMd5 -NoNewline -Encoding ascii
$Version  | Out-File -FilePath $DestVer -NoNewline -Encoding ascii

Write-Host ""
Write-Host "Deployed:" -ForegroundColor Green
Write-Host "  $DestBin  ($Version)"
Write-Host "  $DestMd5"
Write-Host "  $DestVer"
Write-Host ""
Write-Host "HA URL: http://192.168.4.2:8123/local/esphome/air-alarm.bin"
Write-Host "Press 'Update Firmware' in HA to flash each device."
