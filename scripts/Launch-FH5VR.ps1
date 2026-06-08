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
    [switch]$DisableProjection,
    [switch]$KeepOverlays,
    [switch]$HudQuad,
    [switch]$HudOpaque,
    [switch]$HudTransparent,
    [ValidateSet("on","off")]
    [string]$HudPremul = "on",   # quad source is premultiplied alpha (FH5 UI draws onto cleared-transparent RT)
    [ValidateSet("on","off")]
    [string]$HudFlipV = "on",    # flip quad V (SimXR preview's quad-layer V is inverted vs the projection layer)
    [int]$UiRedirect = 18,       # 18 = pre-UI delta (clean eyes + HUD-delta quad, transition-safe). 0 = off.
    [ValidateSet("left","right")]
    [string]$HudPhase = "left",
    [float]$HudW = 1.5,
    [float]$HudX = 0.0,
    [float]$HudY = 0.0,
    [float]$HudZ = -1.5
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\FH5Window.ps1"

if ($SimRuntime -and $RealRuntime) {
    throw "Use only one of -SimRuntime or -RealRuntime."
}
if ($HudOpaque -and $HudTransparent) {
    throw "Use only one of -HudOpaque or -HudTransparent."
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
if ($HudQuad -or $UiRedirect -ne 0) {
    # Quad on by default whenever a redirect is active (uiredirect default = 18); transparent by default so the
    # HUD composites over the clean eyes. -HudOpaque forces an opaque validation panel.
    $hudQuadValue = "on"
    $hudOpaqueValue = if ($HudOpaque) { "on" } else { "off" }
    $control += "`nhudquad=$hudQuadValue`nhudopaque=$hudOpaqueValue`nhudpremul=$HudPremul`nhudflipv=$HudFlipV`nuiredirect=$UiRedirect`nhudphase=$HudPhase`nhudw=$HudW`nhudx=$HudX`nhudy=$HudY`nhudz=$HudZ"
}
[System.IO.File]::WriteAllText($ControlPath, $control, [System.Text.Encoding]::ASCII)
Write-Output "CONTROL $ControlPath ipd=$HalfIpdUnits scale=$WorldScale mode=off rot=$RotationPath proj=$projection poslane=$PosLane tgt=off hudquad=$($HudQuad.IsPresent) hudopaque=$(-not $HudTransparent.IsPresent) uiredirect=$UiRedirect hudphase=$HudPhase"

function Set-ControlValue([string]$Key, [string]$Value) {
    if (-not (Test-Path $ControlPath)) { return }
    $lines = [System.Collections.Generic.List[string]]::new()
    $seen = $false
    foreach ($line in [IO.File]::ReadAllLines($ControlPath)) {
        if ($line -match "^$([Regex]::Escape($Key))=") {
            $lines.Add("$Key=$Value")
            $seen = $true
        } else {
            $lines.Add($line)
        }
    }
    if (-not $seen) {
        $lines.Add("$Key=$Value")
    }
    [IO.File]::WriteAllLines($ControlPath, $lines, [Text.Encoding]::ASCII)
}

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

# Overlay-VEH suppression (called once per boot attempt). Overlays (Xbox Game Bar, Discord, NVIDIA ShadowPlay)
# install Vectored Exception Handlers that recurse on the first-chance AVs FH5's de-DRM generates -> crash.
# Game Bar + Discord kills STICK (no respawning service). NVIDIA's nvspcap64 is respawned by the
# NvContainerLocalSystem service (needs admin: `sc stop NvContainerLocalSystem`, reversible) -> can't be killed
# here. -KeepOverlays skips this. The display driver's own VEH (nvwgf2umx) is unremovable.
function Suppress-Overlays {
    if ($KeepOverlays) { return }
    $killed = @()
    foreach ($n in @('GameBar','GameBarFTServer','XboxGameBarWidgets','GameBarPresenceWriter','Discord')) {
        Get-Process $n -ErrorAction SilentlyContinue | ForEach-Object {
            try { Stop-Process -Id $_.Id -Force -ErrorAction Stop; $killed += $n } catch {}
        }
    }
    Get-CimInstance Win32_Process -Filter "Name='nvcontainer.exe' OR Name='NVIDIA Share.exe' OR Name='NVIDIA Overlay.exe' OR Name='nvsphelper64.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.SessionId -ne 0 } | ForEach-Object {
            try { Stop-Process -Id $_.ProcessId -Force -ErrorAction Stop; $killed += 'nv-overlay' } catch {}
        }
    if ($killed.Count) { Write-Output "OVERLAY-SUPPRESS killed: $([string]::Join(', ', ($killed | Sort-Object -Unique)))" }
}

# 2) launch with BOOT-CRASH RETRY. The Empress de-DRM intermittently storms first-chance AVs during the splash
# (~25-35s in) and dies (see [FH5VEH] EMP.dll+0x21A1C). It's not deterministic, so just relaunch: surviving the
# ~42s danger window means the boot got through. -KeepOverlays implies a single attempt (manual control).
$maxAttempts = if ($KeepOverlays) { 1 } else { 5 }
$bootSurvived = $false
for ($attempt = 1; $attempt -le $maxAttempts; $attempt++) {
    Suppress-Overlays
    Get-Process ForzaHorizon5 -ErrorAction SilentlyContinue | Stop-Process -Force
    if (Test-Path $Log) { Remove-Item $Log -Force -ErrorAction SilentlyContinue }
    Start-Process $Exe
    Write-Output "LAUNCHED $Exe attempt $attempt/$maxAttempts ($(Get-Date -Format HH:mm:ss))"

    # wait for FH5VR.log (proxy DLL loaded + Framework ctor ran)
    $logDeadline = (Get-Date).AddSeconds(40)
    while ((Get-Date) -lt $logDeadline -and -not (Test-Path $Log)) { Start-Sleep -Milliseconds 500 }
    if (-not (Test-Path $Log)) { Write-Output "  attempt ${attempt}: NO_LOG; retrying"; continue }

    # boot-survival monitor: poll the process; retry the instant it dies, else proceed after the danger window.
    $survived = $true
    for ($t = 0; $t -lt 42; $t++) {
        Start-Sleep -Seconds 1
        if (-not (Get-Process ForzaHorizon5 -ErrorAction SilentlyContinue)) { $survived = $false; break }
    }
    if ($survived) { $bootSurvived = $true; Write-Output "BOOT stable (attempt $attempt, FH5VR.log up)"; break }
    Write-Output "  attempt ${attempt}: BOOT CRASH (de-DRM storm); relaunching..."
    Start-Sleep -Seconds 3
}
if (-not $bootSurvived) { Write-Output "RESULT=BOOT_FAILED (all $maxAttempts attempts crashed during boot)"; exit 1 }

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
    if ($HudQuad -or $UiRedirect -ne 0) {
        $readyRecenter = [int][double]::Parse((Get-Date -UFormat %s), [Globalization.CultureInfo]::InvariantCulture)
        Set-ControlValue "recenter" "$readyRecenter"
        Write-Output "HUD-RECENTER seq=$readyRecenter (post-navigation)"
        Start-Sleep -Milliseconds 500
    }
    Write-Output "RESULT=READY"
} else {
    Write-Output "RESULT=NAV_FAILED exit=$navExit"
}
exit $navExit
