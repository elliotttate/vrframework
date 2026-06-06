# FH5 EMPRESS — Clone sub_14072CA00, orientation source, and the ONE write target

DB: `E:\tmp\fh5_re\fh5_c.i64` (base `0x140000000`). RVA = EA − 0x140000000.
Raw disasm quoted below is from idalib over the real image.

---

## Headline (read this first)

1. **`sub_14072CA00` (RVA 0x72CA00) is NOT a CCamDriver clone and never touches the active CCamDriver
   (vtbl 0x145E3F290, size 0x650).** It is a generic field-copy for a **different** container object
   (an array of 6 path/keyframe records, stride 0x6C0), copying `dst+0x320 <- src+0x320` *inside that
   record's own 0x620-byte payload*. The "+0x320" coincidence is interior offset 0x320 of that payload,
   not the camera's +0x320. So the live "+0x320 clobbered by the clone" attribution is wrong — but the
   clobber you observed IS real and has a different cause (point 2/3).

2. **The CCamDriver has NO directly-rotatable orientation basis stored on it.** The orientation is held in
   a separate **view-source object at `CCamDriver+0x48`**, and the world matrix is (re)built every frame by
   calling **view-source `vtable[0x68]`**. The CCamDriver only stores: two f32 *direction/offset* input
   lanes (`+0x530`, `+0x540`) that get summed and fed as a single point into that builder, and the
   *resolved* output rows (`+0x550..+0x568` + `+0x570`). That is exactly why rotating producer `a4`
   (the builder's output) fully rotates the main view, while poking the CCamDriver's stored matrix only
   moved ~6% — `+0x320` is the wrong field, and `+0x540`/`+0x550` are re-derived/overwritten the same frame.

3. **There is no single CCamDriver offset you can rotate that makes the cascades follow**, because the
   cascade fit and the producer both consume the *output of view-source vtable[0x68]*, and that builder is
   re-invoked (by the getter `sub_1407A9DD0`, which the fold calls at its own tail and which the two
   "AC" finalizers also call) AFTER any post-fold poke. **The correct lever is the view-source builder,
   not a CCamDriver field.** Concrete recommendation in the last section.

---

## Q1 — `sub_14072CA00` full field-copy map (what it is, and the +0x320 truth)

Caller `sub_14072B120` (RVA 0x72B120) proves the object type. It builds 320-byte stack records `v15..v24`
then clones them into `a1` (a NOT-CCamDriver container) at six element bases:

```
sub_14072CA00(a1 + 320,  v20)
sub_14072CA00(a1 + 1920, v15)
sub_14072CA00(a1 + 3648, v20)
sub_14072CA00(a1 + 5248, v15)
sub_14072CA00(a1 + 6976, v20)
sub_14072CA00(a1 + 8576, v15)
```
(stride 1728 = 0x6C0; `a1` uses `QueryPerformanceCounter`, vtables `off_145E28xxx` — a path/spline/keyframe
animator, NOT CCamDriver.)

`sub_14072CA00(dst, src)` copies a **0x620-byte payload field-for-field, dst+N <- src+N** (identity offset
mapping — never cross-offset). It is structured as 5 sub-blocks of 0x140 (320) bytes, each prefixed by an
indexed-header fixup (`movsxd r9,[obj+10h]; mov [r9+obj+18h], ...`). The copies in the 0x300..0x600 region
(disasm, all `dst <- src` SAME offset):

| dst offset | src offset | size | insn |
|---|---|---|---|
| +0x2B0 | +0x2B0 | 16 | movups |
| +0x2C0 | +0x2C0 | 16 | movups |
| +0x2D0 | +0x2D0 | 16 | movups |
| +0x2E0 | +0x2E0 | 16 | movups |
| +0x2F0 | +0x2F0 | 16 | movups |
| +0x300 | +0x300 | 16 | movups |
| +0x310 | +0x310 | 16 | movups |
| **+0x320** | **+0x320** | **16** | **movups (0x72CD3D)** |
| +0x330 | +0x330 | 16 | movups |
| +0x340 | +0x340 | 16 | movups |
| +0x350 | +0x350 | 16 | movups |
| +0x360 | +0x360 | 16 | movups |
| +0x370 | +0x370 | 16 | movups |
| +0x380 | +0x380 | 16 | movups |
| +0x390 | +0x390 | 16 | movups |
| +0x3D8..+0x3E8 | same | 4/4/4 | mov (block-4 header) |
| +0x3F0..+0x4D0 | same | 16 ea | movups |
| +0x518..+0x528 | same | 4 ea | mov (block-5 header) |
| +0x530..+0x610 | same | 16 ea | movups |

`[r11+0x320] <- [r10+0x320]` (0x72CD3D/0x72CD45) — **into dst+0x320 from src+0x320**, an object→object
copy at the identical offset, within the path-record payload. It is **block 3** interior (block-3 base
0x298, +0x88). **Not derived from another offset; not the CCamDriver.**

**Conclusion Q1:** `sub_14072CA00` clones `dst+0x320 <- src+0x320` (16 bytes) but on a path/keyframe
container, never the camera. It is not your clobber source on CCamDriver. Do not hook it.

## Q2 — Where the authoritative orientation actually lives

The camera world matrix is produced by ONE shared idiom that appears in three places: the fold
`sub_1407A6300`, and the two finalizers `sub_1407AC2D0` / `sub_1407AC370` (both also invoked via the
getter `sub_1407A9DD0`). The idiom (quoting `sub_1407A6300` disasm):

```
0x7A6380  call rbx                      ; slot102 sub_1407A9880 -> writes this+0x540 (and +0x630/+0x640)
0x7A6382  movups xmm0,[r14+540h]        ; read orientation/offset lane +0x540
0x7A638F  addps  xmm0,[r14+530h]        ;  + base lane +0x530   => ONE float4 'aim point'
0x7A63A2  movaps [rsp+20h],xmm0
0x7A63A7  call [rax+68h]                ; *** view-source(this+0x48)->vtable[0x68] : BUILDS cam2world ***
0x7A63B2  mov [r14+550h],rcx            ; store resolved row0  (qword)
0x7A63BD  mov [r14+558h],rcx            ;   row1
0x7A63C8  mov [r14+560h],rcx            ;   row2
0x7A63D6  mov [r14+568h],rax            ;   row3
0x7A63E0  call [rax+338h]               ; slot103 sub_1407A34D0 -> blended row
0x7A63F8  movups [r14+570h],xmm0        ; store that row at +0x570
0x7A6400  call [rax+20h]                ; slot4 = getter sub_1407A9DD0 (re-publish)
```

`sub_1407AC2D0` is identical but seeds the aim point from `movsd [rbx+630h]` + `movss [rbx+638h]`
(the +0x630 extra-dir lane) instead of +0x540, then `call [rax+68h]` -> writes +0x550..+0x568, and
`call [rax+70h]` -> +0x570.

**So the offsets are:**
- **Orientation/aim INPUT lanes (f32 x4 each), on the CCamDriver:**
  - `+0x530` — base/offset lane (your readback `(-0.384, 0.559, -0.411, w=1)`; w=1 ⇒ treated as a point).
  - `+0x540` — primary direction lane (written by slot102 `sub_1407A9880` from the view-config `a2`:
    no-flag path `[a2+0x3C..0x44]`, flag@+0x600 path `[a2+0x54..0x5C]`, alt `[a2+0x48..0x50]`; packed
    `unpcklps(x,y)+movlhps(z)` ⇒ f32 `[x, y, z, _]`). This is a **direction/euler, not a 3×3 basis.**
  - `+0x630` (f32 x3) + `+0x638` (f32) — extra dir lane (slot102 writes from `[a2+0x64..0x6C]`).
- **Resolved cam2world OUTPUT rows, on the CCamDriver:**
  - `+0x550, +0x558, +0x560, +0x568` — 4 rows (copied as qwords from the builder's return; this is the
    rotation+position the producer a4 and cascades ultimately reference). `+0x568` row3.
  - `+0x570` — one more basis row (your readback `(0.965, 0, 0.261, w=0)`; w=0 ⇒ a unit direction row),
    written from slot103 `sub_1407A34D0` (a velocity/interp blend row, NOT the main rotation).
- **The world camera position** you found at the f64 `+0x550`-region `(-5059,179,-271)` is the row3 of the
  resolved matrix (the qwords stored at +0x550..+0x568 alias your f64 readback). `+0x320`'s position
  `(1056,..)` is unrelated (it is the getter's *vtable pointer* region, not pose data — see Q3 below).

**slot103 `sub_1407A34D0` writes the +0x570 row only** (`movups [rdi],xmm1` where rdi = a2 = `&this+0x570`
caller-supplied); it is a 2-sample blend of the view-source's `vtable[0x28]` outputs (lerp t from +0x5C0/
+0x580 angle). It does **not** write the main rotation — that is the `[rax+68h]` builder.

## Q3 — Why your +0x320 poke was clobbered, and the single best write target

`+0x320` is **the CCamDriver vtable slot offset 100**, not pose data. The getter does
`mov rax,[rbx]; call qword ptr [rax+320h]` (0x7A9E29) — a virtual call through the object's vtable. The
"position (1056,..)" you read at object+0x320 is just whatever 16 bytes live at data offset 0x320 of the
CCamDriver (an unrelated member), which is why rotating it did almost nothing (the ~6% was incidental).

The reason poking the *real* pose (+0x540 / +0x550) doesn't stick: the world matrix is **rebuilt from the
view-source every time the getter runs**, and the getter runs at the tail of the fold AND from both AC
finalizers AND via vtable slot 4. Any value you write to +0x540/+0x550 after `sub_1407A6300` returns is
recomputed on the next getter call within the same frame.

### Recommended lever — rotate the builder's OUTPUT inside the view-source call, or pre-rotate the input lane

There is **no CCamDriver data offset that survives and propagates to both view + cascades** because the
authoritative rotation is not stored on the CCamDriver in basis form. Two viable hooks, best first:

**OPTION A (best — single choke, both view & cascades follow): hook the getter `sub_1407A9DD0`
(EA 0x1407A9DD0, RVA 0x7A9DD0; UNIQUE prologue AOB
`48 8B C4 48 89 58 18 48 89 70 20 55 48 8D 68 A1 48 81 EC B0 00 00 00 0F`), POST-original, and rotate the
4 rows it just wrote to `this+0x550 / +0x558 / +0x560 / +0x568` (+ `+0x570`).** These are the resolved
cam2world rows the producer a4 reads and the cascade fit derives from. Because you write them AFTER the
getter's `[rax+68h]` builder + slot103 have run and the getter does not touch them again before returning,
they persist for that frame. Layout to compose your rotation:
- `+0x550..+0x568` = 4 rows of the cam2world (row-major; row0/1/2 = basis right/up/forward as 16-byte rows,
  row3 = translation/world-position). Multiply rows 0..2 by your head-look rotation (about the camera
  origin), leave row3 (position) at +0x568. This is the SAME matrix the producer a4 carries, so the cascade
  fit (which reads this resolved pose, not +0x540) follows.
- Do the same rotation to the `+0x570` row (the blend/forward row) for consistency, or it will lag slightly.

> Note: the getter has TWO output paths (the `jz loc_1407A9FD1` fast path writes +0x550..+0x568 +0x570 at
> 0x7A9FF2.. / 0x7AA031; the slow path likewise). Hooking POST-original covers both — you read/rotate the
> committed +0x550..+0x570 after the function returns. Verify w-components: +0x568 should hold the world
> position (your f64 (-5059,179,-271)); +0x550/+0x560 rows are the basis.

**OPTION B (pre-rotate the input direction lane): hook the fold `sub_1407A6300` (EA 0x1407A6300,
RVA 0x7A6300; UNIQUE 28-byte AOB
`48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 50 4C 8B F1 48 89 51 48`) and rotate the
`this+0x540` f32 direction lane (and +0x630/+0x638) BEFORE the `[rax+68h]` builder runs**, i.e. hook
mid-function or detour the slot102 result. This makes the builder itself produce a rotated matrix, so
+0x550..+0x570, producer a4, and cascades are all internally consistent (no clobber). Downside: +0x540 is a
*direction*, not a full basis, so you can only steer yaw/pitch of the look vector, not arbitrary roll, and
the mapping from your head quaternion to that direction must be derived. Option A (rotate the resolved 4×4
output) gives full 6-DOF orientation control and is simpler to compose.

### One-line answer
- Single best write target = **the resolved cam2world rows at CCamDriver `+0x550/+0x558/+0x560/+0x568`
  (and `+0x570`), rotated POST-original in a hook on the getter `sub_1407A9DD0` (RVA 0x7A9DD0)**.
- `+0x540`/`+0x530` are f32 *direction/offset* lanes (not a basis); they are the builder's INPUT and get
  re-consumed each frame, so don't rotate them as a basis.
- `+0x320` is a vtable slot, not pose; ignore it.
- `sub_14072CA00` is a path-record clone, unrelated to the camera; ignore it.

Both AOBs verified UNIQUE over real `.text` (0x140001000–0x145D95000) in the prior pass
(`_agent_reports/fh5_empress_camerapos_writer_RE.md`).
