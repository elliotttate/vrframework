# FH5 (EMPRESS) — View-Source Object & Camera-Orientation RE

DB: `E:\tmp\fh5_re\fh5_b.i64`, base `0x140000000`, no anti-tamper. RTTI is **fully present**
(114,102 type descriptors; CCamDriver vtable `0x145E3F290` resolves to `.?AVCCamDriver@Camera@@`).
All scripts: `…\FH5CameraProbe\scripts\ida_viewsource_orient_*.py`. Raw outputs:
`E:\tmp\fh5_re\vs_phase{A..M}.out`.

RVA→EA = rva + 0x140000000.

---

## TL;DR — the injection point

The active camera (`CCamDriver`, vtable `0x145E3F290`) is a **look-at camera**. There is **no
stored orientation quaternion/Euler/matrix** on the view-source. The orientation is *derived
every frame* from two world-space float4 lanes on the camera object:

| Field | Offset | Meaning | Type |
|---|---|---|---|
| **aim point** | `CCamDriver + 0x540` | world point the camera looks AT | `f32[4]` (xyz + pad), little-endian, world units (~100/m) |
| **eye / anchor** | `CCamDriver + 0x530` | world point the camera sits AT | `f32[4]` |

Per frame the builder `S->vtable[0x68]` consumes **`(*(this+0x540) + *(this+0x530))`** (getter
path) — i.e. aimPoint relative to eye — and emits the camera-to-world **basis rows** into
`CCamDriver + 0x550..+0x568` (rows 0-2) and `+0x570` (row 3). Those rows become the producer's
`a4` (assembled by `sub_1407AC2D0`) and are also copied to the orthonormal `+0x320`.

**To inject head-look upstream (shadow-coherent): rotate the aim direction
`d = (*(this+0x540) − *(this+0x530))` by the head yaw/pitch/roll, then write
`*(this+0x540) = *(this+0x530) + R·d`.** Apply it just before the builder runs (hook the getter
`sub_1407A9DD0` @ `0x1407A9DD0`, or the per-frame fold `sub_1407A6300` @ `0x1407A6300`, on
entry). Because the builder output feeds BOTH the main view `a4` AND `+0x320` (and the producer
fits the shadow cascades from that same view basis), rotating the aim lane propagates to the
shadow cascades automatically. Roll is the one caveat (see Q4).

---

## Q1 — The view-source object's type & vtable

### The object chain
`sub_1407A9DD0` (the getter / per-frame republish; **EA `0x1407A9DD0`**, RVA `0x7A9DD0`) operates
on the `CCamDriver` (`a1`, an `__m128*`). It reads a nested object pointer at **`CCamDriver + 0x48`**
(`a1[4].m128_u64[1]`). Quoting the getter:

```c
v7 = (*(*(_QWORD *)a1[4].m128_u64[1] + 120LL))(a1[4].m128_i64[1]);   // S = *(this+0x48); S->vtbl[0x78]()
if ( v7 ) {
  v8 = (*(*(_QWORD *)a1[4].m128_u64[1] + 120LL))(a1[4].m128_i64[1]); // the "target/aim provider" sub-object
  (*(*(_QWORD *)v8 + 648LL))(v8, v20);                                // v8->vtbl[0x288](&aimFloats)
  v18 = _mm_add_ps(a1[84], a1[83]);                                   // input = *(this+0x540) + *(this+0x530)
  ...
  v15 = (*(*(_QWORD *)a1[4].m128_u64[1] + 104LL))(...,v19,&v18);      // S->vtbl[0x68]  ← THE BUILDER
  a1[85]=*v15; a1[86]=v15[2..3];                                      // → this+0x550..0x568 (basis rows 0-2)
  a1[87] = (*(a1->vtbl + 824))(a1, v19);                              // this->vtbl[0x338] → this+0x570 (row 3 blend)
}
```

So there are **two** objects:
* **`S` = `*(CCamDriver + 0x48)`** — the **view-source**. Its vtable holds the BUILDER at slot
  `0x68` (104) and a sub-object accessor at slot `0x78` (120).
* **`v8` = `S->vtable[0x78]()`** — a *target/aim provider* sub-object; its slot `0x288` (648)
  returns the aim-mode/float payload.

### Concrete vtable + RTTI
The camera objects are all in the `Camera@@` namespace and derive from `.?AVICamera@Camera@@`
(vtable **`0x145E3DED8`**). The concrete active gameplay camera is `.?AVCCamDriver@Camera@@`
(vtable **`0x145E3F290`**). All the follow variants share the same vtable shape:

```
CCamDriver  .?AVCCamDriver@Camera@@   vtable 0x145E3F290
CCamFollow  .?AVCCamFollow@Camera@@   vtable 0x145E3FC78   (+High/Low/Extended share builder)
CCamCockpit .?AVCCamCockpit@Camera@@  vtable 0x145E3E200
CCamHood    .?AVCCamHood@Camera@@     vtable 0x145E3EBF0
CCamFree    .?AVCCamFree@Camera@@     vtable 0x145E40D78   (overrides the builder)
ICamera     .?AVICamera@Camera@@      vtable 0x145E3DED8   (base; getter is at its slot 0x698)
```

(Built by `ida_viewsource_orient_g.py`: a single-pass MSVC RTTI COL→TD→vtable index —
99,803 COLs, 48,916 vtables; the camera list is at `vs_phaseG.out:684-732`.)

### Resolved vtable slots (shared by CCamDriver / CCamFollow / CCamCockpit / CCamHood)
| Slot | Hex | Function | Role |
|---|---|---|---|
| 104 | `0x68` | `sub_1405391C0` | **BUILDER** (base impl — returns const fwd row; CCamFree → `sub_1407A3A00`) |
| 112 | `0x70` | `sub_1407A4AA0` | projection/4th-row builder (FOV/aspect) |
| 120 | `0x78` | `sub_1407A9D40` | returns `*(this+1538)` (target-provider presence / sub-object) |
| 144 | `0x90` | `sub_1407A9320` | dispatch to `*(vtbl+664)` = `sub_1407A90C0` |
| 648 | `0x288`| `sub_1407A3B50` (=2) / `sub_1407A3B60` (=3) | aim-mode enum |
| 1680| `0x690`| `sub_1407A4270` / `sub_1407A1760` | misc |
| 1688| `0x698`| **`sub_1407A9DD0`** | the per-frame getter/republish |

> Note: the *base* `ICamera` builder `sub_1405391C0` is a 14-byte stub that just stores the
> constant row `xmmword_145E03280 = [0,0,1,0]`. The real orientation is **not** in the builder;
> it is carried by the aim/eye lanes the builder is *fed* (see Q2/Q4). CCamFree overrides slot
> 0x68 with `sub_1407A3A00`, which copies a stored 4×4 row from `this+0x5E0` — the free-cam is
> the only variant that holds an explicit orientation row.

### Who writes `*(CCamDriver + 0x48)`
The per-frame fold `sub_1407A6300` (RVA `0x7A6300`) sets it directly: line 1 is
`a1[4].m128_u64[1] = a2;` i.e. **`*(this+0x48) = a2`** where `a2` is the view-source passed in by
the framework update. The CCamCockpit ctor helper `sub_14079B750` (RVA `0x79B750`) sets
`*(this+0x48)`-region members and stores the target ref at `this+1544`.

---

## Q2 — The BUILDER and how it produces the pose

There are two layers; this is the key correction to the earlier "builder reads a stored
orientation" hypothesis:

**(a) The orientation is derived as a LOOK-AT, not read from a stored quaternion/matrix.**
The per-frame orientation combiner is **`sub_1407ABF60`** (RVA `0x7ABF60`), a `CCamDriver`/camera
method. Decoding its offsets (`a1+1328=0x530`, `a1+1344=0x540`, `a1+1364=0x554`):

```c
// pick the look DIRECTION v5 based on aim-mode *(this+0x5A0):
switch (*(this+0x5A0)) {
  case 0: v5 = normalize(xmmword_145E03EB0);                 // fixed dir const
  case 2: v5 = track/spline tangent (from S->vtbl[0x40]);    // pre-race / track cam
  case 3: v5 = S->vtbl[0x28](this, 2);                       // car basis row (forward)
  case 4: v5 = S->vtbl[0x28](this, 0);                       // car basis row
  case 5: v5 = -S->vtbl[0x28](this, 2);                      // reversed
  case 6: v5 = -S->vtbl[0x28](this, 0);
}
v32 = S->vtbl[0x08](this);                 // anchor/eye world position
*(this+0x530) = v32;                       // EYE
*(this+0x540) = (v5 * *(float*)(this+0x554)) + v32;   // AIM POINT = eye + dir*dist
```

i.e. **`+0x540` is `eye + lookDir·distance` (the AIM POINT)** and **`+0x530` is the EYE**.
`+0x540` is also written by `sub_1407A9880` (RVA `0x7A9880`, the "+0x540 writer / config
applier") from the camera-config direction fields `[cfg+0x3C..]`/`[cfg+0x54..]`/`[cfg+0x60..]`
packed via `unpcklps`/`movlhps` into a `f32[x,y,z,w]` — confirming the lane is a **direction/point
vector, not Euler angles** despite the earlier "euler/dir" guess.

**(b) The builder turns (aim − eye) into basis rows.**
`sub_1407A9DD0` feeds the builder `v18 = *(this+0x540) + *(this+0x530)` and stores the returned 4
qwords to `this+0x550..0x568`. The 4th row (slot 0x70 `sub_1407A4AA0`) is built from FOV/aspect
(it reads `vtbl+424`,`+416`,`+160` and packs a projection-ish row), and the blend row
(`this+0x338` slot → `sub_1407A34D0`) interpolates between two car-locator rows
(`S->vtbl[0x28](…,0)` and `…,2)`) by a scalar — that is the up/right blend used to bank the camera.

**Representation summary:** orientation = **look-at derived from a world AIM POINT (`+0x540`) and
EYE (`+0x530`)**, both `f32[4]` world-space. No quaternion, no Euler, no stored 3×3 on the
view-source (matching the earlier 0..0x640 orthonormal scan that found nothing).

---

## Q3 — Where the producer's `a4` basis comes from

`a4` is assembled by **`sub_1407AC2D0`** (RVA `0x7AC2D0`; ICamera-side variant `sub_1407AC370`
RVA `0x7AC370`). Quoting it:

```c
sub_1407A9DD0((__m128 *)a1, a2, a3);              // run the getter (rebuild pose lanes)
v4 = *(this+72);                                  // v4 = *(this+0x48) = view-source S
v5 = *v4;                                          // S vtable
v11 = _mm_unpacklo_ps(*(this+1584), *(this+1592)); // input = (this+0x630, this+0x638) extra dir lane
v6 = (*(v5+104))(v4, v12, &v11);                   // S->vtbl[0x68] BUILDER → basis rows
*(this+1360)=*v6; *(this+1368)=v6[1]; *(this+1376)=v6[2]; *(this+1384)=v6[3];  // → this+0x550..0x568
v11 = xmmword_145E03270;                           // (1,0,0,0)
result = (*(v9+112))(v8, v12, &v11);               // S->vtbl[0x70] → 4th row
*(this+1392) = *result;                            // → this+0x570
```

So **the producer `a4` basis rows are the BUILDER output** (`S->vtable[0x68]` fed by the aim/dir
lane) written to `CCamDriver + 0x550..+0x570`, then handed to the producer `sub_140BB1EE0`
(`0x140BB1EE0`, 0 static callers — indirect) as `a4`. The orthonormal `+0x320` copy is a
downstream snapshot of the same rows (built in `sub_1407A6300`/`sub_1407ABF60`), so it is *not*
the source — it is a sibling consumer.

Trace, camera→producer (all confirmed by xref/decompile):
```
camera-config dir  →  +0x540 / +0x630 (aim lane, sub_1407A9880 / sub_1407ABF60)
                   →  BUILDER S->vtable[0x68]  (fed (aim+eye))
                   →  basis rows  +0x550..+0x570
                   →  a4 (sub_1407AC2D0)  →  producer sub_140BB1EE0  →  main view + shadow cascades
                   →  (also copied to +0x320 orthonormal)
```

---

## Q4 — THE injection point (object + offset + representation + how to rotate)

**Object/offset:** `CCamDriver + 0x540` (aim point, `f32[4]`) and `CCamDriver + 0x530` (eye,
`f32[4]`). Active `CCamDriver` instance vtable = `0x145E3F290`.

**Representation:** world-space points (units ≈ 1 cm, ~100/m). The *orientation* the engine uses
is `forward = normalize(aim − eye)`; up/right come from the blend row (car-locator rows blended).

**How to apply a head rotation R (yaw/pitch/roll):**
```
eye = *(CCamDriver+0x530)            // float3
aim = *(CCamDriver+0x540)            // float3
d   = aim - eye                      // look vector (world)
d'  = R * d                          // rotate by head yaw/pitch/roll
*(CCamDriver+0x540) = eye + d'       // write back the rotated aim point
```
Hook on ENTRY of the getter **`sub_1407A9DD0` @ `0x1407A9DD0`** (AOB unique:
`48 8B C4 48 89 58 18 48 89 70 20 55 48 8D 68 A1 48 81 EC B0 00 00 00 0F`) or the per-frame fold
**`sub_1407A6300` @ `0x1407A6300`** (unique 28-byte AOB:
`48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 50 4C 8B F1 48 89 51 48`). Rewrite
`+0x540` before the builder reads it; the builder then produces the head-rotated basis, which
flows into `a4`, `+0x320`, and the cascade fit.

**Yaw + pitch** come out perfectly because look-at fully determines forward. **Roll** is the
caveat: look-at recomputes up from the blend row, so a pure aim-point edit cannot introduce roll.
For roll you must additionally rotate the blend/up row written at `CCamDriver + 0x570`
(`this->vtable[0x338]` → `sub_1407A34D0` output) about the forward axis after the getter — or,
simpler and fully general, keep the existing downstream producer-`a4` hook (already proven in
MEMORY to rotate the camera reversibly) for the *full* 3-DOF rotation and use this upstream aim
edit only when you specifically need the cascades to follow. Because **the cascade fitter reads
the same producer view basis**, applying R to the aim lane upstream is sufficient for
yaw/pitch shadow coherence; roll needs the extra `+0x570` row rotation.

---

## Q5 — Does the shadow-cascade fitter read the view-source orientation, `+0x320`, or `a4`?

The cascade fit lives in/under the producer `sub_140BB1EE0` (it appears in the 0xAB0/2736-byte
cascade-stride scan, and the cascade accessors `sub_140BDB820`/`sub_140BDB860` read per-cascade
matrices at `base + 2736*idx + 0x848/0xD18`). The cascades are fit from the **producer's view
basis = `a4`**, which is the BUILDER output — i.e. the *same* rows that come from the aim/eye
lanes. The fitter does **not** independently read `CCamDriver+0x320` (that is a sibling snapshot)
nor a separate stored view-source orientation (there is none).

**Consequence:** rotating the **aim lane `+0x540`** (upstream of the builder) is sufficient to make
the shadow cascades follow the head-look — exactly the goal. The downstream-only approaches
(editing `+0x320`, or hooking the producer `a4`) rotate the main view but, if applied *after* the
cascade fit, leave the shadows fit to the un-rotated frustum. The aim-lane edit is the correct
upstream, shadow-coherent injection.

---

## Function/EA index (cite-ready)

| Symbol | EA | RVA | Role |
|---|---|---|---|
| `CCamDriver` vtable | `0x145E3F290` | `0x5E3F290` | concrete active camera (`.?AVCCamDriver@Camera@@`) |
| `ICamera` vtable | `0x145E3DED8` | `0x5E3DED8` | base camera interface |
| `CCamFollow` vtable | `0x145E3FC78` | `0x5E3FC78` | follow cam (shares builder) |
| getter / republish | `0x1407A9DD0` | `0x7A9DD0` | ICamera slot 0x698; rebuilds pose; **hook point** |
| per-frame fold | `0x1407A6300` | `0x7A6300` | sets `*(this+0x48)`, runs builder; **hook point** |
| a4 assembler | `0x1407AC2D0` | `0x7AC2D0` | builds producer `a4` from builder output |
| a4 assembler (ICamera) | `0x1407AC370` | `0x7AC370` | variant |
| orientation combiner | `0x1407ABF60` | `0x7ABF60` | computes aim(`+0x540`) = eye + dir·dist |
| `+0x540` config writer | `0x1407A9880` | `0x7A9880` | applies cfg direction → `+0x540`/`+0x630` |
| `+0x530` writer | `0x1407A94B0` | `0x7A94B0` | resolves eye from `carLocator_bumperF` etc. |
| BUILDER (base/slot 0x68) | `0x1405391C0` | `0x5391C0` | returns const fwd row `[0,0,1,0]` |
| BUILDER (CCamFree) | `0x1407A3A00` | `0x7A3A00` | copies stored row `*(this+0x5E0)` |
| 4th-row/proj (slot 0x70) | `0x1407A4AA0` | `0x7A4AA0` | FOV/aspect row |
| blend row (slot 0x338) | `0x1407A34D0` | `0x7A34D0` | up/right blend of car-locator rows → `+0x570` |
| ctor helper | `0x14079B750` | `0x79B750` | inits `+0x530/+0x540`-region; sets ICamera vtable |
| producer | `0x140BB1EE0` | `0xBB1EE0` | consumes `a4`; fits shadow cascades |
| cascade matrix reader | `0x140BDB860` | `0xBDB860` | reads per-cascade view (stride 2736) |

Constants: `xmmword_145E03270 = [1,0,0,0]`, `xmmword_145E03280 = [0,0,1,0]`.

**Lane map (corrected):**
* `+0x530` (`f32[4]`) — **EYE / anchor world position**.
* `+0x540` (`f32[4]`) — **AIM POINT** (eye + lookDir·dist); the orientation INPUT.
* `+0x550..+0x568` — builder output **basis rows 0-2** (= producer `a4` rows 0-2).
* `+0x570` — **blend/up row 3** (`vtbl[0x338]` → `sub_1407A34D0`).
* `+0x320` — orthonormal cam-to-world per-frame **snapshot** (sibling consumer, not a source).
* `+0x630/+0x638` — extra dir lane used by the `sub_1407AC2D0` a4 path.
