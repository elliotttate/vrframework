# FH5VR current stereo audit and fix plan

Scope: offline audit from current `E:\Github\vrframework` source, existing FH5 probe reports, and the user's screenshot. FH5 was not launched and no implementation files were edited.

## Current symptom

The left and right eyes are not just seeing normal stereo disparity. The car/camera appear to be in materially different positions between eyes, while the car shadow remains close to the middle/mono position. This points to an inconsistent stereo application across passes, not a single missing view transform.

## Current implementation state

- Upstream producer hook is `sub_140BB1EE0`, with a main-camera gate on near `~0.1` and far `>2000`. It applies rotation into `a4` and stamps `g_fh5_applied_eye`. See `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:147-186`.
- Translation/IPD is now downstream in the 6912-byte camera cbuffer hook. `Fh5Adapter::apply_stereo()` publishes `fh5cb::set_eye_offset(...)`; `Fh5CameraCbuffer.cpp` applies that offset to `VIEW@0`, `VP@0x40`, `camRelVP@0x100`, and duplicate `0xC40`. See `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:303-316` and `E:\Github\vrframework\src\fh5vr\Fh5CameraCbuffer.cpp:91-127`.
- The cbuffer hook intentionally only accepts main-camera 6912 CBVs: near `~0.1`, far `>2000`, `posW~1`, valid view matrices. See `E:\Github\vrframework\src\fh5vr\Fh5CameraCbuffer.cpp:73-82`.
- Auxiliary/shadow-like 6912 CBVs are known in RenderDoc with near `~1`, far `~1000`, `posW~2`, and are currently rejected. See `E:\SteamLibrary\steamapps\common\ForzaHorizon5\FH5CameraProbe\_agent_reports\renderdoc_audit\SUMMARY.md:51-66`.
- Per-eye projection rewrite is still disabled: `s.write_proj = false`. See `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:326-332`.

## Most likely causes

### 1. IPD/world scale is probably too large

`Fh5Adapter.cpp` hardcodes:

```cpp
constexpr float FH5_HALF_IPD_UNITS = 3.15f;
```

at `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp:312`, assuming about `100 FH5 units / meter`. The shopping-list audit did not prove that scale; it explicitly left world-units scale live-required. If FH5 view units are closer to meters, half-IPD should be about `0.032`, not `3.15`. The screenshot looks like meter-scale separation, not centimeter-scale stereo.

### 2. Shadow/contact-shadow passes are currently mono or history-contaminated

The current cbuffer transform skips auxiliary/secondary 6912 CBVs. Existing captures show aux examples near `1`, far `1000`, `posW 2`. Those may include shadow, cascade, contact-shadow, screen-space shadow, or other secondary views. The old FH5 status doc also flags shadow/cascade and TAA/prev matrices as unresolved. That maps directly to a car body shifted by stereo while the shadow stays in the old center/mono position.

### 3. Eye-copy cadence still needs proof

`D3D12Component.cpp` now copies using `vr->get_current_render_eye()` rather than `g_fh5_applied_eye`. See `E:\Github\vrframework\src\mods\vr\D3D12Component.cpp:165-188`. The comment says these agree during gameplay, but that needs one log proof. If the producer-applied eye and copied eye ever diverge, one swapchain receives the wrong rendered eye.

### 4. Matrix target/math is still not locked by per-draw proof

The stereo cbuffer spec itself states that world/cockpit/HUD field usage is inferred, not proven by shader-reflection/pixel history. It recommends shifting `camRelVP@0x100` for world parallax and also shifting `VP@0x40`/`VIEW@0` for cockpit/car fusion. The current code does all of those at once, so if one path is wrong we cannot tell which field caused the split.

### 5. Projection is still mono

Leaving `a7` untouched is acceptable for bring-up, but it is not final stereo. Once view/IPD magnitude is sane, per-eye asymmetric projection must be enabled and matched to FH5's reverse-Z row-vector `a7` layout.

## Fix plan

### Phase 0 - Add discriminating toggles/logging before changing behavior

Add runtime toggles/env or config flags:

- `FH5VR_IPD_UNITS` or expose half-IPD units separately from `m_ipd_scale`.
- `FH5VR_CBUF_MODE=off|view_vp_only|camrel_only|all`.
- `FH5VR_AUX_CBUF_MODE=off|log_only|shift`.
- `FH5VR_DISABLE_TAA=0/1` once the TAA setting path is wired, or at least force a game-side TAA-off setting for tests.

Add log fields:

```text
[FH5] producer main eye=<producer_eye>
[FH5CB] write eye=<offset_eye> near=<near> far=<far> posW=<posW> ptr=<slot> mode=<main|aux> off=<x,y,z>
[VR-COPY] present=<n> render_eye=<copy_eye> producer_eye=<g_fh5_applied_eye> bbIdx=<idx>
```

Acceptance for Phase 0: a short run proves each present has one coherent copied eye, and every main-camera cbuffer write in that present uses the same eye/offset.

### Phase 1 - Fix IPD magnitude first

Set default half-IPD to a conservative meter-scale value:

- Test `0.0` first: left/right should become visually identical except temporal movement. Shadow and car should align.
- Test `0.032`.
- Test `0.10`.
- Only test `3.15` as the current exaggerated baseline.

If `0.032` looks sane, remove the `100 units/meter` assumption and make the units slider explicit. If none look sane, the cbuffer transform math is wrong and Phase 2 becomes the blocker.

Acceptance: with small IPD, car disparity is small and depth-consistent; the car is not meters apart between eyes.

### Phase 2 - Isolate which cbuffer fields move which surfaces

Use the new cbuffer mode toggle:

- `camrel_only`: should move world/environment parallax.
- `view_vp_only`: should move car/cockpit/local-object path if the report's inference is right.
- `all`: should fuse car and world.

If `view_vp_only` moves the car but `camrel_only` moves the ground/world, then the current "all" strategy is conceptually right and only scale/history/shadow passes remain. If one mode produces the split, fix the matrix multiplication/sign in that path before touching shadows.

Acceptance: car body, road, and festival geometry agree on the same eye offset at a sane IPD.

### Phase 3 - Fix centered shadow

Do not blindly shift all shadow/cascade cameras. First classify:

1. Run with IPD `0`: shadow must align.
2. Run with small IPD and `AUX_CBUF_MODE=log_only`: record aux 6912 CBVs around the visible car/shadow draw.
3. Use RenderDoc pixel history on a dark car-shadow pixel and a car-body pixel. Record EIDs, root signatures, CBV near/far/posW, and whether the draw samples main or aux cbuffer.

Then choose:

- If the visible shadow receiver/contact-shadow draw uses an aux camera CBV, shift that receiver pass with the same eye offset.
- If it is a temporal/screen-space shadow artifact, disable TAA/contact-shadow temporarily or wire per-eye history.
- If it is a true light-space shadow-map generation pass, do not shift the light camera; instead make sure the main receiver pass is stereo-correct.

Acceptance: at a sane IPD, the shadow remains under the car in both eyes.

### Phase 4 - TAA/history containment

AFR makes the previous frame the other eye. The FH5 status doc already flags prev-frame matrices in the 6912 cbuffer as unresolved. Until per-eye history is implemented, disable TAA/motion blur for validation. Then decide whether to:

- keep TAA disabled for first playable build;
- double-buffer previous matrices/resources per eye;
- or patch the 6912 prev-matrix block once it is mapped.

Acceptance: no centered ghost/shadow from the prior eye, no car smear between eyes.

### Phase 5 - Enable calibrated projection

After the view/cbuffer path is stable:

1. Dump full `a7` rows at rest.
2. Enable `s.write_proj` with the transposed OpenXR projection.
3. Validate sign/layout against SimXR and in-game screenshot.
4. Add TAA jitter only after projection is stable.

Acceptance: per-eye frusta match the OpenXR compositor; no off-center stretch, no toe-in look.

## Immediate next patch I would make

Make the mod diagnosable and reduce the default IPD:

1. Replace `FH5_HALF_IPD_UNITS = 3.15f` with a configurable default starting at `0.032f`.
2. Add the `FH5VR_CBUF_MODE` split (`off`, `camrel_only`, `view_vp_only`, `all`).
3. Log producer eye, copy eye, cbuffer eye, cbuffer class, and offset.

This is the smallest change that can tell whether the screenshot is primarily exaggerated IPD, eye-copy desync, wrong matrix target, or missing shadow/auxiliary pass handling.
