<#
  Launch-FH5VR-Retail.ps1 — one-command startup for the RETAIL Steam build (1.688).

  Differs from Launch-FH5VR.ps1 (which is hardcoded for the Empress de-DRM build):
   * targets the retail Steam install + the version.dll proxy (retail bundles NVIDIA Streamline which
     interposes dxgi, so we proxy version.dll, NOT dxgi),
   * NO de-DRM boot-crash retry (retail has no exception storm),
   * the retail menu flow has a Microsoft/Xbox sign-in "Continue" menu Empress lacks, and the producer-based
     scene detection in Navigate-FH5.ps1 doesn't work on retail (producer deferred) — so this drives the menu
     with a fixed, reliable sequence via FH5Window.ps1's class-"App" window targeting (NOT the SimXR "O"
     preview / fullscreen shade).
   * head-look on retail runs from the no-.text-hook DATA worker path (Tick1688DataHeadlook); the mod installs
     ZERO inline hooks here so the integrity CRC has nothing to detect.

  Usage:
    pwsh scripts/Launch-FH5VR-Retail.ps1                 # deploy build, launch, navigate to free-roam (SimXR)
    pwsh scripts/Launch-FH5VR-Retail.ps1 -SkipDeploy     # don't re-copy the DLL
    pwsh scripts/Launch-FH5VR-Retail.ps1 -SkipNavigate   # launch only, leave at the title
#>
param(
    [string]$GameDir   = "E:\SteamLibrary\steamapps\common\ForzaHorizon5",
    [string]$BuildDll  = "E:\Github\vrframework\build-fh5\fh5vr\Release\FH5VR.dll",
    [string]$SimJson   = "E:\Github\OpenXR-Simulator\bin\openxr_simulator.json",
    [switch]$RealRuntime,
    [switch]$SkipDeploy,
    [switch]$SkipNavigate,
    [int]$TitleWaitSec        = 50,   # boot -> title screen
    [int]$AfterStartGameSec   = 6,    # title START GAME -> Continue menu
    [int]$AfterContinueSec    = 22,   # Continue -> save load -> Garage
    [int]$AfterDriveSec       = 20    # Space (Drive) -> free-roam
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\FH5Window.ps1"

$exe = Join-Path $GameDir "ForzaHorizon5.exe"
$log = Join-Path $GameDir "FH5VR.log"
if (-not (Test-Path $exe)) { throw "retail FH5 exe not found: $exe" }

# 0) clean slate + deploy the freshest build as the version.dll proxy
Get-Process ForzaHorizon5 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 3
if (-not $SkipDeploy) {
    if (-not (Test-Path $BuildDll)) { throw "built FH5VR.dll not found: $BuildDll (build first)" }
    Copy-Item $BuildDll (Join-Path $GameDir "version.dll") -Force
    Write-Output "DEPLOY version.dll <- $BuildDll"
}

# 1) runtime env inherited by the child
if (-not $RealRuntime) {
    if (-not (Test-Path $SimJson)) { throw "SimXR json not found: $SimJson" }
    $env:XR_RUNTIME_JSON = $SimJson
    Write-Output "XR_RUNTIME_JSON=$env:XR_RUNTIME_JSON (SimXR)"
} else {
    $rt = (Get-ItemProperty 'HKLM:\SOFTWARE\Khronos\OpenXR\1' -ErrorAction SilentlyContinue).ActiveRuntime
    if ($rt) { $env:XR_RUNTIME_JSON = $rt; Write-Output "XR_RUNTIME_JSON=$rt (real)" }
}

# 2) launch (Steam DRM stub attaches to the running Steam client; no -KeepOverlays/retry needed on retail)
if (Test-Path $log) { Remove-Item $log -Force -ErrorAction SilentlyContinue }
$g = Start-Process -FilePath $exe -WorkingDirectory $GameDir -PassThru
Write-Output ("LAUNCHED pid={0} ({1})" -f $g.Id, (Get-Date -Format HH:mm:ss))

# wait for the mod to load (FH5VR.log) then for the title to render
$dl = (Get-Date).AddSeconds(60)
while ((Get-Date) -lt $dl -and -not (Test-Path $log)) { Start-Sleep -Milliseconds 500 }
if (-not (Test-Path $log)) { Write-Output "RESULT=NO_LOG (proxy didn't load)"; exit 1 }
Write-Output "FH5VR.log up; waiting ${TitleWaitSec}s for the title screen"
Start-Sleep -Seconds $TitleWaitSec
if (-not (Get-Process -Id $g.Id -ErrorAction SilentlyContinue)) { Write-Output "RESULT=EXITED_DURING_BOOT"; exit 1 }

if ($SkipNavigate) { Write-Output "RESULT=READY (title; -SkipNavigate)"; exit 0 }

# 3) navigate the retail menu flow via the class-"App" game window (FH5Window.ps1)
function Key([byte]$vk, [string]$label) {
    $r = Send-FH5GameKey $vk
    Write-Output ("  KEY {0} -> {1}" -f $label, $r)
}
Key 0x0D "Enter (START GAME -> Continue menu)"; Start-Sleep -Seconds $AfterStartGameSec
Key 0x28 "Down"; Start-Sleep -Milliseconds 400
Key 0x26 "Up";   Start-Sleep -Milliseconds 400
Key 0x0D "Enter (Continue -> load -> Garage)";  Start-Sleep -Seconds $AfterContinueSec
Key 0x20 "Space (Drive -> free-roam)";          Start-Sleep -Seconds $AfterDriveSec

$alive = [bool](Get-Process -Id $g.Id -ErrorAction SilentlyContinue)
$datacam = if (Test-Path $log) { (Select-String -Path $log -SimpleMatch "FH5DATACAM] active camera" | Select-Object -Last 1) } else { $null }
Write-Output ("RESULT={0} alive={1} activeCam={2}" -f ($(if($alive){"NAVIGATED"}else{"EXITED"})), $alive, [bool]$datacam)
