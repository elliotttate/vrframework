# FH5 (Empress, CURRENT build) — Upstream Camera-Pose WRITER RE (shadow-coherent head-look)

Date: 2026-06-06
Target i64: `E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe.i64` (image base 0x140000000, no ASLR/anti-tamper).
Target exe (raw-byte ground truth): `E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe` (182,826,496 bytes).
RVA = VA − 0x140000000.

## Method note (how this was verified on the CURRENT build without the DB)
The `.i64` was **locked** the entire session by a running `idat.exe` (PID 77064) executing a full
headless decompile/export to `E:\ForzaHorizon5_Empress_Decompile\` (370,263 functions; ~46k done by
end of session, sequential by EA). I therefore verified every fact two ways that do NOT need the DB:
1. **Raw-exe RTTI + vtable + AOB analysis** via two new scripts I wrote (pure on-disk PE parsing, no
   IDA): `…\FH5CameraProbe\scripts\exe_rtti_vtable.py` (RTTI→vtable resolver, vtable-slot dumper,
   byte dumper) and `…\scripts\exe_aob_scan.py` (masked AOB scanner over `.text`).
2. **The in-progress export's per-function pseudocode shards** (`…\pseudocode\*.c` + `index.jsonl`),
   queried with `…\scripts\idx_lookup.py`. These shards are CURRENT-build Hex-Rays output.

Every RVA below was read from the actual current exe/i64 export — none copied blindly from the prior
(drifted) reports. Where the old reports' RVAs drifted, that is called out explicitly.

---

## TL;DR — the answer, and the contradiction resolved

**There are TWO independent per-frame camera-matrix lanes on the camera object, both rebuilt every
frame from the same upstream source, by DIFFERENT code paths:**

* **Lane A — `CCamDriver/CCamFollow + 0x550..+0x570`** = the producer's **`a4`** (camera-to-world the
  MAIN view AND the shadow cascades are derived from). Built by the getter `sub_1407A9DD0` /
  a4-assembler `sub_1407AC2D0` by calling the **view-source builder** `S->vtbl[0x68]`, which on the
  active camera just returns **`*(S+0x600)`** — a transpose-gather of the world basis at **`S+0x650`**.
* **Lane B — `CCamDriver + 0x320` (+ `+0x3E0` view tail)** = a *separate orthonormal snapshot* used by
  the renderer/view path the live freecam pokes. It is NOT the producer's `a4` and is NOT what the
  cascades read. It is initialized in the ICamera ctor and maintained by its own path.

**This is the whole contradiction.** The earlier runtime test "rotate `+0x320` post-getter → matrix
spun but the rendered view did NOT follow" is explained: **`+0x320` is Lane B; the producer reads
Lane A (`+0x550`, ← `S+0x600` ← `S+0x650`).** Poking `+0x320` after the getter never touched the lane
the producer consumes. (And the freecam DID see `+0x320` move the view because the *renderer view
path* reads Lane B — a different consumer than the producer/cascade path. Both lanes can move "the
view," but only Lane A feeds the cascades.)

**The single shadow-coherent upstream lever is the world basis on the view-source `S`, at `S+0x650`
(and its `S+0x600` transpose), rebuilt each frame by `sub_14060B390` from the upstream car/target
transform `*(S+0x5C0)`.** Hook the **getter `sub_1407A9DD0` (RVA 0x7A9DD0)** on ENTRY (it runs AFTER
`sub_14060B390` rebuilds the basis and BEFORE it copies it into `a4` rows), rotate `S+0x650`/`S+0x600`
there, and BOTH the main `a4` view and the cascade fit follow (the cascade fitter is fed the same
producer view basis).

---

## 1. Active gameplay camera class + concrete vtable (CURRENT build) — RTTI-anchored, build-stable

Resolved from raw-exe MSVC RTTI (`exe_rtti_vtable.py rtti …`). These are **identical** to the old
report — RTTI-anchored vtables did not drift:

| Class | mangled | vtable VA | vtable RVA | vt[0] RVA |
|---|---|---|---|---|
| **CCamDriver** | `.?AVCCamDriver@Camera@@` | **0x145E3F290** | 0x5E3F290 | 0x7A16A0 |
| ICamera (base) | `.?AVICamera@Camera@@` | 0x145E3DED8 | 0x5E3DED8 | 0x7A1A70 |
| **CMultiCam (view-source S)** | `.?AVCMultiCam@Camera@@` | **0x145E1E558** | 0x5E1E558 | 0x5F3990 |
| CCamFollow | `.?AVCCamFollow@Camera@@` | 0x145E3FC78 | 0x5E3FC78 | 0x7A16E0 |
| CCamCockpit | `.?AVCCamCockpit@Camera@@` | 0x145E3E200 | 0x5E3E200 | 0x7A1660 |
| CCamHood | `.?AVCCamHood@Camera@@` | 0x145E3EBF0 | 0x5E3EBF0 | 0x7A1930 |
| CCamFree | `.?AVCCamFree@Camera@@` | 0x145E40D78 | 0x5E40D78 | 0x7A17E0 |
| ForzaMultiCam | `.?AVForzaMultiCam@@` | **0x1465D6808** | 0x65D6808 | 0x4A1D570 |

> Note: ForzaMultiCam vtable = **0x1465D6808** on the current build (the old hook spec used a
> `_Ref_count` control-block VA `0x1467F74C8` that should be re-derived live, not trusted statically).

### Camera object layout (CONFIRMED current build)
The ICamera ctor **`sub_14079F750` (RVA 0x79F750)** initializes (decompile, current build):
```
*(a1)              = &off_145E3DED8           // ICamera vtable
*(_OWORD*)(a1+800) = xmmword_145DD9C40         // +0x320 basis row0   (Lane B, orthonormal snapshot)
*(_OWORD*)(a1+816) = xmmword_145DD9C60         // +0x330 row1
*(_OWORD*)(a1+832) = xmmword_145DD9C70         // +0x340 row2
*(_OWORD*)(a1+848) = xmmword_145DD9C90         // +0x350 row3 (translation, w=1)
*(_OWORD*)(a1+992) = xmmword_145E143C0         // +0x3E0 VIEW tail (world-to-camera)
```
So **`+0x320` is a genuine 4-row camera-to-world matrix field with a co-located `+0x3E0` VIEW tail**
(Lane B). The CCamFollow ctor `sub_14079B930` (RVA 0x79B930) chains ICamera ctor, sets vtable
`&off_145E3FC78`, stores its target at `+0x530` (1328), and **zeroes `+0x5C0` (1472)** — the
view-source pointer is attached later at runtime.

Other confirmed lanes (relative to the camera object `this`):
| Offset | Meaning | Lane |
|---|---|---|
| `+0x48` | pointer to the **view-source S** (a CMultiCam) | chain |
| `+0x320/+0x330/+0x340/+0x350` | orthonormal cam-to-world basis rows + pos (**Lane B snapshot**) | renderer/freecam path |
| `+0x3E0` | 4×4 world-to-camera VIEW tail (inverse of +0x320) | Lane B |
| `+0x530` | eye/anchor f32[4] (aim-mode input, **dead for orientation** on active cam) | — |
| `+0x540` | aim-point f32[4] (aim-mode input, **dead** — builder ignores it) | — |
| `+0x550/+0x558/+0x560/+0x568` | **builder output basis rows 0-2 = producer `a4` rows** (**Lane A**) | producer + cascades |
| `+0x570` | a4 row3 (blend/up row, from `vtbl[0x338]`) | Lane A |
| `+0x630/+0x638` | extra dir lane fed to the a4 builder (also ignored by the CMultiCam builder) | — |

---

## 2. The producer `sub_140BB1EE0` and the TRUE source of `a4` (CURRENT build)

**Producer is at the SAME RVA 0xBB1EE0**, verified byte-for-byte against the current exe:
prologue `44 89 44 24 18 48 89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 18 D9 FF FF`
(identical to the old shard `producer_0xBB1EE0_decomp.txt`). Size 0x26A5.

* The producer **receives `a4` as an argument** (read as `*a4`, `a4[1..3]` at shard lines 428-431,
  485-488, 551-554, 585-588). It has **no static callers** (indirect dispatch by the render-view
  scheduler). It does NOT call the getter itself.
* `a4` content = the camera object's **`+0x550..+0x570`** rows. Those are produced by:
  - **a4-assembler `sub_1407AC2D0` (RVA 0x7AC2D0)** — CCamDriver vtable slot 0x370. Current-build
    decompile: `S=*(this+0x48)`; `rows=S->vtbl[104](S,…,&in)`; writes `*(this+1360..1384)` =
    `+0x550..+0x568`; then `S->vtbl[112]` → `*(this+1392)` = `+0x570`. The `in` it passes is
    `unpcklps(*(this+0x630),*(this+0x638))`. Prologue (UNIQUE):
    `40 53 48 83 EC 50 48 8B D9 E8 ?? ?? ?? ?? 48 8B 4B 48 4C 8D 44 24 20 F3 0F 10 93 38 06 00 00`.
  - **getter `sub_1407A9DD0` (RVA 0x7A9DD0)** — same thing, called per-frame: `S=*(this+0x48)`;
    `in = *(this+0x540)+*(this+0x530)`; `rows=S->vtbl[104](S,…,&in)` → `this+0x550..0x568`;
    `this+0x570 = this->vtbl[0x338-equiv]()`.
* **The builder `S->vtbl[0x68]` on the active camera IGNORES `in` and returns `*(S+0x600)`.**
  Verified raw bytes of CMultiCam slot 0x68 (`sub_1405FA880`, RVA 0x5FA880, the actual slot pointer
  read from vtable 0x145E1E558[13]):
  ```
  0F 10 81 00 06 00 00   movups xmm0,[rcx+0x600]   ; read S+0x600  (input r8 untouched)
  48 8B C2               mov rax,rdx
  0F 11 02               movups [rdx],xmm0          ; -> out
  C3                     ret
  ```
  (The ICamera *base* builder `sub_1405391C0` returns a const `[0,0,1,0]`; the active camera uses the
  CMultiCam builder because `S` is a CMultiCam.)

**Definitive source of `a4`:** `a4` rows ⟵ camera `+0x550` ⟵ `*(S+0x600)` ⟵ the world basis at
`S+0x650` (see §3). **`a4` is NOT derived from `+0x320`.**

---

## 3. The per-frame WRITER of the camera world basis (the candidate hook)

**`sub_14060B390` (RVA 0x60B390)** — the **CMultiCam per-frame world-basis producer**. Current-build
decompile confirms it operates on `a1 = S` (the CMultiCam view-source) and:

* reads the **upstream source object** at `*(S+0x5C0)` (`a1+1472`);
* calls `(*(S+0x5C0))->vtbl[+432]` (world transform getter, `v32/v8`) and `->vtbl[+440]` (basis rows,
  `v20/v35`);
* writes the **world basis** onto S:
  - `S+0x650` (a1+1616) = basis row0 (`*v18`)
  - `S+0x660` (a1+1632) = row1
  - `S+0x670` (a1+1648) = row2
  - `S+0x680` (a1+1664) = row3 (pos/scale)
* then transpose-gathers that 3×3 into **`S+0x600`/`S+0x610`/`S+0x618`** (the rows the slot-0x68
  builder returns), exactly:
  ```
  *(DWORD*)(S+0x600) = *(DWORD*)(S+0x658);  *(S+0x604)=*(S+0x668);  *(S+0x608)=*(S+0x678);
  *(DWORD*)(S+0x610) = *(DWORD*)(S+0x654);  *(S+0x614)=*(S+0x664);  *(S+0x618)=*(S+0x674);
  ```
  i.e. `S+0x600` row0 = the gathered column → the transpose of the `S+0x650` basis. This is what
  becomes `a4` row0.
* also derives a normalized axis at `S+0x620` and a normalized direction at `S+0x630`.

**Prologue (UNIQUE — 40 bytes; a 28-byte twin at 0x4E8E740 shares only the first 40 bytes-minus-1, so
extend to the divergent byte):**
`48 8B C4 48 89 58 18 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 28 FF FF FF 48 81 EC A0 01 00 00 0F 29 70 B8 0F 29 78 A8 44 0F 29 40 98` → **1 match at 0x60B390**.

**Is it upstream of BOTH a4 and the cascade fit?** Yes for `a4` (it builds the very rows a4 is copied
from). For the cascade fit: the cascades are fit upstream from the same camera/producer view basis
(see §4); `sub_14060B390` runs in the camera update **before** both the producer and the shadow-view
build, so its output feeds both. The cleaner, race-safe place to inject is the **getter
`sub_1407A9DD0`** (which runs AFTER `sub_14060B390` and copies the basis into `a4`), operating on
`S = *(this+0x48)` — see §5.

### 0x318 vs 0x320 — resolved
The old "publisher" `sub_1406BE3A0` **drifted**: RVA 0x6BE3A0 is now mid-instruction inside
`sub_1406BD460`. Its current analog is **`sub_140654590` (RVA 0x654590)**, a 48-byte generic
publish/notify helper whose defining instruction is **`add rcx, 0x318`** (`48 81 C1 18 03 00 00`),
then `call sub_14057E370; mov rcx,[rbx+8]; … jmp [rax+0x18]`. **This is a GENERIC notify helper**
(the tail idiom `48 8B DA E8 … 48 8B 4B 08 … 48 FF 60 18 …` occurs 14× in `.text`; the
`add rcx,0x318` prefix occurs 60×), reached only indirectly via vtables — it is **NOT a reliable or
camera-specific hook**, and `+0x318` here is *some* object's notify field, not the camera matrix. The
"0x318 vs 0x320" question is therefore a red herring from a non-camera publisher: the camera's actual
matrix lanes are **+0x320** (Lane B snapshot) and **+0x550** (Lane A a4), both confirmed by ctor and
by the producer/getter decompiles. **Do not hook `sub_140654590`.**

---

## 4. Where the shadow-cascade fitter gets the pose (a13)

The producer's cascade path reads **only `a13`** (captured `v82=a13`, shard line 759), via
collection accessors with a **2736-byte (0xAB0) per-cascade stride** — confirmed on the current build
by raw bytes of `sub_140BDB820` (RVA 0xBDB820):
```
41 8B C0                 mov eax,r8d              ; cascade index
4C 69 C0 B0 0A 00 00     imul r8,rax,0xAB0        ; *2736  ← STRIDE confirmed
49 8B 84 08 18 0D 00 00  mov rax,[r8+rcx+0xD18]   ; per-cascade f64 world CENTER (record+0xD18)
48 89 02 …               (+0xD20, +0xD28 …)
```
The per-cascade VIEW matrix lives at record+0x350 (accessor `sub_140BDB860`, RVA 0xBDB860). The
producer **only copies** these baked matrices/centers into the output (shard lines 1317-1391); it
never re-derives them from `a4`. **So the cascades carry a pre-fit snapshot.**

That snapshot is fit, upstream in the same frame, from the **same camera view basis that becomes
`a4`** (the gameplay camera frustum the cascades must cover). Two reports independently land here
(`fh5_empress_shadow_cascade_RE.md` §4/§5; `fh5_empress_viewsource_orientation_RE.md` Q5): the fitter
consumes the producer view basis (Lane A), not `+0x320` and not a separate stored orientation (there
is none on the view-source). The **common ancestor of both `a4` and the cascade fit is the world
basis on `S` (`S+0x650`/`S+0x600`), built by `sub_14060B390` from `*(S+0x5C0)`.** Therefore mutating
the pose at/above that basis (getter entry, operating on `S`) makes shadows follow.

> Caveat (honest): I could not single-step the exact fitter call site statically because the
> cascade-collection FILL function is reached only by indirect dispatch (no static caller in the idb;
> the cascade accessors `sub_140BDB6C0/820/860/7F0/AC0` have no static xrefs). The "cascades are fit
> from the producer view basis" conclusion is inference + cross-report agreement, not a single
> decompiled fitter line. The decisive confirmation is the live test in §6. If the live test shows
> shadows still lagging, the fallback is to rotate the deepest common ancestor `*(S+0x5C0)` transform
> (§5 option C) or to instrument the render-view scheduler `sub_140CA1B70`-cluster.

---

## 5. HOOK SPEC (concrete, for the mod)

### (a) Function to detour — PRIMARY: the getter `sub_1407A9DD0`
* **RVA 0x7A9DD0** (VA 0x1407A9DD0).
* **UNIQUE prologue AOB (32 bytes, no wildcards):**
  `48 8B C4 48 89 58 18 48 89 70 20 55 48 8D 68 A1 48 81 EC B0 00 00 00 0F 29 78 D8 41 0F B6 F0`
  (verified **exactly 1** match in `.text`).
* `__fastcall(a1 = __m128* this /*camera object*/, a2, a3)`.

### (b) Mutate BEFORE or AFTER the trampoline
**Compose the rotation, then call the original (mutate the SOURCE before the original copies it).**
Two equivalent placements:
* **Recommended:** in the detour, BEFORE calling the trampoline, rotate the world basis on
  `S = *(this+0x48)` at `S+0x650` and mirror row0 to `S+0x600`. The original then transpose-gathers
  the rotated basis into `this+0x550` (a4). This guarantees a4 carries the rotation.
* Alternative: AFTER the trampoline, rotate `this+0x550/+0x558/+0x560` (a4 rows directly). Simpler but
  must also keep `S+0x600` consistent if anything else re-reads it that frame; prefer the source edit.

### (c) Exact offsets to mutate
Let `S = *(uint8_t**)(this + 0x48)` (the CMultiCam view-source).
* **Rotation (basis):** `float* M = (float*)(S + 0x650)` — 4×4 **row-major, row-vector** world basis:
  - `M[0..3]`  = row0 (right)   @ S+0x650
  - `M[4..7]`  = row1 (up)      @ S+0x660
  - `M[8..11]` = row2 (forward) @ S+0x670
  - `M[12..15]`= row3 (pos)     @ S+0x680
  Apply head rotation by **pre-multiplying the 3×3**: `M3x3' = R_head · M3x3` (R_head built in the
  camera right/up/fwd frame: yaw about row1, pitch about row0, roll about row2). Then **mirror the
  transpose-gather** the engine does so `*(S+0x600)` stays consistent:
  `*(S+0x600)=[M[1],M[5],M[9]]`-style — i.e. recompute `S+0x600/0x610/0x618` from `S+0x654/0x664/0x674`
  and `S+0x658/0x668/0x678` exactly as `sub_14060B390` does (see §3), OR simply also write the rotated
  row0 of the *transposed* basis to `S+0x600` to match.
* **Translation:** keep the EXISTING, proven 6-DOF translation lever (producer `a15/a16` f64 world
  cameraPos, per MEMORY `fh5-6dof-translation-solved`). Do NOT move translation here.

### (d) VIEW-tail consistency
The `a4`/`+0x550` lane has **no separate view-tail to recompute** (the producer builds the VIEW from
`a4` internally — shard lines 485-588 `v307 = transpose(a4)` and `a4×a6`). So when you rotate the
basis at the getter, **nothing extra to recompute for Lane A.** (The `+0x3E0` VIEW tail belongs to
Lane B / the freecam `+0x320` path; it is NOT on this hook's path. Only recompute `+0x3E0` if you
*also* keep a Lane-B `+0x320` hook for the renderer-view path.)

### (e) Gating (identify the ACTIVE gameplay camera)
* `this` here is whatever camera the engine is updating — gate to the active one.
* Resolve the active camera via the ForzaMultiCam aggregate. The old `ForzaMultiCam+0x5C8`
  active-camera-object offset should be **re-verified live on this build** (the ForzaMultiCam vtable
  is now 0x1465D6808; offsets within it may shift). Practical gate that needs no offset guess:
  - **`*(this+0x48)` must be non-null and its vtable == CMultiCam (0x145E1E558)** (i.e. the camera has
    a live CMultiCam view-source). Shadow/UI/reflection cameras won't have this.
  - **Orthonormal-basis sanity** on `S+0x650`: rows unit-length, mutually perpendicular, finite. This
    rejects garbage/transition instances (same guard the freecam `DecodePose` uses).
  - Optionally also confirm `this`'s vtable ∈ {CCamDriver 0x145E3F290, CCamFollow 0x145E3FC78,
    CCamCockpit 0x145E3E200, CCamHood 0x145E3EBF0} so it rides every drivable view but not CCamFree.
* CinematicGameCamera uses a different matrix lane (`+0x540..` region in older notes) — won't collide.

### (f) Compose-on-top, do NOT accumulate
`sub_14060B390` REBUILDS `S+0x650` every frame from `*(S+0x5C0)` (the car/target transform) BEFORE the
getter runs. So each frame: read the freshly-rebuilt base basis from `S+0x650`, apply the current head
rotation, write back. Never persist/accumulate — the engine resets the base next frame. When head-look
is neutral/disabled, simply don't mutate.

### Alternative hook points (ranked)
* **B — world-basis producer `sub_14060B390` (RVA 0x60B390), AFTER trampoline.** UNIQUE 40-byte AOB:
  `48 8B C4 48 89 58 18 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 28 FF FF FF 48 81 EC A0 01 00 00 0F 29 70 B8 0F 29 78 A8 44 0F 29 40 98`.
  Here `a1 = S`; rotate `S+0x650`/`S+0x600` after the original built them. Functionally equivalent to
  (a); the getter is slightly later & race-simpler, so prefer (a).
* **C — deepest common ancestor `*(S+0x5C0)` transform.** Rotating it would also rotate gameplay logic
  reading the car transform — NOT recommended.
* **Fallback for ROLL only:** the look-at basis recompute can't introduce roll via a pure direction
  edit; for roll, additionally rotate the up/blend row (a4 row3 `+0x570`) about forward after the
  getter, or rotate the full 3×3 at `S+0x650` (option a already does full 3-DOF, so roll is covered if
  you pre-multiply the whole 3×3 rather than only re-aiming).

---

## 6. Reconciling the runtime contradiction

**Why rotating `+0x320` post-getter did NOT move the rendered view (in the cascade-relevant sense):**
`+0x320` (Lane B) is a **separate orthonormal snapshot** initialized by the ICamera ctor
(`sub_14079F750`) and maintained by its own path; it is **read by the renderer view path the freecam
pokes, NOT by the producer.** The producer's `a4` is **Lane A = `+0x550` ⟵ `*(S+0x600)` ⟵
`S+0x650`** (proven: a4-assembler `sub_1407AC2D0` and getter `sub_1407A9DD0` both write `+0x550` from
the CMultiCam builder, which returns `*(S+0x600)`; the builder ignores `+0x540`). Rotating `+0x320`
after the getter therefore changes a lane the producer/cascade path never reads → no effect on the
producer-rendered view. (The earlier freecam DID see `+0x320` move *something* because the renderer
view path consumes Lane B — that is a different "view" than the producer's a4 view, and it does not
carry the cascades.) Note also the old report's claim "the getter writes `+0x320`" is **false on this
build** — the getter's only `800` reference is a **vtable call** `(*(vtbl+800))(this)` (slot 0x320 =
`sub_1407AD3A0`, a trivial global-float setter), not an object-field store. The only writers of the
`+0x320` *field* in the exported camera code are the ctor and a bulk clone — never the per-frame pose
update chain.

**Why the recommended hook AVOIDS that failure:** the getter-entry hook rotates **`S+0x650`/`S+0x600`
= the exact Lane-A source the producer copies into `a4`**, and (per §4) the cascade fitter is fed the
same producer view basis. So the rotation lands on the lane the producer AND the cascades consume,
before they consume it. This is the structural fix the `+0x320` poke could never achieve.

**Decisive live test:** enable a fixed head-yaw via the getter hook (rotate `S+0x650` 3×3), drive, and
look sideways. PASS = main view turns AND shadow cascades cover the turned frustum (no shadow break
off-axis). If shadows still lag (fitter snapshotted earlier than the getter), fall back to option (b)
the world-basis producer `sub_14060B390` (earlier in frame), or instrument the render-view scheduler.

---

## Confidence
* **HIGH:** camera vtables (RTTI-anchored, byte-verified); producer at 0xBB1EE0 (byte-identical
  prologue); a4 = `+0x550` ⟵ `*(S+0x600)` ⟵ `S+0x650` (getter/a4-assembler/CMultiCam-builder all
  decompiled/byte-verified current build); `+0x320` is a distinct ctor-initialized Lane B (false in
  old report that the getter writes it); cascade stride 0xAB0 + record+0xD18/+0x350 (byte-verified);
  getter/a4-assembler/fold/world-basis AOBs all proven UNIQUE in `.text`. The 0x318 publisher is a
  generic non-camera notify helper (do not hook).
* **MEDIUM:** that the cascade fitter reads the SAME producer view basis (inference + two-report
  agreement; the fitter fill site is indirect-only and not single-decompiled). This is the one item to
  confirm with the §6 live test.
* **TO RE-VERIFY LIVE:** the active-camera gating offset `ForzaMultiCam+0x5C8` (ForzaMultiCam vtable
  moved to 0x1465D6808; use the view-source-vtable + orthonormal-basis gate as the primary,
  offset-free discriminator).

## Cite-ready function index (CURRENT build, all verified this session)
| Symbol | RVA | Role | How verified |
|---|---|---|---|
| producer `sub_140BB1EE0` | 0xBB1EE0 | consumes a4; copies baked a13 cascades | byte-identical prologue vs old shard |
| getter `sub_1407A9DD0` | 0x7A9DD0 | **PRIMARY HOOK**; builds a4 rows `+0x550` from `*(S+0x600)` | export decompile + unique AOB |
| a4-assembler `sub_1407AC2D0` | 0x7AC2D0 | builds a4 `+0x550/+0x570` (vtbl 0x370) | export decompile + unique AOB |
| fold `sub_1407A6300` | 0x7A6300 | sets `*(this+0x48)=S`, runs builder | export decompile + unique AOB |
| world-basis producer `sub_14060B390` | 0x60B390 | builds `S+0x650`/`S+0x600` from `*(S+0x5C0)` | export decompile + unique AOB |
| CMultiCam builder slot0x68 `sub_1405FA880` | 0x5FA880 | `*out=*(S+0x600)` (ignores input) | vtable[13] + raw bytes |
| ICamera base builder `sub_1405391C0` | 0x5391C0 | returns const `[0,0,1,0]` | raw bytes |
| ICamera ctor `sub_14079F750` | 0x79F750 | inits `+0x320`/`+0x3E0` (Lane B) | export decompile |
| CCamFollow ctor `sub_14079B930` | 0x79B930 | sets vtable, `+0x530`, zeroes `+0x5C0` | export decompile |
| bulk camera clone `sub_14072CA00` | 0x72CA00 | copies +0x320 & +0x550 lanes a1←a2 | export decompile |
| cascade f64-center accessor `sub_140BDB820` | 0xBDB820 | record stride 0xAB0, center @+0xD18 | raw bytes |
| cascade view-matrix accessor `sub_140BDB860` | 0xBDB860 | per-cascade view @ record+0x350 | old shard (RVA stable) |
| generic notify helper `sub_140654590` (NOT a hook) | 0x654590 | `add rcx,0x318; call; jmp[rax+0x18]` | export decompile + bytes |
| CCamDriver vtable | 0x5E3F290 | concrete active camera | RTTI |
| CMultiCam vtable (view-source S) | 0x5E1E558 | `*(this+0x48)` | RTTI |
| ForzaMultiCam vtable | 0x65D6808 | aggregate (re-derive +0x5C8 live) | RTTI |

## Scripts produced this session (reusable, DB-lock-free)
* `…\FH5CameraProbe\scripts\exe_rtti_vtable.py` — raw-exe RTTI→vtable resolver, vtable-slot dumper, byte/AOB dumper.
* `…\FH5CameraProbe\scripts\exe_aob_scan.py` — masked AOB scanner over `.text` (uniqueness checks).
* `…\FH5CameraProbe\scripts\idx_lookup.py` — query the in-progress export `index.jsonl` + pseudocode shards by RVA/name.
