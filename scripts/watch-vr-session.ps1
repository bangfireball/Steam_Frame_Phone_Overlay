param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [ValidateRange(1, 3600)][int]$DurationSeconds = 600
)
# Read-only observation. Never transfers sessions, restarts processes, or blocks RDP.
$ErrorActionPreference = "Stop"
$deadline = (Get-Date).AddSeconds($DurationSeconds)
while ((Get-Date) -lt $deadline) {
    $sessions = @(& "$env:SystemRoot\System32\query.exe" session 2>&1 | ForEach-Object { "$_" })
    $processes = @(Get-Process -Name steam,vrserver,vrcompositor,phonecast-vr-stream-receiver -ErrorAction SilentlyContinue |
        Select-Object Name,Id,SessionId)
    [ordered]@{
        time = (Get-Date).ToString("o")
        sessions = $sessions
        processes = $processes
    } | ConvertTo-Json -Depth 4 -Compress | Add-Content -LiteralPath $LogPath -Encoding UTF8
    Start-Sleep -Seconds 1
}
