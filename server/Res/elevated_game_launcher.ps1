param(
    [switch]$NoBanner
)

$ErrorActionPreference = "Continue"
$resDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $resDir
$queueDir = Join-Path $resDir "AdminLauncher"
$queueFile = Join-Path $queueDir "queue.txt"
$logFile = Join-Path $queueDir "launcher.log"
$script:ChannelClickAttempt = 0

New-Item -ItemType Directory -Force -Path $queueDir | Out-Null
if (!(Test-Path $queueFile)) {
    New-Item -ItemType File -Force -Path $queueFile | Out-Null
}

function Write-LauncherLog {
    param([string]$Message)
    $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $Message
    Add-Content -Path $logFile -Value $line
    Write-Host $line
}

function Test-IsElevated {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Stop-BlueTears {
    Get-Process -ErrorAction SilentlyContinue |
        Where-Object { $_.ProcessName -match '^(NClient|GameMon|GameGuard|GameGuard\.des|ncclient)$' } |
        ForEach-Object {
            Write-LauncherLog ("killing {0} pid={1}" -f $_.ProcessName, $_.Id)
            Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
        }
}

function Get-CondaPython {
    return (Join-Path $env:USERPROFILE "anaconda3\python.exe")
}

function Initialize-WindowTools {
    if ("CodexWindowTools" -as [type]) {
        return
    }
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class CodexWindowTools {
    [DllImport("user32.dll")]
    public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool BringWindowToTop(IntPtr hWnd);
}
"@ -ErrorAction SilentlyContinue
}

function Focus-ProcessWindow {
    param([System.Diagnostics.Process]$Process)
    try {
        Add-Type -AssemblyName Microsoft.VisualBasic -ErrorAction SilentlyContinue
        Initialize-WindowTools
        for ($i = 0; $i -lt 60; $i++) {
            Start-Sleep -Milliseconds 500
            $Process.Refresh()
            if ($Process.HasExited) { return }
            $hwnd = $Process.MainWindowHandle
            if ($hwnd -ne [IntPtr]::Zero) {
                [CodexWindowTools]::ShowWindowAsync($hwnd, 9) | Out-Null
                [CodexWindowTools]::BringWindowToTop($hwnd) | Out-Null
                [CodexWindowTools]::SetForegroundWindow($hwnd) | Out-Null
                [Microsoft.VisualBasic.Interaction]::AppActivate($Process.Id) | Out-Null
                Write-LauncherLog ("focused pid={0} hwnd=0x{1:x}" -f $Process.Id, $hwnd.ToInt64())
                return
            }
        }
        Write-LauncherLog ("focus timed out for pid={0}" -f $Process.Id)
    }
    catch {
        Write-LauncherLog ("focus failed for pid={0}: {1}" -f $Process.Id, $_.Exception.Message)
    }
}

function Start-OriginalStub {
    $script:ChannelClickAttempt = 0
    $binDir = Join-Path $root "TW\Bin"
    $exe = Join-Path $binDir "NClient.exe"
    if (!(Test-Path $exe)) {
        Write-LauncherLog "missing TW\Bin\NClient.exe"
        return
    }
    $args = "-Code dummy -OAuthProvider Authenticator -Server 127.0.0.1 -Port 5004"
    Write-LauncherLog "launching original client against 127.0.0.1:5004"
    $proc = Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $binDir -PassThru
    Focus-ProcessWindow -Process $proc
}

function Start-OriginalOfficial {
    $script:ChannelClickAttempt = 0
    $binDir = Join-Path $root "TW\Bin"
    $exe = Join-Path $binDir "NClient.exe"
    if (!(Test-Path $exe)) {
        Write-LauncherLog "missing TW\Bin\NClient.exe"
        return
    }
    $args = "-Code dummy -OAuthProvider Authenticator -Server gss.dlonline.com.tw -Port 5004"
    Write-LauncherLog "launching original client against original GSS host"
    $proc = Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $binDir -PassThru
    Focus-ProcessWindow -Process $proc
}

function Start-Magicmida {
    $script = Join-Path $resDir "admin_magicmida_dump_checkpoint.ps1"
    if (!(Test-Path $script)) {
        Write-LauncherLog "missing Res\admin_magicmida_dump_checkpoint.ps1"
        return
    }
    Write-LauncherLog "starting Magicmida checkpoint wrapper"
    powershell.exe -ExecutionPolicy Bypass -File $script
}

function Invoke-TraceProcess {
    param(
        [string]$Name,
        [string[]]$TraceArgs,
        [int]$TimeoutSeconds
    )

    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $stdout = Join-Path $queueDir ("{0}_{1}.stdout.log" -f $Name, $stamp)
    $stderr = Join-Path $queueDir ("{0}_{1}.stderr.log" -f $Name, $stamp)

    try {
        Write-LauncherLog ("starting {0} trace process: {1}" -f $Name, ($TraceArgs -join " "))
        Write-LauncherLog ("{0} stdout={1}" -f $Name, $stdout)
        Write-LauncherLog ("{0} stderr={1}" -f $Name, $stderr)
        $proc = Start-Process -FilePath $python `
            -ArgumentList $TraceArgs `
            -WorkingDirectory $root `
            -RedirectStandardOutput $stdout `
            -RedirectStandardError $stderr `
            -PassThru
        Write-LauncherLog ("{0} trace pid={1}" -f $Name, $proc.Id)
        if (-not $proc.WaitForExit($TimeoutSeconds * 1000)) {
            Write-LauncherLog ("{0} trace timeout after {1}s; killing pid={2}" -f $Name, $TimeoutSeconds, $proc.Id)
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            Stop-BlueTears
            return
        }
        Write-LauncherLog ("{0} trace finished exit={1}" -f $Name, $proc.ExitCode)
    }
    catch {
        Write-LauncherLog ("{0} trace launch failed: {1}" -f $Name, $_.Exception.Message)
        Stop-BlueTears
    }
}

function Start-ChannelClickCalibration {
    $script = Join-Path $resDir "calibrate_channel_click.py"
    $python = Get-CondaPython
    if (!(Test-Path $script)) {
        Write-LauncherLog "missing Res\calibrate_channel_click.py"
        return
    }
    if (!(Test-Path $python)) {
        Write-LauncherLog "missing conda python: $python"
        return
    }
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $stdout = Join-Path $queueDir ("channel_click_calibration_{0}.stdout.log" -f $stamp)
    $stderr = Join-Path $queueDir ("channel_click_calibration_{0}.stderr.log" -f $stamp)
    Write-LauncherLog "starting elevated channel click calibration"
    $proc = Start-Process -FilePath $python `
        -ArgumentList @($script) `
        -WorkingDirectory $root `
        -RedirectStandardOutput $stdout `
        -RedirectStandardError $stderr `
        -PassThru
    Write-LauncherLog ("channel click calibration pid={0} stdout={1}" -f $proc.Id, $stdout)
}

function Invoke-ChannelClick {
    $script = Join-Path $resDir "click_channel_once.py"
    $python = Get-CondaPython
    if (!(Test-Path $script)) {
        Write-LauncherLog "missing Res\click_channel_once.py"
        return
    }
    if (!(Test-Path $python)) {
        Write-LauncherLog "missing conda python: $python"
        return
    }
    $script:ChannelClickAttempt += 1
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
    $stdout = Join-Path $queueDir ("click_channel_{0}_{1}.stdout.log" -f $script:ChannelClickAttempt, $stamp)
    $stderr = Join-Path $queueDir ("click_channel_{0}_{1}.stderr.log" -f $script:ChannelClickAttempt, $stamp)
    Write-LauncherLog ("clicking channel attempt={0}" -f $script:ChannelClickAttempt)
    $proc = Start-Process -FilePath $python `
        -ArgumentList @($script, "--attempt", [string]$script:ChannelClickAttempt) `
        -WorkingDirectory $root `
        -RedirectStandardOutput $stdout `
        -RedirectStandardError $stderr `
        -PassThru
    if (-not $proc.WaitForExit(30000)) {
        Write-LauncherLog ("channel click timeout; killing pid={0}" -f $proc.Id)
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
}

function Start-OriginalLoginTrace {
    param(
        [string]$Profile = "handler-minimal"
    )
    $script = Join-Path $resDir "trace_original_login_frida.py"
    $python = Join-Path $env:USERPROFILE "anaconda3\python.exe"
    if (!(Test-Path $script)) {
        Write-LauncherLog "missing Res\trace_original_login_frida.py"
        return
    }
    if (!(Test-Path $python)) {
        Write-LauncherLog "missing conda python: $python"
        return
    }
    $findingsDir = Join-Path $resDir "RE_findings"
    New-Item -ItemType Directory -Force -Path $findingsDir | Out-Null
    $traceLog = Join-Path $findingsDir ("login_trace_admin_{0}.log" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
    Write-LauncherLog ("starting original login Frida trace profile={0}" -f $Profile)
    Write-LauncherLog ("original login Frida trace file={0}" -f $traceLog)
    Invoke-TraceProcess -Name "original_login" -TimeoutSeconds 150 -TraceArgs @(
        $script, "--mode", "spawn", "--seconds", "120", "--out", $traceLog, "--profile", $Profile
    )
    Write-LauncherLog "original login Frida trace command returned"
}

function Start-MagicmidaLoginTrace {
    Stop-BlueTears
    $script = Join-Path $resDir "trace_original_login_frida.py"
    $python = Join-Path $env:USERPROFILE "anaconda3\python.exe"
    if (!(Test-Path $script)) {
        Write-LauncherLog "missing Res\trace_original_login_frida.py"
        return
    }
    if (!(Test-Path $python)) {
        Write-LauncherLog "missing conda python: $python"
        return
    }
    $findingsDir = Join-Path $resDir "RE_findings"
    New-Item -ItemType Directory -Force -Path $findingsDir | Out-Null
    $traceLog = Join-Path $findingsDir ("magicmida_trace_admin_{0}.log" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
    Write-LauncherLog "starting Magicmida login/transport Frida trace"
    Write-LauncherLog ("Magicmida login/transport trace file={0}" -f $traceLog)
    Invoke-TraceProcess -Name "magicmida_login" -TimeoutSeconds 180 -TraceArgs @(
        $script, "--target", "magicmida", "--seconds", "120", "--out", $traceLog
    )
    Write-LauncherLog "Magicmida login/transport Frida trace command returned"
}

if (!$NoBanner) {
    Write-Host ""
    Write-Host "Blue Tears elevated launcher is running."
    Write-Host "Queue commands with: powershell -File Res\queue_admin_command.ps1 <command>"
    Write-Host "Allowed commands: launch_original_stub, launch_original_official, launch_magicmida, trace_original_login, trace_key6_gate, trace_key6_min, trace_login_bootstrap, trace_magicmida_login, start_channel_click_calibration, click_channel, kill_game, quit"
    Write-Host ""
}
Write-LauncherLog ("launcher started at {0} elevated={1}" -f $root, (Test-IsElevated))

while ($true) {
    Start-Sleep -Milliseconds 700
    if (!(Test-Path $queueFile)) {
        New-Item -ItemType File -Force -Path $queueFile | Out-Null
        continue
    }

    $lines = @()
    try {
        $lines = Get-Content -Path $queueFile -ErrorAction Stop
        if ($lines.Count -gt 0) {
            Clear-Content -Path $queueFile -ErrorAction Stop
        }
    }
    catch {
        Write-LauncherLog ("queue read failed: {0}" -f $_.Exception.Message)
        continue
    }

    foreach ($raw in $lines) {
        $cmd = ($raw -replace '#.*$', '').Trim().ToLowerInvariant()
        if ($cmd.Length -eq 0) { continue }

        switch ($cmd) {
            "launch_original_stub" { Start-OriginalStub }
            "launch_original_official" { Start-OriginalOfficial }
            "launch_magicmida" { Start-Magicmida }
            "trace_original_login" { Start-OriginalLoginTrace }
            "trace_key6_gate" { Start-OriginalLoginTrace -Profile "key6-gate" }
            "trace_key6_min" { Start-OriginalLoginTrace -Profile "key6-setter-min" }
            "trace_login_bootstrap" { Start-OriginalLoginTrace -Profile "login-bootstrap" }
            "trace_magicmida_login" { Start-MagicmidaLoginTrace }
            "start_channel_click_calibration" { Start-ChannelClickCalibration }
            "click_channel" { Invoke-ChannelClick }
            "kill_game" { Stop-BlueTears }
            "quit" {
                Write-LauncherLog "launcher exiting"
                exit 0
            }
            default { Write-LauncherLog ("unknown command ignored: {0}" -f $cmd) }
        }
    }
}
