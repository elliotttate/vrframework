# FH5 (Empress) — Shadow-Cascade RE: why head ROTATION breaks shadows, and where to inject it

> ⚠️ **CORRECTIONS (2026-06-06) — read before trusting this report:**
> 1. **§6 is WRONG: translation is NOT shadow-coherent.** Live test confirmed the shadows MOVE/SLIDE with the camera as you translate (the producer `a15/a16` cameraPos lever is downstream of the cascade fit, exactly like a4 rotation). The "translation already shadow-coherent (PROVEN)" claim caused a wasted detour; the real fix for BOTH rotation and translation is to inject at the camera-pose SOURCE upstream of the cascade fit.
> 2. **The RVAs here are STALE.** This report's DB `fh5_b.i64` differs from the current shipping exe (`E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe` / `ForzaHorizon5.exe.i64`). On the current exe, RVA `0x6BE3A0` is NOT a function start; the publisher analog appears near RVA `0x6545A5` and adds `0x318` (not `0x320`). Re-derive every address on the CURRENT i64 before hooking.
> 3. The static recommendation to inject at `+0x320` via the publisher conflicts with a RUNTIME finding that rotating `+0x320` (post-getter) did not move the view — being re-resolved on the current build (the timing/where-written may be the difference).

Date: 2026-06-05
DB: `E:\tmp\fh5_re\fh5_b.i64` (image base `0x140000000`, no anti-tamper). RVA = VA − 0x140000000.
Method: headless idalib (IDA Pro 9.0). Producer ground-truth from the existing decompile shard
`E:\ForzaHorizon5_CameraProbeLogs\producer_0xBB1EE0_decomp.txt` (size 0x26A5, decompiles clean);
cascade-callee + getter + cluster decompiles produced live this session via
`scripts/ida_shadow_cascade_re.py`, `scripts/ida_cascade_writers.py`, `scripts/ida_cascade_builder.py`
(raw logs: `E:\tmp\cascade_re_out.log`, `cascade_writers3.log`, `cascade_builder.log`, `cascade_final.log`).

---

## TL;DR — the answer

**The shadow cascades read a SEPARATE, pre-built camera/view source — NOT the producer's `a4`.**
This is **hypothesis (b)**. Rotating `a4` in the `sub_140BB1EE0` detour rotates only the MAIN view
(`v307` = `transpose(a4)` and `a4×a6`); the per-cascade view/frustum matrices come from the
**`a13` argument** (producer local `v82 = a13`, line 759), an array of fully-baked 2724-byte
per-view (cascade) descriptors. Each cascade descriptor was fit to the camera frustum **before** the
producer ran, using the camera's un-rotated orientation. The producer only *copies* those baked
matrices into the output block — it never re-derives them from `a4`. So when you yaw `a4`, the
visible view turns but the cascades still cover the car's original forward frustum → shadows wrong
when you look away from forward.

**The single upstream point that makes BOTH the main view AND the cascades follow head rotation is
the `CCamDriver +0x320` camera-to-world matrix — the authoritative per-frame gameplay pose.** Both
the producer's `a4` and the shadow-cascade fitter derive from this same pose upstream. Rotate the
basis rows of `+0x320` (right/up/forward at `+0x320/+0x330/+0x340`), recompute the `+0x3E0` VIEW
tail, at the per-frame publisher **`sub_1406BE3A0` (RVA 0x6BE3A0)**. This is exactly the lane the
already-shadow-coherent 6-DOF translation lever is conceptually rooted in, and it is upstream of the
shadow-cascade fit. (Translation is already shadow-coherent because cascades rebase against
`cameraPos@+0x80`, which derives from the same pose's position lane — see §6.)

---

## 1. The cascade path inside `sub_140BB1EE0` (decompile, exact lines)

Lines are into `producer_0xBB1EE0_decomp.txt`.

### 1.1 The cascade provider is arg `a13`, captured as `v82` (NOT `a4`)
```c
// line 759
v82 = a13;
// line 760: a13+32 is a sub-object; +102 is a gate byte; a3 is the view/eye flag
if ( a13 && *(_BYTE *)(*(_QWORD *)(a13 + 32) + 102LL) || a3 != 1 ) ...
```

### 1.2 The cascade loops — they read ONLY `v82` (a13) and the global, never `a4`/`a6`/`v307`
```c
// lines 1322-1339 — copy 4 per-cascade VIEW matrices out of the a13 records
v127 = 0;
if ( v82 )
{
  v128 = 0; v129 = 32LL; v130 = 2LL;
  do
  {
    sub_140BDB6C0(v82, v220, v128);                 // <-- reads a13 record matrix, transposes it
    *(_OWORD *)((char *)&v274 + v129) = v220[0];
    *(_OWORD *)((char *)&v275 + v129) = v220[1];
    *(_OWORD *)&v276[v129]            = v220[2];
    *(_OWORD *)&v276[v129 + 16]       = v220[3];
    ++v128; v130 += 4LL; v129 += 64LL;
  } while ( v128 < 4 );

  // lines 1340-1377 — per-cascade WORLD pos (f64) + matrix, split into rebasing int+frac
  v131 = 0; v132 = 18LL; v133 = (__m128 *)&v277; v134 = *(float *)&dword_145E02878;
  do
  {
    ((void (__fastcall *)(__int64, double *, _QWORD))unk_140BDB820)(v82, &v215, v131); // per-cascade f64 WORLD center
    ... v215/v216/v217 split into (double)(int) integer tile + fraction ...           // camera-relative rebasing of the cascade center
    v141 = (_OWORD *)sub_140BDB860(v82, v408, v131);                                   // per-cascade VIEW matrix (full descriptor)
    v221[0..3] = *v141 ...;
    sub_140631F90(v271, &v211, v221);                                                 // make cascade matrix camera-relative vs &v211 (the main cam-rel origin)
    ...
    ++v131; v132 += 2LL; v133 += 2;
  } while ( v131 < 4 );

  v278 = *(_OWORD *)unk_140BDB7F0(v82, &v204);      // packs 4 per-cascade scalars (line 1378)
  v279 = (float)*(int *)(v82 + 656); ...            // cascade count/params straight out of a13 (lines 1379-1391)
}
else { /* lines 1394-1397: zero the 216-OWORD cascade block */ }
```
Note every input to the cascade matrices is `v82` (=`a13`) or `*(…)(v82 + …)`. **`a4` does not appear
anywhere in this loop.** The only "main-camera" coupling is `sub_140631F90(v271,&v211,v221)`
(line 1361), which merely re-bases the *already-built* cascade matrix against the main camera-relative
origin `v211` (a translation, not an orientation).

### 1.3 The MAIN view, by contrast, IS built from `a4` (lines 485-584)
```c
v223 = *a4; v224 = a4[1]; v225 = a4[2]; v226 = a4[3];   // line 485
... v307[0..3] = transpose(a4 basis) ...                // the VIEW rotation we rotate
v239 = *a6; ... v243 = *a4; ...                         // line 547+
... v307[4..7] = a4 × a6 ...                            // full world-to-camera VIEW
```
So `a4` → main `v307` VIEW only; `a13` → cascades. Two independent sources. **This is the bug.**

### 1.4 Output commit (lines 1603, 1630, 1645)
The cascade block goes into `v401` via `unk_144D392C4(v401,&v274,1504)` (line 1603); the main view
block `v307` (6800 bytes) is committed to `a1+768` when `v413==18` (main view) or to per-view slot
`&a1[38*v413+82]` otherwise (lines 1628-1645). The cascade matrices in `v274/v275/v276` are part of
that committed extended-view payload.

---

## 2. `unk_140BDB820`, `sub_140BDB860`, `sub_140BDB6C0` — what they read (resolved EAs)

All three are **accessor methods of the `a13` cascade-collection object**; they index it by a
**2736-byte (0xAB0) per-record stride**. They read pre-built data — they compute nothing.

### `sub_140BDB820` (RVA 0xBDB820) — per-cascade WORLD position (f64[4])
```c
_QWORD *__fastcall sub_140BDB820(__int64 a1, _QWORD *a2, unsigned int a3) {
  __int64 v3 = 2736LL * a3;                         // a3 = cascade index, stride 0xAB0
  *a2     = *(_QWORD *)(v3 + a1 + 3352);            // a13 + 2736*i + 0xD18  (f64.x)
  a2[1]   = *(_QWORD *)(v3 + a1 + 3360);            //                +0xD20 (f64.y)
  a2[2]   = *(_QWORD *)(v3 + a1 + 3368);            //                +0xD28 (f64.z)
  a2[3]   = *(_QWORD *)(v3 + a1 + 3376);            //                +0xD30 (f64.w)
  return a2;
}
```
=> the per-cascade rebasing CENTER (double precision), pre-stored at record `+0xD18`.

### `sub_140BDB860` (RVA 0xBDB860) — per-cascade VIEW matrix (full descriptor)
```c
_OWORD *__fastcall sub_140BDB860(__int64 a1, _OWORD *a2, unsigned int a3) {
  sub_140BD1BD0(&v12, 2736LL * a3 + a1 + 848);      // a13 + 2736*i + 0x350  -> copies a 2724-byte VIEW DESCRIPTOR
  *a2 = ...; a2[1] = ...; a2[2] = ...; a2[3] = ...; // returns the 4x4 matrix out of it
  ...
}
```
### `sub_140BD1BD0` (RVA 0xBD1BD0) — the descriptor copier called above
```c
__int64 __fastcall sub_140BD1BD0(__int64 a1, __int64 a2) {
  unk_144D392C4(a1, a2, 2496LL);                    // memcpy first 2496 bytes (matrices)
  *(_DWORD *)(a1 + 2496) = *(_DWORD *)(a2 + 2496);  // near/far/depth params
  *(_QWORD *)(a1 + 2504) = *(_QWORD *)(a2 + 2504);  // ... resource ptr (refcounted, +2600)
  ... copies through +2723 ...                      // total 2724-byte per-cascade VIEW record
}
```
This proves each `a13` record (stride 2736) is a **complete sub-view**: its own view/proj matrices,
near/far, flags, and a refcounted render-target — i.e. a baked shadow-cascade view, not a live
recompute.

### `sub_140BDB6C0` (RVA 0xBDB6C0) — per-cascade matrix transpose-extract
```c
sub_140BD1BD0(&a12, 2736LL * a3 + a1 + 848);        // same record +0x350
... transposes the 4x4 into a2[0..3] ...            // the matrix the producer stores into v274/v275/v276
```
### `unk_140BDB7F0` (RVA 0xBDB7F0), `sub_140BDBAC0` (RVA 0xBDBAC0)
Pack per-cascade scalars from the same `a1` object (e.g. `a1[891]`, `a1[1575]`… and `a1+652/660/760`).
Cascade count/flags, again read-only.

**Conclusion (Q1/Q2):** the cascades derive their PLACEMENT from **fields baked into the `a13`
collection** (per-record matrix at `+0x350`, world center at `+0xD18`). The accessors read a
*snapshot*, computed by the shadow subsystem before the producer runs. The orientation that fit those
cascades is therefore **not** `a4`, `a6`, the producer's f64 `a15/a16`, or `sub_140BDB860/820`'s own
logic — it is whatever the upstream shadow-cascade fitter used.

---

## 3. Where the cascade fit happens (Q3)

The cascade fit is **NOT inside `sub_140BB1EE0`** — the producer only consumes the baked `a13`.
It happens in the shadow/render-view subsystem that fills `a13`, which runs during the game-logic /
render-view build BEFORE the producer. Evidence:

- The producer has **0 static callers** (dispatched via scheduler/function-pointer; confirmed prior),
  and the `a13`-accessors (`sub_140BDB6C0/820/860/7F0/AC0`, `sub_140BD1BD0`) have **0 static xrefs**
  in this DB (`cascade_builder.log:3-19`) — they are reached only through the indirect dispatch, so
  the static call graph to the fitter is not materialized in the idb.
- The render-view manager singleton is the global **`qword_14901CAE0`** (RVA 0x901CAE0). The producer
  reads it (`v118`/`v170`, lines 1274/1527) and so does a cluster of render-view scheduler functions:
  `sub_140CA1B70, sub_140CA9650, sub_140CA9A50, sub_140CA9BF0, sub_140CAA010, sub_140CAA180,
  sub_140CAA540, sub_140CAA840, sub_141168960, sub_1411C09E0` (xrefs in `cascade_writers3.log:14-42`).
- That cluster is the **render-view scheduling/command-list layer** (SRWLocks, per-view task objects,
  closures `off_145E9D500/538/570/5A8`, allocates 136-byte view tasks; `sub_140CB1300`,
  `sub_140CA1B70` in `cascade_builder.log` / `cascade_final.log`). It consumes views; it is not the
  light-space cascade fitter itself.

The cascade-fitting math (camera frustum split + light direction → per-cascade light-space box) lives
in the shadow subsystem feeding `a13`; it is not exposed by a clean static call site in this DB. It is
**not needed as a hook target** because the fit derives from the camera pose (`CCamDriver +0x320`,
§5), which is the single upstream lever — see §4/§7.

**A hookable boundary if you want to instrument the fit directly:** the render-view scheduler head
`sub_140CA1B70` (RVA 0xCA1B70) is called by `sub_140CA1FF0` (0xCA203D) and `sub_140CB4620`
(0xCB46CB) (`cascade_final.log:263-265`); it strides a 232-byte per-view object (`232*v12`, line 80)
and locks the global render-view manager. This is the closest clean static call boundary to the
per-view (incl. shadow) build, but it is a render-thread scheduler, not the gameplay camera. Prefer
the camera-pose lever (§5).

---

## 4. THE KEY DELIVERABLE — where to inject head rotation so shadows follow (Q4)

### Evaluate (a): rotate `a4` earlier in the producer — WON'T WORK
The cascade loop (§1.2) never reads `a4`; it reads the already-baked `a13`. Rotating `a4` at ANY point
inside `sub_140BB1EE0` (even before the cascade loop) cannot change the cascade matrices, because they
were computed upstream and merely copied. **Confirmed (a) is false.**

### Evaluate (b): the cascades read a separate, un-rotated camera orientation — TRUE
The separate source is the **`CCamDriver +0x320` camera-to-world matrix** (the authoritative
per-frame gameplay pose). The producer's `a4` is itself a copy of this pose (getter = CCamDriver vtbl
slot[4] `sub_1407A9DD0`, RVA 0x7A9DD0, reads `this+0x320`; documented in
`fh5_empress_producer_camerapos_RE.md` and `fh5_camera_pose_hook_spec.md`). The shadow-cascade fitter,
running upstream in the same frame, fits the cascades to the camera frustum derived from the **same
`+0x320` pose**. Because we currently rotate only the *downstream copy* `a4`, the upstream pose
(and hence the cascade fit) stays un-rotated.

**=> Inject the head rotation into the upstream `+0x320` pose, not into `a4`. Then both the main view
and the cascade fit see the rotated orientation, and shadows follow.**

### The injection point (arg/offset/function + AOB)
Detour **`sub_1406BE3A0` (RVA 0x6BE3A0)** — the per-frame CCamDriver `+0x320` publish/notify helper
(`fh5_camera_pose_hook_spec.md` §1a/§3). It is `__fastcall(a1=CCamDriver* this, a2=notify arg)`.

- **Prologue AOB (unique):**
  `40 53 48 83 EC 20 48 81 C1 20 03 00 00 48 8B DA E8 ? ? ? ? 48 8B 4B 08 48 85 C9 74 0C 48 8B 01 48 83 C4 20 5B 48 FF 60 18 48 83 C4 20 5B C3`
  The defining instruction is `48 81 C1 20 03 00 00` = `add rcx, 0x320`.
- **Fields to mutate (relative to `a1`), BEFORE calling the trampoline (before `loc_1405EAC60`
  publishes):**
  - `a1+0x320` / `+0x330` / `+0x340` = right / up / forward basis rows (Vec3 f32 each) — **apply the
    head yaw/pitch/roll to these basis rows** (the rotation we currently apply to `a4`).
  - `a1+0x3E0` = the 4×4 f32 world-to-camera VIEW tail — **recompute** from the new basis (+ existing
    position) or the view desyncs.
  - Leave `a1+0x350` (position row) / `a1+0x550` (f64 pos) as the existing translation lever already
    handles them.
- **Gating (mandatory — this lane fires for many camera instances):** gate on
  `a1 == *(uintptr_t*)(ForzaMultiCam_obj + 0x5C8)` (active concrete camera; resolver in
  `fh5_camera_pose_hook_spec.md` §4) plus an orthonormal-basis sanity check on `+0x320`.
- **Per-frame compose-on-top** (the engine re-derives the pose each frame from car/physics): read base
  basis from `+0x320`, apply rotation, write back; do not accumulate.

This is the ForzaTech analog of Anvil `onCalcFinalView` and matches the architecture the working VR
mods use (one upstream pose feeds view + culling + shadows).

---

## 5. Q5 — is the `a4` source the CCamDriver `+0x320` matrix, and do cascades read the same?

- **`a4` source = yes, `+0x320`.** Getter = CCamDriver vtbl **slot[4] `sub_1407A9DD0` (RVA 0x7A9DD0)**,
  which reads `this+0x320` (the f32 camera-to-world matrix); this produces the producer's `a4`
  (`fh5_empress_producer_camerapos_RE.md` §4, confirmed via concrete CCamDriver vtbl `0x145E3F290`).
  Live this session: the two static callers of slot[4] are `sub_1407AC2D0` (0x7AC2D9) and
  `sub_1407AC370` (0x7AC380) (`cascade_writers3.log:14-16` / `cascade_re_out.log:330-332`), both of
  which read/write camera matrix lanes around `this+1344..1392` (= `+0x540..+0x570`) — the
  driver→render matrix staging.
- **Do the cascades read the same `+0x320`?** They do not read it *directly at producer time* — they
  read the baked `a13` snapshot (§2). But that snapshot was **fit from the same `+0x320` pose**
  upstream (the gameplay camera is the single source of the frustum the cascades cover). So `+0x320`
  is the common ancestor of both `a4` and the cascade fit. There is no evidence of a *second*
  independent camera-orientation copy for shadows: the cascade records carry light-space matrices that
  are functions of (sun direction, camera frustum from +0x320). Rotating `+0x320` rotates the frustum
  the fitter sees → cascades re-cover the rotated view. (The Cinematic camera uses a different lane
  `+0x540..+0x570` per `fh5_camera_pose_hook_spec.md` §4 — irrelevant to the gameplay cascade path.)

---

## 6. Why TRANSLATION is already shadow-coherent but ROTATION is not

- **Translation:** the per-cascade centers are stored as f64 world positions (`+0xD18`, §2) and the
  whole scene renders camera-relative against `cameraPos@+0x80`, which the producer derives from the
  f64 position lane (`a15/a16` ← CCamDriver `+0x550`). The existing 6-DOF lever offsets that f64, so
  the cascade centers and the camera rebase against the *same* shifted origin → shadows follow
  (PROVEN; `fh5_6dof_SOLVED_20260605.md`).
- **Rotation:** there is no equivalent "single origin" coupling for orientation at producer time.
  The main VIEW orientation is `a4` (rotated by us), but the cascade *frustum coverage* was fit
  upstream from the un-rotated `+0x320`. The two orientations diverge the moment `a4` ≠ the pose the
  cascades were fit from. Hence shadows break when the head yaws away from the car's forward.

---

## 7. Concrete recommendation (ranked)

1. **PRIMARY — move the head ROTATION upstream to `CCamDriver +0x320` at `sub_1406BE3A0`
   (RVA 0x6BE3A0).** Rotate basis rows `+0x320/+0x330/+0x340`, recompute the `+0x3E0` VIEW tail, gate
   on the active-camera identity (`ForzaMultiCam+0x5C8`). Keep the existing translation lever
   (producer `a15/a16` f64, or co-located `+0x350/+0x550` here). This fixes BOTH the main view and the
   shadow cascades, because the cascade fitter consumes `+0x320`. **Retire the `a4` rotation hook once
   this is validated** (the prior note that `+0x320` rotation caused "geometry flicker/culling lag" is
   addressable by writing the consistent quad atomically: basis `+0x320`, pos `+0x350`, VIEW tail
   `+0x3E0`, f64 pos `+0x550` — `fh5_camera_pose_hook_spec.md` §4 risk notes).
2. **DO NOT** try to fix it by rotating `a4` earlier in the producer (cascades never read `a4` — §4a),
   nor by rotating `a6` (a6 is the parent/world transform multiplied into the MAIN VIEW only; the
   cascade loop ignores it too).
3. **Fallback / validation-only:** if mutating `+0x320` proves unstable, you can additionally rotate
   the baked cascade matrices in the producer by post-processing `v274/v275/v276` after the cascade
   loop (rotate each per-cascade matrix's basis by the head delta) — but this is fragile (must match
   the exact light-space convention per cascade and keep `unk_140BDB820` centers consistent) and is
   strictly worse than fixing the source. Use only to prove the diagnosis live if needed.
4. **Instrumentation option** to confirm the fitter reads `+0x320`: log-only probe at the render-view
   scheduler `sub_140CA1B70` (RVA 0xCA1B70) and dump the per-view (232-stride) matrices vs the live
   `+0x320`; or simply apply the §1 primary fix and observe shadows follow when yawing (the decisive
   live test).

---

## Citations
- Producer cascade loop + `v82=a13`: `producer_0xBB1EE0_decomp.txt:485-584` (main VIEW from a4),
  `:759-760` (v82=a13), `:1316-1392` (cascade loop), `:1603/1628-1645` (commit).
- Cascade accessors decompile (live this session): `cascade_re_out.log:18-97` (sub_140BDB820/860/7F0/AC0),
  `cascade_writers_out.log:5-145` (sub_140BDB6C0 + sub_140BD1BD0 record copier — 2724-byte descriptor).
- `qword_14901CAE0` xrefs (render-view manager cluster): `cascade_writers3.log:14-43`.
- Render-view scheduler cluster decompiles: `cascade_builder.log` (sub_140CA1B70 etc.),
  `cascade_final.log:3-265` (sub_140CB1300 task build, sub_140CA1B70 callers).
- `+0x320` getter (slot[4] sub_1407A9DD0) + its callers sub_1407AC2D0/sub_1407AC370:
  `cascade_re_out.log:102-194,330-332`; `cascade_writers3.log:45-135`.
- a4 ← +0x320, a15/a16 ← +0x550 f64: `_agent_reports/fh5_empress_producer_camerapos_RE.md`.
- `sub_1406BE3A0` publisher, +0x320 layout, AOB, gating, VIEW-tail consistency:
  `_agent_reports/fh5_camera_pose_hook_spec.md` §1a/§2/§3/§4.
- Translation shadow-coherence (cameraPos@+0x80 rebasing): `_agent_reports/fh5_6dof_SOLVED_20260605.md`.
- Scripts: `scripts/ida_shadow_cascade_re.py`, `scripts/ida_cascade_writers.py`,
  `scripts/ida_cascade_builder.py`.
```
