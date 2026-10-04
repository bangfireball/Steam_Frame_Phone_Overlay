param(
    [ValidatePattern('^\d{6}$')]
    [string]$PairCode = "123456",
    [ValidateRange(1, 65535)]
    [int]$Port = 49321,
    [switch]$NoSteamVR,
    [ValidateSet("None", "Baseline", "Visible")]
    [string]$Diagnostic = "None",
    [switch]$ResumeAfterConsoleTransfer,
    [int]$ExpectedSessionId = -1
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$receiver = Join-Path $projectRoot "out\build\windows-x64\bin\phonecast-vr-stream-receiver.exe"
if ($Diagnostic -eq "Visible") {
    $receiver = Join-Path $projectRoot "out\build\windows-x64-diagnostic\bin\phonecast-vr-stream-receiver.exe"
}
$logDirectory = Join-Path $projectRoot "out\logs"
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null

function Get-SessionRow([int]$SessionId) {
    $rows = & "$env:SystemRoot\System32\query.exe" session 2>$null
    return $rows | Where-Object {
        $_ -match "\s$SessionId\s+(Active|Disc|Conn)\s"
    } | Select-Object -First 1
}

function Test-ConsoleSession([int]$SessionId) {
    $row = Get-SessionRow $SessionId
    return $null -ne $row -and $row -match "^\s*>?console\s"
}

function Assert-ConsoleSession {
    if (-not (Test-ConsoleSession $currentSessionId)) {
        throw "Session $currentSessionId left the physical console (possibly an RDP reconnect). Aborting startup."
    }
}

function Stop-PhoneCastReceiver {
    $processes = Get-Process -Name "phonecast-vr-stream-receiver" -ErrorAction SilentlyContinue
    if ($processes) {
        Write-Host "Stopping the existing PhoneCast receiver..."
        $processes | Stop-Process -Force
        $processes | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
    }
}

function Get-SteamExecutable {
    $runningSteam = Get-Process -Name "steam" -ErrorAction SilentlyContinue | Select-Object -First 1
    $candidates = @()
    if ($runningSteam -and $runningSteam.Path) {
        $candidates += $runningSteam.Path
    }
    $candidates += @(
        (Join-Path ${env:ProgramFiles(x86)} "Steam\steam.exe"),
        (Join-Path $env:ProgramFiles "Steam\steam.exe")
    )
    return $candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
}

function Stop-SteamVrRuntime {
    $runtimeNames = @("vrdashboard", "vrwebhelper", "vrmonitor", "vrcompositor", "vrserver")
    $vrMonitor = Get-Process -Name "vrmonitor" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($vrMonitor) {
        Write-Host "Stopping SteamVR before transferring the desktop to the console..."
        $vrMonitorPath = $vrMonitor.Path
        if ($vrMonitorPath -and (Test-Path $vrMonitorPath)) {
            & $vrMonitorPath -shutdown 2>$null | Out-Null
        }

        $deadline = (Get-Date).AddSeconds(15)
        while ((Get-Process -Name "vrserver" -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 500
        }
    }

    $remaining = Get-Process -Name $runtimeNames -ErrorAction SilentlyContinue
    if ($remaining) {
        Write-Host "Forcing remaining SteamVR processes to stop..."
        $remaining | Stop-Process -Force
        $remaining | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
    }
}

function Stop-SteamClient {
    $steam = Get-Process -Name "steam" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $steam) {
        return
    }

    $steamPath = Get-SteamExecutable
    Write-Host "Stopping Steam so it can be restarted after the console transfer..."
    if ($steamPath) {
        Start-Process -FilePath $steamPath -ArgumentList "-shutdown" -Wait -ErrorAction SilentlyContinue | Out-Null
    }

    $deadline = (Get-Date).AddSeconds(20)
    while ((Get-Process -Name "steam" -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
    }

    $remaining = Get-Process -Name @("gameoverlayui", "steamwebhelper", "steam") -ErrorAction SilentlyContinue
    if ($remaining) {
        Write-Host "Forcing remaining Steam client processes to stop..."
        $remaining | Stop-Process -Force
        $remaining | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
    }
}

if ($Diagnostic -ne "Baseline" -and -not (Test-Path $receiver)) {
    throw "Receiver not found at '$receiver'. Build the current review receiver before launching PhoneCast."
}

$currentSessionId = (Get-Process -Id $PID).SessionId

if ($Diagnostic -ne "None" -and -not $ResumeAfterConsoleTransfer) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $sessionLog = Join-Path $logDirectory "session-$Diagnostic-$stamp.jsonl"
    $watcher = Join-Path $PSScriptRoot "watch-vr-session.ps1"
    Start-Process -FilePath "$PSHOME\powershell.exe" -WindowStyle Hidden -ArgumentList @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ('"{0}"' -f $watcher),
        "-LogPath", ('"{0}"' -f $sessionLog)
    ) | Out-Null
    Write-Host "Diagnostic mode: $Diagnostic. Session monitoring for 10 minutes: $sessionLog"
    Write-Host "Save game progress first: RDP recovery restarts Steam and SteamVR."
}

if ($ResumeAfterConsoleTransfer) {
    if ($ExpectedSessionId -lt 0 -or $currentSessionId -ne $ExpectedSessionId) {
        throw "The console recovery helper started in unexpected session $currentSessionId (expected $ExpectedSessionId)."
    }

    Write-Host "Waiting for Windows session $ExpectedSessionId to attach to the physical console..."
    $deadline = (Get-Date).AddSeconds(90)
    while (-not (Test-ConsoleSession $ExpectedSessionId)) {
        if ((Get-Date) -ge $deadline) {
            throw "Windows session $ExpectedSessionId did not attach to the console within 90 seconds."
        }
        Start-Sleep -Milliseconds 500
    }
    Write-Host "Console transfer complete. Waiting for the physical display stack to settle..."
    Start-Sleep -Seconds 10
    Write-Host "Restarting Steam and the VR stack in session $ExpectedSessionId..."
} else {
    # Explorer can retain an old RDP SESSIONNAME value after the same Windows
    # session is transferred back to the console, so query the live session table.
    $currentSessionRow = Get-SessionRow $currentSessionId
    $isRemoteSession = if ($currentSessionRow) {
        $currentSessionRow -match "rdp-tcp"
    } else {
        $env:SESSIONNAME -like "RDP-*"
    }

    if ($isRemoteSession) {
        Write-Host "Remote Desktop session detected. PhoneCast will stop Steam and SteamVR, disconnect RDP,"
        Write-Host "reattach this Windows session to the physical console, and restart the VR stack there."
        Write-Host "Reconnect the Steam Frame after the RDP window closes."

        Stop-PhoneCastReceiver
        Stop-SteamVrRuntime
        Stop-SteamClient

        $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
        $recoveryStdout = Join-Path $logDirectory "console-recovery-$timestamp.stdout.log"
        $recoveryStderr = Join-Path $logDirectory "console-recovery-$timestamp.stderr.log"
        $scriptArguments = @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", ('"{0}"' -f $PSCommandPath),
            "-PairCode", $PairCode,
            "-Port", $Port,
            "-Diagnostic", $Diagnostic,
            "-ResumeAfterConsoleTransfer",
            "-ExpectedSessionId", $currentSessionId
        )
        if ($NoSteamVR) {
            $scriptArguments += "-NoSteamVR"
        }

        $helper = Start-Process -FilePath "powershell.exe" `
            -ArgumentList $scriptArguments `
            -WindowStyle Hidden `
            -RedirectStandardOutput $recoveryStdout `
            -RedirectStandardError $recoveryStderr `
            -PassThru

        Write-Host "Transferring session $currentSessionId to the console..."
        try {
            $transfer = Start-Process -FilePath "$env:SystemRoot\System32\tscon.exe" `
                -ArgumentList @($currentSessionId, "/dest:console") `
                -Verb RunAs `
                -Wait `
                -PassThru
            if ($transfer.ExitCode -ne 0) {
                throw "tscon exited with code $($transfer.ExitCode)."
            }
        } catch {
            Stop-Process -Id $helper.Id -Force -ErrorAction SilentlyContinue
            throw "Could not transfer the RDP session to the console. $($_.Exception.Message)"
        }

        exit 0
    }
}

if (-not (Test-ConsoleSession $currentSessionId)) {
    throw "PhoneCast requires Windows session $currentSessionId to be attached to the physical console."
}

if ($Diagnostic -ne "None") {
    Stop-PhoneCastReceiver
}

$running = Get-Process -Name "phonecast-vr-stream-receiver" -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($null -ne $running) {
    if ($running.SessionId -ne $currentSessionId) {
        throw "PhoneCast is already running in Windows session $($running.SessionId), not console session $currentSessionId."
    }
    $runningPath = if ($running.Path) { [System.IO.Path]::GetFullPath($running.Path) } else { "" }
    $desiredPath = [System.IO.Path]::GetFullPath($receiver)
    if (-not [string]::Equals($runningPath, $desiredPath,
                              [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Host "Replacing the running PhoneCast receiver with '$receiver'..."
        Stop-PhoneCastReceiver
        $running = $null
    } else {
        Write-Host "PhoneCast VR is already running from the current canonical build (PID $($running.Id))."
        exit 0
    }
}

$existingSteam = Get-Process -Name "steam" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $existingSteam) {
    $steamExecutable = Get-SteamExecutable
    if (-not $steamExecutable) {
        throw "Steam could not be found. Install or start Steam, then run this launcher again."
    }

    Assert-ConsoleSession
    Write-Host "Starting Steam in console session $currentSessionId..."
    Start-Process -FilePath $steamExecutable -ArgumentList "-silent" | Out-Null
    $deadline = (Get-Date).AddSeconds(45)
    while (-not ($existingSteam = Get-Process -Name "steam" -ErrorAction SilentlyContinue | Select-Object -First 1)) {
        if ((Get-Date) -ge $deadline) {
            throw "Steam did not start within 45 seconds."
        }
        Start-Sleep -Milliseconds 500
    }
    Start-Sleep -Seconds 5
}
if ($existingSteam.SessionId -ne $currentSessionId) {
    throw "Steam is running in Windows session $($existingSteam.SessionId), not console session $currentSessionId."
}

$existingVrServer = Get-Process -Name "vrserver" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($existingVrServer -and $existingVrServer.SessionId -ne $currentSessionId) {
    throw "SteamVR is running in Windows session $($existingVrServer.SessionId), not console session $currentSessionId. Stop it before continuing."
}

if (-not $NoSteamVR -and -not $existingVrServer) {
    $steamRoots = @()
    if ($existingSteam.Path) {
        $steamRoots += Split-Path -Parent $existingSteam.Path
    }
    $steamRoots += @(
        (Join-Path ${env:ProgramFiles(x86)} "Steam"),
        (Join-Path $env:ProgramFiles "Steam")
    )
    $vrStartupCandidates = @($steamRoots | ForEach-Object {
        if ($_) { Join-Path $_ "steamapps\common\SteamVR\bin\win64\vrstartup.exe" }
    } | Where-Object { Test-Path $_ } | Select-Object -Unique)
    if ($vrStartupCandidates.Count -eq 0) {
        throw "SteamVR is not running and vrstartup.exe could not be found. Install or start SteamVR, then run this launcher again."
    }

    $vrStartup = $vrStartupCandidates[0]
    Assert-ConsoleSession
    Write-Host "Starting SteamVR in console session $currentSessionId from '$vrStartup'..."
    Start-Process -FilePath $vrStartup | Out-Null
    $deadline = (Get-Date).AddSeconds(60)
    while (-not ((Get-Process -Name "vrserver" -ErrorAction SilentlyContinue) -and
                  (Get-Process -Name "vrcompositor" -ErrorAction SilentlyContinue))) {
        if ((Get-Date) -ge $deadline) {
            throw "SteamVR server and compositor did not become ready within 60 seconds."
        }
        Start-Sleep -Milliseconds 500
    }
    Start-Sleep -Seconds 2
}

$vrServer = Get-Process -Name "vrserver" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $NoSteamVR -and $vrServer.SessionId -ne $currentSessionId) {
    throw "SteamVR started in unexpected Windows session $($vrServer.SessionId)."
}

if ($Diagnostic -eq "Baseline") {
    Write-Host "Baseline ready: SteamVR only; PhoneCast is NOT running. Test streaming for at least 5 minutes."
    exit 0
}

$stdout = Join-Path $logDirectory "vr-receiver.stdout.log"
$stderr = Join-Path $logDirectory "vr-receiver.stderr.log"
$receiverArguments = @("--pair-code", $PairCode, "--port", $Port)
if ($Diagnostic -eq "Visible") {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $stdout = Join-Path $logDirectory "visible-$stamp.stdout.log"
    $stderr = Join-Path $logDirectory "visible-$stamp.stderr.log"
    $receiverArguments += "--diagnostic-visible"
}
$workingDirectory = Split-Path -Parent $receiver
Assert-ConsoleSession
$process = Start-Process -FilePath $receiver `
    -WorkingDirectory $workingDirectory `
    -ArgumentList $receiverArguments `
    -RedirectStandardOutput $stdout `
    -RedirectStandardError $stderr `
    -PassThru

Start-Sleep -Seconds 2
if ($process.HasExited) {
    $details = if (Test-Path $stderr) { Get-Content $stderr -Raw } else { "No error log was produced." }
    throw "PhoneCast exited during startup.`n$details"
}
if ($process.SessionId -ne $currentSessionId) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    throw "PhoneCast started in unexpected Windows session $($process.SessionId)."
}

Write-Host "PhoneCast VR started."
Write-Host "PID: $($process.Id)"
Write-Host "Windows console session: $currentSessionId"
Write-Host "Port: $Port"
Write-Host "Pairing code: $PairCode"
Write-Host "Logs: $logDirectory"
