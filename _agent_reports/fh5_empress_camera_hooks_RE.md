# FH5 Empress — Camera Hook RE (deterministic per-frame active-camera levers)

Date: 2026-06-05. DB: `E:\tmp\fh5_re\fh5_c.i64` (image base `0x140000000`, no anti-tamper).
RVA→EA = rva + 0x140000000. All AOB uniqueness counts are from a full scan of **all
executable segments** (the two code regions `.text` 0x140001000–0x145D95000 and the
high 0x149A7D000–0x14B208000), verified count==1 unless noted.

> NOTE on segment naming: the segment literally named `.text` in this DB is the high
> region `0x149A7D000`. The *real* game code (RVA 0x1xxxxx–0x5Dxxxxx) lives in the
> FIRST `.text` segment `0x140001000–0x145D95000`. Scan BOTH for AOB uniqueness.

---

## 0. TL;DR — what changed from the old (other-Steam) build

| Concept | OLD build | EMPRESS (this DB) |
|---|---|---|
| CCamDriver pose-publisher | RVA 0x6BE3A0, `add rcx,0x320` then vtbl call | **No `add/lea +0x320` exists on a camera object.** CCamDriver layout is different (fields at +0x630/+0x634/+0x640/+0x644, +1480/+1632). The `+0x320` AOB `48 81 C1 20 03 00 00` has **0 hits** in this image. |
| ForzaMultiCam state bridge | RVA 0x746BB0, reads `[a1+0x198]`, active slot +0x5C8, pose +0x660 | The +0x198/+0x5C8/+0x660 triple-offset function on Empress (`sub_1406C2D10`) is a **UI/event reflection serializer, NOT the camera bridge** (its +0x5C8/+0x660 fields are "RainbowCooldown"/"WinOnlinePostEvent" reflection descriptors). The real ForzaMultiCam is identified by its vtables + accessors below. |
| Active-cam slot | multicam+0x5C8 (control), control+0x10 (object) | **ForzaMultiCam +0x5C0 = active camera OBJECT ptr; +0x5C8 = its shared_ptr CONTROL block** (refcount at ctrl+8). Verified in `sub_1406078D0`. |

**Best deterministic per-frame hook for the live active camera (recommended):**
the upstream view PRODUCER **`sub_140BB1EE0` (RVA 0xBB1EE0)** — already proven to rotate
the rendered camera (per project memory). Unique prologue AOB below. For *getting the
active CCamDriver object pointer race-free on the sim thread*, hook the ForzaMultiCam
camera-stack manager **`sub_1406078D0` (RVA 0x6078D0)** whose `this` (rcx) is the live
ForzaMultiCam; read object at `this+0x5C0` (and control/refcount at `this+0x5C8`).

---

## 1. Resolution chain (verified)

```
ForzaMultiCam (this)
  ├─ +0x5C0 (1472)  = active camera OBJECT pointer (the live CCamDriver*-ish)
  ├─ +0x5C8 (1480)  = std::shared_ptr CONTROL block for the active camera
  │                    (atomic refcount at control+0x8; lock xadd/lock inc seen)
  ├─ +0x660 (1632)  = camera pose / transform block (zero-inited to identity rows
  │                    xmmword_145DD9C40.. in the installer)
  └─ vtable @ +0    = off_145E1E558   ;  secondary vtable @ +0x530 = off_145E1E8F0

CCamDriver object
  ├─ vtable @ +0    = off_145E3F290  (concrete, "CCamDriver", confirmed by ctor)
  ├─ base-class vtable (CCamera) @ +0 set first by base init = off_145E3E200
  ├─ +1544 (0x608)  = owner/name ptr (set by base init sub_14079B750)
  ├─ +1584/+1588/+1600/+1604 = small init fields (ctor sub_14079B8A0)
  └─ concrete vtable slot[5] (off +0x28 → +800 method) is the per-frame Update path
```

Proof of the +0x5C0/+0x5C8 shared_ptr pair, from `sub_1406078D0` @ ~0x140607CD?:
```c
v32 = *(_QWORD *)(a1 + 1480);                 // a1+0x5C8 = active-cam shared_ptr control
if (v32) _InterlockedIncrement((v32 + 8));    // refcount at control+8
v31[3] = *v18;                                //   v18 = (a1 + 1472) → object ptr (+0x5C0)
v31[4] = *(_QWORD *)(a1 + 1480);              //   pairs object(+0x5C0) with control(+0x5C8)
```
i.e. on Empress the active camera is a `shared_ptr<Cam>` stored as
{object@+0x5C0, control@+0x5C8}. The old build's "control→object +0x10" is replaced by
this adjacent object/control pair. The refcount is at `control+0x8` (confirmed in
`sub_1405F2430`, `sub_1406078D0`, `sub_14060B390` via `lock xadd/inc [ctrl+8]`).

---

## 2. CCamDriver constructor / factory (Query 3)

**`sub_14079B8A0` (RVA 0x79B8A0)** — CCamDriver constructor. The UNIQUE producer of the
concrete vtable reference `lea rax, off_145E3F290`:
```c
__int64 __fastcall sub_14079B8A0(__int64 a1, char a2) {
  strcpy(v5, "Driver");
  sub_14079B750(a1, &unk_148072030, v5);   // base CCamera init → base vtable off_145E3E200, name
  *(_BYTE *)(a1 + 1604) = a2;
  *(_QWORD *)a1 = &off_145E3F290;           // <-- install CCamDriver concrete vtable
  *(_QWORD *)(a1 + 1588) = 0;
  *(_DWORD *)(a1 + 1584) = 0;
  *(_DWORD *)(a1 + 1600) = 0;
  return a1;
}
```
Disasm of the vtable store (the one xref to 0x145E3F290 in the whole image):
```
0x14079B8EE [48 8D 05 9B 39 6A 05] lea rax, off_145E3F290
0x14079B8F5 [88 9F 44 06 00 00]    mov [rdi+644h], bl
0x14079B900 [48 89 07]             mov [rdi], rax        ; *this = &off_145E3F290
```
Called twice from the **installer `sub_1405EF780`** (calls @ 0x1405EFDE3, 0x1405EFE82).

Base init **`sub_14079B750` (RVA 0x79B750)** sets the base CCamera vtable `off_145E3E200`,
stores owner at `+1544`, name-string at `+1552`, and registers via `sub_1407B6930`.

---

## 3. ForzaMultiCam installer / active-slot writer (Query 2 + 3)

**`sub_1405EF780` (RVA 0x5EF780)** — ForzaMultiCam initializer (the function that builds
the multicam and CONSTRUCTS the CCamDrivers). It zero/identity-inits the active-cam slot
and pose block on `r15` (= ForzaMultiCam `this`):
```
0x1405EF89F  mov  [r15+5C0h], rsi          ; +0x5C0 active-cam OBJECT = null
0x1405EF8A6  mov  [r15+5C8h], rsi          ; +0x5C8 active-cam CONTROL = null   (active slot)
0x1405EF8FE  movups [r15+660h], xmm1       ; +0x660 pose block row (identity, xmmword_145DD9C60)
0x1405EF90D  movups [r15+670h], xmm0       ;  ...pose continues
0x1405EF91C  movups [r15+680h], xmm1
0x1405EF924  mov  dword ptr [r15+690h], 3F490FDBh   ; ~0.785398 (pi/4) — an angle field
```
Called from `sub_140620900` (call @ 0x140620A26).

Prologue AOB (UNIQUE, count==1, 37 bytes):
```
48 8B C4 48 89 58 20 4C 89 40 18 48 89 50 10 48 89 48 08 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 48 FA FF FF
```
`this` = rcx (a1) = ForzaMultiCam. Runs once at multicam construction (not per-frame).

---

## 4. ForzaMultiCam active-camera STACK MANAGER (best object-getter hook)

**`sub_1406078D0` (RVA 0x6078D0)** — manages the camera ring/stack and the active-cam
shared_ptr. Signature `__fastcall(__int64 this, unsigned int idx, char a3, char a4)`.
`this`(rcx) is the live ForzaMultiCam. It reads/writes the active-cam shared_ptr pair
(+0x5C0 object / +0x5C8 control) and bumps refcount at control+8 — this is exactly where
the active camera is *resolved/installed*, so hooking here gives you the live
ForzaMultiCam and the active CCamDriver via `this+0x5C0`.

Prologue AOB (UNIQUE, count==1, 27 bytes — verified against all exec segments):
```
48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 81 EC C0 00 00 00 45 0F B6 E1
```
(disasm: `mov [rsp+18h],rbx`; push rbp; push rsi; push rdi; push r12; push r13;
push r14; push r15; `sub rsp,0C0h`; `movzx r12d,r9b`). To read the active camera inside the hook:
`ForzaMultiCam* mc = (ForzaMultiCam*)rcx; void* activeCam = *(void**)((char*)mc + 0x5C0);`

Two more genuine accessors of the +0x5C8 active slot (refcount manipulators), useful as
cross-checks / alternative hooks:
- **`sub_140607CD4` site in `sub_1406078D0`**: `mov rax,[rdi+5C8h]; mov [rbx+20h],rax; call [rax+10h]` — copies the control block and dispatches a vtbl method (`+0x10`). This is the closest Empress analog of the old "control→object +0x10" deref.
- **`sub_14060B390` (RVA 0x60B390)**: `mov rbx,[rsi+5C8h]` (control), `mov rcx,[rsi+5C0h]` (object), then heavy SSE pose math writing `[a1+0x630]/[a1+0x660-ish]` — a per-activation camera-blend/quaternion computation. `this`=rsi=ForzaMultiCam.

---

## 5. Recommended PRIMARY per-frame hook — the view PRODUCER

**`sub_140BB1EE0` (RVA 0xBB1EE0)** — the per-frame view producer already located and
hooked by this project (rotating its input matrix a4 rotates the rendered camera; proven
reversible). This is the deterministic per-frame callsite and the correct VR foundation.

Prologue AOB (UNIQUE, count==1, 28 bytes):
```
44 89 44 24 18 48 89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 18 D9
```
Disasm:
```
0x140BB1EE0 [44 89 44 24 18]          mov  [rsp+18h], r8d
0x140BB1EE5 [48 89 54 24 10]          mov  [rsp+10h], rdx
0x140BB1EEA  push rbp; push rbx; push rsi; push rdi; push r12; push r13; push r14; push r15
0x140BB1EF4 [48 8D AC 24 ...]         lea  rbp, [rsp-...]
```
Args: `fastcall`; `rcx`=a1 (view-producer/context this), `rdx`=a4 input view matrix
(written at +0x10 home slot), `r8d`=a3 (written at +0x18). This runs on the
sim/render-view thread once per produced main camera (see project memory: producer can
render multiple main cams/frame; use recency-stickiness to pick free-roam).

---

## 6. CCamDriver concrete vtable map (0x145E3F290) — per-frame Update candidate

First slots (EA → target RVA):
```
[0] sub_1407A16A0   (dtor-ish, __fastcall(this,char))
[1] sub_1407A6F70
[2] sub_1407A6430
[3] sub_1407A6F00
[4] sub_1407A9DD0   <-- per-frame state/pose method (see below)
[5] sub_1407A3220
[6] sub_1407ACD00
...
[14] sub_1407A4AA0  (float*, ...)
[19] sub_1407AC6C0  (__int64*, float)
```

**`sub_1407A9DD0` (RVA 0x7A9DD0)** = concrete vtable slot[4]; a per-frame camera method
that dispatches `(*(a1+0))(a1+800)` (a +800 vtbl method, i.e. the camera's own Update),
queries an active sub-object via `(*(...+120))(...)`, fetches a transform via `(...+648)`,
and writes computed vectors into `a1[85..87]` (camera-relative fields ~ offsets
0x550–0x570) and a flag at `a1[96]`. Useful if you want a hook whose `this`(rcx) is the
CCamDriver itself on the camera thread.

Prologue AOB (UNIQUE, count==1, 23 bytes):
```
48 8B C4 48 89 58 18 48 89 70 20 55 48 8D 68 A1 48 81 EC B0 00 00 00
```
`this` = rcx = CCamDriver (`__m128* a1`).

---

## 7. The +0x198/+0x5C8/+0x660 triple-hit function (NOT the camera bridge — documented to avoid a trap)

**`sub_1406C2D10` (RVA 0x6C2D10)** is the only function touching all three of +0x198,
+0x5C8, +0x660, which made it look like the old 0x746BB0 bridge. It is actually a
**reflection/serialization writer** (`sub_1409407D0(dst, &descriptor, this+field, 0)` per
field). The field descriptors decode to UI/event names, NOT camera data:
- field @ this+0x5C8 (1480) descriptor `0x145E2E880` → "RainbowCooldown"/"RainbowPreEvent"/"RainbowPostEvent"
- field @ this+0x660 (1632) descriptor `0x145E2EBA0` → "WinOnline…PostEvent…"/"WinShowcase…ByTime"
The +0x198 here is a vtable-method offset (`(*(*a2 + 408))(...)`), not a multicam ptr load.
Do **not** use this as the camera bridge. (Its prologue AOB, for the record, count==1, 35B:
`48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 48 FF FF FF 48 81 EC B8 01 00 00 48 8B DA`.)

---

## 8. Hook table (copy-paste)

| Purpose | Function | EA / RVA | `this`/args | Thread | Unique AOB |
|---|---|---|---|---|---|
| **PRIMARY per-frame camera rotate/override** | view producer `sub_140BB1EE0` | 0x140BB1EE0 / 0xBB1EE0 | rcx=ctx, rdx=a4 view-matrix, r8d=a3 | sim/render-view | `44 89 44 24 18 48 89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 18 D9` |
| **Get live active CCamDriver race-free** | multicam stack mgr `sub_1406078D0` | 0x1406078D0 / 0x6078D0 | rcx=ForzaMultiCam; activeCam=`[rcx+0x5C0]`, ctrl=`[rcx+0x5C8]` | sim/camera | `48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 81 EC C0 00 00 00 45 0F B6 E1` |
| CCamDriver per-frame Update (this=camera) | vtbl[4] `sub_1407A9DD0` | 0x1407A9DD0 / 0x7A9DD0 | rcx=CCamDriver | camera | `48 8B C4 48 89 58 18 48 89 70 20 55 48 8D 68 A1 48 81 EC B0 00 00 00` |
| Active-cam blend/quat compute (this=multicam) | `sub_14060B390` | 0x14060B390 / 0x60B390 | rcx=ForzaMultiCam; `[rcx+0x5C0]` obj, `[rcx+0x5C8]` ctrl | sim/camera | `48 8B C4 48 89 58 18 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 28 FF FF FF 48 81 EC A0 01 00 00 0F 29 70 B8 0F 29 78 A8 44 0F 29 40 98` |
| CCamDriver ctor (factory) | `sub_14079B8A0` | 0x14079B8A0 / 0x79B8A0 | rcx=new CCamDriver, dl=flag | init | installs `off_145E3F290`; see §2 |
| ForzaMultiCam installer (writes +0x5C8 slot) | `sub_1405EF780` | 0x1405EF780 / 0x5EF780 | rcx=ForzaMultiCam | init | `48 8B C4 48 89 58 20 4C 89 40 18 48 89 50 10 48 89 48 08 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 48 FA FF FF` |

Known vtable constants verified present in THIS DB:
- CCamDriver concrete `0x145E3F290` (named `off_145E3F290`) — 1 xref (the ctor §2). ✓
- CCamDriver refcount `0x145E1FF90` (`unk_145E1FF90`) — xrefs in `sub_1405EF780`. ✓
- ForzaMultiCam refcount `0x1465D4990`, concrete `0x1465D6808` / `0x1465D6BA0` — present; slots dumped (vtable-only refs, no direct lea-rip in code path). ✓
- ForzaMultiCam vtables also seen as `off_145E1E558` (+0) and `off_145E1E8F0` (+0x530) in ctor/dtor `sub_1405F2430`.

---

## 9. Files / scripts produced
- Analysis scripts: `E:\SteamLibrary\steamapps\common\ForzaHorizon5\FH5CameraProbe\scripts\ida_empress_cam_hooks5.py` … `ida_empress_cam_hooks9.py`
- Raw outputs: `E:\tmp\fh5_re\phase5.out` … `phase9.out`
