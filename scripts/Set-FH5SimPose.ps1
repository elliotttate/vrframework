<#
  Set-FH5SimPose.ps1 - send a deterministic pose to the OpenXR Simulator.

  Coordinates are OpenXR stage-space metres:
    +X right, +Y up, -Z forward.

  Examples:
    pwsh scripts/Set-FH5SimPose.ps1 -Recenter
    pwsh scripts/Set-FH5SimPose.ps1 -Forward 2
    pwsh scripts/Set-FH5SimPose.ps1 -Back 2
    pwsh scripts/Set-FH5SimPose.ps1 -Right 0.5 -Up 0.2
#>
param(
    [float]$X = 0.0,
    [float]$Y = 1.70,
    [float]$Z = 0.0,
    [float]$Forward = 0.0,
    [float]$Back = 0.0,
    [float]$Right = 0.0,
    [float]$Left = 0.0,
    [float]$Up = 0.0,
    [float]$Down = 0.0,
    [float]$Yaw = 0.0,
    [float]$Pitch = 0.0,
    [float]$Roll = 0.0,
    [switch]$Recenter,
    [int]$Repeat = 3,
    [int]$IntervalMs = 80,
    [ValidateSet("","input540","viewtail","ccam320","ccam320_d550","clone0","clone1","clone2","downstream","off")]
    [string]$PosLane = "",
    [switch]$PreserveManual,
    [string]$PosePath = "$env:LOCALAPPDATA\OpenXR-Simulator\head_pose_command.json",
    [string]$ControlPath = "E:\tmp\fh5vr_ctl.txt"
)
$ErrorActionPreference = "Stop"

function Format-Float([float]$v) {
    return $v.ToString("0.######", [Globalization.CultureInfo]::InvariantCulture)
}

function Write-PoseCommand([string]$path, [float]$px, [float]$py, [float]$pz, [float]$yaw, [float]$pitch, [float]$roll) {
    $dir = Split-Path -Parent $path
    if ($dir) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $json = ('{{"x":{0},"y":{1},"z":{2},"yaw":{3},"pitch":{4},"roll":{5}}}' -f `
        (Format-Float $px), (Format-Float $py), (Format-Float $pz),
        (Format-Float $yaw), (Format-Float $pitch), (Format-Float $roll))
    [IO.File]::WriteAllText($path, $json, [Text.Encoding]::ASCII)
}

function Read-Control([string]$path) {
    $h = [ordered]@{
        ipd = "0.032"
        scale = "1"
        mode = "off"
        rot = "driver"
        proj = "on"
        poslane = "ccam320"
        tgt = "off"
        fwd = "0"
        strafe = "0"
        up = "0"
        recenter = "0"
    }
    if (Test-Path $path) {
        foreach ($line in [IO.File]::ReadAllLines($path)) {
            if ($line -match '^([^=]+)=(.*)$') {
                $h[$Matches[1]] = $Matches[2]
            }
        }
    }
    return $h
}

function Write-Control([string]$path, [hashtable]$h) {
    $dir = Split-Path -Parent $path
    if ($dir) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $order = @("ipd", "scale", "mode", "rot", "proj", "poslane", "tgt", "fwd", "strafe", "up", "recenter")
    $lines = foreach ($key in $order) {
        if ($h.Contains($key)) { "$key=$($h[$key])" }
    }
    [IO.File]::WriteAllText($path, ($lines -join "`n"), [Text.Encoding]::ASCII)
}

function Clear-ManualControl([hashtable]$h) {
    if ($PreserveManual) { return }
    $h["tgt"] = "off"
    $h["fwd"] = "0"
    $h["strafe"] = "0"
    $h["up"] = "0"
}

$px = $X + $Right - $Left
$py = $Y + $Up - $Down
$pz = $Z - $Forward + $Back

if ($Repeat -lt 1) { $Repeat = 1 }
for ($i = 0; $i -lt $Repeat; ++$i) {
    Write-PoseCommand $PosePath $px $py $pz $Yaw $Pitch $Roll
    if ($i + 1 -lt $Repeat) { Start-Sleep -Milliseconds $IntervalMs }
}

if ($Recenter) {
    Start-Sleep -Milliseconds 700
    $control = Read-Control $ControlPath
    $seq = (Get-Random -Minimum 1 -Maximum 2147483647)
    $control["recenter"] = [string]$seq
    if ($PSBoundParameters.ContainsKey("PosLane") -and -not [string]::IsNullOrWhiteSpace($PosLane)) {
        $control["poslane"] = $PosLane
    }
    Clear-ManualControl $control
    Write-Control $ControlPath $control
    Write-Output "RECENTER seq=$seq control=$ControlPath"
} elseif ($PSBoundParameters.ContainsKey("PosLane") -and -not [string]::IsNullOrWhiteSpace($PosLane)) {
    $control = Read-Control $ControlPath
    $control["poslane"] = $PosLane
    Clear-ManualControl $control
    Write-Control $ControlPath $control
    Write-Output "POSLANE $PosLane control=$ControlPath"
}

Write-Output ("POSE x={0} y={1} z={2} yaw={3} pitch={4} roll={5} path={6}" -f `
    (Format-Float $px), (Format-Float $py), (Format-Float $pz),
    (Format-Float $Yaw), (Format-Float $Pitch), (Format-Float $Roll), $PosePath)
