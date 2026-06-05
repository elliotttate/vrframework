<#
  Launch-FH5VR.ps1 — launch FH5 Empress with FH5VR.dll deployed against the SimXR runtime, then drive
  to free-roam by STAGE DETECTION (not blind timing):
    1. launch, wait for FH5VR.log,
    2. press Enter (GAME window only) every 5s until the producer fires = gameplay camera is live,
    3. press Escape ONCE (GAME window) to dismiss the showcase prompt -> free-roam.

  Uses FH5Window.ps1 so keystrokes go to the "Forza Horizon 5" window, NOT the in-process SimXR "O"
  preview window (which steals Process.MainWindowHandle). Sets XR_RUNTIME_JSON for the child process.
#>
param(
    [int]$TimeoutSec = 240,
    [string]$SimJson = "E:\Github\OpenXR-Simulator\bin\openxr_simulator.json"
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\FH5Window.ps1"

$Exe = "E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe"
$Log = "E:\Games\ForzaHorizon5Empress\FH5VR.log"

# 0) clean slate
Get-Process ForzaHorizon5 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 4
if (Test-Path $Log) { Remove-Item $Log -Force }

# 1) runtime env inherited by the child
$env:XR_RUNTIME_JSON = $SimJson
Write-Output "XR_RUNTIME_JSON=$env:XR_RUNTIME_JSON"

# 2) launch
Start-Process $Exe
Write-Output "LAUNCHED $Exe  ($(Get-Date -Format HH:mm:ss))"

# 3) wait for FH5VR.log (proxy DLL loaded + Framework ctor ran)
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline -and -not (Test-Path $Log)) { Start-Sleep -Milliseconds 500 }
if (-not (Test-Path $Log)) { Write-Output "RESULT=NO_LOG (FH5VR.dll did not load)"; exit 1 }
Write-Output "FH5VR.log up at $((Get-Item $Log).CreationTime.ToString('HH:mm:ss'))"

function Producer-Live { (Get-Content $Log -ErrorAction SilentlyContinue | Select-String -Pattern 'producer fired|producer calls|main=') | Select-Object -First 1 }

# 4) Enter (game window) every 5s until the gameplay camera (producer) is live
$pressed = 0
while ((Get-Date) -lt $deadline) {
    if (-not (Get-Process ForzaHorizon5 -ErrorAction SilentlyContinue)) { Write-Output "RESULT=CRASH during nav"; exit 2 }
    if (Producer-Live) { break }
    $ok = Send-FH5GameKey -Vk 0x0D   # Enter -> START GAME / continue prompts (game window only)
    $pressed++
    Write-Output "  enter #$pressed (game-focused=$ok)"
    Start-Sleep -Seconds 5
}
if (-not (Producer-Live)) { Write-Output "RESULT=TIMEOUT producer never fired (gameplay camera not reached)"; exit 1 }
Write-Output "GAMEPLAY CAMERA LIVE (producer firing)"

# 5) ONE Escape (game window) to dismiss 'DRIVE TO THE SHOWCASE EVENT' -> free-roam
Start-Sleep -Seconds 2
$ok = Send-FH5GameKey -Vk 0x1B
Write-Output "  ESC (game-focused=$ok) -> free-roam"
Write-Output "RESULT=READY"
