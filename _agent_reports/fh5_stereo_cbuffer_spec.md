# FH5 Stereo Camera-Constant-Buffer Spec (per-eye IPD)

Scope: READ-ONLY reverse-engineering synthesis. FH5 was not launched for this document.
Goal: give an implementable spec for producing a per-eye lateral (IPD) offset on the main
world camera, inside the existing dxgi-proxy mod, that does NOT cancel under FH5's
camera-relative rendering.

Sources cited inline as `file:line`. Primary inputs:
- `E:\SteamLibrary\steamapps\common\ForzaHorizon5\FH5CameraProbe\src\DxgiProxy.cpp` (proven downstream transform + ring tracking)
- `_agent_reports\renderdoc_audit\ForzaCockpit_6912_field_map.md` and `..._matrix_offsets.csv` and `..._cbv_classification.csv` (ground-truth layout)
- `_agent_reports\renderdoc_audit\ForzaLookingRightCockpit_6912_field_map.md`, `ForzaChaseCam_6912_field_map.md` (cross-capture confirmation)
- `_agent_reports\upstream_offline\SUMMARY.md` (consolidated 6912 field map + bridge function)
- `docs\A7_PROJECTION_FIELD_MAP.md` (producer arg roles + projection convention)

---

## 0. TL;DR (what to implement)

1. The world-camera cbuffer is **6912 bytes**, identified by `near≈0.1 @ 0x0A0`, `far≈50000 @ 0x0A4`,
   `posW≈1.0 @ 0x08C`, valid VIEW @ `0x000`. (`DxgiProxy.cpp:127-134`, `ForzaCockpit_6912_cbv_classification.csv:3`)
2. The matrix the **world geometry actually samples for clip-space** is **`camRelVP @ 0x100`** (camera-relative
   view-projection, translation zeroed). This is the matrix to shift for parallax. `VIEW @ 0x000` and
   `VP @ 0x040` carry the absolute camera and are used by world-space / position-dependent terms.
3. To locate the cbuffer's CPU pointer: **reuse DxgiProxy's existing mechanism** — hook
   `CreateCommittedResource(vtbl[27])` + `CreatePlacedResource(vtbl[29])`, `Map()` each UPLOAD/CPU-visible
   buffer once, and resolve GPU-VA→CPU via the recorded ring map; trigger the write from the
   `CreateConstantBufferView(vtbl[17])` hook (`Hook_CBV`). This is already proven live. (recommended over a code-byte committer hook)
4. The per-eye shift is a **view-space lateral translation by signed half-IPD `d`** applied as a LEFT
   pre-multiply of a translation `T(d,0,0)_view` onto the camera-relative VP (and a paired update to VIEW/VP).
   It does not cancel because the geometry is already baked relative to the *original* camera origin; moving
   the *view* origin in view space re-projects that fixed geometry → real horizontal parallax. Formula in §4.

---

## 1. Exact 6912B camera cbuffer field map

### 1.1 Matrix / vector conventions (the load-bearing part)

All matrices in this cbuffer are stored **row-major**, and the engine uses the **row-vector** convention
`clip = world · M` (world-position row-vector multiplied on the LEFT of the matrix), confirmed two ways:

- The producer decompile reads `a7` (projection) as four rows and forms VP via
  `clip = view.xxxx·a7[0] + view.yyyy·a7[1] + view.zzzz·a7[2] + view.wwww·a7[3]`, i.e. `view · a7`, not `a7 · view`.
  (`docs\A7_PROJECTION_FIELD_MAP.md:20-27`, lines "row-major 4×4, row-vector convention")
- The producer's `a4` = VIEW = camera-to-world with **origin in row 3**.
  (`docs\A7_PROJECTION_FIELD_MAP.md:107-109`)

BUT note the **important subtlety for VIEW as stored in the cbuffer**: the proven downstream code
(`DxgiProxy.cpp`) treats the **VIEW block @ 0x000 as a world→view (column-vector) matrix with translation
in COLUMN 3** — i.e. translation at linear floats `m[3],m[7],m[11]` and bottom row `[0,0,0,1]`. This is
explicit in `InvRigid` (`DxgiProxy.cpp:119-120`, reads `tx=V.m[3], ty=V.m[7], tz=V.m[11]`) and the
`LooksView` validator which checks `m[12],m[13],m[14]≈0, m[15]≈1` (last ROW is `0,0,0,1`)
(`DxgiProxy.cpp:124`). The field map agrees: `+0x030 firstRow0 = 0,0,0,1`
(`ForzaCockpit_6912_field_map.md:20`) — the 4th row of VIEW is `(0,0,0,1)`, and the translation
(`-5296.., 865.., 2923..`) lives in the **column**, appearing in the field map as the separate
`cameraPos @ 0x080` packing.

Reconciliation: `a4` (the producer's camera-to-world, origin in row3) and the cbuffer's `VIEW @ 0x000`
(world→view, translation in column3) are **inverses/transposes of each other** — the producer builds
world→view from camera-to-world before committing. **For implementation, trust the cbuffer-side
convention used by `DxgiProxy.cpp` (column-vector world→view, last row `0,0,0,1`)** because that is the
layout actually present in the 6912B block we will edit, and it is the layout the proven freecam used.
The producer-side row-vector convention only matters if you instead hook the upstream producer (not the
recommended path here).

`VP @ 0x040` and `camRelVP @ 0x100` are `view * proj` composites and follow the same storage:
they transform a (world or camera-relative) row-vector position into clip space; their 4th row is the
perspective/translation row and is generally non-trivial (reverse-Z, near 0.1 lands at `[3][2]`).
(`docs\A7_PROJECTION_FIELD_MAP.md:44,61`)

### 1.2 Canonical offsets (every known field)

Offsets are byte offsets into the 6912B block. "lin floats" = float index = offset/4.
Confirmed by the consolidated map (`upstream_offline\SUMMARY.md:88-99`) and the per-capture
field maps; each row cites the strongest single source line.

| Offset | Float idx | Field | Type / convention | Evidence |
|---|---|---|---|---|
| `0x000` | 0..15 | **VIEW** (primary) | mat4, world→view, column-vector, last row `(0,0,0,1)`, translation in col3 | `ForzaCockpit_6912_field_map.md:17` (`+0x000 view-like 5/5`), validator `DxgiProxy.cpp:124`, used `DxgiProxy.cpp:140` |
| `0x040` | 16..31 | **VP** (primary, world view-projection) | mat4, world→clip, row-vector multiply, reverse-Z | `ForzaCockpit_6912_field_map.md:21`, `cbv_classification.csv` near/far gate; used `DxgiProxy.cpp:141` (`p+64`) |
| `0x080` | 32..35 | **cameraPos** (world) `.xyz`, `.w=posW` | float4; `.w≈1.0` for main cam | `ForzaCockpit_6912_field_map.md:147` (`+0x080 pos-like vec=-5296.38,865.158,2923.32,1`); posW read `DxgiProxy.cpp:131` (`p+140`=0x8C) |
| `0x08C` | 35 | **posW** scalar = cameraPos.w | float ≈ 1.0 (main-cam discriminator) | `ForzaCockpit_6912_field_map.md:217` (`+0x08C = 1`); gate `DxgiProxy.cpp:132` |
| `0x0A0` | 40 | **near** | float ≈ 0.1 | `ForzaCockpit_6912_field_map.md:218` (`+0x0A0 = 0.1 near-like`); `cbv_classification.csv:3`; read `DxgiProxy.cpp:131` (`p+160`) |
| `0x0A4` | 41 | **far** | float ≈ 50000 | `ForzaCockpit_6912_field_map.md:219` (`+0x0A4 = 50000 far-like`); read `DxgiProxy.cpp:131` (`p+164`) |
| `0x0A8` | 42 | near' (≈0.1000002 = C·near) | float, derived depth param | `ForzaCockpit_6912_field_map.md:220` |
| `0x0AC` | 43 | C ≈ 1.00000203 = far/(far−near) | float, reverse-Z depth scale | `ForzaCockpit_6912_field_map.md:221`; producer `A7_PROJECTION_FIELD_MAP.md:40-44` |
| `0x100` | 64..79 | **camRelVP** (camera-RELATIVE view-projection) | mat4, camera-relative→clip; identical to VP but with **translation/origin column zeroed** (row0 `.w=0`) | `ForzaCockpit_6912_field_map.md:145` (`+0x100 diffCamRel=0`, row0 `...,0`); used `DxgiProxy.cpp:141` (`p+256`) |
| `0x140` | 80..83 | cameraPos **dup #1** (exact) | float4, == `0x080` | `ForzaCockpit_6912_field_map.md:148` (`+0x140 diffPos=0`); `upstream_offline\SUMMARY.md:96` |
| `0x150` | 84..87 | cameraPos **dup #2** (exact) | float4, == `0x080` | `ForzaCockpit_6912_field_map.md:149` (`+0x150 diffPos=0`) |
| `0x170` | 92..95 | cameraPos **dup #3** (rounded/quantized, ints `-5296,865,2923,1`) | float4 | `ForzaCockpit_6912_field_map.md:150` (`+0x170 diffPos=0.378906`) |
| `0xC20` | 776..779 | cameraPos **dup #4** (rounded, == `0x170`) | float4 | `ForzaCockpit_6912_field_map.md:151`; `upstream_offline\SUMMARY.md:98` |
| `0xC40` | 784..799 | **camRelVP dup** (exact/near-exact, `diffCamRel ~0..3.6e-7`) | mat4, == `0x100` | `ForzaCockpit_6912_field_map.md:146` (`+0xC40 diffCamRel=3.57628e-07`); `upstream_offline\SUMMARY.md:99,101` |
| `0x1180` | 1120..1123 | cameraPos dup (exact) — capture-dependent | float4, == `0x080` | `ForzaCockpit_6912_field_map.md:152` (eid=13304) |
| `0xD00 / 0xD40 / 0xE00` | — | secondary VIEW/VP/camRelVP block (appears in some draws, e.g. eid=11267) | same convention | `ForzaCockpit_6912_field_map.md:175-177` |

Cross-capture confirmation that these offsets are STABLE (not coincidence): the same `+0x000` VIEW,
`+0x040` VP, `+0x100` camRelVP, `+0x080/0x140/0x150` cameraPos, `+0xC20` rounded pos, `+0xC40` camRelVP-dup
appear identically in the LookingRight capture (`ForzaLookingRightCockpit_6912_field_map.md:140-148`) and the
ChaseCam capture (`ForzaChaseCam_6912_field_map.md:141-149`), with only the numeric values changing with the
viewpoint. near=0.1/far=50000/posW=1 are invariant across all three
(`ForzaLookingRightCockpit_6912_field_map.md:179-180`, `ForzaChaseCam_6912_field_map.md:185-186`).

**Duplicate-handling rule for the patch:** because cameraPos and camRelVP each appear in multiple slots,
a robust per-eye write must update ALL copies that the world might sample, OR (cleaner) only update the
camera-relative composite(s) the world clip path reads — see §2. The proven freecam edited only
`VIEW@0x000`, `VP@0x040`, `camRelVP@0x100` and that was sufficient to move the rendered world
(`DxgiProxy.cpp:145`), so the `0xC40` camRelVP-dup and pos-dups are either not sampled by the main world
clip path in those draws or tolerate staleness for one frame. For stereo, treat `0xC40` as a candidate to
also patch and validate (it is an exact current-frame copy of `0x100`).

### 1.3 Beyond ~0x100: large parameter region

Offsets from ~`0x180` to `0xBF0` are mostly small constant tiles (1s, 0.5s, FOV-ish scalars, packed
post-process / tonemap / shadow-cascade params), not camera matrices — `firstRow0` patterns like
`1,1,1,1`, `0.5,0.5,0,1`, `0,-25,-25,-25`, etc. (`ForzaCockpit_6912_field_map.md:44-133`). These are NOT
part of the stereo lever and should be left untouched. The only camera-bearing fields are those in §1.2.

---

## 2. Which matrix does WORLD geometry sample vs COCKPIT/car vs HUD?

This is the critical question, and the reports do **not give a per-draw shader-reflection proof of which
cbuffer field index each draw reads** (the camwrite/callstack probes failed — see "Evidence gaps" below).
So this section gives the **strongest supported inference**, clearly labeled.

### 2.1 What the reports DO establish

- All main-world draws bind the **same 6912B cbuffer** (the one gated by near0.1/far50000/posW1), under a
  few root-signature/param combos: `rootSig 867 / rootParam 4` (most common main-cam), `rootSig 794 /
  rootParam 0`, `rootSig 873 / rootParam 2`. Root sig/param alone does NOT distinguish target; the content
  signature does. (`renderdoc_audit\SUMMARY.md:89-94`, `cbv_classification.csv:4,7,20,21`)
- The block contains three distinct camera transforms: `VIEW` (world→view), `VP` (world→clip, absolute),
  and `camRelVP` (camera-relative→clip, translation zeroed). The existence of a dedicated
  **camera-relative** VP (`0x100`, row0 `.w = 0`) is the fingerprint of **camera-relative rendering**:
  large-world geometry is submitted with positions already expressed relative to the camera, then
  transformed by `camRelVP` to avoid float precision loss at world coords of thousands of meters
  (cameraPos here is `-5296, 865, 2923`). (`ForzaCockpit_6912_field_map.md:145`, `upstream_offline\SUMMARY.md:99`)

### 2.2 The inference (which field each surface reads)

| Surface | Matrix it samples (inferred) | Reasoning |
|---|---|---|
| **World / environment / track** (the thing that needs parallax) | **`camRelVP @ 0x100`** (and the `cameraPos` dups to rebase vertices) | This is the whole reason a camera-relative VP exists. World vertices arrive camera-relative; only `camRelVP` (translation-free) maps them to clip. A world-space `VP` would lose precision. Confirms the user's live finding: translating `a4.row3` (world camera origin) cancels because world geometry is already camera-relative — the absolute origin is divided out. |
| **Cockpit / car body / driver** | likely **`VP @ 0x040`** or a per-object model matrix × `VP`; car is small and near origin, precision is a non-issue, and the car is rendered in its own local/world frame, not camera-relative | The car does not benefit from camera-relative rebasing; engines typically draw the ego-vehicle with the absolute VP. Treat as a candidate that may need the SAME view-space shift applied to `VP`/`VIEW` (which §4 provides), so cockpit and world stay fused. |
| **HUD / 2D overlays / racing-line chevrons / waypoint arrows** | a 2D/ortho transform OR the same camera cbuffer but should NOT receive parallax | The proven freecam specifically had to FREEZE the line/arrow draws (redirect their CBV to an untransformed copy) so they stayed anchored while the world moved — proving those draws read the SAME camera bytes through directly-created CBVs, while the world reads them via `CopyDescriptors` copies. (`DxgiProxy.cpp:83-88, 446-477`) For stereo this means: HUD must be drawn at zero-disparity (or a fixed comfortable depth), NOT given the IPD shift, or it will diverge between eyes. |

### 2.3 Decisive conclusion for stereo

**Shift `camRelVP @ 0x100` to get world parallax** (primary lever), and apply the SAME view-space lateral
shift to `VP @ 0x040` and `VIEW @ 0x000` so cockpit/car and any world-space-sampling effects stay fused
with the world. Also patch the `camRelVP` duplicate at `0xC40` (exact copy) defensively. Do NOT shift the
HUD path; if the HUD reads the same cbuffer via a directly-created CBV, use the proven redirect trick
(point its CBV at an un-shifted copy) — see `DxgiProxy.cpp:446-477`.

**Evidence gap (state plainly):** The renderdoc per-draw "which field index" proof was not obtained —
`ForzaCockpit_camwrite.md` failed to open the capture (`opencap fail` API-level error,
`ForzaCockpit_camwrite.md:2-3`); `ForzaCockpit_callstacks.md` found `chunks_with_callstack=0`
(`ForzaCockpit_callstacks.md:3`); `ForzaCockpit_camwrite4.md` found the 6912 CBV had no Map/CopyBufferRegion
writer in the capture (`CBVs total=295 ; 6912-byte CBV backing resource ids=[]`,
`ForzaCockpit_camwrite4.md:3,30,34`), meaning the cbuffer was filled by **coherent mapped writes the
capture did not attribute to a resource** — consistent with a persistently-mapped upload ring. So the
mapping above is **inference from the layout + the live freecam behavior**, not a shader-reflection dump.
A single live confirmation (per §5) would lock it.

---

## 3. Minimal runtime mechanism to locate the cbuffer's CPU pointer

Two options, then a recommendation.

### Option (a) — PORT DxgiProxy's resource-ring tracking + CBV-triggered write  [RECOMMENDED]

This is the mechanism that is **already proven live** in `DxgiProxy.cpp`. Minimal subset to port into the
vrframework dxgi proxy (which already owns the D3D12 device/swapchain):

What to hook (all are device-vtable indices, install on the GAME device, not a dummy one —
`DxgiProxy.cpp:537-550`):
1. **`ID3D12Device::CreateCommittedResource` = vtbl[27]** → `Hook_Committed` (`DxgiProxy.cpp:266,287-296,544`).
   On success, if heap is `UPLOAD` or CPU-visible `CUSTOM`, call `TrackBuffer`.
2. **`ID3D12Device::CreatePlacedResource` = vtbl[29]** → `Hook_Placed` (`DxgiProxy.cpp:268,298-311,545`).
   Same, after querying heap props.
3. **`ID3D12Device::CreateConstantBufferView` = vtbl[17]** → `Hook_CBV` (`DxgiProxy.cpp:270,446-483,546`).
   This is the per-frame trigger.

`TrackBuffer` (`DxgiProxy.cpp:276-285`): for each BUFFER-dimension, CPU-visible resource, call
`res->Map(0,nullptr,&cpu)` ONCE (persistent map — never Unmap), record `{GPU-VA, width, cpu}` into a
vector `g_buffers` under a mutex. (`DxgiProxy.cpp:68-70, 282-284`)

`ResolveVA(va, need)` (`DxgiProxy.cpp:485-495`): given a CBV's `BufferLocation` (a GPU-VA) and a size,
linear-scan `g_buffers` newest-first for the ring that contains `[va, va+need)`, return `cpu + (va - base)`.

`Hook_CBV` (`DxgiProxy.cpp:446-477`): when a CBV with `SizeInBytes == 6912` and 256-aligned BufferLocation
arrives, `ResolveVA` it, run the strong main-camera content check `LooksMainCameraCb`
(`DxgiProxy.cpp:127-134`: near∈[0.08,0.2] @0xA0, far∈[45000,55000] @0xA4, posW≈1 @0x8C, valid VIEW@0,
finite VP@0x40 and camRelVP@0x100), and — if enabled — apply the per-eye write **in place** with a
refill-hash guard so each physical slot is transformed at most once per engine refill
(`SlotDone`/`SlotMark`, `DxgiProxy.cpp:153-156, 183-194`). SEH-guard the write (the mapped ptr can dangle
if its ring was freed; `DxgiProxy.cpp:159-171, 183-194`).

Pros:
- Proven to find and edit the exact world camera buffer live, reaching the world (via `CopyDescriptors`
  copies) regardless of descriptor path (`DxgiProxy.cpp:136-138`).
- No exe-code patching, no AOB on game functions, no ABI risk; survives game updates as long as the D3D12
  vtable indices hold (stable across D3D runtime).
- The content gate (`LooksMainCameraCb`) cleanly rejects shadow/reflection/aux 6912B cbuffers
  (near=1/far=1000/posW=2, or near=0.01) — already validated against the captures
  (`renderdoc_audit\SUMMARY.md:51-66`, `cbv_classification.csv:2,17,18`).

Cons:
- Per-eye AFR (two submits/frame) means the write must apply the correct sign `d` for the eye currently
  being rendered. The mod must know which eye it is stamping (the memory note `g_fh5_applied_eye` records
  exactly this lesson — eye-stamp the copy or both eyes go identical).
- The full async ring scan (`ScanRingsOverwrite`, `DxgiProxy.cpp:173-179, 617-623`) is a starvation risk
  and was abandoned in favor of the bounded `Hook_CBV` write (`DxgiProxy.cpp:226-232` ECL note). Port only
  the bounded path; do NOT port the async `OverwriteThread`.

### Option (b) — HOOK a specific committer function

Two candidates:
- **`sub_141017C30` @ RVA 0x1017C30** — the "unique committer of the view-constants block" per memory.
  BUT the offline ranker marks it a **"dead/static committer trap, keep penalized until runtime proves
  otherwise"** (`upstream_offline\SUMMARY.md:30`, also ranked #4/penalized at line 30). Risky.
- **bridge `sub_140746BB0` @ inner probe `0x140746C6B`** — copies the provider-produced state into
  `*[a1+0x198] + 0x660` (`ForzaMultiCam+0x660`); the inner point `0x140746C6B` is "after protected
  builder/finalizer calls and before the copy begins" (`upstream_offline\SUMMARY.md:60-69`). This sits
  UPSTREAM of the cbuffer and operates on engine camera state, not the 6912B render block.

What must be hooked for (b): AOB-scan the function prologue in the live image (the proxy already has
`ScanModuleAobFind`, `DxgiProxy.cpp:335-356`), `MH_CreateHook` it (MinHook is already linked), reconstruct
the exact struct field being copied (`a1+0xC4..0x188` → `dst+0x660`), and translate a view-space IPD into
whatever representation that struct holds (rotation+position, not a render VP). Then the engine re-derives
VIEW/VP/camRelVP from it.

Pros:
- Upstream: the engine derives culling/TAA/VP coherently from one edited source (no per-copy chase, no
  `0xC40`/pos-dup staleness). This is the "proper" anvil-style injection (same philosophy as the proven
  producer hook `sub_140BB1EE0`, `DxgiProxy.cpp:360-409`).

Cons:
- Requires reverse-engineering the exact `ForzaMultiCam+0x660` struct field semantics (orientation/position
  encoding, units) before you can inject an IPD — substantial extra RE, none of it proven yet.
- AOB on game code is fragile across updates; ABI/`__fastcall` arg reconstruction risk (the producer hook
  needed a hand-built 25-arg signature, `DxgiProxy.cpp:109-110, 360-362`).
- For a *lateral IPD* specifically, the upstream lever has the SAME cancellation hazard the user already
  hit: if the field is a world-space camera position, shifting it laterally in WORLD space gets divided out
  by camera-relative rendering exactly like `a4.row3` did. The shift MUST be expressed in the camera's local
  (view) frame. The render-side `camRelVP` edit (§4) sidesteps this entirely by working in view space after
  the camera-relative rebasing.

### Recommendation

**Use Option (a).** It is already proven to locate and edit the precise world camera buffer, it works in
view space (so the IPD does not cancel), it requires zero new game-code RE, and it is the lowest-risk port.
Keep Option (b)/`sub_140BB1EE0` producer hook as the eventual "coherent culling" upgrade, but ship stereo
on (a). Specifically port: `BufRec`/`g_buffers`/`TrackBuffer`/`ResolveVA`, the three device-vtable hooks
(`Hook_Committed`/`Hook_Placed`/`Hook_CBV`), `LooksMainCameraCb`/`LooksView`, and the bounded in-`Hook_CBV`
write with the `SlotDone`/`SlotMark` refill guard. Do NOT port `OverwriteThread`/`ScanRingsOverwrite`
(starvation) or `Hook_ECL` (passthrough only). (`DxgiProxy.cpp` line refs throughout §3.)

---

## 4. Per-eye view-space IPD shift math

### 4.1 Setup, conventions, and helpers

From `DxgiProxy.cpp:117-122`:
- `Mat4` is 16 floats row-major. `Mul(A,B)` = row-major product `r[i][j] = Σ_k A[i][k]·B[k][j]`.
- `VIEW @ 0x000` (call it `V`) is **world→view, column-vector**: translation in linear floats
  `V.m[3],V.m[7],V.m[11]`, last row `(0,0,0,1)`. So for a world point as a COLUMN vector `pw`,
  `pview = V · pw`. (validator `DxgiProxy.cpp:124`; `InvRigid` `DxgiProxy.cpp:119-120`)
- `InvRigid(V)` returns the rigid inverse `V⁻¹` (view→world): transposes the 3×3 rotation and negates the
  rotated translation. (`DxgiProxy.cpp:119-120`)
- `RotP(M)` = rotation-only copy of `M` (zeros translation, sets `[15]=1`). (`DxgiProxy.cpp:121`)
- `T3(M)` = transpose of the 3×3 rotation block (translation zeroed). (`DxgiProxy.cpp:122`)
- `VP @ 0x040` (`P`) and `camRelVP @ 0x100` (`R`) are world→clip / camRel→clip composites used as
  `clip = pos · M` (row-vector); but since we only LEFT-multiply by a *view-space rigid* transform below,
  we operate on them consistently with how the proven freecam did (`DxgiProxy.cpp:143`).

The IPD half-offset is a signed scalar `d` (world units; per memory ≈ 100 units/meter, so for IPD 64mm,
`d = ±0.032 m × 100 = ±3.2` units, sign per eye: left eye negative, right eye positive — eye-stamped).

### 4.2 The view-space lateral translation

We want to move the camera (the view origin) by `d` along its **own right axis** (view-space +X), keeping
orientation fixed. In view space that is a pure translation. Because `V` is world→view (column-vector),
a camera that is shifted by `+d` along its right axis corresponds to a NEW world→view matrix:

```
Vnew = Tview(-d) · V          // Tview(-d) is a column-vector translation by -d in X
```

where `Tview(t)` is the column-vector translation matrix (translation in column 3):
```
Tview(t).m =  [1 0 0 t]
              [0 1 0 0]
              [0 0 1 0]
              [0 0 0 1]
```
Rationale (column-vector world→view): a point's view coordinates are `pview = V·pw`. If the eye moves
`+d` along view-X, every point's view-X coordinate decreases by `d`, i.e. `pview' = Tview(-d)·pview =
Tview(-d)·V·pw`. Hence `Vnew = Tview(-d)·V`. This is a LEFT (pre-)multiply in the column-vector world,
which translation-only and orientation-preserving — exactly a lateral eye offset, no toe-in (toe-in lives
in the per-eye projection skew `[2][0]`, handled separately per `docs\A7_PROJECTION_FIELD_MAP.md:84`).

Equivalently, using ONLY the existing helpers (no new `Tview`): since `Tview(-d)` differs from identity
only in `m[3] = -d`, the in-place edit is literally:
```
Vnew = V; Vnew.m[3] -= d;     // subtract d from the X-translation column of world→view
```
That single-cell edit IS `Tview(-d)·V` because left-multiplying a column-vector world→view by a pure
X-translation only adds to the translation column's X entry. (Derivable from `Mul`; verify against
`InvRigid`'s `tx=V.m[3]` indexing, `DxgiProxy.cpp:120`.)

### 4.3 Propagating the shift to VP and camRelVP

The projection-composites must be rebuilt so the world re-projects from the shifted eye. Let `Pview` be the
projection alone (`Pview = V⁻¹ · VP` conceptually). We do NOT have `Pview` separately, but we can apply the
SAME left view-space translation to the composites, because for a row-vector composite `M = Vrow · Proj`
(world→clip), a view-space eye shift is a left-multiply by the view-space translation expressed in the
composite's frame. The clean, helper-only way mirrors the freecam's rotation path
(`DxgiProxy.cpp:142-145`), substituting a translation `Tv` for the rotation `A`:

Define the **world-space delta** of the eye shift:
```
// world-space translation that moves the camera by +d along its right axis:
//   right_world = first ROW of the view→world rotation = first COLUMN of V's 3x3
//   (since Vinv = InvRigid(V), right_world = (Vinv.m[0], Vinv.m[1], Vinv.m[2]) is camera +X in world)
Mat4 Vinv = InvRigid(V);
float rx = Vinv.m[0], ry = Vinv.m[1], rz = Vinv.m[2];   // camera right axis, in world
```

**VIEW (world→view), exact (single-cell, preferred):**
```
Mat4 Vnew = V;
Vnew.m[3] -= d;     // == Tview(-d) · V ; orientation unchanged
```

**VP (world→clip):** VP = V-as-world→view composed with projection. The shift must enter at the view stage.
Build it from the shifted view using the existing decomposition trick. Because we cannot factor out `Proj`
cheaply, apply the world-space camera translation to VP via its right-multiply form. For a row-vector
world→clip `P` (so `clip = pw_row · P`), translating the camera by world delta `t = +d·right_world`
re-bases world positions by `-t` before projection:
```
// Trow(-t): row-vector world translation (translation in ROW 3 = floats 12,13,14)
Mat4 Trow_negt = identity;
Trow_negt.m[12] = -d*rx;  Trow_negt.m[13] = -d*ry;  Trow_negt.m[14] = -d*rz;
Mat4 VPnew = Mul(Trow_negt, P);     // == translate world by -t, then project
```

**camRelVP (camRel→clip) — THE parallax lever:** `camRelVP` is the camera-relative composite (origin
zeroed). The world geometry it consumes is already expressed relative to the ORIGINAL camera. Shifting the
eye by `+d` along view-X means the camera-relative positions must be re-based by `-d` along view-X BEFORE
the (translation-free) projection. Since `camRelVP`'s input is in the *original camera's* frame and the
shift is along the camera's own right axis, the rebase is a pure view-space X translation in the SAME
camera frame — i.e. translate camera-relative positions by `(-d, 0, 0)` in view space, then apply the
original camRelVP rotation/projection:
```
// view-space, row-vector rebase of camera-relative positions by -d along view +X:
Mat4 Tcr = identity;
Tcr.m[12] = -d;  Tcr.m[13] = 0;  Tcr.m[14] = 0;   // row-vector translation in view/cam-relative space
Mat4 camRelVPnew = Mul(Tcr, R);     // R = camRelVP @ 0x100
```
This is exact: because `camRelVP` already has its translation column zeroed (row0`.w=0`,
`ForzaCockpit_6912_field_map.md:145`), its input frame's axes coincide with view-space axes up to the
camera rotation baked into `R`'s 3×3; pre-translating the camera-relative input by `-d` along view-X is the
correct re-projection of the SAME geometry from the shifted eye.

Patch the `camRelVP` duplicate at `0xC40` with the same `camRelVPnew` (it is an exact copy, §1.2).

Write order each frame (per the bounded `Hook_CBV` path): read `V,P,R` from `p+0, p+64, p+256`; compute
`Vnew,VPnew,camRelVPnew` with the eye-stamped `d`; `memcpy` back to `p+0, p+64, p+256` (and `p+0xC40`);
mark the slot hash (`SlotMark`, `DxgiProxy.cpp:189`). Mirror `DxgiProxy.cpp:139-146`, swapping the rotation
build for the translation build above.

### 4.4 Why it does NOT cancel (the crux)

The user proved that translating `a4.row3` (the producer's WORLD camera origin) does not move the rendered
camera: world geometry is submitted camera-relative, so the absolute camera origin is subtracted out before
projection — moving it just moves both the geometry's rebase point and the camera together, a no-op.

The view-space shift here is different in two decisive ways:
1. **It is applied AFTER camera-relative rebasing, in the camera's own frame.** `camRelVPnew = Tcr · R`
   pre-translates the camera-RELATIVE positions (which are FIXED for this frame — they were computed
   against the ORIGINAL camera) by `-d` along view-X. The geometry's rebase point is the original camera;
   only the *view origin* moves. So the fixed geometry projects from a laterally-offset eye → genuine
   horizontal disparity that scales with `1/depth` (near objects shift more than far) = correct stereo
   parallax. Nothing cancels because the geometry positions are NOT recomputed from the new origin; they
   are the original-frame positions re-projected.
2. **For VP/VIEW it is a view-space (camera-local) translation, not a world translation of the origin.**
   `Vnew = Tview(-d)·V` changes where the view ZERO sits along the camera's right axis while keeping every
   world point's identity; `VPnew = Trow(-d·right_world)·P` re-bases world positions relative to the shifted
   eye. Because the offset is along the camera's instantaneous right axis (derived live from `V`), and the
   car/cockpit/world all share this same `V`, all surfaces shift consistently and stay mutually fused — the
   two eyes differ only by the lateral baseline `2d`, which is exactly stereo.

In short: the cancellation the user saw is specific to perturbing the *world origin that camera-relative
rendering divides out*. Perturbing the *view origin in view space* (equivalently, pre-translating the
already-rebased camera-relative geometry) is downstream of that division and therefore survives.

### 4.5 Compact formula summary (helper-only)

Given `d` (signed half-IPD, world units, eye-stamped), `V = mat4@0x000`, `P = mat4@0x040`,
`R = mat4@0x100`, and `Vinv = InvRigid(V)`, `right = (Vinv.m[0], Vinv.m[1], Vinv.m[2])`:

```
// VIEW (world->view, column-vector): lateral eye shift
Vnew        = V;  Vnew.m[3] -= d;                        //  = Tview(-d) · V

// VP (world->clip, row-vector): rebase world by -d*right, then project
Trow        = I;  Trow.m[12] = -d*right.x; Trow.m[13] = -d*right.y; Trow.m[14] = -d*right.z;
VPnew       = Mul(Trow, P);

// camRelVP (camRel->clip): rebase camera-relative geometry by -d along view +X  [PARALLAX LEVER]
Tcr         = I;  Tcr.m[12] = -d;                        // view-space row-vector X translation
camRelVPnew = Mul(Tcr, R);
// also write camRelVPnew to 0xC40 (exact dup of 0x100)
```

(`I` = identity mat4. Right eye `d = +IPD/2·scale`, left eye `d = -IPD/2·scale`; per MEMORY world scale
≈ 100 units/m. Toe-in / asymmetric frustum is handled in the per-eye projection skew `[2][0]`/`[2][1]`,
NOT here — see `docs\A7_PROJECTION_FIELD_MAP.md:84-89`.)

### 4.6 Sign / convention items to confirm live (1 cheap test each)

1. **Sign of `d` per eye** vs view-space +X: dump `V`, derive `right`, verify right eye moves scene LEFT
   in the image (objects parallax correctly). Flip `d` sign globally if reversed.
2. **camRelVP input-frame axis alignment:** the `Tcr.m[12] = -d` assumes camera-relative X == view X. If
   the engine's camera-relative frame is rotated vs pure view space, replace `Tcr` with the world-delta
   form `Mul(Trow(-d*right), R)` using `right` from `Vinv` (same as VP) and compare a near object's
   disparity. (Both reduce to the same thing if camRel frame == view frame.)
3. **`0xC40` necessity:** test with and without patching the dup; the proven freecam did NOT patch it and
   still moved the world (`DxgiProxy.cpp:145`), so it may be optional, but for stereo correctness patch it
   unless proven redundant.

---

## 5. One-shot live validation to lock the inferences (optional, when game is run)

- Confirm WORLD samples `camRelVP`: with the mod, shift ONLY `camRelVP@0x100` (+`0xC40`) and leave
  `VP/VIEW` untouched; the world should gain disparity while cockpit/HUD do not. Then shift ONLY `VP/VIEW`;
  cockpit should gain disparity while distant world (camera-relative) does not. This isolates which surface
  reads which field — the proof the offline captures could not provide (§2.3 gap).
- Confirm eye-stamping: log `g_fh5_applied_eye` per submit; ensure L and R get opposite `d` (the recorded
  bug was both eyes ending identical when the copy was not eye-stamped — MEMORY `fh5-vr-stereo-calibration`).

---

## Appendix A — Field offsets quick table (copy/paste)

```
VIEW       0x000  (16f)  world->view, col-vector, last row (0,0,0,1), trans @ m[3],m[7],m[11]
VP         0x040  (16f)  world->clip (row-vector multiply), reverse-Z
cameraPos  0x080  (4f)   world xyz, .w=posW(~1.0 main cam) ; .w scalar @0x08C
near       0x0A0  (1f)   ~0.1
far        0x0A4  (1f)   ~50000
near'      0x0A8  (1f)   ~0.1000002 (C*near)
C          0x0AC  (1f)   ~1.00000203 (far/(far-near))
camRelVP   0x100  (16f)  camRel->clip (row0.w=0) ; PARALLAX LEVER
camPos d1  0x140  (4f)   exact dup of 0x080
camPos d2  0x150  (4f)   exact dup of 0x080
camPos d3  0x170  (4f)   rounded dup (ints)
camPos d4  0xC20  (4f)   rounded dup (== 0x170)
camRelVP d  0xC40 (16f)  exact dup of 0x100 (patch alongside 0x100)
[secondary VIEW/VP/camRelVP @ 0xD00/0xD40/0xE00 in some draws]
CBV size   6912 bytes ; main-cam gate: near0.1@0xA0 & far50000@0xA4 & posW~1@0x8C & valid VIEW@0
```

## Appendix B — DxgiProxy.cpp subset to port (file:line)

```
Mat4 + Mul/InvRigid/RotP/T3            DxgiProxy.cpp:117-122
LooksView / LooksMainCameraCb / F32   DxgiProxy.cpp:124-134
BufRec / g_buffers / TrackBuffer      DxgiProxy.cpp:68-70, 276-285
Hook_Committed (vtbl[27])             DxgiProxy.cpp:287-296
Hook_Placed   (vtbl[29])              DxgiProxy.cpp:298-311
ResolveVA                             DxgiProxy.cpp:485-495
Hook_CBV     (vtbl[17]) + 6912 gate   DxgiProxy.cpp:446-483
SlotDone/SlotMark refill hash         DxgiProxy.cpp:153-156
bounded in-place write (model on)     DxgiProxy.cpp:139-146 (swap rotation build for §4 translation build)
InstallDeviceVtableHooks (indices)    DxgiProxy.cpp:537-550
DO NOT PORT: OverwriteThread/ScanRingsOverwrite  DxgiProxy.cpp:173-179, 617-623 (starvation)
DO NOT PORT: Hook_ECL (passthrough)   DxgiProxy.cpp:226-232
```
