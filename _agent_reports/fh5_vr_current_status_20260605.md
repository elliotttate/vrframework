# FH5 VR Mod Current Status

Date: 2026-06-05

This is the current working state of the FH5 VR effort in `E:\Github\vrframework` targeting:

- Game: `E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe`
- Deployed proxy/mod DLL: `E:\Games\ForzaHorizon5Empress\dxgi.dll`
- Runtime log: `E:\Games\ForzaHorizon5Empress\FH5VR.log`
- State file: `E:\tmp\fh5_state.txt`
- Control file: `E:\tmp\fh5vr_ctl.txt`

## Short Version

We now have a reliable-ish script path to launch FH5, get through the front menu, and land in cockpit/free-roam gameplay with the OpenXR Simulator runtime.

We also have OpenXR Simulator pose commands reaching the mod and being written through the current upstream camera writer. The log proof is strong: simulator `z=-2` becomes `headT=[0,0,2]` and `FH5CAM off=(...,2.000) applied=1 layout=ccam matrix=0x320`.

However, the user-visible problem is not solved yet: you are currently not seeing forward/back/head translation move the rendered view. That means the current `CCamDriver +0x320/+0x360` writer is either not the final visible render-position lever for this cockpit path, is being overwritten/neutralized by another camera-relative lane, or only affects part of the engine state while the visible driver view uses another source.

Do not mark 6DOF as fixed yet. Current status is: **OpenXR movement is published and written, but visual movement is still not accepted.**

## 2026-06-05 Update: Lane Selector + Readback Discriminators

The current build now has explicit controls for the assumptions that were previously getting blended together:

- `poslane=ccam320` writes the active camera `+0x320/+0x360` path only.
- `poslane=ccam320_d550` writes `+0x320/+0x360` plus a raw f64 `+0x550` mirror. The old `+0x550` getter-shadow hook remains disabled.
- `poslane=clone0`, `clone1`, or `clone2` writes one detected matrix clone only.
- `poslane=downstream` enables the final 6912-byte camera-cbuffer path as a visual positive control.
- `poslane=off` disables positional writes.

The writer now emits `[FH5LANE]` once per heartbeat with source pose, intended output pose, post-write `+0x320` readback, raw `+0x550`, and the first three clone summaries. This is the line to use when challenging whether `FH5CAM applied=1` actually affected the lane FH5 renders from.

IPD is now treated like OpenXR translation: `ipd` is half-IPD in metres, and `scale` converts both head translation and IPD to FH5 camera-lane units. That means:

```text
ipd=0.032 scale=1    -> 0.032 lane units
ipd=0.032 scale=100  -> 3.2 lane units
```

This directly tests the assumption that `scale=1` was physical enough for the FH5 camera object. If the visible camera lane is centimetre-ish, `scale=1` is expected to look nearly static.

Recommended live test order:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Launch-FH5VR.ps1 -SimRuntime -Scale 100 -PosLane ccam320 -TimeoutSec 180 -NavigateTimeoutSec 300
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Recenter -PosLane ccam320
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Forward 2
```

If that does not visibly move, keep the same running game and only change the lane:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Recenter -PosLane ccam320_d550
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Forward 2

powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Recenter -PosLane clone0
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Forward 2

powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Recenter -PosLane downstream
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Forward 2
```

Interpretation rules:

- If `downstream` visibly moves but `ccam320` does not, the simulator and visual comparator are good; the upstream lane is wrong or incomplete.
- If `ccam320_d550` moves but `ccam320` does not, the raw f64 mirror is required; keep the getter-shadow hook disabled.
- If one clone moves cleanly, use `[FH5LANE]` to verify that clone is a stable visible camera mirror before promoting it beyond diagnostic mode.
- If none of the lanes move with `scale=100`, challenge the assumption that the current OpenXR Simulator command is being consumed during the visible gameplay frame, even if the idle log looks healthy.

## Current Launch / Get In Game Workflow

From `E:\Github\vrframework`:

```powershell
cmake --build E:\Github\vrframework\build-fh5 --config Release --target FH5VR
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Deploy-FH5VR.ps1 -SimRuntime
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Launch-FH5VR.ps1 -SimRuntime -TimeoutSec 180 -NavigateTimeoutSec 300
```

Expected successful end:

```text
RESULT=READY
```

The launch script now writes a clean control file:

```text
ipd=0.032
scale=1
mode=off
rot=driver
proj=on
tgt=off
fwd=0
strafe=0
up=0
recenter=<seq>
```

What the launcher does today:

1. Kills any stale `ForzaHorizon5.exe`.
2. Clears stale state/nav/log files.
3. Sets `XR_RUNTIME_JSON` to `E:\Github\OpenXR-Simulator\bin\openxr_simulator.json` when `-SimRuntime` is used.
4. Starts FH5.
5. Runs `Navigate-FH5.ps1`.
6. Sends startup Enter #1.
7. Sends startup Enter #2 after the configured delay.
8. Handles the stale `Splash` page by sending a front-menu `Continue` Enter.
9. Moves from `boot` to `post_menu` as soon as real 3D gameplay starts.
10. Uses Escape to back out of non-driving 3D cameras such as `CCamFree`.
11. Uses Enter to skip `CinematicGameCamera`.
12. Waits for stable `CCamDriver`, then returns `RESULT=READY`.

Current state file after a good run should look like:

```text
gameplay=1
scene=showcase
screen=unknown
ui_screen=unknown
ui_reliable=0
ui_pages=PauseMenuTiled,Hud
ui_blocking_pages=
camera=CCamDriver
xinput_hooked=1
```

The raw UI scanner still sees pooled/stale `PauseMenuTiled,Hud` during cockpit gameplay. That is expected for now. Automation must use `ui_blocking_pages`, not raw `ui_pages`.

## OpenXR Simulator Movement Commands

New helper:

```text
E:\Github\vrframework\scripts\Set-FH5SimPose.ps1
```

Coordinates are OpenXR stage-space metres:

- `+X` right
- `+Y` up
- `-Z` forward

Use:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Recenter
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Forward 2
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Back 2
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Right 0.5 -Up 0.2
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1
```

Important: run `-Recenter` while the simulator is at the neutral pose before testing movement. Otherwise the relative pose can be zeroed against a non-neutral simulator pose.

The simulator command file is:

```text
%LOCALAPPDATA%\OpenXR-Simulator\head_pose_command.json
```

The simulator may consume/delete that file quickly. That is normal.

## Current Movement Evidence

After a clean recenter, `Set-FH5SimPose.ps1 -Forward 2` produced this live log evidence:

```text
[FH5POSE] recentered HMD rest pose seq=1454064176
[VR-POSE] stageMid=(0.0000,1.7000,-2.0000) finalPos=(0.0000,1.7000,-2.0000)
[FH5POSE] headT(units)=[0.000 0.000 2.000] driverOff=[0.032 0.000 2.000]
[FH5CAM] off=(0.032,0.000,2.000) applied=1 source=cached layout=ccam matrix=0x320
```

`Set-FH5SimPose.ps1 -Back 3` also flips the sign as expected:

```text
[VR-POSE] stageMid=(0.0000,1.7000,3.0000)
[FH5POSE] headT(units)=[0.000 0.000 -3.000]
[FH5CAM] off=(0.032,0.000,-3.000) applied=1 layout=ccam matrix=0x320
```

So the data path is alive:

```text
OpenXR Simulator -> OpenXR runtime pose -> FH5 adapter -> FH5CAM writer -> CCamDriver +0x320/+0x360
```

But user-visible movement is not currently visible. That is the main unresolved issue.

## What We Are Trying To Solve

The real goal is a proper upstream VR camera path like REFramework/starfield2vr/anvilengine2vr:

1. Full 6DOF head pose affects the camera upstream.
2. Per-eye IPD affects the camera upstream.
3. Per-eye projection is asymmetric and upstream.
4. Geometry, shadows, chevrons, culling, motion vectors, and TAA all consume the same eye camera.
5. AER/sequential eye rendering stays synchronized.
6. HUD moves to a VR quad instead of being stereo-warped in the world image.

The specific blocker right now:

```text
OpenXR simulator movement reaches FH5CAM logs, but the visible cockpit/rendered camera does not visibly move.
```

That means the current upstream lane is not yet enough.

## Major Issues We Hit

### 1. Downstream cbuffer patching was not enough

The old final camera cbuffer rewrite could move visible geometry, but shadows/chevrons/TAA/culling stayed derived from the mono camera. This matches the problem seen in the screenshot: car/camera shifted between eyes while shadows stayed centered.

Conclusion: downstream buffer patching is useful for bring-up, but it cannot be the final architecture.

### 2. Producer args `a4.row3`, `a17`, and `a18` were inert for position

Large offsets applied to producer arguments did not move the rendered camera. This confirmed FH5 camera-relative rebasing had already happened or those fields were not the live position lever.

### 3. Old Steam-version RE offsets were not reliable

Some prior IDA results came from a Steam build. We moved to Empress-specific vtables/AOBs and stopped trusting stale direct VAs.

### 4. `CCamDriver +0x550` getter-shadow path caused sky/culling flicker

There is a double-position lane around `CCamDriver +0x550`, and prior reports suggested it was important. Live testing showed that hooking/replacing this getter path for OpenXR was unstable. Even small offsets could flip cockpit into a sky/culling view.

Current code intentionally avoids/restores the `+0x550` getter-shadow path for OpenXR and uses the direct active-camera writer instead.

### 5. `CCamDriver +0x320/+0x360` direct writer is stable, but not visually sufficient yet

The direct writer is stable: it does not reproduce the old all-white/cockpit-sky flicker, and logs show offsets are applied. But because the user does not see movement, the visible cockpit path likely depends on another lane or the engine rewrites/uses another camera aggregate after our write.

### 6. UI/menu detection had stale page objects

The UI scanner sees pooled/stale pages such as `Splash` and `PauseMenuTiled` after they are no longer visible. This caused wrong Enter/Escape decisions during startup.

Mitigations now in place:

- `ui_pages` remains raw diagnostic output.
- `ui_blocking_pages` is filtered and used for automation.
- stale `Splash` can trigger front-menu `Continue`, but only while still in non-gameplay front-menu state.
- once `scene=world3d` or `gameplay=1`, stale Splash cannot keep the script in boot.

### 7. Menu automation still reaches cockpit/free-roam, not a selected event race

Current automation gets into cockpit/free-roam gameplay. It does not yet select and start a specific race event. If "race" means "actual event race", that is a separate navigation task after the VR camera path is stable.

## Current Code / Script Anchors

- Launch flow: `scripts\Launch-FH5VR.ps1`
- Menu navigation: `scripts\Navigate-FH5.ps1`
- Simulator pose helper: `scripts\Set-FH5SimPose.ps1`
- UI state writer/filter: `src\fh5vr\Fh5MenuNav.cpp`
- UI heap/vtable scanner: `src\fh5vr\Fh5UiScreen.cpp`
- FH5 pose adapter: `src\fh5vr\Fh5Adapter.cpp`
- Upstream camera writer: `src\fh5vr\Fh5CamDriver.cpp`
- OpenXR pose normalization/logging: `src\vr\runtimes\OpenXR.cpp`

## Current Best Theory For "No Visible Movement"

The simulator movement is not dead. It is reaching the mod.

The likely problem is one of these:

1. The visible cockpit render does not use the `CCamDriver +0x320` position as its final render-position source.
2. FH5 uses `+0x320` for one derived camera state but another camera-relative position lane drives the final visible view.
3. Our write is being overwritten between the writer thread and the render/camera consumer.
4. The position lane needs an additional synchronized mirror write, but not via the unstable getter-shadow hook.
5. The apparent no-movement is due to cockpit/vehicle interior anchoring hiding small metre-scale translations; larger controlled visual tests are needed, but current `+5m` screenshot did not show an obvious change.

The important conclusion: **do not keep assuming the current writer is solved just because the log says `applied=1`. The visible frame is the acceptance test.**

## Next Work Items

### Priority 1: Prove the visible camera's actual translation source

Need a new discriminator that compares several candidate lanes while watching the visible frame:

- `CCamDriver +0x320/+0x360` direct writer only.
- f64 position mirror write to `+0x550` as a raw field write, not the previous getter-shadow hook.
- any ForzaMultiCam active aggregate around `+0x650..+0x680` as a readback/proof lane only first.
- the matrix clone reported by `matrix clone scan object=... clones=3 view_tails=1`.

The next test should write one lane at a time with a very large controlled offset and use screenshots or a pixel/feature comparison, not just logs.

### Priority 2: Add explicit diagnostics for source vs written vs readback

For every active camera frame, log:

- source `+0x320` before write
- written `+0x320`
- readback `+0x320` after a short delay
- `+0x550` raw f64 values
- active ForzaMultiCam aggregate readback
- whether the producer cbuffers reflect the same shifted position

This will tell us if the write is overwritten or simply not consumed by visible rendering.

### Priority 3: Keep `+0x550` getter-shadow disabled

The getter-shadow approach caused the sky/culling flicker. If we test `+0x550` again, do it as a direct f64 mirror write coupled to `+0x320`, not by vtable-shadowing the getter.

### Priority 4: Re-test OpenXR movement after each lane change

Use:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Recenter
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Forward 5
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Back 5
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1
```

Acceptance is not `FH5CAM applied=1`. Acceptance is visible forward/back/left/right/up/down motion in cockpit/free-roam without sky flicker, culling breaks, or eye desync.

### Priority 5: Only after visual movement works, continue stereo correctness

After visible 6DOF works:

- validate IPD with `ipd=0`, then `ipd=0.032`;
- verify AER/source eye synchronization;
- verify shadows/chevrons move with the eye camera;
- add/validate upstream asymmetric projection;
- handle TAA history per eye;
- continue HUD quad work.

## Known Good Commands

Build and deploy:

```powershell
cmake --build E:\Github\vrframework\build-fh5 --config Release --target FH5VR
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Deploy-FH5VR.ps1 -SimRuntime
```

Launch and navigate to cockpit/free-roam:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Launch-FH5VR.ps1 -SimRuntime -TimeoutSec 180 -NavigateTimeoutSec 300
```

Monitor state only:

```powershell
Get-Content E:\tmp\fh5_state.txt
Select-String -Path E:\Games\ForzaHorizon5Empress\FH5VR.log -Pattern '\[VR-POSE\]|\[FH5POSE\]|\[FH5CAM\]|\[VR-COPY\]' | Select-Object -Last 80
```

Drive simulator pose:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Recenter
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Forward 2
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1 -Back 2
powershell -NoProfile -ExecutionPolicy Bypass -File E:\Github\vrframework\scripts\Set-FH5SimPose.ps1
```

## Current Bottom Line

We can start the game and get into cockpit/free-roam automatically. We can drive OpenXR Simulator poses into the mod, and the current upstream writer logs show those offsets being applied. But the user does not see movement in the rendered view, so the current upstream writer is not sufficient.

The next fix should be a focused translation-source investigation, not more menu automation and not a return to downstream cbuffer-only patching.
