# Deploy-FH5VR.ps1 — stage FH5VR.dll as a dxgi.dll proxy next to ForzaHorizon5.exe.
#
# The Empress build has no EAC/Arxan, so a dxgi.dll proxy auto-loads (proven by FH5CameraProbe). This
# script:
#   1. renames the game-dir dxgi.dll (if any) -> dxgi_real.dll  (the proxy forwards real exports there),
#      falling back to copying System32\dxgi.dll as dxgi_real.dll when the game dir has none,
#   2. copies the freshly built FH5VR.dll in as dxgi.dll,
#   3. drops the dynamic-loaded OpenXR loader next to the exe,
#   4. clears XR_RUNTIME_JSON by default so the system OpenXR runtime is used; -SimRuntime opts into SimXR.
#
# Usage:
#   pwsh scripts/Deploy-FH5VR.ps1                 # system OpenXR runtime
#   pwsh scripts/Deploy-FH5VR.ps1 -SimRuntime     # set User XR_RUNTIME_JSON to SimXR
#   pwsh scripts/Deploy-FH5VR.ps1 -Undo           # restore dxgi_real.dll -> dxgi.dll, remove staged files

param(
    [string]$GameDir   = "E:\Games\ForzaHorizon5Empress",
    [string]$BuildDir  = "E:\Github\vrframework\build-fh5",
    [string]$Config    = "Release",
    [string]$LoaderDll = "E:\Github\tapir-unreal-engine\Engine\Binaries\ThirdParty\OpenXR\win64\openxr_loader.dll",
    [string]$SimJson   = "E:\Github\OpenXR-Simulator\bin\openxr_simulator.json",
    [switch]$RealRuntime,
    [switch]$SimRuntime,
    [switch]$Undo
)

$ErrorActionPreference = "Stop"
$exe       = Join-Path $GameDir "ForzaHorizon5.exe"
$proxy     = Join-Path $GameDir "dxgi.dll"
$realDxgi  = Join-Path $GameDir "dxgi_real.dll"
$builtDll  = Join-Path $BuildDir "fh5vr\$Config\FH5VR.dll"

if (-not (Test-Path $exe)) { throw "FH5 exe not found at $exe (pass -GameDir)" }

if ($Undo) {
    if (Test-Path $proxy)    { Remove-Item $proxy -Force; Write-Host "removed proxy dxgi.dll" }
    if (Test-Path $realDxgi) { Move-Item $realDxgi $proxy -Force; Write-Host "restored dxgi_real.dll -> dxgi.dll" }
    Write-Host "undo complete."
    return
}

if (-not (Test-Path $builtDll)) {
    throw "Built FH5VR.dll not found at $builtDll. Build first:`n  cmake --build $BuildDir --config $Config --target FH5VR"
}

# 1. Ensure dxgi_real.dll exists (the forward target). Prefer the game's own dxgi.dll; else System32.
if (-not (Test-Path $realDxgi)) {
    if ((Test-Path $proxy) -and -not (Get-Item $proxy).VersionInfo.FileDescription) {
        # An existing dxgi.dll that is NOT our proxy: promote it to the real forward target.
        Move-Item $proxy $realDxgi -Force
        Write-Host "moved existing dxgi.dll -> dxgi_real.dll"
    } else {
        Copy-Item (Join-Path $env:WINDIR "System32\dxgi.dll") $realDxgi -Force
        Write-Host "copied System32\dxgi.dll -> dxgi_real.dll"
    }
}

# 2. Stage the proxy.
Copy-Item $builtDll $proxy -Force
Write-Host "staged FH5VR.dll -> $proxy"

# 3. OpenXR loader next to the exe (dynamic-loaded; matches XrSimTest.cpp).
if (Test-Path $LoaderDll) {
    Copy-Item $LoaderDll (Join-Path $GameDir "openxr_loader.dll") -Force
    Write-Host "staged openxr_loader.dll"
} else {
    Write-Warning "OpenXR loader not found at $LoaderDll - the mod will fail to load XR until one is present"
}

# 4. Runtime selection.
if ($SimRuntime) {
    if (-not (Test-Path $SimJson)) { throw "SimXR json not found at $SimJson" }
    [Environment]::SetEnvironmentVariable("XR_RUNTIME_JSON", $SimJson, "User")
    Write-Host "XR_RUNTIME_JSON (User) -> $SimJson  (SimXR; restart the launcher to inherit)"
} else {
    [Environment]::SetEnvironmentVariable("XR_RUNTIME_JSON", $null, "User")
    if ($RealRuntime) {
        Write-Host "RealRuntime: cleared User XR_RUNTIME_JSON; system OpenXR runtime will be used."
    } else {
        Write-Host "cleared User XR_RUNTIME_JSON; system OpenXR runtime will be used."
    }
}

Write-Host "`nDeploy complete. Launch FH5; FH5VR.log lands beside the exe. Undo with -Undo."
