<#
  Navigate-FH5.ps1 — state-driven, image-free navigator to FREE-ROAM driving.

  Reads E:\tmp\fh5_state.txt (written every ~200ms by FH5VR's Fh5MenuNav module) and drives input until
  the LIVE rendered-scene signal reports free-roam (scene=world3d, far~5000, gameplay camera rendering).
  This replaces the old blind-timing launcher flow (press Enter, sleep 30s, press Enter, sleep 18s): every
  action is gated on the actual scene the game is rendering, not a fixed sleep.

  STATE SCHEMA (fh5_state.txt, key=value):
    scene = menu_or_loading | showcase | world3d   (showcase=far>30000 intro/cinematic; world3d=far~5000)
    far, near, gameplay(0/1), main_rate, camera, screen, ui_pages, ui_blocking_pages, xinput_hooked, inject_seq

  INPUT:
    - Default: the keyboard/game-window path (FH5Window.ps1 -> the FH5 game window).
    - -UseXInput ALSO writes E:\tmp\fh5_nav.txt controller commands for the in-process XInput injector
      (belt-and-suspenders once that path is confirmed).
    - -MonitorOnly: read + print state only, send NOTHING (safe smoke test against a running game).

  EXIT: RESULT=READY (free-roam reached) | RESULT=CRASH | RESULT=TIMEOUT.
#>
param(
    [string]$StatePath = 'E:\tmp\fh5_state.txt',
    [string]$NavPath   = 'E:\tmp\fh5_nav.txt',
    [int]$TimeoutSec   = 300,
    [int]$SettleSec    = 3,          # how long scene=world3d must hold before we call it free-roam
    [int]$SecondEnterDelaySec = 30,  # proven startup route: Enter, wait, Enter
    [int]$MenuContinueDelaySec = 12, # front-menu Continue retry delay after the second startup Enter
    [ValidateSet('Enter','Escape')]
    [string]$MenuContinueKey = 'Enter',
    [ValidateSet('Enter','Escape')]
    [string]$OverlayCleanupKey = 'Escape',
    [int]$DriverSettleSec = 12,       # cockpit/driver cam is valid only after a longer quiet hold
    [int]$UnknownFallbackSec = 20,    # world3d held this long with an UNRESOLVED camera -> accept (anti-deadlock)
    [int]$StartupUnknownFallbackSec = 25, # only press the first startup Enter on unknown UI after this long
    [int]$HubLoadSec = 7,             # the festival "My Cars" hub must be a stable/loaded menu this long before the single Escape
    [int]$HubRetrySec = 14,           # if STILL at the hub this long after the Escape (it didn't land), allow ONE retry
    [switch]$SkipBootSequence,        # for attaching to an already-running game; starts in post-menu handling
    [switch]$UseXInput,             # also emit controller commands to the in-process injector
    [switch]$MonitorOnly            # observe + print only; never send input
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\FH5Window.ps1"

if (-not (Get-Command Send-FH5GameKey -ErrorAction SilentlyContinue)) {
    Write-Output 'RESULT=ERROR (FH5Window.ps1 did not provide Send-FH5GameKey)'; exit 3
}

# Virtual-key codes.
$VK = @{ Enter = 0x0D; Esc = 0x1B; Up = 0x26; Down = 0x28; Left = 0x25; Right = 0x27; Space = 0x20 }

$script:navSeq  = 0
$script:lastAct = @{}
$script:phase = if ($SkipBootSequence) { 'post_menu' } else { 'boot' }
$script:lastInputAt = $null

function Get-State {
    if (-not (Test-Path $StatePath)) { return $null }
    $h = @{}
    try { foreach ($l in [IO.File]::ReadAllLines($StatePath)) { if ($l -match '^([A-Za-z_]+)=(.*)$') { $h[$Matches[1]] = $Matches[2] } } }
    catch { return $null }   # mid-write swap; try again next tick
    return $h
}

function CoolOK([string]$key, [int]$ms) {
    $now = Get-Date
    if (-not $script:lastAct.ContainsKey($key) -or ($now - $script:lastAct[$key]).TotalMilliseconds -ge $ms) {
        $script:lastAct[$key] = $now; return $true
    }
    return $false
}

function Send-Key([int]$vk, [string]$why) {
    if ($MonitorOnly) { Write-Output "  [monitor] would press 0x$('{0:X2}' -f $vk) ($why)"; return }
    $r = Send-FH5GameKey -Vk $vk
    $script:lastInputAt = Get-Date
    Write-Output ("  key 0x{0:X2} ({1}) -> {2}" -f $vk, $why, $r)
}

function Send-Pad([string]$btn, [string]$why) {
    if (-not $UseXInput) { return }
    $script:navSeq++
    $script:lastInputAt = Get-Date
    if (-not $MonitorOnly) {
        $dir = Split-Path -Parent $NavPath; if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
        try {
            [IO.File]::WriteAllText($NavPath, ("seq={0}`ncmd=press {1}" -f $script:navSeq, $btn), [Text.Encoding]::ASCII)
        } catch {
            Write-Output ("  pad {0} ({1}) seq={2} skipped: {3}" -f $btn, $why, $script:navSeq, $_.Exception.Message)
            return
        }
    }
    Write-Output ("  pad {0} ({1}) seq={2}" -f $btn, $why, $script:navSeq)
}

function Send-NamedKey([string]$name, [string]$why) {
    if ($name -eq 'Escape') {
        Send-Key $VK.Esc "$why via Escape"
        Send-Pad 'B' $why
    } else {
        Send-Key $VK.Enter "$why via Enter"
        Send-Pad 'A' $why
    }
}

function Set-Phase([string]$next) {
    if ($script:phase -ne $next) {
        Write-Output "  phase $($script:phase) -> $next"
        $script:phase = $next
    }
}

Write-Output ("Navigate-FH5: target=free-roam timeout=${TimeoutSec}s settle=${SettleSec}s continueKey=${MenuContinueKey} overlayKey=${OverlayCleanupKey} xinput={0} monitor={1}" -f [bool]$UseXInput, [bool]$MonitorOnly)
$deadline   = (Get-Date).AddSeconds($TimeoutSec)
$worldSince = $null
$driverSince = $null
$startupEnterCount = 0
$firstEnterAt = $null
$secondEnterAt = $null
$menuContinueSent = $false
$showcaseDriverEscapeSent = $false
$postShowcaseEscapeSince = $null
$startupUnknownSince = $null
# Festival "My Cars" hub -> game: the game loads to the My Cars/Garage hub menu; a SINGLE Escape from it
# enters free-roam/race. A SECOND Escape toggles back to the hub. So we press Escape exactly ONCE, only after
# the hub has fully loaded (stable $HubLoadSec), and never again once in-game ($gameEntered latches true).
$gameEntered = $false
$hubStableSince = $null
$hubEscapeAt = $null
$hubEscapeCount = 0
# The My-Cars festival hub reads in the state file as world3d/CCamDriver/screen=Splash -- IDENTICAL to real
# free-roam (the hub renders a 3D driving-camera scene behind the menu). So we cannot detect it from state.
# Instead, once a stable driving-camera state is reached we press SPACE ("Drive", per the on-screen hint),
# which ENTERS free-roam from the hub and is a harmless accelerate-tap if already driving -- it NEVER opens a
# menu (unlike Escape, which toggles). $driveSent ensures we send it a bounded number of times.
$driveSent = 0
$driveSentAt = $null

while ((Get-Date) -lt $deadline) {
    if (-not (Get-Process ForzaHorizon5 -ErrorAction SilentlyContinue)) { Write-Output 'RESULT=CRASH (FH5 not running)'; exit 2 }

    $s = Get-State
    if ($null -eq $s) { Write-Output '  waiting for fh5_state.txt (mod not up yet)'; Start-Sleep -Seconds 1; continue }

    $scene = $s['scene']
    Write-Output ("[{0}] phase={1} scene={2} far={3} gameplay={4} main_rate={5} cam={6} screen={7} ui={8}/{9}/{10} block={11} xinput={12}" -f `
        (Get-Date -Format HH:mm:ss), $script:phase, $scene, $s['far'], $s['gameplay'], $s['main_rate'], $s['camera'], $s['screen'],
        $s['ui_screen'], $s['ui_reliable'], $s['ui_visible'], $s['ui_blocking_pages'], $s['xinput_hooked'])

    $camForState = [string]$s['camera']
    $screenForState = [string]$s['screen']
    $uiPages = [string]$s['ui_pages']
    $uiBlockingPages = [string]$s['ui_blocking_pages']
    $uiReliable = ([string]$s['ui_reliable'] -eq '1')
    $loadingLike = (
        $screenForState -match '^(BaseLoadingScreen|Loading|FMVLoading|SeasonTransitionFMVLoading|WaitForInstallFromSplash)$' -or
        $screenForState -match 'Loading$'
    )
    $splashLike = ($screenForState -match '^(Splash|SplashE3)$')
    $trustedLoadingLike = ($uiReliable -and $loadingLike)
    $trustedSplashAfterStartup = ($uiReliable -and $splashLike -and $startupEnterCount -ge 2)
    $trustedSplashFrontMenu = (
        $trustedSplashAfterStartup -and
        $scene -eq 'menu_or_loading' -and
        [string]$s['gameplay'] -ne '1' -and
        ($camForState -eq 'unknown' -or $camForState -eq '')
    )
    $trustedOverlayMenu = (
        $uiReliable -and
        $screenForState -match '^(GenericMenu|OptionsMenu|HudOptions|DestinationMenu|TuneMenu|UpgradesMenu|RivalsMenu|StartingGrid|CarSelectGarage|MyHorizonLife|FestivalPassScene|AccoladeCategoriesScene|PauseMenuTiled|MapScene|MapSceneInteractive|MapSceneBase)$'
    )
    $uiPagesOverlay = ($uiBlockingPages -match '(^|,)(CarSelectGarage|GenericMenu|OptionsMenu|HudOptions|DestinationMenu|TuneMenu|UpgradesMenu|RivalsMenu|StartingGrid|MyHorizonLife|FestivalPassScene|AccoladeCategoriesScene|MapScene|MapSceneInteractive|MapSceneBase|PhotoModeScene)(,|$)')
    $drivingCameraLike = ($camForState -match '^CCam(FollowLow|FollowHigh|Hood|BumperHigh|Driver)$')
    $loadingBlocksBootInput = ($trustedLoadingLike -and $scene -eq 'menu_or_loading' -and [string]$s['gameplay'] -ne '1')
    $frontMenuLike = (
        $scene -eq 'menu_or_loading' -and
        -not $loadingBlocksBootInput -and
        -not $trustedSplashFrontMenu -and
        -not $trustedOverlayMenu -and
        [string]$s['gameplay'] -ne '1' -and
        ($camForState -eq 'unknown' -or $camForState -eq '')
    )
    $frontMenuScreenKnown = ($screenForState -ne 'unknown' -and $screenForState -ne '')
    $frontMenuStartupReady = ($frontMenuLike -and ($frontMenuScreenKnown -or $uiReliable))

    if ($script:phase -eq 'boot') {
        # Fresh-launch front menu handling. Continue/confirm is safe only while no gameplay camera exists
        # (far=0/gameplay=0/camera=unknown). Once gameplay/camera activity starts, leave boot
        # and reserve overlay cleanup for Garage/menu overlays.
        $worldSince = $null
        $driverSince = $null
        $now = Get-Date
        if ($loadingBlocksBootInput) {
            Write-Output "  trusted loading screen; waiting"
            Start-Sleep -Milliseconds 800
            continue
        }
        elseif ($trustedSplashFrontMenu -and -not $menuContinueSent -and $secondEnterAt -and (($now - $secondEnterAt).TotalSeconds -ge $MenuContinueDelaySec)) {
            Send-NamedKey $MenuContinueKey 'front menu Continue (Splash page stale)'
            $menuContinueSent = $true
        }
        elseif ($trustedSplashFrontMenu -and $menuContinueSent -and $script:lastInputAt -and (($now - $script:lastInputAt).TotalSeconds -ge $MenuContinueDelaySec)) {
            Send-NamedKey $MenuContinueKey 'front menu Continue retry (Splash page stale)'
        }
        elseif ($trustedSplashFrontMenu) {
            Write-Output "  startup Enters sent; Splash still reliable, waiting for render transition"
        }
        elseif ($trustedOverlayMenu) {
            if (-not $menuContinueSent -or ($script:lastInputAt -and (($now - $script:lastInputAt).TotalSeconds -ge $MenuContinueDelaySec))) {
                Send-NamedKey $OverlayCleanupKey "trusted overlay cleanup ($screenForState)"
                $menuContinueSent = $true
            }
            Start-Sleep -Milliseconds 800
            continue
        }
        elseif ($startupEnterCount -eq 0 -and $frontMenuLike -and -not $frontMenuStartupReady) {
            if ($null -eq $startupUnknownSince) {
                $startupUnknownSince = $now
                Write-Output "  front menu candidate not classified yet; waiting before startup Enter #1"
            }
            elseif (($now - $startupUnknownSince).TotalSeconds -ge $StartupUnknownFallbackSec) {
                Send-Key $VK.Enter 'startup enter #1 (unknown-screen fallback)'
                Send-Pad 'A' 'startup confirm #1'
                $startupEnterCount = 1
                $firstEnterAt = $now
            }
        }
        elseif ($startupEnterCount -eq 0 -and $frontMenuStartupReady) {
            Send-Key $VK.Enter 'startup enter #1'
            Send-Pad 'A' 'startup confirm #1'
            $startupEnterCount = 1
            $firstEnterAt = $now
        }
        elseif ($startupEnterCount -eq 1 -and $firstEnterAt -and $frontMenuStartupReady -and $scene -eq 'menu_or_loading' -and [string]$s['gameplay'] -ne '1' -and (($now - $firstEnterAt).TotalSeconds -ge $SecondEnterDelaySec)) {
            Send-Key $VK.Enter 'startup enter #2'
            Send-Pad 'A' 'startup confirm #2'
            $startupEnterCount = 2
            $secondEnterAt = $now
        }
        elseif (-not $frontMenuLike) {
            Set-Phase 'post_menu'
        }
        Start-Sleep -Milliseconds 800
        continue
    }

    # The festival "My Cars"/Garage HUB is the post-load menu. CRITICAL: the hub renders a 3D driving-camera
    # scene BEHIND the menu, so the camera reads as CCamDriver while a GARAGE/CarSelectGarage menu UI is on top.
    # The flow is: load -> HUB (menu over a 3D scene) -> ONE Escape -> free-roam/race. So whenever a hub MENU
    # UI is present we must Escape it (NOT treat the 3D-scene camera as free-roam). We wait for it to FULLY load
    # ($HubLoadSec) then press Escape ONCE; retry only if STILL at the hub $HubRetrySec later. We only reach the
    # scene switch (which declares READY) once NO hub menu UI remains -> guarantees READY = real free-roam.
    $hubLike = ($trustedOverlayMenu -or $uiPagesOverlay -or $camForState -eq 'CCamFollowExtended')
    if ($hubLike) {
        $worldSince = $null
        $driverSince = $null
        if ($null -eq $hubStableSince) {
            $hubStableSince = Get-Date
            Write-Output "  festival hub MENU present ($screenForState block=$uiBlockingPages cam=$camForState ui=$uiPages); letting it FULLY load (${HubLoadSec}s) before the single Escape..."
        }
        elseif (((Get-Date) - $hubStableSince).TotalSeconds -ge $HubLoadSec -and
                ($hubEscapeCount -eq 0 -or ($hubEscapeAt -and ((Get-Date) - $hubEscapeAt).TotalSeconds -ge $HubRetrySec))) {
            $hubEscapeCount++
            Send-NamedKey 'Escape' "enter game from festival hub [single Escape #$hubEscapeCount] ($screenForState)"
            $hubEscapeAt = Get-Date
            $gameEntered = $true   # we've performed the entering Escape; suppress other Escape-senders below
        }
        Start-Sleep -Milliseconds 800
        continue
    }
    # No hub menu UI -> re-arm the hub-load timer so a future hub re-show needs a fresh full-load hold.
    $hubStableSince = $null

    switch ($scene) {
        'world3d' {
            # 3D world is rendering. Use the active CAMERA CLASS (when resolved) to split free-roam DRIVING
            # from the garage: CCamFollow*/Hood/Bumper/Driver = driving views = free-roam; CCamFollowExtended
            # = garage/extended-follow menu; CCamFree*/Photo = non-driving. 'unknown' = not resolved yet -> fall
            # back to the sustained-world3d heuristic.
            $cam = [string]$s['camera']
            if ($cam -eq 'CCamFollowExtended') {
                $worldSince = $null
                # Garage is a MENU, so Escape safely backs out (unlike free-roam where it opens pause).
                if (CoolOK 'garageback' 4000) { Send-Key $VK.Esc 'back out of garage'; Send-Pad 'B' 'back' }
            }
            elseif ($cam -match '^CCam(FollowLow|FollowHigh|Hood|BumperHigh)$') {
                $driverSince = $null
                if ($null -eq $worldSince) { $worldSince = Get-Date; Write-Output "  -> world3d (cam=$cam); confirming it holds..." }
                elseif (((Get-Date) - $worldSince).TotalSeconds -ge $SettleSec) {
                    Write-Output "RESULT=READY (free-roam: world3d sustained, cam=$cam)"; exit 0
                }
            }
            elseif ($cam -eq 'CCamDriver') {
                $worldSince = $null
                if ($null -eq $driverSince) {
                    $driverSince = Get-Date
                    Write-Output "  -> cockpit/driver candidate (could be the My-Cars hub, indistinguishable in state); will press Drive to enter free-roam..."
                }
                else {
                    # The My-Cars festival hub looks identical to free-roam in the state file. Press SPACE
                    # ("Drive", per the on-screen hint) up to 3x (3s apart) to ENTER free-roam from the hub.
                    # Space is non-toggling: it drives from the hub, harmlessly accelerates if already free-roam,
                    # and NEVER opens a menu. Then READY after the post-Drive quiet hold.
                    if ($driveSent -lt 3 -and ($null -eq $driveSentAt -or ((Get-Date) - $driveSentAt).TotalSeconds -ge 3)) {
                        $driveSent++
                        Send-Key $VK.Space "Drive: enter free-roam from My-Cars hub (Space) [#$driveSent]"
                        $driveSentAt = Get-Date
                    }
                    $quietSec = if ($script:lastInputAt) { ((Get-Date) - $script:lastInputAt).TotalSeconds } else { 9999 }
                    if ($driveSent -ge 1 -and ((Get-Date) - $driverSince).TotalSeconds -ge $DriverSettleSec -and $quietSec -ge $DriverSettleSec) {
                        Write-Output "RESULT=READY (free-roam: driver camera, drove from hub, quiet ${DriverSettleSec}s)"; exit 0
                    }
                }
            }
            elseif ($cam -eq 'CinematicGameCamera') {
                # In-world scripted cinematic (e.g. the intro drive). SKIP it forward with Enter/A — Escape does
                # NOT skip a cinematic (and can open a menu). Trusted UI overlays are handled before this switch,
                # so this cannot press Enter into PauseMenuTiled or another menu.
                $worldSince = $null
                $driverSince = $null
                if (CoolOK 'skipcine' 5000) { Send-Key $VK.Enter 'skip in-world cinematic'; Send-Pad 'A' 'skip' }
            }
            elseif ($cam -eq 'unknown' -or $cam -eq '') {
                # Prefer the camera class, but DON'T deadlock if the resolver is idle (e.g. running without VR so
                # the upstream writer never resolves the camera): accept world3d held for the long fallback window.
                $driverSince = $null
                if ($null -eq $worldSince) { $worldSince = Get-Date; Write-Output "  -> world3d, camera unresolved; anti-deadlock fallback timer (${UnknownFallbackSec}s)..." }
                elseif (((Get-Date) - $worldSince).TotalSeconds -ge $UnknownFallbackSec) {
                    Write-Output "RESULT=READY (free-roam: world3d sustained ${UnknownFallbackSec}s, camera unresolved)"; exit 0
                }
            }
            elseif ($cam -match '^CCam(Free|FreeTargetCar|FreeTrack)$') {
                $worldSince = $null
                $driverSince = $null
                if (-not $gameEntered -and (CoolOK 'world3dback' 6000)) {
                    Send-Key $VK.Esc "back out of non-driving 3D camera ($cam)"
                    Send-Pad 'B' 'back'
                }
            }
            else {
                $worldSince = $null
                $driverSince = $null
                if (-not $gameEntered -and (CoolOK 'world3dback' 6000)) {
                    Send-Key $VK.Esc "back out of unresolved/non-driving 3D camera ($cam)"
                    Send-Pad 'B' 'back'
                }
            }
        }
        'showcase' {
            # The far-plane based scene label can stay "showcase" even while a CCamDriver object is active.
            # A CCamDriver here is often the real cockpit/free-roam view after an overlay cleanup. Do not send
            # an extra Escape just because the far-bucket still says showcase; that reopens pause/menu overlays.
            $cam = [string]$s['camera']
            $worldSince = $null
            if ($cam -eq 'CCamDriver') {
                if ($null -eq $driverSince) {
                    $driverSince = Get-Date
                    Write-Output "  -> showcase-labeled driver candidate (could be the My-Cars hub); will press Drive..."
                }
                else {
                    # Same as world3d: press SPACE ("Drive") to enter free-roam from the My-Cars hub (non-toggling).
                    if ($driveSent -lt 3 -and ($null -eq $driveSentAt -or ((Get-Date) - $driveSentAt).TotalSeconds -ge 3)) {
                        $driveSent++
                        Send-Key $VK.Space "Drive: enter free-roam from My-Cars hub (Space) [#$driveSent]"
                        $driveSentAt = Get-Date
                    }
                    $quietSec = if ($script:lastInputAt) { ((Get-Date) - $script:lastInputAt).TotalSeconds } else { 9999 }
                    $driverHeldSec = ((Get-Date) - $driverSince).TotalSeconds
                    if ($driveSent -ge 1 -and $driverHeldSec -ge $DriverSettleSec -and $quietSec -ge $DriverSettleSec) {
                        Write-Output "RESULT=READY (showcase driver camera, drove from hub, quiet ${DriverSettleSec}s)"; exit 0
                    }
                }
            }
            else {
                $driverSince = $null
                if (-not $gameEntered -and (CoolOK 'showcaseback' 6000)) { Send-Key $VK.Esc 'back out of post-menu 3D overlay'; Send-Pad 'B' 'back' }
            }
        }
        default {
            # If we are still on the non-gameplay front menu, Continue is selected and Enter is correct.
            # Once gameplay/camera activity exists, Back/Escape is the recovery action for overlays.
            $worldSince = $null
            $driverSince = $null
            if ($frontMenuLike) {
                if (CoolOK 'frontmenuretry' 7000) { Send-Key $VK.Enter 'front menu continue retry'; Send-Pad 'A' 'front menu continue' }
            }
            elseif (-not $gameEntered -and (CoolOK 'postmenuback' 7000)) {
                Send-Key $VK.Esc 'post-menu back/cleanup'
                Send-Pad 'B' 'back'
            }
        }
    }
    Start-Sleep -Milliseconds 800
}

Write-Output 'RESULT=TIMEOUT (never reached free-roam)'
exit 1
