# FH5 Camera World-Pose Hook Spec (Upstream of Rebasing)

Date: 2026-06-05
Scope: READ-ONLY reverse engineering. FH5 was not launched; no code edited. This is a
hook spec to be implemented directly.

Image base in the live exe / contracts: `0x140000000`. RVA = VA - 0x140000000.
(IDA decompile shards carry the true RVA in the low 6 hex digits of `sub_14XXXXXXX`.)

---

## TL;DR (the one recommendation)

Move the rendered camera by mutating the **`CCamDriver` concrete camera object**, which is
the per-frame WORLD camera pose that feeds FH5's camera-relative geometry rebasing — the
ForzaTech analog of Anvil's `onCalcFinalView`. Two co-located lanes in that object:

| Lane | Object offset | Type | Holds | Confidence |
|---|---|---|---|---|
| Camera-to-world matrix, row 3 (translation) | `active_object + 0x350` | 3× f32 (+1.0 at +0x35C) | **camera WORLD POSITION** | High — proven live to move the camera + view |
| Camera-to-world matrix basis rows 0/1/2 | `+0x320` / `+0x330` / `+0x340` | 3× f32 each | right / up / forward | High — orientation co-located |
| World-to-camera (VIEW) tail matrix | `+0x3E0` (= `+0x320 + 0xC0`) | 4×4 f32 | inverse of the +0x320 matrix | High — must be kept consistent |
| Double-precision position lane | `+0x550..+0x568` | 4× f64 | high-precision world position | High — proven live: clean position move |

The single best hook: **detour `sub_1406BE3A0` (RVA `0x6BE3A0`)**, the per-frame
CCamDriver `+0x320` publish/notify helper, and in the detour mutate `this+0x350` (add
forward/strafe/up + per-eye IPD along the rotated basis rows at `+0x320/+0x330/+0x340`),
recompute the `+0x3E0` VIEW tail, and (optionally) the `+0x550` f64 position. Because this
is the camera-to-world pose consumed BEFORE rebasing, shadows/culling/chevrons follow.

Orientation is co-located, so this hook can ALSO do rotation and the separate
`sub_140BB1EE0` a4 rotation hook can be retired — but see §5 risk notes; keeping rotation
in `sub_140BB1EE0` and only doing translation here is the safer first split.

The position field is what we ADD our offset to:
`*(Vec3*)(active_object + 0x350) += worldForward*fb + worldStrafe*lr + worldUp*ud + worldRight*ipd_signed`.

---

## 1. Decompile of the three requested functions

### 1a. `sub_1406BE3A0` — CCamDriver `+0x320` publish/notify helper
RVA `0x6BE3A0`. Decompile shard: `E:\ForzaHorizon5_IDA_Decompile\pseudocode\fh5_000053.c:2577`.

```c
__int64 __fastcall sub_1406BE3A0(__int64 a1 /*this = CCamDriver*/, __int64 a2 /*notify arg*/)
{
  __int64 result;
  __int64 v4;
  result = loc_1405EAC60(a1 + 0x320);   // 0x1406BE3A6: add rcx,320h ; 0x1406BE3B0: call loc_1405EAC60
  v4 = *(_QWORD *)(a2 + 8);              // 0x1406BE3B5: mov rcx,[rbx+8]
  if ( v4 )                             // optional listener/callback
    return (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v4 + 24LL))(v4); // jmp [vt+0x18]
  return result;
}
```
Disasm (`working_camera_offsets.md:147`, `concrete_camera_matrix_context.md:28`):
```
0x1406BE3A6 add  rcx, 320h
0x1406BE3B0 call loc_1405EAC60          ; reads/publishes the +0x320 4-row matrix
0x1406BE3B5 mov  rcx, [rbx+8]
0x1406BE3C6 jmp  qword ptr [rax+18h]    ; optional callback->vfunc(+0x18)
```
- **Reads:** the 4-row camera-to-world matrix at `this+0x320..+0x350` (via `loc_1405EAC60`,
  which takes `this+0x320` and pushes/publishes that matrix to a consumer).
- **Writes:** nothing in this body directly; it is a *publisher/notifier* of the +0x320
  matrix. `a1` (`rcx`) is the concrete `CCamDriver` object (`this`).
- Reached by `.rdata`/vtable cells `0x1467E46C8`, `0x1467F5738` (related-secondary table
  slot 38), `0x147022038`. This is the lane that exposes the finalized driver matrix to the
  rest of the frame.
- **This is the latest clean per-frame touch of the driver world pose before it is consumed
  downstream.** Mutating `this+0x320/+0x350` on entry (before the `loc_1405EAC60` publish)
  makes the published pose carry the offset.

### 1b. `sub_140746BB0` — provider→ForzaMultiCam aggregate bridge (the +0x660 copy)
RVA `0x746BB0`. Decompile: `fh5_000055.c:14175`. Disasm: `working_camera_offsets.md:172`.

```c
char __fastcall sub_140746BB0(__int64 a1 /*bridge/wrapper*/)
{
  __int64 provider = *(_QWORD *)(a1 + 0x188);   // 0x140746BBF
  unsigned int mode = *(_DWORD *)(a1 + 0x190);  // 0x140746BC9
  // provider->vfunc(+0x10) called before each stage (refresh/lock)
  loc_14086C340(a1,        &provider, mode);     // 0x140746BF6 prepass/setup (PROTECTED)
  loc_1408CF800(a1 + 0xC4, &provider, mode);     // 0x140746C36 PRODUCER -> writes src block (PROTECTED)
  sub_14085FD60(a1 + 0xC4, &provider);           // 0x140746C66 FINALIZER (PROTECTED)

  // 0x140746C6B: dst = *(ForzaMultiCam**)(a1 + 0x198) + 0x660; copy 0xC4 bytes
  _OWORD *dst = (_OWORD *)(*(_QWORD *)(a1 + 0x198) + 0x660);
  dst[0]  = *(_OWORD *)(a1 + 0xC4);  // -> FMC+0x660  (matrix ROW 1)
  dst[1]  = *(_OWORD *)(a1 + 0xD4);  // -> FMC+0x670  (matrix ROW 2)
  dst[2]  = *(_OWORD *)(a1 + 0xE4);  // -> FMC+0x680  (matrix ROW 3 = translation row)
  dst[3]  = *(_OWORD *)(a1 + 0xF4);  // -> FMC+0x690
  dst[4]  = *(_OWORD *)(a1 + 0x104); // -> FMC+0x6A0
  dst[5]  = *(_OWORD *)(a1 + 0x114); // -> FMC+0x6B0
  dst[6]  = *(_OWORD *)(a1 + 0x124); // -> FMC+0x6C0
  dst[7]  = *(_OWORD *)(a1 + 0x134); // -> FMC+0x6D0
  dst[8]  = *(_OWORD *)(a1 + 0x144); // -> FMC+0x6E0
  dst[9]  = *(_OWORD *)(a1 + 0x154); // -> FMC+0x6F0
  dst[10] = *(_OWORD *)(a1 + 0x164); // -> FMC+0x700
  dst[11] = *(_OWORD *)(a1 + 0x174); // -> FMC+0x710
  *(_DWORD *)&dst[12] = *(_DWORD *)(a1 + 0x184); // -> FMC+0x720 (final scalar/flag)
  return 1;
}
```
- **Bridge object fields:** `a1+0x0C4` source camera-state block (0xC4 bytes), `a1+0x188`
  provider/ref, `a1+0x190` mode, `a1+0x198` = `ForzaMultiCam*` destination.
- **Reads:** builds `a1+0xC4..0x188` via PROTECTED producer `loc_1408CF800` /finalizer
  `sub_14085FD60` (`.pdata`-only, not decompilable — do NOT hook inside them).
- **Writes:** `ForzaMultiCam+0x660..+0x724` (rows 1/2/3 of the FMC aggregate matrix +
  trailing projection/frustum lanes). **Does NOT write `ForzaMultiCam+0x650` (row 0)** —
  that row has a different producer.
- Only xref is `.pdata` (`0x14A4BDE44`); it is dispatched indirectly via a scheduler/
  function-pointer table (no static caller). Hookable mid-function at `0x140746C6B`.

### 1c. `sub_1406B0C20` — ForzaMultiCam aggregate matrix GETTER (downstream)
RVA `0x6B0C20`. Decompile: `fh5_000053.c:663`. Disasm: `working_camera_offsets.md:76`.

```c
_OWORD *__fastcall sub_1406B0C20(_OWORD *this_ /*ForzaMultiCam*/, _OWORD *out)
{
  out[0] = this_[101];  // FMC + 0x650  (ROW 0)
  out[1] = this_[102];  // FMC + 0x660  (ROW 1)
  out[2] = this_[103];  // FMC + 0x670  (ROW 2)
  out[3] = this_[104];  // FMC + 0x680  (ROW 3)
  return out;           // pure copy-out, NO producer behavior
}
```
- **Reads** `FMC+0x650..+0x68F` (4-row matrix). **Writes** caller buffer only.
- Pure reader. Reached by vtable cells `0x1470500C8` (FMC primary slot 12) and
  `0x1467F52D8`. **Too late** to hook for culling — it is consumed after CPU
  visibility/draw-list already used the driver camera.

### Which one is the latest-in-frame write of the camera WORLD POSITION + VIEW basis?
**None of these three *originate* the world pose.** Ordering from authoritative→derived:

1. `CCamDriver` object `+0x320` matrix (row3 = world position) — the authoritative
   per-frame gameplay camera-to-world pose. `sub_1406BE3A0` *publishes* it.
2. `sub_140746BB0` bridge — copies a provider-built block into the `ForzaMultiCam`
   aggregate `+0x660..` (rows 1–3 + frustum lanes). Mid-pipeline, after the driver pose is
   already produced; does not write row 0 (+0x650).
3. `sub_1406B0C20` getter — pure read of the FMC aggregate. Latest, downstream.

The position the renderer rebases against (`cameraPos@0x80` in the 6912-byte CBV) is
derived from #1. So the correct mutation point is the `CCamDriver` object, and the cleanest
once-per-frame touch of it is `sub_1406BE3A0`.

---

## 2. The camera WORLD POSITION field — exact offset, type, finalize site

**Proven matrix layout** (from the working live freecam DLL
`E:\SteamLibrary\steamapps\common\ForzaHorizon5\FH5CameraProbe\src\CamDriverMatrixFreecamDll.cpp:60`
and its `DecodePose`/`MatrixFromPose` at lines 193–232; `CAMERA_OFFSETS.md:22`):

The camera-to-world matrix at `CCamDriver + 0x320` is **row-major, basis rows + translation
in row 3**:

```
+0x320  m[0..2]   = right   (Vec3 f32),  m[3]  = 0     @ +0x32C
+0x330  m[4..6]   = up      (Vec3 f32),  m[7]  = 0     @ +0x33C
+0x340  m[8..10]  = forward (Vec3 f32),  m[11] = 0     @ +0x34C
+0x350  m[12..14] = POSITION (Vec3 f32), m[15] = 1.0   @ +0x35C   <-- WORLD CAMERA POSITION
```

- **Exact position offset:** `CCamDriver_object + 0x350`, type **3 contiguous float32**
  (with `1.0f` w at `+0x35C`). This is the value the live freecam adds its (forward, strafe,
  up) translation to (`CamDriverMatrixFreecamDll.cpp:227-229` writes `m[12..14]`).
- A **second, higher-precision position lane** exists at `CCamDriver + 0x550..+0x568` = 4×
  **double (f64)**. Getter `sub_140844F30` (RVA 0x844F30) converts these doubles to a
  float4; raw getter `sub_140844FB0`. The camdriver-fields report
  (`subagent_camdriver_fields\report.md:13`) flags this as "the clean double4 lane —
  writing it cleanly moves camera position." Treat `+0x350` (f32 matrix row) as the lane the
  renderer/view consumes, and `+0x550` (f64) as the source-of-truth position that may be
  re-projected to `+0x350` each frame; write **both** to be safe (see §5).
- **Co-located VIEW (world-to-camera) tail** at `CCamDriver + 0x3E0` (= +0x320 + 0xC0), a
  full 4×4 f32 inverse: rows are the transposed basis with translation =
  `(-dot(pos,right), -dot(pos,up), -dot(pos,forward))`. The live freecam writes this
  alongside (`ViewTailFromPose`, `CamDriverMatrixFreecamDll.cpp:234-253`,
  `WriteOutput:586`). It must be recomputed whenever +0x320/+0x350 changes or the view will
  desync.

**Where it is finalized each frame / instruction:**
- The +0x320 matrix is *published* by `sub_1406BE3A0` at `0x1406BE3B0 call loc_1405EAC60`
  (RVA 0x6BE3B0). That is the deterministic per-frame touch site for the hook.
- Bulk camera-state CLONE that copies the whole driver object (incl. +0x320 row and +0x3E0
  view tail) lives at `0x1407B6F4D`/`0x1407B6F55` (read/write of `[r10/r11+320h]`),
  continuing +0x330..+0x390 and +0x3E0.. (`concrete_camera_matrix_context.md:53,203`). This
  is a generic object clone (copies +0x240..+0x610), not the per-frame world-pose producer —
  do not hook it.
- A generic single-row setter `sub_140687480` (RVA 0x687480) writes one xmmword to
  `this+0x3E0`, but that slot is widely reused across classes; only relevant if `rcx` is
  proven to be the live CCamDriver (`subagent_camdriver_fields\report.md:118`).

---

## 3. Recommended hook (single best)

### Primary: detour `sub_1406BE3A0` (CCamDriver +0x320 publisher)
- **Function RVA:** `0x6BE3A0` (VA `0x1406BE3A0`).
- **Prologue AOB** (64-byte, from `_agent_reports\upstream_offline\fh5_hook_aob_contracts.md`,
  `ccamdriver_helper_sub_1406BE3A0`):
  - raw: `40 53 48 83 EC 20 48 81 C1 20 03 00 00 48 8B DA E8 AB C8 F2 FF 48 8B 4B 08 48 85 C9 74 0C 48 8B 01 48 83 C4 20 5B 48 FF 60 18 48 83 C4 20 5B C3`
  - masked: `40 53 48 83 EC 20 48 81 C1 20 03 00 00 48 8B DA E8 ? ? ? ? 48 8B 4B 08 48 85 C9 74 0C 48 8B 01 48 83 C4 20 5B 48 FF 60 18 48 83 C4 20 5B C3`
  - Short, unique signature: `48 81 C1 20 03 00 00` is `add rcx, 0x320` — the defining
    instruction. Combined with the `40 53 48 83 EC 20` prologue it is unambiguous.
- **`this` / arg holding the pose:** `__fastcall(a1 = rcx, a2 = rdx)`. `a1` = the concrete
  `CCamDriver` object (the pose container). The pose fields are `a1 + 0x320` (matrix) and
  `a1 + 0x550` (f64 position). `a2` is the notify-callback arg — do not touch.
- **Field offsets to add the offset to (relative to `a1`):**
  - `a1 + 0x350` (Vec3 f32) — **add** the world-space translation. This is the rebasing origin.
  - `a1 + 0x320 / +0x330 / +0x340` (right/up/forward Vec3 f32) — use as the basis to rotate
    the per-eye IPD and the forward/strafe/up inputs into world space; mutate here if also
    doing rotation.
  - `a1 + 0x3E0` (4×4 f32 VIEW tail) — recompute from the mutated +0x320 pose
    (`ViewTailFromPose`).
  - `a1 + 0x550..+0x568` (4× f64) — add the same world translation in double precision.
- **Detour body (before calling the trampoline / before `loc_1405EAC60` publishes):**
  1. Gate to the MAIN gameplay camera (see §4): verify `a1` == the active driver object
     resolved via `ForzaMultiCam+0x5C8`, or that `a1`'s control vtable ==
     `std::_Ref_count_obj2<Camera::CCamDriver>` vtable (IDA VA `0x1467F74C8`).
  2. Read base pose from `a1+0x320` (right/up/forward/pos). Save originals for restore.
  3. Compute world delta = `forward*fb + right*lr + up*ud` (+ per-eye `right * (±ipd/2)`).
  4. `pos' = pos + delta`; write `a1+0x350 = pos'`, write `a1+0x550 = (f64)pos'`.
  5. If doing rotation here: rotate the basis rows and write +0x320/+0x330/+0x340.
  6. Recompute and write the +0x3E0 VIEW tail from the new basis+pos.
  7. Call the original (trampoline); let `loc_1405EAC60` publish the mutated matrix.
- **Save/restore:** Because the game *re-derives* the driver pose from car/physics each frame
  (the freecam DLL re-reads the base every frame, `CamDriverMatrixFreecamDll.cpp:684-690`),
  the safest model is **compose-on-top per frame, not persistent accumulation**: each frame
  read the engine base from `+0x320`, add the current offset, write the result. Do NOT
  permanently overwrite — let the engine's next-frame production reset the base. No explicit
  restore is needed if the offset is reapplied every frame from the freshly-produced base,
  but keep a "neutral when disabled" path that simply does not mutate.

### Alternative (code-hook, more coherent block, slightly later): `sub_140746BB0` @ `0x140746C6B`
- **RVA:** `0x746C6B` (mid-function, after producer+finalizer, before the copy to FMC+0x660).
- **AOB** (`state_bridge_inner_140746C6B`):
  - raw: `48 8B 8B 98 01 00 00 0F 10 07 48 8B 5C 24 38 48 81 C1 60 06 00 00 48 8B 74 24 40 0F 11 01 ...`
- At this site `rdi == a1+0xC4` (coherent source block) and `rcx` becomes `FMC+0x660`. The
  block maps to the FMC aggregate matrix rows 1–3 at `+0x660/+0x670/+0x680` (row3 +0x680 =
  translation). **Caveat:** this writes the FMC aggregate which is a getter-fed *downstream*
  lane (does not write row0 +0x650), and prior live tests indicate the FMC aggregate is
  consumed too late for culling. Use only as a correlation/secondary point, not the primary
  rebasing fix. (A prior live attempt to detour at this exact address crashed —
  `subagent_camera_bridge_finalizer\report.md:187` — prefer a clean trampoline if used.)

### Orientation: co-located → can retire `sub_140BB1EE0` a4 rotation, OR keep split
Orientation (basis rows) is co-located with position in the same +0x320 matrix, so this hook
*can* do both translation and rotation, allowing the separate `sub_140BB1EE0` a4 hook to be
retired. **However**, the prior live result was: writing the +0x320 matrix late produced
correct pitch but "geometry flicker/culling lag," while `sub_140BB1EE0` a4 rotation is already
proven clean and reversible. **Recommended first split:** do TRANSLATION (the new capability)
at `sub_1406BE3A0` since position is what rebasing needs upstream, and KEEP rotation in
`sub_140BB1EE0` until the +0x320 rotation path is validated flicker-free. Once §4 gating and
the +0x3E0/+0x550 consistency writes are confirmed to eliminate flicker, fold rotation in and
retire the a4 hook.

---

## 4. Sanity / risk notes

### Is `sub_1406BE3A0` called once per frame for the MAIN camera only?
- It is the publisher for *whatever* concrete camera owns the +0x320 lane. FH5 has many
  camera objects (CCamDriver, CCamHood, CCamBumperHigh, CCamFollowLow/High, CCamFree*, plus
  Cinematic and shadow/reflection cameras). Expect this lane to fire for **multiple camera
  instances** and possibly for non-main passes. **You MUST gate.**
- The CinematicGameCamera uses a *different* matrix lane (`+0x540..+0x570`, getter
  `0x1419D5730`, `subagent_projection_culling\report.md:97`), so it will not collide with the
  +0x320 gameplay path — good.

### How to identify the MAIN gameplay camera (gating)
Use the proven active-camera resolver (`CAMERA_OFFSETS.md:73`):
```
module_base       = GetModuleHandle("ForzaHorizon5.exe")
fmc_control       = scan_heap_qword( module_base + (0x147053C50 - 0x140000000) )  // Ref_count vtbl of ForzaMultiCam
fmc_object        = fmc_control + 0x10
active_cam_object = *(uintptr_t*)(fmc_object + 0x5C8)   // active concrete camera object
active_cam_ctrl   = *(uintptr_t*)(fmc_object + 0x5D0)
```
In the detour, **gate on `a1 == active_cam_object`**. That is the single, race-validated
discriminator for the camera the player sees. As a secondary check, confirm `a1`'s control
block vtable == `0x1467F74C8` (`std::_Ref_count_obj2<Camera::CCamDriver>` vtable, RVA
0x67F74C8) when the active mode is a CCamDriver; for hood/bumper/chase, gate purely on the
`+0x5C8` active-object identity so the offset rides every drivable view.
- Secondary gate (cheap, optional): the +0x320 matrix must pass a basis sanity check
  (orthonormal rows, |w-1|<0.25, finite) before mutating — exactly the `DecodePose` guard the
  live freecam already uses (`CamDriverMatrixFreecamDll.cpp:193-211`). This rejects
  shadow/UI/garbage instances automatically.
- Do NOT gate by near/far here (those live in the FSR2 dispatch struct / 6912 CBV, not the
  CCamDriver object).

### Will mutating the world position here move the rebasing origin (cameraPos@0x80)? — data flow
Yes, this is the upstream lane. Traced flow (best static + prior live evidence):

```
CCamDriver +0x320 matrix (row3 +0x350 = world pos, f64 mirror +0x550)
        │   (authoritative per-frame gameplay camera-to-world pose; published by sub_1406BE3A0)
        ▼
[engine camera-relative rebasing + frustum/visibility build consume the driver pose]
        ▼
6912-byte render CBV:  cameraPos@0x80  (rebasing origin),  VIEW@0,  VP@0x40,  camRelVP@0x100
        │   vertex path: geom_rel = geom_world - cameraPos@0x80;  clip = geom_rel × camRelVP@0x100
        ▼
view/proj COMMITTER sub_140BB1EE0 (a4=cam-to-world, a17/a18=campos)  <-- proven live to IGNORE
                                                                         position edits, because
                                                                         rebasing already happened
        ▼
FSR2/DLSS projection consumers (sub_1456E1010 etc.) — downstream, not the source
```
- The decisive live fact you already have: editing position at the committer
  `sub_140BB1EE0` does nothing, because rebasing (which computes `cameraPos@0x80`) ran
  *upstream* of it. The only place upstream of rebasing that holds the world position is the
  `CCamDriver` object pose (+0x350 / +0x550). Therefore mutating +0x350 (and +0x550) at
  `sub_1406BE3A0` moves the value that becomes `cameraPos@0x80`, so shadows, culling, and
  chevrons follow.
- One residual unknown the implementer should verify live: whether `cameraPos@0x80` is sourced
  from the f32 matrix row (+0x350) or the f64 lane (+0x550). Writing BOTH removes the
  ambiguity. The f64 lane is the higher-confidence "clean position move" per the camdriver
  report.
- `ForzaMultiCam+0x650..+0x680` (getter `sub_1406B0C20`) and the bridge `+0x660` write are
  *downstream* aggregates and are getter-fed/late; they are NOT the rebasing origin. Confirmed
  too-late for culling in `subagent_projection_culling\report.md:318` and
  `subagent_multicam_matrix\report.md:185`.

### Other risks
- Concurrency: the driver pose is touched by the camera/sim thread; `sub_1406BE3A0` runs on
  that thread, so an in-detour mutation is naturally race-free vs. the publish (unlike an
  external poke from a worker thread). Still keep the mutation tiny and branchless.
- Flicker: if the +0x3E0 VIEW tail or the f64 +0x550 lane is left inconsistent with the new
  +0x350, expect the previously-seen flicker. Always write the consistent triple
  (+0x320 basis, +0x350 pos, +0x3E0 view tail, +0x550 f64 pos) atomically within the detour.
- Multiple instances / per-eye: the function fires per active camera, not per eye. Apply the
  per-eye IPD by stereo phase (the eye currently being rendered) the same way the renderer
  hook tracks `g_fh5_applied_eye`; do not assume two calls per frame.

---

## Citations (file:line)
- `sub_1406BE3A0` decompile: `E:\ForzaHorizon5_IDA_Decompile\pseudocode\fh5_000053.c:2577`
- `sub_1406BE3A0` disasm: `working_camera_offsets\working_camera_offsets.md:147`;
  `concrete_camera_matrix_context\concrete_camera_matrix_context.md:28`
- `sub_140746BB0` decompile: `pseudocode\fh5_000055.c:14175`; disasm `working_camera_offsets.md:172`
- `sub_140746BB0` field map (src+0xC4 → FMC+0x660 rows): `subagent_camera_bridge_finalizer\report.md:60,149`
- `sub_1406B0C20` decompile: `pseudocode\fh5_000053.c:663`; disasm `working_camera_offsets.md:76`
- Matrix layout (row3=pos, +0x3E0 view tail): `src\CamDriverMatrixFreecamDll.cpp:60,193-253,684-690`;
  `CAMERA_OFFSETS.md:22`
- +0x550 f64 position lane: `subagent_camdriver_fields\report.md:13,73`; `subagent_camdriver_ypr\report.md:84`
- +0x320 4-row matrix + bulk clone: `concrete_camera_matrix_context.md:53,203,222`
- Active-camera resolver / gating: `CAMERA_OFFSETS.md:73-84`
- Rebasing / cameraPos@0x80 / committer ignores pos: `_agent_reports\upstream_offline\SUMMARY.md:88-99`;
  task prompt live finding
- Culling/flicker explanation (FMC aggregate too late): `subagent_projection_culling\report.md:318`;
  `subagent_multicam_matrix\report.md:185`
- AOBs: `_agent_reports\upstream_offline\fh5_hook_aob_contracts.md` (`ccamdriver_helper_sub_1406BE3A0`,
  `state_bridge_inner_140746C6B`, `forzamulticam_getter_sub_1406B0C20_downstream`)
- CCamDriver Ref_count vtable VA 0x1467F74C8: `subagent_camdriver_fields\report.md:38-44`
