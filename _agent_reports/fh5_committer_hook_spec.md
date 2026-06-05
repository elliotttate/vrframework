# FH5 Camera-CBuffer Committer — Hook Spec & Conflict Resolution

Date: 2026-06-05
Scope: READ-ONLY reverse-engineering. Game was NOT launched; no code edited. All conclusions
drawn from existing IDA decompile, prior runtime logs, and RenderDoc audit artifacts already
on disk.

---

## TL;DR / Verdict

**The memory note (`fh5-view-writer.md`) is WRONG and the offline ranker is RIGHT.**
`sub_141017C30` is a **dead/static committer at runtime** — it was **proven `calls=0` in a live
run** while the frame loop was clearly running (slSetConstants fired 931×). **Do NOT hook it.**

There is **no single per-frame "committer" function** that memcpys the 6912-byte render camera
cbuffer. That block is filled by a **Coherent Mapped Memory write into the persistently-mapped
UPLOAD ring** (no clean call site in the decompile). The project already solved this the right
way: a **bounded, SEH-guarded, refill-detected in-place transform inside the
`CreateConstantBufferView` hook** in `src/DxgiProxy.cpp` — this is race-free and is **confirmed
working live (all 6 DOF, no crash)**. The user's premise that "scanning crashes" applies only to
the *full upload-ring scan*; the *bounded per-CBV-creation* transform does not scan and does not
crash.

**Recommendation: do not hunt for a native committer. Reuse the working `Hook_CBV` mechanism in
`DxgiProxy.cpp` (§5).** If a CPU-side native hook is still wanted as an upstream lane, the only
real per-frame producer is `sub_140BB1EE0` (the proven view producer), not any memcpy committer.

---

## 1. `sub_141017C30` — full decompile & what it actually does

Source: `E:\ForzaHorizon5_IDA_Decompile\pseudocode\fh5_000087.c:2049-2058`
Index: `E:\ForzaHorizon5_IDA_Decompile\index.jsonl:86938`
(`ea=0x141017C30`, `size=37`, `pseudo_lines=8`, `.text`, `is_thunk=false`)

```c
/* ea=0x141017C30 name=sub_141017C30 size=37 */
void *__fastcall sub_141017C30(__int64 a1, const void *a2)
{
  void *result; // rax
  result = memcpy((void *)(a1 + 48), a2, 0x1A00uLL);  // copy 6656 bytes a2 -> a1+48
  *(_BYTE *)(a1 + 6744) = 1;                            // set dirty flag at a1+0x1A58
  return result;
}
```

Disassembly cross-check (from `fh5_hook_aob_contracts.md:53`, raw bytes on disk):
`40 53 48 83 EC 20 48 8B D9 41 B8 00 1A 00 00 48 83 C1 30 E8 BA 81 39 04 C6 83 58 1A 00 00 01 48 83 C4 20 5B C3`
= `push rbx; sub rsp,20; mov rbx,rcx; mov r8d,0x1A00; add rcx,0x30; call memcpy; mov byte [rbx+0x1A58],1; add rsp,20; pop rbx; ret`. Confirms the pseudocode exactly.

Signature / behavior:
- **Params:** `a1` = `__int64` (destination *object*, `rcx`), `a2` = `const void*` (source block, `rdx`). 2 args, `__fastcall`.
- **Writes:** 0x1A00 = **6656 bytes** from `a2` into `a1+0x30` (=a1+48), then sets a 1-byte dirty
  flag at `a1+0x1A58` (=a1+6744 = 48 + 6656 + 40).
- **`a2` is the SOURCE** (freshly-built view constants), **`a1+48` is the DESTINATION** (a member
  buffer inside a persistent parameter object). It is a trivial "copy view block into object +
  mark dirty" setter.
- **Block is 6656 (0x1A00), not 6912 (0x1B00)** — 256 bytes smaller than the GPU render cbuffer.

So *structurally* it looked like the perfect committer (size ≈ the camera block, a dirty flag that
implies a later upload). That is exactly why the offline decompile pass nominated it. The problem
is purely runtime liveness (§3).

---

## 2. Does it write the ~6656/6912 camera block? Is it per-frame?

- **Does it write a camera-shaped block?** Structurally yes — 6656 bytes, dirty flag. But this is
  256 B short of the render cbuffer (6912), so even if live it would be a *different* (upstream)
  copy, not the GPU-read render block with VIEW@0 / VP@0x40 / camRelVP@0x100 at the render layout.
- **Which arg holds the destination?** `a1 + 48` (object member). `a2` is the source.
- **Per-frame at runtime? NO.** See §3 — proven `calls=0` over an ~8 s live run.
- **Direct callers in the decompile:** NONE. Grepping all 765k pseudocode functions for
  `sub_141017C30(` returns only its own definition shard
  (`fh5_000087.c`). It is reached (if ever) only via an indirect/vtable dispatch, consistent with
  it being a cold/unused setter on a parameter-object class.

---

## 3. DECISIVE runtime evidence — the conflict is resolved

A prior build of the exact committer hook exists and was **run live**:
- Source: `E:\SteamLibrary\steamapps\common\ForzaHorizon5\FH5CameraProbe\src\ViewWriterHookDll.cpp`
  (MinHook on `kWriterRVA = 0x1017C30`, log-only, also hooks `slSetConstants` for a campos reference).
- Runtime log: `E:\ForzaHorizon5_CameraProbeLogs\view_writer\fh5_vw_20260604_114330.log`

The log (verbatim, lines 1-7):
```
#fh5 view-writer hook; module=0x7FF7C0590000 writer=0x7FF7C15A7C30 (LOG-ONLY; control file enable)
sl_hooked,0x7FFEC45B4050
writer_hooked,0x7FF7C15A7C30
status,calls=0,dcalls=0,slcalls=244,...campos=-5058.7/179.5/-271.7,...
status,calls=0,dcalls=0,slcalls=478,...campos=-5058.7/179.5/-271.7,...
status,calls=0,dcalls=0,slcalls=705,...campos=-5058.7/179.5/-271.7,...
status,calls=0,dcalls=0,slcalls=931,...campos=-5058.7/179.5/-271.7,...
```

Interpretation (decisive):
- Hook installed at the **correct VA** (`0x7FF7C15A7C30` = module `0x7FF7C0590000` + RVA `0x1017C30`).
- **`slSetConstants` fired 931×** over ~8 s with a live moving campos → the **render/frame loop was
  running** and producing real per-frame camera data.
- **`sub_141017C30` fired `calls=0` the entire run.** It is **not invoked per-frame** (and not at
  all in that session) → **dead/static committer trap, runtime-confirmed.**

The project's own status doc records the same conclusion:
`FH5CameraProbe\RENDERER_FREECAM_STATUS.md:169-170`:
> "`sub_141017C30` (the `0x1A00` memcpy committer) is **not** the per-frame writer (`calls=0`) — the
> real per-frame path is in obfuscated decompile gaps; catch it at runtime, not statically."

**Resolution: the offline ranker's "keep penalized until runtime proves otherwise" was the correct
call; runtime proved it dead. The MEMORY.md note `fh5-view-writer.md` should be corrected/retired.**

---

## 4. Alternative write-sites (in case you still want a native CPU hook)

None of these is a clean memcpy of the 6912 render block — that block has no single committer
(it's a coherent-mapped upload write). The candidates below are the *upstream* camera-state and
*binding* sites that the project identified. Each is annotated with whether it touches the RENDER
cbuffer (6912 with VIEW/VP/camRelVP) or an UPSTREAM camera struct.

| RVA / VA | Name | What it does | RENDER 6912 block? |
| --- | --- | --- | --- |
| `0x1017C30` / `0x141017C30` | `sub_141017C30` | memcpy 6656 B src→`a1+48`, set dirty `a1+6744`. **Runtime-dead (`calls=0`).** | NO — upstream object member, 6656 B, and dead |
| `0x746BB0` / `0x140746BB0` | `sub_140746BB0` | **State bridge.** Builds a ~0xC4-byte block at `a1+0xC4..0x188`, copies 13 OWORDs+1 dword into `*[a1+0x198] + 0x660` (ForzaMultiCam+0x660). Decompile: `fh5_000055.c:14175-14220`. | NO — writes camera-to-world POSE into ForzaMultiCam (gameplay/cull state), not the VP cbuffer |
| `0x746C6B` / `0x140746C6B` | (inner of `sub_140746BB0`) | The point after the protected builder/finalizer, before the copy: `rdi=a1+0xC4`, `rbx=a1`, dst `*[rbx+0x198]+0x660`. Best *probe* point on the bridge. | NO — same pose copy as above |
| `0x6BE3A0` / `0x1406BE3A0` | `sub_1406BE3A0` | `CCamDriver+0x320` matrix/state lane helper (reads `a1+0x320`, calls a vtable entry). Best *upstream gameplay-camera* lane for culling coherence. | NO — gameplay camera state, upstream of render |
| `0x6B0C20` / `0x1406B0C20` | `sub_1406B0C20` | Downstream **getter** of ForzaMultiCam render block (movaps reads of `+0x650..+0x680`). Useful as proof/readback, not a write site. | reads a derived block; not the VP cbuffer write |
| `0x56E1010` / `0x1456E1010` | `sub_1456E1010` | Builds **FSR2 projection constants** (near/far/fov). Downstream consumer/sink. | NO — upscaler proof sink |
| `0xBB1EE0` / `0x140BB1EE0` | `sub_140BB1EE0` | **The per-frame VIEW PRODUCER** (Empress RVA 0xBB1EE0). Proven: detour it and rotate input matrix `a4` → the rendered camera rotates (yaw proven, reversible). This is the real upstream per-frame camera hook. | Produces the view that *feeds* the render cbuffer; NOT a memcpy of the 6912 block, but it is the live per-frame source |

Runtime CreateConstantBufferView **binding** path (from
`_agent_reports\runtime_backtrace_rvas.txt`, captured live 2026-06-04). These are the descriptor
**bind/alloc** frames, **not** the matrix producer — confirmed to be allocator/refcount machinery:
- `0x1409A3010` (contains backtrace ea `0x1409A3021`): allocator helper (`48*a3` size →
  `sub_144E27860`), `fh5_000063.c:22612`. Not a writer.
- `0x140BB43B0` (contains `0x140BB4401`): ref/object-management around CBV creation,
  `fh5_000072.c:12646`. Not a writer.
- `0x140E9FE80` (contains `0x140E9FEC1`) and `0x140BB48F0` (contains `0x140BB48F6`): thin wrappers.

The backtrace file's own note is explicit:
> "This is the BINDING path. The matrix PRODUCER writes via memcpy into the persistently-mapped
> UPLOAD buffer (per RenderDoc audit) on a different path."

RenderDoc upload analysis confirms there is **no CopyBufferRegion / mapped write that targets a
6912-CBV-backing resource** that the tooling could resolve
(`_agent_reports\renderdoc_audit\ForzaCockpit_upload.md`,
`ForzaCockpit_camwrite4.md`: "Does any CopyBufferRegion target a 6912-CBV-backing resource? set()";
the camera block is written via "Internal::Coherent Mapped Memory Write" — i.e., the CPU writes
directly into the persistently-mapped upload ring, no API call to hook). The `*camwrite*.md`
captures that tried to find the producer failed to even replay (`ForzaCockpit_camwrite.md`:
"opencap fail").

---

## 5. RECOMMENDATION — the single best hook to get a live pointer to the 6912 block

**Do not look for a native committer; there isn't one. Use the renderer-layer interception that is
already built and proven working live.**

### 5a. Primary recommendation — `CreateConstantBufferView` hook (already working)
Mechanism (implemented in `FH5CameraProbe\src\DxgiProxy.cpp`, documented in
`RENDERER_FREECAM_STATUS.md` §3-§4):

- Hook `ID3D12Device::CreateConstantBufferView` (**vtable index 17** on the game's device).
- For every CBV whose `SizeInBytes == 6912 (0x1B00)`, resolve its `BufferLocation` GPU-VA → CPU
  pointer using a GPU-VA→CPU map built from hooked `CreateCommittedResource` (vtbl 27) /
  `CreatePlacedResource` (vtbl 29) + `Map()` of UPLOAD/CPU-visible buffers.
- This hands you a **live CPU pointer to the exact 6912-byte render camera cbuffer the GPU is about
  to read**, at the moment it is bound (post-data-write, pre-draw) — i.e. **race-free without
  scanning.** Transform VIEW@+0, VP@+0x40, camRelVP@+0x100 in place under `__try` (SEH).

This is the literal answer to the requirement "a live pointer to the 6912B render camera cbuffer at
the moment it is written." It is already validated live: pitch/yaw/roll/translation all move the
whole world cleanly, no crash (`RENDERER_FREECAM_STATUS.md` §2, §7).

**6912-byte render block layout (RenderDoc-confirmed, `RENDERER_FREECAM_STATUS.md` §3 +
`renderdoc_audit\*_6912_field_map.md`):**
| Field | Offset | Notes |
| --- | --- | --- |
| VIEW (4×4) | +0x000 | column-vector / row-major; VP = Proj·VIEW |
| VIEW-PROJECTION (4×4) | +0x040 | |
| cameraPos | +0x080 | `.w == 1.0` for the MAIN camera |
| near | +0x0A0 | **0.1** (main camera) |
| far | +0x0A4 | **50000** (main camera) |
| camera-relative VP (4×4) | +0x100 | VP with translation/col-3 zeroed |
| cameraPos duplicates | +0x140, +0x150 | |
| quantized cameraPos | +0xC20 | |
| camRelVP duplicate | +0xC40 | exact current-frame dup in captures (history candidate) |

**Block-size / validity detection (the discriminator — root sig alone is NOT enough,
`renderdoc_audit\SUMMARY.md`):**
- `SizeInBytes == 6912`, AND
- `near ∈ [0.08, 0.2]` at +0xA0 AND `far ∈ [45000, 55000]` at +0xA4, AND
- `cameraPos.w ≈ 1.0` at +0x8C, AND a sane orthonormal VIEW matrix at +0x000
  (`LooksLikeView`: finite, row 3 ≈ (0,0,0,1), row norms ≈ 1).
- Auxiliary/secondary 6912 CBVs exist with near≈1/far≈1000/pos.w≈2 (or near≈0.01) — **reject these.**
- Apply at most **once per refill** (hash the 64-byte VIEW; store the hash you wrote) to avoid
  double-applying the rotation → degenerate matrix → black screen.

AOB note: there is **no native prologue AOB** for this path because the hook is on the D3D12
device vtable, not a code-pattern. The vtable indices above (17 / 27 / 29 / 10) are the contract.

### 5b. If you specifically want a native CPU-side per-frame hook (upstream lane)
Use **`sub_140BB1EE0`** (Empress RVA `0xBB1EE0`), the **proven per-frame view producer**: detour it
and transform its input/output view matrix (`a4`) — yaw is already proven reversible there (memory
note `fh5-upstream-hook-pivot.md`). This is the correct upstream foundation if you want the
transform *before* the cbuffer is filled (so culling/TAA see a consistent view). It is **not** a
memcpy of the 6912 block — it produces the view that feeds it. AOB/contract for it is not in
`fh5_hook_aob_contracts.md` (that file only covers the bridge/binding/committer candidates), so
pull its prologue from the decompile if you go this route.

**Do NOT use** for the in-place 6912 write:
- `sub_141017C30` — runtime-dead (`calls=0`), wrong size (6656), upstream object member.
- `sub_140746BB0` / `0x140746C6B` — writes a 0xC4 pose block into ForzaMultiCam+0x660 (gameplay/cull
  state), not the VP cbuffer.

---

## 6. Repo prior-work summary (grep results for 1017C30 / committer / view-constants / 6656)

Files found and what they say:
- `src/ViewWriterHookDll.cpp` — **the committer hook itself** (MinHook on RVA 0x1017C30,
  log-only by default, auto-discovers campos offset, post-multiply transform of VIEW/VP/camRelVP at
  +/-128 around campos). Built (`build-vs\Release\FH5ViewWriterHookDll.dll` exists). **Ran once;
  proved `calls=0` (dead).**
- `_agent_reports\upstream_offline\SUMMARY.md:30` — ranks it "still a dead/static committer trap;
  keep penalized until runtime proves otherwise." (Correct.)
- `_agent_reports\upstream_offline\fh5_hook_aob_contracts.md:48-54` — names it
  `dead_static_committer_sub_141017C30` and gives its on-disk bytes.
- `RENDERER_FREECAM_STATUS.md:169-170` — records `calls=0` runtime result and the pivot to the
  in-CBV-hook transform.
- `CMakeLists.txt` — builds the `FH5ViewWriterHookDll` target.
- `docs\FH5_CAMERA_AND_HOOKS.md`, `docs\PARALLEL_RE_SHOPPING_LIST.md`,
  `UPSTREAM_HOOK_PLAN.md` — reference the view-writer / committer concept.

Net: the committer was fully implemented, hooked live, and **disproven**. The project moved to the
working renderer-layer (`DxgiProxy.cpp`) interception.

---

## 7. Citations index
- Decompile body: `E:\ForzaHorizon5_IDA_Decompile\pseudocode\fh5_000087.c:2049-2058`
- Index entry: `E:\ForzaHorizon5_IDA_Decompile\index.jsonl:86938`
- Bridge `sub_140746BB0`: `E:\ForzaHorizon5_IDA_Decompile\pseudocode\fh5_000055.c:14171-14220`
- Binding-path frames: `E:\ForzaHorizon5_IDA_Decompile\pseudocode\fh5_000063.c:22610-22625`,
  `fh5_000072.c:12643-12690`
- Runtime proof: `E:\ForzaHorizon5_CameraProbeLogs\view_writer\fh5_vw_20260604_114330.log:1-7`
- Committer hook source: `E:\SteamLibrary\...\FH5CameraProbe\src\ViewWriterHookDll.cpp`
- Working mechanism: `E:\SteamLibrary\...\FH5CameraProbe\src\DxgiProxy.cpp` +
  `RENDERER_FREECAM_STATUS.md` §3-§4
- Offline ranker verdict: `..\_agent_reports\upstream_offline\SUMMARY.md:30`
- AOB contracts: `..\_agent_reports\upstream_offline\fh5_hook_aob_contracts.md`
- Upload/no-committer evidence: `..\_agent_reports\renderdoc_audit\ForzaCockpit_upload.md`,
  `ForzaCockpit_camwrite4.md`, `runtime_backtrace_rvas.txt`
- 6912 layout: `..\_agent_reports\renderdoc_audit\*_6912_field_map.md`, `SUMMARY.md`
