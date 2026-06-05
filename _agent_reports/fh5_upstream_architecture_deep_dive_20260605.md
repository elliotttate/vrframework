# FH5 upstream architecture deep dive

Scope: offline/source audit plus the user's live discriminator results from 2026-06-05. I did not launch FH5. This report answers what must move upstream to make FH5 match the architecture used by REFramework, starfield2vr, and anvilengine2vr.

## Bottom line

The current FH5 mod is not fully upstream. It is split across two different layers:

| Layer | Current FH5 mod | Result |
| --- | --- | --- |
| Head/view rotation | Mostly upstream through `sub_140BB1EE0` arg `a4` | Good enough to rotate the rendered/culling camera in current tests. |
| View translation, IPD, and 6DOF head translation | Downstream through the 6912-byte GPU camera cbuffer | Moves visible geometry, but derived systems stay mono or stale. |
| Per-eye projection | Built but disabled, `s.write_proj = false` | Still mono projection. |
| TAA/history | Not wired per eye | AFR history is still other-eye history. |

That explains the current symptoms: car/body geometry can move per eye while shadow/contact effects, chevrons, culling, motion vectors, or history still come from the mono camera. The downstream cbuffer patch is useful as a bring-up/debug lever, but it is not equivalent to the three working production mods because it lands after important engine derivation.

The needed architecture is: inject the full eye pose and per-eye projection at, or before, the engine camera-pose/final-view producer that feeds camera-relative rebasing, culling, route/chevron projection, shadow receiver state, motion vectors, and TAA previous matrices.

## Where the current code is split

### `sub_140BB1EE0` producer hook

Current source: `E:\Github\vrframework\src\fh5vr\Fh5Adapter.cpp`.

- The hook is installed for `fh5_view_projection_producer` with fallback RVA `0xBB1EE0` at `Fh5Adapter.cpp:275-287`.
- Main-camera gate is near `0.06..0.2` and far `>2000` at `Fh5Adapter.cpp:147-151`.
- The hook mutates `a4` before calling the original at `Fh5Adapter.cpp:194-247`.
- It can also mutate `a7` before the original at `Fh5Adapter.cpp:236-240`, but only if `s.write_proj` is true.
- The original function is called after the temporary mutations, then inputs are restored at `Fh5Adapter.cpp:247-254`.

This is upstream of the GPU 6912-byte cbuffer, but it is not necessarily upstream of the selected camera pose/origin. The new live tests strongly imply it is after camera-relative origin selection for translation.

### Downstream 6912-byte cbuffer hook

Current source: `E:\Github\vrframework\src\fh5vr\Fh5CameraCbuffer.cpp`.

- `Fh5Adapter::apply_stereo()` intentionally sends head translation and IPD into `fh5cb::set_eye_offset(...)` at `Fh5Adapter.cpp:290-352`.
- `Fh5CameraCbuffer.cpp` hooks D3D12 resource/CBV creation and resolves 6912-byte CBVs by GPU VA at `Fh5CameraCbuffer.cpp:204-315`.
- Main-camera identity gate is `near ~0.1`, `far >2000`, `posW ~1`, sane view matrix at `Fh5CameraCbuffer.cpp:139-149`.
- `TransformStereo()` mutates `VIEW@0`, `VP@0x40`, `camRelVP@0x100`, and duplicate `0xC40` at `Fh5CameraCbuffer.cpp:157-201`.

This is after camera-relative rebasing and after many engine systems have already consumed the mono pose. It can make the visible world/car move, but it cannot make all upstream derived data coherent.

## Live discriminator results

The user ran the control-file upstream offset test against the current `sub_140BB1EE0` hook:

- `tgt=a4`, `fwd=300`: no visible camera movement. Same car position and banner distance.
- `tgt=a17`, `fwd=1500`: no visible camera movement. Same car/banner; only crowd animation changed.
- `tgt=a18`, `fwd=1500`: command was applied; visual result not yet included in the pasted transcript.

Interpretation:

- `a4.row3` is confirmed not to be the visible camera-position lever. It cancels under FH5's camera-relative rendering.
- `a17` is confirmed not to be the visible camera-position lever.
- If `a18` also shows no movement, then all currently exposed position-looking args in `sub_140BB1EE0` are ruled out for upstream translation.
- Even if `a18` moves the visible view, it still needs a shadow/chevron/culling check before accepting it as the full upstream pose lever.

## Correct upstream model for FH5

There are at least three relevant layers:

1. **True camera pose producer / selected camera state.**
   - This is the layer the working mods target conceptually.
   - It must run before camera-relative origin, culling/frustum planes, shadow receiver state, route/chevron projection, motion vectors, and TAA history are derived.
   - FH5 candidates: the accessible camera builder/final-view path, `CCamDriver+0x320`, `sub_1406BE3A0`, and the `ForzaMultiCam` bridge.

2. **Render view/projection producer, `sub_140BB1EE0`.**
   - Proven useful for view rotation.
   - Now likely proven too late for world-position/origin translation.
   - Still a candidate for per-eye projection if `a7` is consumed before final view/projection constants are emitted.

3. **Renderer cbuffer, 6912-byte CBV.**
   - Proven useful for visible-pixel movement and freecam bring-up.
   - Structurally downstream of derived camera systems.
   - Cannot by itself fix centered shadows, chevrons, culling, motion vectors, or TAA history without chasing each derived pass separately.

The architectural fix is to move translation/IPD/head translation from layer 3 to layer 1. Ideally rotation moves there too, so one full eye pose is the source for all derived systems.

## True upstream candidates already mapped

### Candidate A: `sub_1406BE3A0` / `CCamDriver+0x320`

Evidence:

- `FH5CameraProbe\_agent_reports\upstream_offline\SUMMARY.md:27` ranks `sub_1406BE3A0` / `CCamDriver+0x320` as the best upstream-lane candidate for culling coherence.
- `SUMMARY.md:41-48` shows the function calls `loc_1405EAC60(a1 + 0x320)` then dispatches `vtable+24` from `a2+8`.
- AOB contract: `FH5CameraProbe\_agent_reports\upstream_offline\fh5_hook_aob_contracts.md:32-38`.

Why it matters:

- `+0x320` is the camera-driver matrix/state lane that prior reports associate with culling-coherent camera state.
- It is earlier than the 6912 CBV and likely earlier than the `ForzaMultiCam` bridge.

Risk:

- A direct post-write matrix poke may be too late if consumers already ran, or too early if a later builder/finalizer overwrites it. Runtime order must be proven.
- Do not assume the builder is inaccessible. Current working assumption is that the relevant builder/final-view path is accessible and should be inspected/probed directly.

### Candidate B: `sub_140746BB0` / inner `0x140746C6B` bridge

Evidence:

- `fh5_hook_aob_contracts.md:8-22` gives the prologue and inner bridge bytes.
- `sub_140746BB0` builds/finalizes a `0xC4..0x188` source block, then copies it into `*[a1+0x198] + 0x660`.
- The inner copy starts at `0x140746C6B`: load destination from `[rbx+0x198]`, add `0x660`, then copy 13 OWORDs plus one dword. See `sub_140746BB0_140746BB0.asm.txt:44-74`.

Why it matters:

- It is the clearest concrete place where a provider-produced camera state block is copied into `ForzaMultiCam+0x660`.
- It can be instrumented to correlate source pose, destination pose, and the later 6912-byte CBV in the same frame.

Risk:

- Prior reports mark it as a bridge/correlation point, probably later than the selected driver/culling camera.
- If culling/chevrons/shadow state are already derived before this copy, patching it may still not fix everything.

### Candidate C: accessible camera builder / final-view path

Evidence:

- The true target is the function or dispatch path that constructs/finalizes the selected camera pose before `CCamDriver+0x320`, the `ForzaMultiCam` bridge, camera-relative rebasing, and culling/frustum derivation.
- Existing field maps still give useful state lanes to correlate, including `+0x320`, `+0x540`, yaw-related fields around `+0x5C0`, roll/pitch-related fields around `+0x5F0..+0x5F8`, and position-ish lanes around `+0x550..+0x568`.
- Treat gateway/dispatch entries as instrumentation options only. They are not required because of code protection; they are useful only if they are the cleanest call boundary for the accessible builder/finalizer.

Why it matters:

- If the bridge is too late, the only way to be truly upstream is to hook the accessible builder/final-view path before it emits the pose consumed by culling and render derivation.
- This is closest to "hook the engine's camera pose producer" in the same architectural sense as the three working mods.

Risk:

- Hardest path. Field semantics and execution cadence still need runtime proof.
- Prior game-logic camera work failed for cockpit pitch, so direct semantic-field injection may not express all VR pose degrees cleanly.

## What must change in vrframework

### 1. Stop treating the 6912 CBV hook as final stereo architecture

Keep it as:

- a debug/bring-up lever;
- a fallback for visible-pixel-only experiments;
- a way to copy/log final current/previous camera constants;
- a short-term "make it move" patch.

Do not treat it as the final answer for shadows/chevrons/TAA. It is downstream by construction.

### 2. Add a narrow upstream pose-probe module

Add hooks or log-only probes for:

- `sub_1406BE3A0`;
- `0x140746C6B` / `sub_140746BB0` inner bridge;
- optionally `sub_1406B0C20` as readback only;
- then, if needed, the accessible camera builder/final-view function or its cleanest dispatch boundary.

Each probe should log, per main-camera frame:

- frame/eye stamp from vrframework;
- object pointers;
- source/destination matrix/state rows;
- camera position-like lanes;
- whether the later 6912 CBV sees matching position/view/projection;
- whether a test offset moves visible view, car shadow, and chevrons together.

This is not broad RE. It is the exact runtime correlation needed to find the single upstream pose point.

### 3. Use constant-offset tests at each candidate before implementing VR math

For each candidate, use a huge controlled offset first:

1. Disable downstream CBV offset: `ipd=0`, `mode=off`.
2. Apply a constant test offset at the candidate.
3. Observe:
   - visible camera/view moves;
   - car body moves coherently with road/world;
   - car shadow remains under the car;
   - route/chevrons stay aligned;
   - no culling holes.

Accept a candidate only if derived systems move with the camera. If only visible geometry moves, the candidate is still downstream.

### 4. Move full eye pose to the accepted upstream candidate

Once a candidate passes the constant-offset test:

- build one full eye pose per AFR frame from OpenXR:
  - same head orientation for both eyes;
  - full 6DOF head translation, recentered;
  - signed per-eye IPD along camera/head right;
  - correct FH5 units-per-meter;
- compose it onto the engine's selected camera pose before derivation;
- update any corresponding camera-position fields in the same state block;
- keep one stable main-camera identity gate by object/pointer if possible, not by matrix shape alone.

The final version should not split rotation and translation across layers. If layer 1 accepts both, remove rotation/IPD from the downstream CBV path and keep the CBV hook for diagnostics.

### 5. Enable calibrated per-eye projection upstream

`Fh5Adapter.cpp` already stages `a7`, but leaves `s.write_proj = false`.

Needed:

- validate FH5 reverse-Z row-vector `a7` layout against the current OpenXR projection;
- write asymmetric per-eye projection at the same upstream derivation level as pose, or keep `sub_140BB1EE0 a7` only if it is proven before final frustum/culling derivation;
- make sure culling/frustum planes use the same projection as the rendered eye.

If projection is written only after culling/frustum derivation, it will repeat the same class of bug as downstream translation.

### 6. Fix TAA/history per eye

Until this is done, TAA should be disabled for validation.

Final options:

- double-buffer previous view/projection/camera-relative matrices per eye and feed the correct previous-eye state on each AFR frame;
- identify and patch the previous/current history fields in the 6912-byte block once they are mapped;
- or keep TAA disabled for the first playable build.

The current `0xC40` duplicate is a current-frame `camRelVP` duplicate in existing captures, not proven previous history. It must not be assumed to solve TAA.

## Exact next work

1. Finish the `a18` visual discriminator.
   - If `a18 fwd=1500` moves nothing, `sub_140BB1EE0` is ruled out for translation.
   - If it moves view only, it is not enough; check shadow/chevrons.
   - If it moves view + shadow + chevrons, it becomes the top upstream translation lane.

2. Add a log-only/probe hook for `0x140746C6B`.
   - Dump source `rdi + 0x00..0xC0` and destination `*[rbx+0x198] + 0x660`.
   - Correlate with later 6912 CBV `cameraPos`, `VIEW`, `VP`, and `camRelVP`.
   - Add an optional huge offset test to the copied pose block only after dump correlation.

3. Add a log-only/probe hook for `sub_1406BE3A0`.
   - Capture `this = a1`, inspect `a1+0x320`, call timing, and downstream 6912 match.
   - Optional test: mutate `a1+0x320` before or after original for one frame and observe whether derived systems follow.

4. If both bridge and `sub_1406BE3A0` fail to move derived systems, escalate to the accessible camera builder/final-view path directly. Use gateway/dispatch boundaries only if they are the cleanest way to intercept that path.

5. Only after an upstream position lever passes the derived-system test, port the current VR pose math there and retire downstream IPD as the primary path.

## Conclusion

The pasted assessment is directionally right, with one correction: `sub_140BB1EE0` is upstream relative to the 6912 GPU cbuffer, but it is not necessarily upstream enough for translation/origin. Your `a4` and `a17` tests strongly confirm that distinction.

The current problem is therefore an architecture-layer mismatch, not just a bad IPD scale. To match the three working mods, FH5 needs one coherent upstream pose/projection injection before derived data is built. The best known path is a narrow runtime correlation between `CCamDriver+0x320` / `sub_1406BE3A0`, the `sub_140746BB0` bridge at `0x140746C6B`, and the final 6912-byte CBV. Broad IDA is not the next step; targeted probe hooks are.
