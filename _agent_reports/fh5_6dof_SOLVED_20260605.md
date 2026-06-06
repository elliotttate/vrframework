# FH5 VR — Upstream 6DOF Head Translation SOLVED (2026-06-05)

Status: **FIXED and verified live.** OpenXR head translation now visibly moves the rendered camera,
upstream of rebasing, so culling/shadows follow. Reversible. Both the manual control lever and a real
OpenXR (Simulator) head pose were confirmed on the Empress build with screenshots.

## The fix (the lever)

The per-frame view producer `sub_140BB1EE0` (Empress RVA `0xBB1EE0`) reads the camera **WORLD position**
as **double precision** from two pointer args:
- `a15` — its bits ARE a pointer → `f64[4]` (x,y,z,w)
- `a16` — a pointer → `f64[4]`

It splits those into the camera-relative rebasing origin written to `cameraPos@+0x80` of the 6912-byte
render cbuffer. ALL geometry/culling/shadows rebase against this. So the translation lever is:

**In the producer detour (`Hook_Producer`, Fh5Adapter.cpp), when engaged + `poslane=proda15`:**
1. compute the world offset = camera-local head offset (right/up/fwd) rotated by the camera basis (Mp rows),
2. add it to `a15[0..2]` and `a16[0..2]` (SEH-guarded),
3. call the trampoline,
4. restore the originals (engine re-derives the source each frame → compose-on-top, no accumulation).

Runs on the producer/sim thread → race-free. The camera-local offset comes from
`fh5cam::current_local_offset()` = manual control (`tgt=driver` + `fwd/strafe/up`) **plus** the published
OpenXR head/IPD pose. Rotation stays on the proven `a4` path (the mod routes `rot=driver`→producer-a4 when
`poslane=proda15`; pitch has no settable lane so rotation must be `a4`).

## What was WRONG (ruled out live — do not revisit)

- **CCamDriver `+0x320` matrix write** — a DERIVED output. Writing it logs `applied=1` but moves nothing.
  This was the old deployed path and the reason "OpenXR movement published but not visible." (Same wall FH6
  vrmod-private hit: 120+ correct matrix writes, zero visual.)
- **CCamDriver `+0x540` input lane** (4×f32 "CameraSpaceXYZOffset"/CamShake, consumed by fold
  `sub_1407A6300`) — proven INERT for camera position live: writing `f540.z=-300` changed neither `+0x320`
  nor the f64 `+0x550`. It feeds a different subsystem. (The `input540` poslane + the fold hook on
  `sub_1407A6300` remain in the code but are not the camera-position lever.)
- **`a4.row3`, `a17`, `a18`** — post-rebasing, inert (older "a4.row3 is the lever" notes are superseded).
- **`+0x550` getter-shadow** (vtable swap) — caused sky/culling flicker; fragile. (The f64 campos *does*
  live at CCamDriver `+0x550..+0x568`; getter = CCamDriver vtbl slot[12] `sub_1407A3C80` RVA 0x7A3C80 —
  but offset it via the producer `a15`, not by shadowing the getter.)

## Live proof (Empress, OpenXR Simulator, Horizon Festival México cockpit)

- Baseline: cockpit interior (wheel/dash/hands).
- `poslane=proda15 tgt=driver fwd=10` → camera flew forward out of the car (3rd-person). `tgt=off` → back to cockpit.
- `scale=5` + `Set-FH5SimPose -Forward 2` → `[FH5POSE] headT(units)=[0,0,10]` → clean 3rd-person chase, full
  scene/crowd/car visible, **no culling holes**. Neutral pose → back to cockpit.

## Code changes (uncommitted WIP in E:\Github\vrframework)

- `Fh5CameraCbuffer.hpp/.cpp` — added `kPosLaneProducerA15 = 9`; parse `poslane=proda15`/`a15`; name;
  default `g_ctl_pos_lane = kPosLaneProducerA15`.
- `Fh5CamDriver.hpp/.cpp` — added `fh5cam::current_local_offset()` (manual + OpenXR, ungated); worker
  no-ops the write for `proda15` (producer hook owns it).
- `Fh5Adapter.cpp` — `Hook_Producer` offsets/restores `a15`/`a16` f64 for `proda15`; `rot=driver`→producer-a4
  for `proda15`/`input540`; added a prologue sanity-check to the raw-RVA `+0x540` fold-hook install.
- `scripts/Launch-FH5VR.ps1` — `-PosLane` default → `proda15`.

## Remaining polish (not blockers)

1. **Sign/axis calibration** per axis (a `−1` Z-flip basis term exists; sim Forward produced a chase view —
   may want forward = into-scene). Verify right/up map naturally for a seated headset.
2. **World scale** — `scale` in `E:\tmp\fh5vr_ctl.txt`. ~1 unit ≈ sub-meter→meter; pick the value that gives
   1:1 head tracking on the real headset (`scale=1` is the conservative baseline now deployed).
3. **Per-eye IPD** — was `ipd=0` during the proof. Enable (`ipd=0.032`, deployed) and verify stereo separation;
   IPD already rides `current_local_offset` via the published per-eye pose.
4. **Stability** — recurring `nvwgf2umx.dll` `0xc00000fd` (stack overflow) crash is PRE-EXISTING (seen across
   sessions, unrelated to this lever); worth a separate pass (likely the D3D12 eye-copy / SimXR path).
5. Validate shadows/chevrons/TAA explicitly with a nonzero offset capture (expected coherent since upstream).
