param(
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet("launch_original_stub", "launch_original_official", "launch_magicmida", "trace_original_login", "trace_key6_gate", "trace_key6_min", "trace_login_bootstrap", "trace_magicmida_login", "start_channel_click_calibration", "click_channel", "kill_game", "quit")]
    [string]$Command
)

$ErrorActionPreference = "Stop"
$resDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$queueDir = Join-Path $resDir "AdminLauncher"
$queueFile = Join-Path $queueDir "queue.txt"

New-Item -ItemType Directory -Force -Path $queueDir | Out-Null
Add-Content -Path $queueFile -Value $Command
Write-Host "queued admin command: $Command"
