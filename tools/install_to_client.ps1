param(
    [Parameter(Mandatory=$true)]
    [string]$ClientRoot
)

$ErrorActionPreference = "Stop"

$mirror = Resolve-Path (Join-Path $PSScriptRoot "..")
$client = Resolve-Path $ClientRoot

$resSrc = Join-Path $mirror "server\Res"
$resDst = Join-Path $client "Res"
$binDst = Join-Path $client "TW\Bin"

if (!(Test-Path $resDst)) {
    throw "Client Res directory not found: $resDst"
}
if (!(Test-Path $binDst)) {
    throw "Client TW\Bin directory not found: $binDst"
}

New-Item -ItemType Directory -Force -Path (Join-Path $resDst "RE_findings") | Out-Null

$serverFiles = @(
    "gss_stub_server_v3.py",
    "gss_stub_server_v4.py",
    "live_character_db.py",
    "nid_hash.py",
    "run_master_ladder.py",
    "elevated_game_launcher.ps1",
    "queue_admin_command.ps1",
    "click_channel_once.py",
    "calibrate_channel_click.py",
    "channel_click_calibration.json",
    "channel_click_sequence.json"
)

foreach ($name in $serverFiles) {
    Copy-Item -Force -LiteralPath (Join-Path $resSrc $name) -Destination (Join-Path $resDst $name)
}

Copy-Item -Force -LiteralPath (Join-Path $resSrc "RE_findings\gss_key_window_20260525.json") -Destination (Join-Path $resDst "RE_findings\gss_key_window_20260525.json")
Copy-Item -Force -LiteralPath (Join-Path $resSrc "RE_findings\xor_keystream_full_CANONICAL.hex") -Destination (Join-Path $resDst "RE_findings\xor_keystream_full_CANONICAL.hex")

$currentDll = Join-Path $binDst "NpSlayer.dll"
if (Test-Path $currentDll) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    Copy-Item -Force -LiteralPath $currentDll -Destination (Join-Path $binDst "NpSlayer_before_profile78_$stamp.dll")
}

Copy-Item -Force -LiteralPath (Join-Path $mirror "client_crack\bin\NpSlayer.dll") -Destination $currentDll

Write-Host "Installed milestone server/crack files into $client"

