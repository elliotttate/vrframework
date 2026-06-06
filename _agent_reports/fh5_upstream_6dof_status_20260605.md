# FH5 VR Upstream 6DOF Status - 2026-06-05

## Goal

Move the FH5 VR mod from the current half-upstream / half-downstream stereo path to true upstream 6DOF camera-pose injection.

The working target is the same architecture used by REFramework, starfield2vr, and anvilengine2vr:

- inject the per-eye view pose before the engine derives render state;
- inject per-eye asymmetric projection at the same upstream level;
- let shadows, chevrons, culling, motion vectors, TAA history, and render geometry all consume the same eye camera.

The current downstream cbuffer rewrite proves the stereo math and OpenXR copy path, but it happens too late. It can move visible car/world geometry while derived systems still use the mono camera. That explains the current symptoms: different car/camera position between eyes, centered car shadow, chevrons/HUD-style world markers not matching the visible eye, and likely TAA/motion-vector mismatch.

## Active Paths

- Mod repo: `E:\Github\vrframework`
- Game target: `E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe`
- Runtime log: `E:\Games\ForzaHorizon5Empress\FH5VR.log`
- Control file: `E:\tmp\fh5vr_ctl.txt`
- Current upstream test target: `tgt=driver`, control target value `5`

Current neutral control-file state:

```text
ipd=0
scale=100
mode=off
tgt=off
fwd=0
strafe=0
up=0
```

## Current Code Areas

Files currently changed for this line of work:

- `E:\Github\vrframework\src\fh5vr\Fh5CamDriver.cpp`
- `E:\Github\vrframework\src\fh5vr\Fh5CamDriver.hpp`
- `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp`
- `E:\Github\vrframework\src\fh5vr\Fh5CameraCbuffer.cpp`
- `E:\Github\vrframework\scripts\Launch-FH5VR.ps1`

Git diff size at report time:

```text
scripts/Launch-FH5VR.ps1       |   6 +-
src/fh5vr/Fh5Adapter.cpp       | 111 +++++---
src/fh5vr/Fh5CamDriver.cpp     | 594 ++++++++++++++++++++++++++++++++++++-----
src/fh5vr/Fh5CamDriver.hpp     |  35 ++-
src/fh5vr/Fh5CameraCbuffer.cpp |   8 +-
```

This report is a new file under `_agent_reports` and does not revert or overwrite those active edits.

## What Is Proven

### Producer Hook Is Alive

The main view/projection producer is still the proven live hook:

- Function: `sub_140BB1EE0`
- Empress RVA: `0xBB1EE0`
- Current mod hook name: `fh5_view_projection_producer`

It is firing continuously and the main-camera gate works:

```text
[11:32:48.449] [info] [FH5] producer fired: calls=68501 mainHits=29752 | near(a8)=0.1000 far(a9)=5000.0 is_main=true
[11:32:48.533] [info] [FH5] producer fired: calls=69001 mainHits=29970 | near(a8)=0.0100 far(a9)=1000.0 is_main=false
[11:32:48.610] [info] [FH5] producer fired: calls=69501 mainHits=30190 | near(a8)=0.1000 far(a9)=5000.0 is_main=true
```

The producer is still useful for:

- main-camera identification;
- rotation injection;
- pose-basis hints for offline/dynamic pointer resolution;
- future upstream projection rewrite through `a7`.

### Producer Position-Looking Args Are Not the Translation Lever

Large live offset tests on the producer args did not move the rendered camera:

- `a4.row3` with a large forward offset produced no visible camera movement.
- `a17` with a large forward offset produced no visible camera movement.
- `a18` was also tested as the remaining position-looking producer arg and did not become the practical upstream translation path.

Conclusion: `sub_140BB1EE0` is upstream relative to the final GPU cbuffer, but not upstream enough for camera translation/origin. The visible render is camera-relative; writing producer position-looking fields can cancel out or be ignored by the real upstream camera-origin path.

Older notes that described `a4.row3` as the visible translation lever should be treated as superseded by these live tests.

### CCamDriver +0x320 / +0x360 Is Still the Best Upstream Pose Lane

Prior standalone freecam work proved that writing the active camera object's:

- `object + 0x320`: row-major camera-to-world matrix;
- `object + 0x360`: inverse view-tail derived from that matrix;

can move the FH5 camera coherently.

Important correction:

- Bad offset: `+0x3E0`
- Proven offset: `+0x360`

The current mod was corrected to write `+0x360`, not `+0x3E0`.

## Current Empress Build Problem

The deterministic CCamDriver and ForzaMultiCam bridge hooks identified from Steam 1.688 / older reports do not match the current Empress 1.405 executable.

Current live log:

```text
[11:31:49.490] [error] FuncRelocation 'fh5_ccamdriver_publish': pattern not found
[11:31:49.490] [error] FuncRelocation 'fh5_ccamdriver_publish': no fallback offset provided
[11:31:49.490] [warning] [FH5] CCamDriver AOB not found
[11:31:49.634] [error] FuncRelocation 'fh5_forzamulticam_state_bridge': pattern not found
[11:31:49.635] [error] FuncRelocation 'fh5_forzamulticam_state_bridge': no fallback offset provided
[11:31:49.635] [warning] [FH5] ForzaMultiCam bridge AOB not found
```

The stale fallback RVAs were unsafe:

- `0x6BE3A0`: CCamDriver hook fallback from the other build;
- `0x746BB0`: ForzaMultiCam bridge fallback from the other build.

Those fallbacks are now set to `0`, so the mod does not hook unrelated Empress code. This was the correct safety fix. Only the producer fallback `0xBB1EE0` is currently safe on the tested Empress build.

Current stable startup proof:

```text
[11:31:49.635] [info] [FH5] engine seam live -> enable_engine_thread()
[11:31:49.635] [info] [FH5CAM] upstream CCamDriver position writer thread up (hook-published driver/FMC pointer; idle until tgt=driver)
[11:31:49.703] [info] [FH5CTL] control-file poller started
[11:31:49.703] [info] [FH5CTL] ipd=0.000 scale=100.0 mode=0 | UPSTREAM tgt=0 fwd=0.0 strafe=0.0 up=0.0
```

## What We Changed

### Added Upstream Camera Driver Worker

`Fh5CamDriver` now has a worker thread that idles until:

```text
tgt=driver
```

or equivalently:

```cpp
fh5cb::ctl_up_tgt() == 5
```

When it has a camera object, it applies:

- `fwd`
- `strafe`
- `up`

to the camera object's own basis vectors and writes a modified camera pose to `object + 0x320`, plus recomputed inverse view-tail to `object + 0x360`.

The worker now keeps a clean source pose so repeated writes are idempotent. If the engine has not refreshed `+0x320` since our last write, it recomputes from the stored source instead of accumulating offset on top of offset.

### Added Pointer Publish APIs

The camera worker can now receive camera pointers through several paths:

```cpp
void publish_driver(uintptr_t object);
void publish_multicam(uintptr_t object);
void publish_pose_hint(const float* matrix16);
void publish_candidate_pointer(uintptr_t value, const float* matrix16);
```

Current hook callsites:

- `publish_pose_hint(a4)` is enabled from the main producer hook.
- `publish_driver(a1)` is available in the CCamDriver hook, but that hook is not installed on Empress because its AOB misses.
- `publish_multicam([a1+0x198])` is available in the bridge hook, but that hook is not installed on Empress because its AOB misses.
- `publish_candidate_pointer(...)` callsites exist but are disabled with `#if 0`.

### Disabled Unsafe Producer Arg Pointer Probe

A producer-argument pointer probe was added to test whether one of the producer arguments directly exposed:

- a CCamDriver object;
- a shared-pointer control block;
- a ForzaMultiCam object;
- a wrapper containing one.

That callsite is currently disabled with `#if 0`.

Reason: repeated startup/menu crashes occurred after the first non-main producer/menu render. Windows Event Log showed:

```text
Faulting module: nvwgf2umx.dll
Fault offset: 0x00000000000f3972
```

No `[FH5CAM] producer arg captured ...` log appeared before those crashes. That means the probe was destabilizing the menu/startup path before it produced a useful pointer. It should only return later as a gated probe after the main gameplay camera is definitely live.

### Fixed Control Poller Startup Logging

`Fh5CameraCbuffer.cpp` now logs when the control poller starts and resets `g_ctl_started` if `CreateThread` fails.

Evidence:

```text
[11:31:49.703] [info] [FH5CTL] control-file poller started
[11:31:49.703] [info] [FH5CTL] ipd=0.000 scale=100.0 mode=0 | UPSTREAM tgt=0 fwd=0.0 strafe=0.0 up=0.0
```

Earlier, lack of `[FH5CTL]` lines made it ambiguous whether upstream control input was being parsed. That ambiguity is gone.

### Corrected Launch Readiness Detection

`Launch-FH5VR.ps1` now treats the producer as live only when it sees:

```text
is_main=true
```

or:

```text
[FH5] main=
```

This avoids the old false-positive where any `producer fired` line, including a menu/non-main producer with `near=0 far=1`, could mark the game as ready.

Current caveat: the script contains a single Escape press after producer readiness, but the user observed it can still miss entering free-roam. The next launch fix should retry or verify the Escape/free-roam transition instead of sending one blind Escape.

## Offline RE Updates

Current Empress RTTI/vtable values were derived offline from the executable and added to the worker.

Refcount vtables:

```text
ForzaMultiCam refcount vtable: 0x1465D4990
CCamDriver refcount vtable:    0x145E1FF90
```

Concrete camera vtables:

```text
CCamFollowLow       0x145E3FFC0
CCamFollowHigh      0x145E40308
CCamDriver          0x145E3F290
CCamHood            0x145E3EBF0
CCamBumperHigh      0x145E3E550
CCamFree            0x145E40D78
CCamFreeTargetCar   0x145E41208
CCamFreeTrack       0x145E415B8
CCamFollowExtended  0x145E40650
```

ForzaMultiCam concrete vtables:

```text
0x1465D6808
0x1465D6BA0
```

These are Empress 1.405 constants, not Steam 1.688 constants.

## Shape Resolver Status

The worker has a fallback shape resolver. It scans writable private memory for a 4x4 camera-to-world matrix whose basis matches the producer's live `a4` pose hint, then treats:

```text
matrix_address - 0x320
```

as a candidate camera object.

Previous hits:

```text
[FH5CAM] shape resolver hit object=0x2D359FE60 score=3.744 checked=5173495 basis=1860 shape=6
[FH5CAM] captured active camera object=0x2D359FE60 source=driver driver_published=1 fmc=0x0 fmc_published=0
[FH5CAM] camera=0x2D359FE60 driver_published=1 fmc_published=0 pos=(-299.268,-3.067,-20.618) tgt=5 off=(0.000,0.000,300.000) applied=0
```

Later shape-resolver work also found `0x49B94CE50`.

Problems:

- The scan takes tens of seconds.
- It is broad and noisy.
- It can find stale or non-active camera-like copies.
- It is not good enough as the final active-camera path.

A stale guard that compared producer row3 position against CCamDriver position was removed. That guard falsely rejected plausible candidates because producer row3 can be camera-relative and differ by kilometers from CCamDriver's world position. Basis matching is the safer discriminator than absolute position matching.

Current conclusion: the shape resolver is useful as a diagnostic fallback, but not the final solution.

## Current Blocker

The upstream write path compiles and starts, but it still lacks a deterministic active camera pointer on the current Empress build.

At present:

- producer hook: installed and firing;
- control poller: started and parsing;
- upstream worker: started and idle until `tgt=driver`;
- CCamDriver hook: not installed because AOB misses;
- ForzaMultiCam bridge hook: not installed because AOB misses;
- shape scan: available but too slow/noisy for a stable 6DOF path;
- producer arg pointer probe: disabled because it caused NVIDIA driver crashes during startup/menu.

Until the live active CCamDriver pointer is resolved quickly and deterministically, upstream 6DOF camera control cannot be accepted.

## Planned Next Actions

### 1. Stabilize Launch And Free-Roam Entry

Fix `Launch-FH5VR.ps1` so it does not just send one Escape after producer readiness.

Recommended behavior:

- wait for main-camera producer readiness;
- send Escape only to the real game window;
- retry Escape a small number of times if the free-roam prompt remains;
- verify with screenshot/window state or a stable gameplay-camera condition before returning `RESULT=READY`.

This matters because upstream camera tests are invalid if the car/player is moved by menu state, showcase prompts, or other traffic during setup.

### 2. Replace Broad Shape Scan With Targeted RTTI/Vtable Active-Slot Resolver

Use the current Empress vtables to build a targeted resolver:

1. Scan writable private memory for current Empress ForzaMultiCam concrete/refcount vtables.
2. Validate candidate ForzaMultiCam objects.
3. Read active camera shared pointer at `ForzaMultiCam + 0x5C8`.
4. Validate the active camera object against known camera vtables.
5. Validate `object + 0x320` basis against the producer pose hint.
6. Validate `object + 0x360` view-tail consistency.

This should be much faster and less noisy than scanning for every possible 4x4 matrix in process memory.

### 3. Reintroduce Producer Pointer Probe Only After Main Camera Is Live

If targeted RTTI/vtable scanning still misses, re-enable the producer candidate pointer API in a safer form:

- only after `is_main=true` has been observed for a stable number of frames;
- only while `tgt=driver` is engaged;
- only probe a few selected args per frame, not every pointer-looking arg every producer call;
- add rate-limited logs and hard safety checks;
- stop probing after first accepted active camera object.

The prior crash came from probing too early and too broadly during startup/menu.

### 4. Verify Upstream Camera Control With Control-File Offsets

Once the active camera pointer is deterministic, run controlled visual/log tests:

```text
tgt=driver
fwd=100
strafe=0
up=0
```

Then:

```text
tgt=driver
fwd=0
strafe=100
up=0
```

Then:

```text
tgt=driver
fwd=0
strafe=0
up=100
```

Expected log shape:

```text
[FH5CAM] captured active camera object=0x... source=...
[FH5CAM] camera=0x... pos=(...) tgt=5 off=(...) applied=1 source=engine
```

Expected visual result:

- view moves in the requested local camera direction;
- disabling `tgt=driver` restores the original source pose;
- car/player location changes caused by the world should not be mistaken for camera-control success.

### 5. Feed OpenXR 6DOF Translation Upstream

After manual `fwd/strafe/up` works upstream, route OpenXR head translation into the same upstream writer.

This replaces the downstream translation path that currently shifts final camera cbuffers after derived data has already been computed.

Acceptance criteria:

- moving head forward/back/left/right/up/down changes the upstream camera pose;
- motion axes are correct and not cross-mapped;
- shadows and chevrons follow the same eye pose as visible geometry.

### 6. Add Per-Eye Upstream Projection

Use the producer projection argument (`a7`) to inject the runtime's asymmetric per-eye projection upstream.

Known state:

- `a7` is the high-value upstream projection arg;
- `a8/a9` are near/far indicators;
- full projection field map still needs final live confirmation if we change more than the already-understood rows.

Projection should be solved after the active camera pointer is deterministic, because projection alone cannot fix the centered-shadow/chevron mismatch.

### 7. Handle TAA History And HUD Quad Later

Once upstream view translation and projection are coherent:

- split or swap TAA history per eye;
- handle motion-vector consistency;
- move HUD/UI to a VR quad instead of leaving it as flat SBS/game-screen UI.

These are important, but they come after the upstream camera pointer and pose writer are stable.

## Acceptance Criteria For This Stage

Do not call the upstream 6DOF stage fixed until all of these are true:

- The active CCamDriver pointer resolves deterministically on Empress without a broad tens-of-seconds matrix scan.
- `tgt=driver` manual offsets move the camera in all three local axes.
- Disabling `tgt=driver` restores the source pose without accumulated drift.
- The car shadow and world chevrons move consistently with the visible eye camera.
- OpenXR head translation drives the upstream writer, not only the downstream cbuffer path.
- Left and right eye car/camera positions are coherent at `ipd=0` and correctly separated at nonzero IPD.

## Practical Conclusion

The architecture conclusion is unchanged: downstream cbuffer patching is the wrong final layer for FH5 6DOF stereo. The correct path is upstream pose and projection injection, matching the proven production VR mods.

The immediate engineering blocker is not stereo math. It is finding the live active camera object reliably in the current Empress 1.405 build. The next highest-value fix is a targeted ForzaMultiCam/CCamDriver RTTI-vtable resolver using the current Empress vtables, with the broad shape scan demoted to diagnostic fallback.

## 2026-06-05 Live Update: Empress Refcount Resolver Works

The Steam-derived addresses were rechecked with IDA Pro headless against the current Empress database:

```text
E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe.i64
sha256=c6024a3ed46bc2a2b08a5f079c3ea72dfc51ee397ab5beb2ab36459937f11714
```

That invalidated the old `0x1467F...` / `0x1406B...` Steam constants for the current executable. The current mod now uses exact Empress camera refcount vtables around `0x145E1FF40..0x145E20148` and exact concrete camera tables around `0x145E3E550..0x145E415B8`.

The deterministic path that worked live is:

1. scan writable private memory for exact camera `std::_Ref_count_obj2<Camera::...>` vtable values;
2. treat `control + 0x10` as the embedded camera object;
3. validate the object by a sane module vptr, a decodable `object + 0x320` camera-to-world matrix, and basis match against the live producer pose hint;
4. write the upstream pose to `object + 0x320` and the derived inverse-view tail to `object + 0x360`.

Live proof from `E:\Games\ForzaHorizon5Empress\FH5VR.log`:

```text
[FH5CAM] refcount resolver hit object=0x5364189E0 control=0x5364189D0 refcount_ida=0x145E1FF40 vtable=0x145E3FFC0 score=2.931 ...
[FH5CAM] camera=0x5364189E0 ... pos=(-403.818,192.414,5079.074) tgt=5 off=(0.000,0.000,100.000) applied=1 source=engine
[FH5CAM] camera=0x5364189E0 ... pos=(-303.818,197.064,5128.858) tgt=5 off=(50.000,0.000,0.000) applied=1 source=cached
[FH5CAM] camera=0x5364189E0 ... pos=(-303.818,242.197,5074.424) tgt=5 off=(0.000,50.000,0.000) applied=1 source=cached
[FH5CAM] restored source camera pose on driver disable object=0x5364189E0
```

The launcher issue was also confirmed: producer readiness can occur while FH5 is still in the garage menu. `Launch-FH5VR.ps1` now sends the visible `Space = Drive` command, waits for the transition to settle, and does **not** press Escape by default. Escape opens/closes the pause/menu flow once free-roam is already live, so it is now only available through the explicit `-DismissWithEscape` switch.

Remaining work after this update:

- make the refcount scan faster by narrowing memory ranges or caching candidate control blocks;
- route OpenXR head translation into this upstream writer;
- add per-eye upstream projection;
- verify shadow/chevron/TAA coherence with small per-eye offsets in free-roam.

## 2026-06-05 Live Update: OpenXR Translation Routed Upstream

The upstream writer now consumes the OpenXR head/IPD offset directly. The debug control file must keep `scale` nonzero; `scale=0` was the reason the OpenXR 6DOF path appeared inert during one live test.

Current launcher default:

```text
ipd=3.15
scale=100
mode=off
tgt=off
```

`mode=off` only disables the downstream camera-cbuffer fallback. OpenXR head translation and IPD still flow upstream through the CCamDriver writer.

Fresh-process proof from `E:\Games\ForzaHorizon5Empress\FH5VR.log`:

```text
[FH5CAM] camera=0x532CC06A0 ... pos=(3.150,0.000,0.000) tgt=0 manual=0 vr=1 eye=1 off=(3.150,0.000,0.000) applied=1
[FH5POSE] eye=1 headT(units)=[0.00 0.00 0.00] driverOff=[3.15 0.00 0.00] halfIPD=3.15
[FH5POSE] eye=1 headT(units)=[76.67 16.67 100.00] driverOff=[79.82 16.67 100.00] halfIPD=3.15
[FH5CAM] camera=0x532CC06A0 ... pos=(79.817,16.667,100.000) tgt=0 manual=0 vr=1 eye=1 off=(79.817,16.667,100.000) applied=1
```

A deterministic simulator-axis check with `scale=100` showed:

```text
right 1m -> headT=[100.00 0.00 0.00], driverOff=[100.00 0.00 0.00]
up    1m -> headT=[0.00 100.00 0.00], driverOff=[0.00 100.00 0.00]
fwd   1m -> headT=[0.00 0.00 100.00], driverOff=[0.00 0.00 100.00]
reset    -> headT=[0.00 0.00 0.00], driverOff=[0.00 0.00 0.00]
```

Remaining work after this update:

- verify the same translation behavior through the user's actual OpenXR device, not only the simulator file-command path;
- verify visual shadow/chevron coherence with small nonzero OpenXR offsets;
- add per-eye upstream projection;
- address TAA history and HUD quad work after pose/projection are coherent.

## 2026-06-05 Live Update: Getter Path Removed, Direct Writer Stable At Physical Scale

This supersedes the earlier `scale=100` / large-IPD bring-up values. The current deployed build uses physical-scale OpenXR offsets:

```text
ipd=0.032
scale=1
mode=off
tgt=off
```

Live testing showed the `CCamDriver +0x550` getter-shadow path is not a safe 6DOF lever. Even with `ipd=0`, a constant OpenXR height offset of only `0.300` through the getter flipped the cockpit into the sky/culling view. That reproduced the cockpit/sky flash without any per-eye IPD alternation, so the failure was the getter path itself, not just stereo timing.

The fix now deployed to `E:\Games\ForzaHorizon5Empress\dxgi.dll` routes OpenXR translation/IPD through the direct active-camera pose writer:

- restore/avoid the `+0x550` getter shadow for OpenXR;
- require the active object to be `CCamDriver`;
- write the shifted pose to `CCamDriver +0x320`;
- recompute and write the inverse-view tail at `CCamDriver +0x360`;
- keep OpenXR rotation on the producer `a4` path only, so rotation is not double-applied through the object writer;
- write only the active camera object for OpenXR (`clones=0`), avoiding shape-based clone writes during VR.

Live proof from `E:\Games\ForzaHorizon5Empress\FH5VR.log` after rebuilding/deploying:

```text
[FH5POSE] headT(units)=[0.000 0.300 0.000] driverOff=[0.000 0.300 0.000] halfIPD=0.000
[FH5CAM] camera=0x536588350 ... tgt=0 manual=0 vr=1 off=(0.000,0.300,0.000) applied=1 source=engine layout=ccam matrix=0x320 clones=0

[FH5POSE] headT(units)=[0.300 0.000 0.000] driverOff=[0.300 0.000 0.000] halfIPD=0.000
[FH5CAM] camera=0x536588350 ... tgt=0 manual=0 vr=1 off=(0.300,0.000,0.000) applied=1 source=cached layout=ccam matrix=0x320 clones=0

[FH5POSE] headT(units)=[0.000 0.000 0.300] driverOff=[0.000 0.000 0.300] halfIPD=0.000
[FH5CAM] camera=0x536588350 ... tgt=0 manual=0 vr=1 off=(0.000,0.000,0.300) applied=1 source=cached layout=ccam matrix=0x320 clones=0

[FH5POSE] headT(units)=[0.000 0.000 0.000] driverOff=[0.032 0.000 0.000] halfIPD=0.032
[FH5CAM] camera=0x536588350 ... tgt=0 manual=0 vr=1 eye=1 off=(0.032,0.000,0.000) applied=1 source=cached layout=ccam matrix=0x320 clones=0
[FH5CAM] camera=0x536588350 ... tgt=0 manual=0 vr=1 eye=0 off=(-0.032,0.000,0.000) applied=1 source=cached layout=ccam matrix=0x320 clones=0
```

Visual result: cockpit stayed stable during height, strafe, forward, and `ipd=0.032` tests. The old all-white / cockpit-sky flicker did not reproduce on the direct writer path.

Remaining work after this update:

- verify with the user's physical headset movement, not only simulator file-command poses;
- keep watching for eye-timing artifacts under real OpenXR, even though the simulator/IPD sweep was stable;
- add per-eye upstream projection;
- verify shadows/chevrons/TAA with nonzero IPD in gameplay captures;
- continue HUD quad work after view/projection are coherent.
