# FH5 EMPRESS — Real per-frame CCamDriver camera-pose writer/publisher RE

DB: `E:\tmp\fh5_re\fh5_c.i64` (image base `0x140000000`, no anti-tamper). Real `.text`: `0x140001000`–`0x145D95000`.
All EAs are Empress (RVA = EA − 0x140000000). All AOB counts verified by `ida_bytes.bin_search` over real `.text` only.

---

## TL;DR / Recommendation

**The "+0x320" in the stale Steam spec is NOT a data member at object+0x320 — it is a VTABLE SLOT OFFSET.**
The getter `sub_1407A9DD0` does `call qword ptr [rax+0x320]` (vtable slot 100) on the CCamDriver. There is
NO `mov [reg+0x320]` data store anywhere in any CCamDriver method. The actual per-frame camera-to-world
pose lives at **CCamDriver+0x530 / +0x540 / +0x550..+0x570** (not +0x320). That is why the old
`sub_1406BE3A0` AOB (`add rcx,0x320`) is meaningless on Empress — it was a vtable-publish idiom, not a pose store.

**BEST SINGLE HOOK POINT to rotate the camera so BOTH main view and shadow cascades follow:**

> **`sub_1407A6300`  EA `0x1407A6300`  RVA `0x7A6300`** — the per-frame "+0x540 fold".
> Hook POST-original. It is the LAST per-frame writer that builds the final camera basis into
> `this+0x530/+0x540` (orientation/offset) and `this+0x550..+0x570` (the resolved cam2world rows),
> and it runs BEFORE the getter (`sub_1407A9DD0`, vtbl slot 4) produces the producer's `a4`, and BEFORE
> the upstream cascade fit. Rotating `this+0x540` (and/or the +0x550 rows) at the tail of this function
> makes everything downstream — producer `a4` view and the cascade frustum derived from it — follow.

Verified-unique prologue AOB (28 bytes; the 24-byte form is NOT unique — 30 hits):

```
48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 50 4C 8B F1 48 89 51 48
```
(`mov [rsp+8],rbx; mov [rsp+10h],rsi; mov [rsp+18h],rdi; push r14; sub rsp,50h; mov r14,rcx; mov [rcx+48h],rdx`)
Count over real `.text` = **1 (UNIQUE)**.

Alternative (if you must hook a single-purpose wrapper rather than the shared fold):
- **`sub_1407A6430`** EA `0x1407A6430` RVA `0x7A6430` — thin wrapper that calls the fold then commits.
  AOB (16B, UNIQUE): `40 53 48 83 EC 20 48 8B D9 E8 C2 FE FF FF 48 8B`.
- **`sub_1407A9DD0`** (getter / vtbl slot 4) EA `0x1407A9DD0` RVA `0x7A9DD0` — see Q5; AOB (24B, UNIQUE):
  `48 8B C4 48 89 58 18 48 89 70 20 55 48 8D 68 A1 48 81 EC B0 00 00 00 0F`. Hooking here rotates the
  matrix it reads from +0x530/+0x540, which feeds the producer a4 AND is the same data the cascade path
  consumes — but the getter is only invoked from two CCamDriver finalize methods (not every render path),
  so the fold is the more deterministic choke point.

---

## Q1 — Functions that WRITE [reg+0x320] (matrix store at object+0x320)

Rigorous disassembler-operand scan (op0 = memory dest, disp == 0x320) over real `.text`:
- 2671 raw `20 03 00 00` dwords; **35** functions contain a real `mov/movups/... [reg+0x320]` store.
- **NONE of them reference the CCamDriver vtable `0x145E3F290`.** None are camera methods.
- The three known false positives behaved as expected and were excluded: `sub_140592800` (bulk-init),
  `sub_14072CA00` (clone), `sub_140BD1F70` (clone). Other high-store funcs (`sub_140A40470`,
  `sub_140A69B20`, `sub_1410991C0`) are also bulk/clone (size>0x800, ≥3 strided +0x320 stores).
- The three `mov [rcx+0x320]` candidates (`sub_140E89760` ctor-like init, `sub_140A93EC0` SRWLock setup,
  `sub_140C57490`) were decompiled — none operate on a CCamDriver; their `[rcx+0x320]` is an unrelated
  struct field.

**Conclusion: there is no per-frame data store to CCamDriver+0x320.** The camera-to-world is NOT held at
+0x320. The "+0x320" reference in the old spec is the vtable call offset (slot 100), proven in Q2/Q3.

## Q2 — Does sub_1407A6300 write THIS object's +0x320?

**No. It does not write +0x320 at all.** Full disasm (`0x7A6300`) shows it builds the per-frame pose into
the **+0x550 region** and leaves +0x320 untouched as a data field. Key sequence:

```
0x1407A6300  mov [rcx+48h],rdx ; mov [rcx+50h],r8        ; stash a2(view src)/a3 at this+0x48/+0x50
0x1407A632E  add rcx,60h        ; call sub_1407B8A60       ; init sub-block at this+0x60
0x1407A633A  lea rcx,[r14+5C0h]  ; call loc_1407B8B30
0x1407A635E  call [rax+328h]     ; this vtable slot 101 (sub_1407A9640)
0x1407A6380  call rbx            ; this vtable slot 102 (sub_1407A9880) -> writes this+0x540/+0x530 lanes
0x1407A6382  movups xmm0,[r14+540h]; addps xmm0,[r14+530h] ; fold +0x540 + +0x530
0x1407A63A7  call [rax+68h]      ; on a2 (view src) -> returns 4x4 rows
0x1407A63B2  mov [r14+550h],rcx ; ... [r14+558h]; [r14+560h]; [r14+568h]   <-- cam2world rows stored at +0x550
0x1407A63E0  call [rax+338h]     ; this vtable slot 103 (sub_1407A34D0) "fold into cam2world"
0x1407A63F8  movups [r14+570h],xmm0                        ; final row/translation stored at +0x570
0x1407A6400  call [rax+20h]      ; this vtable slot 4 = the GETTER sub_1407A9DD0 (publish)
```

The "`call [rax+0x338]` fold" the prior agent saw is vtable **slot 103 = `sub_1407A34D0`**. That function
returns an `__m128` (`*a2 = addps(...)`) that the fold stores at **`this+0x570`** — it writes THIS object's
+0x570, NOT +0x320 and NOT another object. So: **sub_1407A6300 writes this+0x550..+0x570 (the cam2world),
not +0x320.**

## Q3 — Per-frame publish (Empress analog of old Steam sub_1406BE3A0)

The old `add rcx,0x320; call vtable` idiom maps to the getter, which on Empress is a **`call [rax+0x320]`
through the object's own vtable**, decoded from raw bytes at `0x1407A9E29`:

```
0x1407A9E1C  mov rax,[rbx]            ; rbx = this (CCamDriver), rax = vtable
0x1407A9E1F  mov rcx,rbx
0x1407A9E22  mov byte ptr [rbx+366h],0
0x1407A9E29  call qword ptr [rax+320h]  ; <-- VTABLE SLOT 100 dispatch (= "the 0x320")
```

So `0x320` is a vtable displacement, not a data displacement. Scanning real `.text`:
- `add rcx,0x320` (`48 81 C1 20 03 00 00`): **68 hits** — none in CCamDriver; all unrelated +0x320 struct walks.
- `lea r64,[rcx+0x320]` (`48 8D ?? 20 03 00 00`, base==rcx): exactly **ONE** hit at `0x1415641EA` in
  `sub_141564040` — a JUMPOUT stub unrelated to the camera (does not ref the vtable). So no
  `lea [this+0x320]` publish exists either.

The real per-frame "publish/notify" is the getter `sub_1407A9DD0` (vtbl slot 4), which is the function the
producer ultimately reads through (Q5). Its UNIQUE 24-byte prologue AOB:
`48 8B C4 48 89 58 18 48 89 70 20 55 48 8D 68 A1 48 81 EC B0 00 00 00 0F` (count==1).

## Q4 — LAST per-frame writer of the camera pose before render-view/cascade build

**`sub_1407A6300` (EA 0x1407A6300, RVA 0x7A6300).** It is the function after which the camera basis
(`+0x530/+0x540`) and resolved cam2world rows (`+0x550..+0x570`) hold the final value for the frame; its
last act is to call the getter (slot 4) which republishes that pose into the producer's `a4`. Its wrappers
`sub_1407A6430`, `sub_1407A6DC0`, `sub_1407A6DF0` (force-decompiled — all are `sub_1407A6300(); call
[*this+0x20] (getter)`), and `sub_1407A6E30` simply call the fold then re-invoke the getter. So the fold is
the single last writer.

**Yes — hooking `sub_1407A6300` post-original and rotating the resolved pose (`this+0x540` basis and/or the
`this+0x550..+0x570` rows) is correct.** Because the getter (producer a4) and the upstream cascade fit both
derive from this same just-written pose, both follow.

Unique prologue AOB (28B, count==1):
`48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 50 4C 8B F1 48 89 51 48`.
(The 24-byte truncation `... 4C 8B F1` is NOT unique — 30 hits, mostly a `sub_144E6xxxx` thunk farm; you
MUST use ≥28 bytes including the `48 89 51 48` = `mov [rcx+48h],rdx`.)

## Q5 — Who calls the +0x320 getter sub_1407A9DD0?

Direct (non-vtable) crefs: only **two** sites:
- `sub_1407AC2D0` @ `0x1407AC2D0` (RVA 0x7AC2D0) — reads +0x1584 lane, calls getter, writes +0x1360..+0x1390.
- `sub_1407AC370` @ `0x1407AC370` (RVA 0x7AC370) — calls getter, queries view via vtable, writes +0x1360..+0x1392.

The getter is ALSO installed in the CCamDriver vtable at slot 4 (`vtbl+0x20 -> 0x1407A9DD0`), so it is
additionally reached by indirect `call [this+0x20]` from the fold and its wrappers (the producer a4 path).

**The shadow-cascade / render-view builder does NOT call this getter directly, and does NOT read a data
field at +0x320.** Cascades are fit upstream from the camera frustum derived from the SAME +0x540/+0x550
pose that the fold writes. Therefore:
- Hooking the getter and rotating its returned matrix rotates the producer a4 view, but does NOT by itself
  guarantee the cascades follow (they read the +0x540/+0x550 pose, not the getter's return).
- Hooking the **fold `sub_1407A6300`** and rotating the +0x540 basis / +0x550 rows rotates the source both
  the getter and the cascade path consume → **both follow.** This is why the fold is the recommended choke point.

---

## CCamDriver layout facts (from ctor sub_14079B8A0, RVA 0x79B8A0)
- `*(QWORD*)this = &off_145E3F290` (concrete CCamDriver vtable). Type name string "Driver".
- vtable slot map (this-dispatched): slot 4 (+0x20) = getter `sub_1407A9DD0`; slot 100 (+0x320) =
  `sub_1407AD3A0`; slot 101 (+0x328) = `sub_1407A9640`; slot 102 (+0x330) = `sub_1407A9880` (+0x540 basis
  writer); slot 103 (+0x338) = `sub_1407A34D0` (fold-into-cam2world, writes the +0x570 row).
- Per-frame pose data lives at **+0x530, +0x540** (basis lanes) and **+0x550, +0x558, +0x560, +0x568,
  +0x570** (cam2world rows). +0x320 is NOT a pose field; it is only ever the vtable-call offset.
