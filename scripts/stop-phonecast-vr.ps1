$ErrorActionPreference = "Stop"
$processes = Get-Process -Name "phonecast-vr-stream-receiver" -ErrorAction SilentlyContinue
if (-not $processes) {
    Write-Host "PhoneCast VR is not running."
    exit 0
}

$processes | Stop-Process -Force
Write-Host "PhoneCast VR stopped."
