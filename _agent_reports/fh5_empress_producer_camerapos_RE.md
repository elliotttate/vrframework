# FH5 (Empress build) — Producer `sub_140BB1EE0` & the upstream camera WORLD-POSITION lever

Date: 2026-06-05
DB: `E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe.i64` (image base `0x140000000`, NO anti-tamper).
RVA = VA − 0x140000000. All addresses below are from THIS Empress DB (verified live via idalib),
NOT the older Steam dump.

Method: headless idalib (`idapro.open_database(..., run_auto_analysis=False)`). Producer decompile
from the existing Empress shard `E:\ForzaHorizon5_CameraProbeLogs\producer_0xBB1EE0_decomp.txt`
(size `0x26A5`, decompiled clean in 2.17 s). Writer/vtable scans from
`scripts/ida_fast_writers_vtbl.py` → `_agent_reports/fh5_empress_writers_vtbl_FAST.md`. Helper/clone
decompiles from `scripts/ida_lever_finalize.py` → `_agent_reports/fh5_empress_lever_finalize.md`.

---

## TL;DR — the lever

**The camera WORLD position consumed by the producer to build VIEW@+0x40 and cameraPos@+0x80 is
a DOUBLE-PRECISION (f64) world transform passed INTO the producer as a pointer argument — the
`a15`/`a16` argument pair (4×f64 each) — NOT `a4.row3` and NOT `a17`/`a18`.**

- `a4` (the proven rotation lever) is the camera-to-world **rotation basis** (rows 0–2); its row3
  is a camera-relative origin ≈ 0 — that is exactly why editing `a4.row3` was INERT.
- `a17`/`a18` are optional **float4 cameraPos OVERRIDES** that, in the main path, are either null
  or set equal to the f64-derived value — which is why editing them was INERT too.
- The producer reads the **f64 world position from the pointer in `a15` (and `a16`)**, then does
  the classic large-world **double→float "integer tile + fractional" split** to build the
  camera-relative rebasing origin (`cameraPos@+0x80`) and the full VIEW translation
  (`= −R·cameraPos`). Shift that f64 source and everything downstream (VIEW, VP, cameraPos,
  culling, shadows) follows.

**Persistent source / race-free hook (recommended):** the f64 the producer dereferences lives in
the active camera object. In THIS build the **CCamDriver camera-to-world matrix is at `+0x320`
(f32; getter = vtbl slot[4] `sub_1407A9DD0`, RVA 0x7A9DD0) and the f64 world-position lane is at
`+0x550..+0x5D0` (getter = vtbl slot[12] `sub_1407A3C80`, RVA 0x7A3C80)** — both CONFIRMED in this
DB via the concrete CCamDriver vtable `0x145E3F290`. The cleanest deterministic point to apply an
offset is either: (A) **hook the producer `sub_140BB1EE0` and add the world offset to the f64 it
reads via `a15`/`a16`** (in-detour, on the camera/sim thread → race-free; simplest), or (B) **hook
the CCamDriver f64 getter `sub_1407A3C80`** so the persistent source carries the offset. See §4/§Rec.

---

## 1. Producer `sub_140BB1EE0` (RVA 0xBB1EE0) — signature & 25-arg map

Prologue AOB (verified, this DB):
`44 89 44 24 18 48 89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 18 D9 FF FF`
(= `mov [rsp+18],r8d; mov [rsp+10],rdx; push rbp/rbx/rsi/rdi/r12..r15; lea rbp,[rsp-...]`).

IDA-inferred prototype (25 args, `__fastcall`). The float/double/pointer types are reliable; the
exact semantic role is annotated from the body + the existing A7 projection map:

```c
void __fastcall sub_140BB1EE0(
    int *a1,        // [1] BIG render-view descriptor object (read as a1[...] ints,
                    //     *((_OWORD*)a1+N)); holds flags, FOV-scales, clip, blend, etc.
    __int64 a2,     // [2] aux ptr (saved to [rsp+10])
    int a3,         // [3] view/eye index-ish flag (a3 != 1 gates a path)
    __m128 *a4,     // [4] CAMERA-TO-WORLD matrix (row-vector; rows0-2 = right/up/fwd basis;
                    //     row3 = CAMERA-RELATIVE origin ≈0). *** ROTATION lever (proven) ***
    double a5,      // [5] f64 — paired with a15/a16 as world-position source (see below)
    __m128 *a6,     // [6] second matrix (world/parent transform; multiplied with a4 to form VIEW)
    __m128 *a7,     // [7] PROJECTION (row-major row-vector; FOV @[0][0]/[1][1]) — see A7 doc
    float a8,       // [8] NEAR (=0.1)   reverse-Z
    float a9,       // [9] FAR  (=50000) reverse-Z; depth params = far/(far-near), near*that
    __int128 *a10,  // [10] vec/quad input (stored into v328)
    int a11,        // [11]
    unsigned a12,   // [12] view-bucket key (used in sub_140BA4CE0(&a1[38*idx+82], a12))
    __int64 a13,    // [13] MULTI-VIEW / cascade provider (v82); yields PER-VIEW f64 world coords
                    //      via unk_140BDB820 + matrices via sub_140BDB860 (shadow/split path)
    _QWORD *a14,    // [14] callback sink object (vtbl+0x38 invoked with the built blocks)
    double a15,     // [15] *** f64 WORLD-POSITION SOURCE (pointer-in-double): read as
                    //      **(_QWORD**)&a15 → 4×f64 at +0,+8,+16,+24. THE MAIN POS LEVER ***
    __int64 a16,    // [16] *** second 4×f64 world source (read as *(double*)a16 .. +24) ***
    __m128 *a17,    // [17] optional cameraPos OVERRIDE float4 (if non-null → v204=*a17, w=1)
    __m128 *a18,    // [18] optional cameraPos OVERRIDE float4 #2 (if non-null → v414=*a18, w=1)
    float a19,      // [19] scalar (1/a19, 1/a19^2 packed into v333 — likely inv-resolution/scale)
    __int128 *a20,  // [20]
    __int128 *a21,  // [21]
    unsigned a22,   // [22]
    unsigned a23,   // [23]
    int a24,        // [24]
    __int64 a25);   // [25] quality/settings object (v86); gates jitter/TAA, near/far overrides
```

### The cameraPos / VIEW-translation data flow (producer body, exact lines)

Source line numbers below are into `producer_0xBB1EE0_decomp.txt`.

1. **World position enters as f64 via the `a5`/`a15`/`a16` group** (lines 425–462):
   ```c
   v28 = a5;
   if ( a15 == 0.0 ) {                      // degenerate fallback: derive from a4 via sub_140631F90
       v218[0..3] = a4 rows;  sub_140631F90(v303, v213, v218);
       ... a15/a5 rebuilt from that ...
   } else {                                 // *** MAIN PATH ***
       v29 = **(_QWORD**)&a15;              // a15 is a POINTER to 4×f64 (world coords)
       v30 = *(_QWORD*)(*(_QWORD*)&a15 + 8);
       v31 = *(_QWORD*)(*(_QWORD*)&a15 + 16);
       v32 = *(double*)(*(_QWORD*)&a15 + 24);
       retaddr = *(double*)a16;             // a16 is a SECOND pointer to 4×f64
       a15 = *(double*)(a16+16);  a5 = *(double*)(a16+24);
   }
   v411=v29; v413=v32; v412=v31;            // stash the 4 doubles
   v35 = cvtpd_ps(v29,v30,v31,v32);         // f64 → f32 camera position float4
   v211 = v35;  v204 = v35;                 // default cameraPos
   if (a17) { v209=*a17; v209.w=1; v204=v209; }   // a17 OVERRIDES (INERT in main path)
   v414 = v35;
   if (a18) { v210=*a18; v210.w=1; v414=v210; }   // a18 OVERRIDES (INERT in main path)
   ```

2. **VIEW = a4 × a6** (lines 485–574): builds the 4×4 world-to-camera basis in `v307[4..7]`.

3. **VP = VIEW × a7** (lines 633–662): row-vector multiply, `v404..v407`.

4. **Camera-relative ORIGIN split** (lines 669–716): each of the 8 doubles (from a15+a16) is split
   into integer part `(double)(int)x` and fraction `x − (int)x`, then `cvtpd_ps` packed into
   `v402` (integer/tile floats) and `v403` (fractional floats). **This is the rebasing origin that
   becomes `cameraPos@+0x80` (and its several duplicate slots) in the 6912-byte CBV.** Geometry is
   then rendered camera-relative (`clip = camRelVP × (worldPos − cameraPos)`).

5. **Depth params** from a8/a9 (lines 1028–1032): `C = a9/(a9−a8)`, `v309 = {near, far, C·near, C}`.

6. **Multi-view / cascade path** (lines 1322–1392, gated on `a13`/`v82`): iterates and calls
   `unk_140BDB820(v82,&v215,i)` returning **per-view f64 world coords** (again split int+frac at
   lines 1346–1372) and `sub_140BDB860` for matrices — this is the shadow-cascade / split-screen
   world-pos provider (separate from the main a15/a16 path).

7. **OUTPUT commit** (lines 1408, 1630, 1645): the assembled view-constants block `v307`
   (9×OWORD = VIEW[0..3], VP[4..7], +1) is memcpy'd via `unk_144D392C4(dst, v307, 6800)` into
   either `a1+768` (when the view index `v413==18`, the main view) or a per-view slot
   `&a1[38*v413+82]`. **6800 (0x1A90) bytes ≈ the 6912 render CBV.** This is the real per-frame
   committer (the old `sub_141017C30` is a dead/cold setter — confirmed `calls=0` live).

**Conclusion (Query 2):** the value that builds the VIEW translation and `cameraPos@+0x80` is the
**f64 world transform pointed to by `a15` (+`a16`)**. `a4.row3`, `a17`, `a18` are all
non-load-bearing for the rebasing origin in the normal gameplay path — matching the live "INERT"
results.

---

## 3. One level in: `sub_140631F90` (the camera-relative transform helper)

Used 4× by the producer (lines 432, 439, 1361, 1475) to transform a 4-row matrix/position by a
reference (`v213`/`v211`). Prologue AOB (verified):
`48 8B C4 48 81 EC 98 00 00 00 41 0F 10 50 20 0F 29 70 E8 …` (RVA 0x631F90, size 0x2FD). Role: it
composes the f64-derived reference origin with a matrix to produce the camera-relative rows that go
into `v274`/`v275`/`v276` and the VIEW block (the engine's "make-relative-to-camera-origin"
primitive). (The headless force-decompile only produced a JUMPOUT shell because the body needed full
re-linearization; the AOB + role are confirmed and sufficient — it is not a hook target anyway.)

---

## 5. Sanity check (Query 5) — the MEMORY constants are VALID in this Empress DB

(from `fh5_empress_writers_vtbl_FAST.md`)

| Constant | Result in Empress DB |
|---|---|
| CCamDriver concrete vtable `0x145E3F290` | **VALID** — in `.rdata` (`off_145E3F290`); slots = real funcs (`[0]→sub_1407A16A0`, `[1]→sub_1407A6F70`, …); RTTI col @ vt−8 = `0x146899FD0`; constructed at `0x14079B8EE` in `sub_14079B8A0` |
| ForzaMultiCam vtable A `0x1465D6808` | **VALID** — `.rdata`; slots real (`[0]→sub_144A1D570`, `[2]→sub_140602220`…); RTTI col = `0x147317D38` |
| ForzaMultiCam vtable B `0x1465D6BA0` | **VALID** — `.rdata`; slots real (`[0]→sub_1405F4360`…); RTTI col = `0x147317DD0` (distinct from A) |

All three constants from MEMORY are confirmed for this build.

---

## 4. Writers/readers of the camera-object lanes (+0x320 / +0x550 / +0x5B8..+0x5C8 / +0x660)

The raw displacement-byte scan (`fh5_empress_writers_vtbl_FAST.md`) is **too noisy** to pinpoint
the writer: `+0x320`/`+0x350`/`+0x550` are generic struct offsets reused by hundreds of classes
(enums/refcounts) and by large strided memset/clone functions whose internal offsets collide with
the camera offsets. **Verified false positives** (do NOT use as camera writers):
- `sub_140592800` (RVA 0x592800) — a 16 KB **bulk grid initializer** (memset-like; fills
  `a1+0..+0x4000` with a repeating 64-byte `(a2,a3,a4)` pattern). The `[rcx+320h]/[+350h]/[+550h]`
  the scan flagged are just strided fill offsets, not a camera pose. CONFIRMED by decompile.
- `sub_14072CA00` (RVA 0x72CA00) — a bulk **object clone** (`a1 ← a2`, OWORD-by-OWORD). CONFIRMED.
- `sub_140BD1F70` (RVA 0xBD1F70) — a pose/aggregate **clone** between two objects. (Both clones —
  do NOT hook.)

**Reliable result — the CCamDriver vtable pose GETTERS (these feed the producer's a4 / a15):**
By scanning the *concrete CCamDriver vtable* `0x145E3F290` slots for camera-offset references
(robust, not noisy):
- **slot[4] = `sub_1407A9DD0` (RVA 0x7A9DD0)** — reads **`this+0x320`** → the f32 camera-to-world
  matrix getter (this produces the producer's `a4`).
- **slot[12] = `sub_1407A3C80` (RVA 0x7A3C80)** — reads **`this+0x550`** → the f64 world-position
  getter (this produces the producer's `a15`/`a16` f64 source).
- f64 lane accessors on the same object: `sub_1407A9210` (RVA 0x7A9210) reads/writes the f64 quad
  at `(double*)this+183..186` (= **`this+0x5B8..+0x5D0`**) and round-trips it through float; its
  prologue AOB is `40 53 48 83 EC 70 F2 0F 10 89 D0 05 00 00 …` (the `movsd xmm1,[rcx+5D0h]`
  immediately identifies the f64 lane). `sub_1407A9880` (RVA 0x7A9880) writes a `+0x540` aux lane.

So the **CCamDriver object holds** (this build): `+0x320` f32 camera-to-world matrix (getter
slot[4]), and the f64 world-position lane around `+0x550..+0x5D0` (getter slot[12]). These getters
are what the engine invokes to fill the producer's `a4` and `a15`/`a16` each frame. (A dedicated
per-frame *writer* of these lanes could not be isolated statically because of offset-collision
noise; it is reached indirectly. The producer hook below sidesteps the need for it.)

---

## Recommendation — which value to modify, where, how (race-free)

Two viable upstream hooks, in order of preference:

### A. (Simplest, race-free) Hook the producer `sub_140BB1EE0` and offset the f64 it reads
- **Hook EA:** `0x140BB1EE0` (RVA 0xBB1EE0). Prologue AOB (above) is unique.
- In the detour, before calling the trampoline, read the 4×f64 at `*(double(*)[4])(a15)` and
  `*(double(*)[4])(a16)`, **add the world-space offset** (forward·fb + right·lr + up·ud +
  right·±ipd/2, basis taken from `a4` rows0-2) to the position components, write back, then call
  original. Because the producer runs on the camera/sim thread, this is naturally race-free and it
  is strictly UPSTREAM of rebasing → cameraPos@+0x80, VIEW, VP, culling and shadows all follow.
- Gate: apply only when near≈0.1 (`a8∈[0.08,0.2]`) AND far≈50000 (`a9∈[45000,55000]`) — the same
  identity gate the existing a4 rotation path uses (it already isolates the main gameplay view).
  Keep the proven `a4` rotation hook for orientation; add translation here.
- Caveat: `a15`/`a16` point into engine-owned memory that is re-derived next frame, so mutate
  **per-frame, compose-on-top** (add to the freshly-produced value each call; do not accumulate).

### B. (Object-level) Hook the CCamDriver pose GETTER `sub_1407A3C80` (reads +0x550 f64) and/or
###    `sub_1407A9DD0` (reads +0x320 matrix), and offset the value it returns
- These are the **confirmed** vtable getters (CCamDriver concrete vtbl `0x145E3F290` slots 12 and 4)
  the engine calls to build the producer's `a15`/`a16` (f64 world pos) and `a4` (matrix). Detour
  `sub_1407A3C80` (RVA 0x7A3C80) and add the world offset to the f64 position before it is returned;
  this moves the persistent source so EVERY consumer (producer, culling, audio) sees the shift.
- The persistent storage lives in the CCamDriver object: `+0x320` f32 camera-to-world matrix
  (row3 = position), f64 world-position lane at `+0x550..+0x5D0`. Gate on `this` == the active
  camera (resolve via the ForzaMultiCam active-camera slot) and an orthonormal-basis sanity check of
  `this+0x320`. A clean per-frame *writer* to detour could not be isolated statically (offset
  collision noise), so hooking the getter is the reliable object-level lane.
- This is the ForzaTech analog of Anvil `onCalcFinalView`. More invasive than A (needs gating + the
  getter may be called for multiple cameras/frame). Prefer A unless you need the persistent object to
  carry the offset for non-render consumers.

Note: the producer `sub_140BB1EE0` has **0 static call sites** — it is dispatched indirectly
(function-pointer/scheduler), so there is no caller to hook to build `a4/a15/a16`. Hook the producer
itself (A) or the getter (B).

**Do NOT** edit `a4.row3`, `a17`, or `a18` (proven inert), and do NOT hook `sub_141017C30` (dead).

---

## Citations
- Producer decompile: `E:\ForzaHorizon5_CameraProbeLogs\producer_0xBB1EE0_decomp.txt`
  (lines 10–35 signature; 425–462 f64 source; 467–484 a17/a18 overrides; 485–574 VIEW; 633–662 VP;
  669–716 cam-rel split; 1028–1032 depth; 1322–1392 cascade; 1408/1630/1645 commit).
- A7 projection field map: `docs/A7_PROJECTION_FIELD_MAP.md` (a7=proj, a8=near, a9=far, a17/a18=campos).
- Writer/vtable scan: `_agent_reports/fh5_empress_writers_vtbl_FAST.md` (Query 5 vtables; +0x320/350/550/660 accessors).
- Helper/clone/writer decompiles + producer callers + pose-writer EA/AOB:
  `_agent_reports/fh5_empress_lever_finalize.md`.
- Scripts: `scripts/ida_fast_writers_vtbl.py`, `scripts/ida_lever_finalize.py`.
