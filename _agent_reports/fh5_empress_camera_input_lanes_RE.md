# FH5 (Empress build) — Camera Input Lanes & Per-Frame Camera-Build RE

DB: `E:\tmp\fh5_re\fh5_b.i64` (image base `0x140000000`, no anti-tamper).
RVA → EA = rva + 0x140000000.  Decompiler: idalib / Hex-Rays (set_max_time 180).
Scripts (single-DB-open each):
`scripts/ida_camera_input_lanes.py`, `…pass2.py`, `…pass3.py`, `…pass4.py`, `…pass5.py`.
Raw logs alongside this file (`fh5_empress_camera_*_rawlog.md`).

Scripts (follow-up): `scripts/ida_camera_followup.py`, `…followup2.py`
(logs `fh5_empress_camera_followup*_rawlog.md`).

---

## FOLLOW-UP (hook implementation answers)

**1) UNIQUE entry AOB for `sub_1407A6300` (RVA 0x7A6300)** — 28 bytes, verified `count==1`
across both `.text` segments (24-byte prologue was count=30; the next insn `mov [rcx+48h],rdx`
makes it unique):
```
48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 50 4C 8B F1 48 89 51 48
```
Covers `0x7A6300..0x7A6318`. Resolve by AOB with fallback RVA `0x7A6300`.

**2) Per-frame dispatch.** `sub_1407A6300` has 4 xrefs: one real wrapper `sub_1407A6430`
(= **CCamDriver vtable[2]**, slot `0x145E3F2A0`) which is invoked **virtually** (no direct
callers — dispatched off `this->vtbl[2]`), plus 3 JUMPOUT thunks (`sub_1407A6DC0/6DF0/6E30`,
themselves vtable slots `0x145E3E8B0`/`0x145E3F5F0`/`0x145E3EF50` — adjacent camera-mode
vtables). `sub_1407A6300` is itself installed directly at vtable slots `0x145E3E210`,
`0x145E3E560`, `0x145E3EC00`. So it is a **per-camera-object virtual method**: it fires once
per *active* camera object per update, not in a tight loop. Hooking the function entry will
fire for **every** camera object that runs its update (cockpit/chase/replay/etc. each have a
CCamDriver-derived `this`). **You must gate by `this` (rcx)** — identify the active gameplay
camera (e.g. the one whose vtable is the concrete CCamDriver `0x145E3F290`, or match the
known active-camera `this`) and only apply your offset to it; otherwise you'll move every
camera. Expect a small handful of hits/frame, not hundreds.

**3) Writers of `[obj+0x540]` — the engine DOES populate it.** Besides init `sub_140C98BA0`
and copy `sub_14072CA00`, three **CCamDriver-class vtable methods write `[this+0x540]`**:
| writer (RVA) | vtable slot | write |
|---|---|---|
| `sub_1407A9880` `0x7A9880` | `0x145E3F5C0` | `movups [rbx+540h], xmm5/xmm3` (×4) |
| `sub_1407A9B60` `0x7A9B60` | `0x145E3EF20` | `movups [rcx+540h], xmm0`; `mov [rcx+540h], eax` |
| `sub_1407ABF60` `0x7ABF60` | `0x145E41978` | `movups [rsi+540h], xmm1` |

(The `sub_141033xxx` writers at `0x141032D7A`/`0x141033163` are the channel router writing a
*different* object's +0x540, not in the cam cluster; the rest of the 39 hits are unrelated
structs.) Crucially, **the fold `sub_1407A6300` only READS +0x540 (`0x7A6382`) — it never
writes it first** (verified: writes=[] reads=[0x7A6382] for the fold and all its wrappers).
=> **The engine writes the base camera-space offset into +0x540 each frame via those vtable
setters, THEN the fold reads it.** Therefore: **ADD your VR offset to the existing +0x540 and
restore it after the original fold returns** (read original at hook entry, write
`orig + vr_offset`, call original, write `orig` back). Do NOT blind-overwrite or you clobber
the engine's own camera-space offset (CamShake / look-around / etc.).

**4) Width/type at +0x540.** The fold reads it as **128-bit / 4× f32** (`movups xmm0,[+0x540]`)
and `addps` to `[+0x530]` (also 128-bit, camera-space) → camera-space confirmed. The lane is a
**4-float vector**; lanes [0..2] = XYZ camera-space offset, lane [3] is present in the 16-byte
slot but is a w/pad term (the engine's default identity rows write `[…,0]`/`[…,1]` w-components;
treat +0x540 as `{x,y,z, w}` with w left at its engine value — write only x/y/z for translation,
preserve the existing 4th float when you add-and-restore). Result of the transform is written as
a full 32-byte block at +0x550..+0x568 plus +0x570 (the derived matrix rows — engine-owned).

---

## TL;DR / recommendation

- The FH6 lanes exist on this build, **but the `CameraSpace*Offset` / `HeadTracking*` strings
  are a CHANNEL-SOURCE ENUM**, not direct struct-offset names (see §1). They are a
  `{name_ptr, enum_id}` data table, no code xrefs — a data-driven input router.
- The **live, hookable translation lane is the camera object field at `+0x540`** (a 4-float
  camera-space offset vector). The per-frame camera method **`sub_1407A6300` (RVA `0x7A6300`)**
  does, register-relative on the CCamDriver `this` (r14):
  `xmm0 = [this+0x540] + [this+0x530]` → transform → write the live matrix block at
  `[this+0x550 .. +0x568]` and `[this+0x570]`, which then folds into the `+0x320` camera-to-world.
- **Write your translation offset into `CCamDriver+0x540` (3–4 floats) on the sim/camera thread,
  right before the engine consumes it.** Hook the per-frame consumer `sub_1407A6300`
  (or its CCamDriver vtable[2] wrapper `sub_1407A6430`) and set `+0x540` at function entry.
- `+0x540` is the ADD lane; `+0x550..+0x570` is the DERIVED OUTPUT (do not write it — same trap
  as `+0x320`). This matches the FH6 finding that the `+0x540` family moved the camera as TRANSLATION.

---

## 1. Lane strings → CHANNEL ENUM table (Query 1)

All requested names exist in `.rdata` as a contiguous `{char* name, u64 enum_id}` (16-byte) array.
They have **no direct code xref** (pure data table consumed by a reflection/router) — the registrar
that walks them is the big data-driven builder `sub_141033D80` (RVA `0x1033D80`), which references
the `"CameraOffset"` string (`0x145ED4E60`) and assembles these channels into stack descriptors.

| name string (EA)                              | enum_id | ptr-slot (EA)   |
|-----------------------------------------------|--------:|-----------------|
| `CarSpaceXYZImpulse`  `0x145E43020`           | 4       | `0x145E41DE0`   |
| `CameraSpaceYPROffset`  `0x145E43038`         | 5       | `0x145E41DF0`   |
| `CameraSpaceYPRImpulse`  `0x145E43050`        | 6       | `0x145E41E00`   |
| `CameraSpaceXYZOffset`  `0x145E43068`         | 7       | `0x145E41E10`   |
| `CameraSpaceXYZImpulse`  `0x145E43080`        | 8       | `0x145E41E20`   |
| `FOV`  `0x145E3DAB0`                           | 9       | `0x145E41E30`   |
| `CameraSpaceYPRMultiplier`  `0x145E43190`     | 27 (0x1B)| `0x145E41F50`  |
| `CameraSpaceXYZMultiplier`  `0x145E431B0`     | 28 (0x1C)| `0x145E41F60`  |
| `CameraSpaceYPRSet`  `0x145E431D0`            | 29 (0x1D)| `0x145E41F70`   |
| `CameraSpaceXYZSet`  `0x145E431E8`            | 30 (0x1E)| `0x145E41F80`   |
| `WorldSpace`  `0x145E43200`                    | 0       | `0x145E41F90`   |
| `CarSpace`  `0x145E43210`                      | 1       | `0x145E41FA0`   |
| `ShoulderRoll`  `0x145E42AF8`                  | 30 (0x1E)| `0x145E42340`  |
| `HeadHomeOffsetX`  `0x145E42B08`              | 31 (0x1F)| `0x145E42350`   |
| `HeadHomeOffsetY`  `0x145E42B18`              | 32 (0x20)| `0x145E42360`   |
| `HeadHomeOffsetZ`  `0x145E42B28`              | 33 (0x21)| `0x145E42370`   |
| `HeadTrackingX`  `0x145E42B38`                | 34 (0x22)| `0x145E42380`   |
| `HeadTrackingY`  `0x145E42B48`                | 35 (0x23)| `0x145E42390`   |
| `HeadTrackingZ`  `0x145E42B58`                | 36 (0x24)| `0x145E423A0`   |
| `HeadTrackingYaw`  `0x145E42B68`             | 37 (0x25)| `0x145E423B0`   |
| `HeadTrackingPitch`  `0x145E42B78`           | 38 (0x26)| `0x145E423C0`   |
| `OneWhenStationary`  `0x145E42B90`           | 39 (0x27)| `0x145E423D0`   |
| `PitchAngleDegrees`  `0x145E42BA8`           | 40 (0x28)| `0x145E423E0`   |

Other camera strings (with code xrefs):
- `"HeadTrackingThread"` `0x145E59780` ← `sub_14009B500` (`0x9B500`)
- `"MaxTraceDistanceIsCameraSpace"` `0x145E9F238` ← `sub_140CD4690` (`0xCD4690`)
- `"CameraOffset"` `0x145ED4E60` ← `sub_141033D80` (`0x1033D80`)  (the channel registrar)
- `"CameraOffsetPerItem"` `0x1461E4480` ← `sub_142D35FB0` (`0x2D35FB0`)

**Interpretation:** the named lanes are *sources* fed into the camera input router by enum id.
The router deposits the resulting vectors into the live camera object's offset fields. On THIS
build the live storage that the per-frame build actually reads is the object field `+0x540`
(camera-space XYZ offset, 4 floats) — confirmed in §2/§3.

---

## 2. Per-frame CCamDriver build / fold function (Query 2)

### Primary consumer — `sub_1407A6300` (RVA `0x7A6300`), the ADDITIVE FOLD
CCamDriver-cluster virtual method (`r14` = `this`). Full body (size 0x121):

```
0x7A6315  mov   r14, rcx                      ; this
0x7A635E  call  [rax+328h]                    ; vtbl: refresh base pose
0x7A636A  mov   rbx, [rax+330h]               ; vtbl slot
0x7A6380  call  rbx                           ; compute base camera-space value into [this+0x530]
0x7A6382  movups xmm0, [r14+540h]             ;  <-- LOAD +0x540 OFFSET LANE (4 floats)
0x7A638F  addps  xmm0, [r14+530h]             ;  xmm0 = base(+0x530) + offset(+0x540)
0x7A63A2  movaps [rsp+20h], xmm0
0x7A63A7  call  [rax+68h]                     ; transform(sum) -> rax => result rows
0x7A63B2  mov   [r14+550h], ...               ;  write derived matrix block
0x7A63BD  mov   [r14+558h], ...
0x7A63C8  mov   [r14+560h], ...
0x7A63D6  mov   [r14+568h], ...               ;  (+0x550..+0x568 = 32B / two rows)
0x7A63E0  call  [rax+338h]                    ; vtbl: fold into cam2world
0x7A63F8  movups [r14+570h], xmm0             ;  +0x570 result row
0x7A6400  call  [rax+20h]                     ; commit
```
Caller (CCamDriver **vtable[2]** = slot `0x145E3F2A0`): `sub_1407A6430` (`0x7A6430`):
```c
sub_1407A6430(this){ sub_1407A6300(this); return (*(this->vtbl[4]))(this, …); }
```
`sub_1407A6300` is also installed at vtable slots `0x145E3E210`, `0x145E3E560`, `0x145E3EC00`
(ForzaMultiCam / camera-mode vtables adjacent to CCamDriver `0x145E3F290`).

### Per-frame state propagator — `sub_14072CA00` (RVA `0x72CA00`)
A field-by-field camera-state copy (`a1`=dst, `a2`=src) invoked 6× per frame from
`sub_14072B120` (`0x72B120`). It copies the matrix block AND the offset lanes together,
proving they live in one contiguous camera-state struct:
```
0x72CD45  movups [r11+320h], xmm0   ; cam2world row0  (offset 0x320 = dec 800)
0x72CD55  movups [r11+330h], xmm1   ; … +0x330/+0x340/+0x350/+0x360 (full 4x4 + tail)
0x72CFAA  movups [r11+540h], xmm1   ; +0x540 OFFSET LANE  (dec 1344)  <-- copied with matrix
0x72CFBA  movups [r11+550h], xmm0   ; +0x550 DERIVED      (dec 1360)
```
(In its decompile: `*(_OWORD*)(a1+1344)=*(_OWORD*)(a2+1344);` and `+1360` — i.e. +0x540/+0x550.)

### Reset/identity initializer — `sub_140C98BA0` (RVA `0xC98BA0`)
Writes identity rows from consts into `[rcx+0x300..+0x570]` (the same block), incl.
`movups [rcx+540h],xmm0; movups [rcx+550h],xmm1`. This is init, not the per-frame fold —
do not hook for the lever.

---

## 3. Which lane drives TRANSLATION + exact offsets (Query 3)

From `sub_1407A6300`:
- **`+0x540` (CCamDriver object, 4× f32, camera-space XYZ)** is the **ADDITIVE TRANSLATION lane**:
  `result = transform( base[+0x530] + offset[+0x540] )`. Writing a nonzero vector at `+0x540`
  shifts the camera position by exactly that (camera-space) amount before the matrix is built.
- `+0x530` is the engine-computed base camera-space value (do not write; recomputed each frame).
- `+0x550 .. +0x570` is the DERIVED output (matrix rows). Same "derived, writing it is futile"
  trap as `+0x320` — do NOT write these.
- `+0x320 .. +0x368` is the final camera-to-world block downstream; folded from the above.

Axis/sign: the offset is applied in **camera space** (added pre-transform). The reset constants
at `0x145DD9C40..C90` carry identity rows plus a `[0,0,0,-1]` / `-1` term (basis Z-flip,
consistent with the documented `B = diag(1,1,-1)` engine convention). For VR head translation,
write the head delta (meters → world units ≈ ×100 per memory) into `+0x540` as `[x, y, z, 0]`
in camera space; verify sign empirically (start with +X = right, +Y = up, −Z = forward and flip
per the −1 basis term if it goes the wrong way).

Object-relative confirmation (pass3, same base register for both write+read):
| fn (RVA) | matrix write | lane access | base reg |
|---|---|---|---|
| `sub_140C98BA0` `0xC98BA0` | `[rcx+320h]`,`[rcx+330h]` | `[rcx+540h]`,`[rcx+550h]` (init) | rcx |
| `sub_14072CA00` `0x72CA00` | `[r11+320h..+360h]` | `[r10/r11+540h]`,`[+550h]` (copy) | r10→r11 |
| `sub_1407A6300` `0x7A6300` | folds via vtbl | `[r14+540h]` read → `[r14+550h]` write | r14 (this) |

---

## 4. Hookable AOBs (Query 4)

All verified unique-count via byte scan of both `.text` segments unless noted.

- **`sub_1407A6300`** (RVA `0x7A6300`) — the additive-fold consumer (HOOK TARGET):
  - prologue (24B), count in image = 30 (NOT unique alone — anchor by RVA or extend):
    `48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 50 4C 8B F1`
  - To make it unique, anchor on the body site (`movups xmm0,[r14+540h]; addps xmm0,[r14+530h]`):
    at `0x7A6382`: `0F 10 86 40 05 00 00  0F 58 86 30 05 00 00`
    (count in image = 1 — unique).

- **`sub_1407A6430`** — CCamDriver vtable[2] wrapper that calls the fold (alt hook, cleaner this):
  it is the **value at vtable slot `0x145E3F2A0`** (CCamDriver vtbl `0x145E3F290` + index 2).
  Hook by patching the vtable slot, or AOB at entry (it is a tiny thunk → call sub_1407A6300).

- **`sub_14072CA00`** (RVA `0x72CA00`) — per-frame state copy (carries +0x540), prologue (22B), **count=1 UNIQUE**:
  `48 8B 41 10 4C 8B D2 4C 8B D9 4C 63 48 04 48 8B 42 10 4C 63 40 04`
  Its per-frame caller `sub_14072B120` (`0x72B120`) prologue (24B) **count=1 UNIQUE**:
  `48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 48 F4 FF FF`

- **`sub_1407A6460`** (RVA `0x7A6460`) — ForzaMultiCam init (sets up object, negates pos into
  `a1[88]`), prologue (24B) **count=1 UNIQUE**:
  `40 53 55 57 41 57 48 81 EC 98 00 00 00 4C 8B F9 48 89 51 48 4C 89 41 50`

- **`sub_1407A16A0`** (RVA `0x7A16A0`) — CCamDriver vtable[0] = dtor → `operator delete(this,1616)`
  ⇒ **sizeof(CCamDriver) = 0x650**, so `+0x540`/`+0x550` are valid in-object fields.

---

## 5. Sanity check — CCamDriver vtable & fields (Query 5)

CCamDriver concrete vtable **`0x145E3F290`** is in `.rdata`, COL at `[-8]` = `0x146899FD0`,
slots all point into `.text`:
```
[0] 0x1407A16A0  (dtor; frees 1616=0x650 bytes => object size 0x650)
[1] 0x1407A6F70
[2] 0x1407A6430  <-- calls sub_1407A6300 (the +0x540 additive fold)
[3] 0x1407A6F00
[4] 0x1407A9DD0
[5] 0x1407A3220
```
ForzaMultiCam vtables `0x1465D6808` and `0x1465D6BA0` also valid (COLs `0x147317D38`,`0x147317DD0`).
`+0x540` (1344) and `+0x550` (1360) both < object size 0x650 (1616) — **fields are plausible and
confirmed used register-relative on the `this` pointer**. The `+0x320` matrix block (800) is also
in-bounds.

---

## Recommended write & thread

1. **Hook `sub_1407A6300` (RVA `0x7A6300`) at entry** (or patch CCamDriver vtable slot index 2
   `0x145E3F2A0` to a trampoline). `rcx` = CCamDriver `this`.
2. Just before the original runs, write the desired camera-space translation offset to
   **`*(float[4])(this + 0x540) = {dx, dy, dz, 0}`** (camera-space; world units ≈ cm, ×100/m).
   The engine then computes `transform([this+0x530] + [this+0x540])` into `+0x550..+0x570` and
   folds it into `+0x320`, so culling stays coherent (camera-relative rendering preserved).
3. This runs on the **camera/sim thread** that drives CCamDriver, so the write is race-free with
   respect to the consumer (set it inside the same call). Do not write `+0x320`, `+0x550`, or
   `+0x530` (derived/engine-owned).
4. For rotation (yaw/pitch/roll head tracking), the parallel `YPR` lane family (enum 5/6) is the
   companion; this build keeps prior findings (yaw +0x5C4, roll +0x5F8) — combine with `+0x540`
   for full 6-DOF, but the **clean translation lever is `+0x540`**.

### Key EAs (cite)
- Lane channel table base ≈ `0x145E41DE0`–`0x145E423E0` (.rdata); names per §1.
- Additive fold: `sub_1407A6300` @ **RVA 0x7A6300**; fold site `0x7A6382` (`[+0x540]+[+0x530]`).
- CCamDriver vtable[2] wrapper: `sub_1407A6430` @ **RVA 0x7A6430**, slot `0x145E3F2A0`.
- Per-frame state copy: `sub_14072CA00` @ **RVA 0x72CA00**; caller `sub_14072B120` @ **RVA 0x72B120**.
- Reset/init: `sub_140C98BA0` @ **RVA 0xC98BA0**; consts `0x145DD9C40..0x145DD9C90`.
- CCamDriver vtable **0x145E3F290**, dtor `sub_1407A16A0` @ **RVA 0x7A16A0**, sizeof **0x650**.
