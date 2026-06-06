<#
  Launch-FH5VR.ps1 — launch FH5 Empress with FH5VR.dll deployed against the real OpenXR runtime, then hand
  off to Navigate-FH5.ps1 for closed-loop menu navigation.

  Uses FH5Window.ps1 so keystrokes go to the "Forza Horizon 5" window, NOT the in-process SimXR "O"
  preview window (which steals Process.MainWindowHandle when simulator mode is explicitly requested).
#>
param(
    [int]$TimeoutSec = 240,
    [string]$SimJson = "E:\Github\OpenXR-Simulator\bin\openxr_simulator.json",
    [string]$RealJson = "",
    [switch]$SimRuntime,
    [switch]$RealRuntime,
    [int]$NavigateTimeoutSec = 300,
    [int]$MenuDelaySec = 30,
    [int]$MenuContinueDelaySec = 12,
    [ValidateSet("Enter","Escape")]
    [string]$MenuContinueKey = "Enter",
    [ValidateSet("Enter","Escape")]
    [string]$OverlayCleanupKey = "Escape",
    [int]$SettleSec = 3,
    [int]$DriverSettleSec = 12,
    [Alias("PostMenuWaitSec")]
    [int]$PostMenuSettleSec = 18,
    [switch]$SkipNavigate,
    [switch]$SkipMenuEscape,
    [switch]$UseXInput,
    [string]$ControlPath = "E:\tmp\fh5vr_ctl.txt",
    [Alias("Scale")]
    [float]$WorldScale = 1.0,
    [Alias("Ipd")]
    [float]$HalfIpdUnits = 0.032,
    [ValidateSet("angle","driver","a4","off")]
    [string]$RotationPath = "a4",   # working interim: clean yaw/pitch/roll head-look (shadows follow). "angle" = shadow-coherent cam+0x90 Euler injection (CAMERA_VR_FIX_GUIDE). "driver" = +0x320 matrix path.
    [ValidateSet("camsrc","proda15","input540","viewtail","ccam320","ccam320_d550","clone0","clone1","clone2","downstream","off")]
    [string]$PosLane = "proda15",
    [switch]$DisableProjection
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\FH5Window.ps1"

if ($SimRuntime -and $RealRuntime) {
    throw "Use only one of -SimRuntime or -RealRuntime."
}

$Exe = "E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe"
$Log = "E:\Games\ForzaHorizon5Empress\FH5VR.log"
$State = "E:\tmp\fh5_state.txt"
$Nav = "E:\tmp\fh5_nav.txt"

# 0) clean slate
Get-Process ForzaHorizon5 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 4
if (Test-Path $Log) { Remove-Item $Log -Force }
if (Test-Path $State) { Remove-Item $State -Force }
if (Test-Path $Nav) { Remove-Item $Nav -Force }

# Keep debug control-file leftovers from silently disabling OpenXR 6DOF. `mode=off`
# leaves the downstream cbuffer fallback off. `-PosLane input540` switches the
# active test to the Empress CCamDriver +0x540 additive input lane after launch.
$controlDir = Split-Path -Parent $ControlPath
if ($controlDir) { New-Item -ItemType Directory -Path $controlDir -Force | Out-Null }
$recenter = [int][double]::Parse((Get-Date -UFormat %s), [Globalization.CultureInfo]::InvariantCulture)
$projection = if ($DisableProjection) { "off" } else { "on" }
$control = "ipd=$HalfIpdUnits`nscale=$WorldScale`nmode=off`nrot=$RotationPath`nproj=$projection`nposlane=$PosLane`ntgt=off`nfwd=0`nstrafe=0`nup=0`nrecenter=$recenter"
[System.IO.File]::WriteAllText($ControlPath, $control, [System.Text.Encoding]::ASCII)
Write-Output "CONTROL $ControlPath ipd=$HalfIpdUnits scale=$WorldScale mode=off rot=$RotationPath proj=$projection poslane=$PosLane tgt=off"

# 1) runtime env inherited by the child
if ($SimRuntime) {
    if (-not (Test-Path $SimJson)) { throw "SimXR runtime JSON not found: $SimJson" }
    $env:XR_RUNTIME_JSON = $SimJson
    Write-Output "XR_RUNTIME_JSON=$env:XR_RUNTIME_JSON (SimXR)"
} else {
    $runtimeJson = $RealJson
    if ([string]::IsNullOrWhiteSpace($runtimeJson)) {
        $runtimeJson = (Get-ItemProperty -Path 'HKLM:\SOFTWARE\Khronos\OpenXR\1' -ErrorAction SilentlyContinue).ActiveRuntime
    }
    if ([string]::IsNullOrWhiteSpace($runtimeJson) -or -not (Test-Path $runtimeJson)) {
        Remove-Item Env:\XR_RUNTIME_JSON -ErrorAction SilentlyContinue
        Write-Output "XR_RUNTIME_JSON cleared for child (no valid HKLM runtime found)"
    } else {
        $env:XR_RUNTIME_JSON = $runtimeJson
        Write-Output "XR_RUNTIME_JSON=$env:XR_RUNTIME_JSON (real runtime)"
    }
}

# 2) launch
Start-Process $Exe
Write-Output "LAUNCHED $Exe  ($(Get-Date -Format HH:mm:ss))"

# 3) wait for FH5VR.log (proxy DLL loaded + Framework ctor ran)
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline -and -not (Test-Path $Log)) { Start-Sleep -Milliseconds 500 }
if (-not (Test-Path $Log)) { Write-Output "RESULT=NO_LOG (FH5VR.dll did not load)"; exit 1 }
Write-Output "FH5VR.log up at $((Get-Item $Log).CreationTime.ToString('HH:mm:ss'))"

if ($SkipNavigate) {
    Write-Output "NAV skipped by -SkipNavigate"
    Write-Output "RESULT=READY (launch only)"
    exit 0
}

$navArgs = @(
    "-NoProfile", "-ExecutionPolicy", "Bypass",
    "-File", "$PSScriptRoot\Navigate-FH5.ps1",
    "-TimeoutSec", "$NavigateTimeoutSec",
    "-SettleSec", "$SettleSec",
    "-SecondEnterDelaySec", "$MenuDelaySec",
    "-MenuContinueDelaySec", "$MenuContinueDelaySec",
    "-MenuContinueKey", "$MenuContinueKey",
    "-OverlayCleanupKey", "$OverlayCleanupKey",
    "-DriverSettleSec", "$DriverSettleSec"
)
if ($UseXInput) { $navArgs += "-UseXInput" }
Write-Output "NAV: Navigate-FH5.ps1 timeout=${NavigateTimeoutSec}s secondEnter=${MenuDelaySec}s continueDelay=${MenuContinueDelaySec}s continueKey=${MenuContinueKey} overlayKey=${OverlayCleanupKey} driverSettle=${DriverSettleSec}s"
& powershell @navArgs
$navExit = $LASTEXITCODE
if ($navExit -eq 0) {
    Write-Output "RESULT=READY"
} else {
    Write-Output "RESULT=NAV_FAILED exit=$navExit"
}
exit $navExit
