param(
    [ValidatePattern('^\d{6}$')]
    [string]$PairCode = "123456",
    [ValidateRange(1, 65535)]
    [int]$Port = 49321,
    [switch]$NoSteamVR
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$receiver = Join-Path $projectRoot "out\build\windows-x64\bin\phonecast-vr-stream-receiver.exe"
$logDirectory = Join-Path $projectRoot "out\logs"

# Explorer can retain an old RDP SESSIONNAME environment value after the same
# Windows session is transferred back to the physical console. Query the live
# session table by ID instead of trusting that inherited environment variable.
$currentSessionId = (Get-Process -Id $PID).SessionId
$sessionRows = & "$env:SystemRoot\System32\query.exe" session 2>$null
$currentSessionRow = $sessionRows | Where-Object {
    $_ -match "\s$currentSessionId\s+(Active|Disc|Conn)\s"
} | Select-Object -First 1
$isRemoteSession = if ($currentSessionRow) {
    $currentSessionRow -match "rdp-tcp"
} else {
    $env:SESSIONNAME -like "RDP-*"
}
if ($isRemoteSession) {
    throw "PhoneCast must be launched from the physical Windows console, not Remote Desktop. RDP can break VRLink D3D11 textures."
}

$running = Get-Process -Name "phonecast-vr-stream-receiver" -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($null -ne $running) {
    Write-Host "PhoneCast VR is already running (PID $($running.Id))."
    exit 0
}

if (-not (Test-Path $receiver)) {
    throw "Receiver not found at '$receiver'. Build the windows-x64 preset first."
}

if (-not $NoSteamVR -and -not (Get-Process -Name "vrserver" -ErrorAction SilentlyContinue)) {
    $steamCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Steam\steam.exe"),
        (Join-Path $env:ProgramFiles "Steam\steam.exe")
    ) | Where-Object { $_ -and (Test-Path $_) }
    if ($steamCandidates.Count -eq 0) {
        throw "SteamVR is not running and Steam could not be found. Start SteamVR, then run this launcher again."
    }

    Write-Host "Starting SteamVR..."
    Start-Process -FilePath $steamCandidates[0] -ArgumentList "steam://rungameid/250820" | Out-Null
    $deadline = (Get-Date).AddSeconds(60)
    while (-not (Get-Process -Name "vrserver" -ErrorAction SilentlyContinue)) {
        if ((Get-Date) -ge $deadline) {
            throw "SteamVR did not become ready within 60 seconds."
        }
        Start-Sleep -Milliseconds 500
    }
    Start-Sleep -Seconds 2
}

New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$stdout = Join-Path $logDirectory "vr-receiver.stdout.log"
$stderr = Join-Path $logDirectory "vr-receiver.stderr.log"
$workingDirectory = Split-Path -Parent $receiver
$process = Start-Process -FilePath $receiver `
    -WorkingDirectory $workingDirectory `
    -ArgumentList @("--pair-code", $PairCode, "--port", $Port) `
    -RedirectStandardOutput $stdout `
    -RedirectStandardError $stderr `
    -PassThru

Start-Sleep -Seconds 2
if ($process.HasExited) {
    $details = if (Test-Path $stderr) { Get-Content $stderr -Raw } else { "No error log was produced." }
    throw "PhoneCast exited during startup.`n$details"
}

Write-Host "PhoneCast VR started."
Write-Host "PID: $($process.Id)"
Write-Host "Port: $Port"
Write-Host "Pairing code: $PairCode"
Write-Host "Logs: $logDirectory"
