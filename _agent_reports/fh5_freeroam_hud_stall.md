# FH5 VR — Free-Roam HUD-on-Quad: The Render-Stall Wall

> Status: the draw-redirect's **resolution is fixed and correct** for the free-roam eye-source HUD draws, but **enabling `uiredirect` hard-stalls FH5's free-roam render within ~1 s** — and the stall has been isolated to the **per-draw hook work itself**, not the redirect action, not focus loss. Root cause not yet pinpointed. Menu/hub HUD-on-quad works today; only free-roam is blocked.

---

## Goal

Building a VR mod for **Forza Horizon 5** (Empress de-DRM build) — `FH5VR.dll`, a `dxgi.dll` proxy using OpenXR (repo `E:\Github\vrframework`).

Specific objective: render the in-game **HUD (and menus) onto a flat head-locked OpenXR quad layer** while the 3D world stays stereo, with **clean (HUD-free) eyes**.

Mechanism: a D3D12 command-list **"draw redirect"** — peel the HUD draws off the eye-source buffer onto a separate render target (which becomes the quad source), leaving the eye buffer as clean world.

---

## Background / architecture (confirmed)

- **Eye copy.** Each frame the mod copies the game's swapchain backbuffer (the "eye source") into the OpenXR eye. See `D3D12Component::on_frame` (`D3D12Component.cpp:136`); `eye_texture = swapchain->GetBuffer(...)` (`D3D12Component.cpp:159-168`). Live backbuffer = **1600x843, DXGI format 24 = `R10G10B10A2_UNORM`**.
- **Frame structure (PIX-proven, `FH5InGame.wpix`).** The game renders the world composite + HUD into an intermediate RGBA8 buffer (resource `4099`) then `CopyResource` → swapchain → `Present`. The world composite is a **3-vertex fullscreen `DrawInstanced`** with pixel-shader DXBC hash `7783d957...`, matched at PSO-create time. The HUD draws follow the composite **into the same buffer**.
- **The redirect subsystem** lives in `Fh5CameraCbuffer.cpp`. It hooks:
  - `CreateRenderTargetView` — device vtable 20 (`Hook_RTV`, `Fh5CameraCbuffer.cpp:487`)
  - `CreateGraphicsPipelineState` — device vtable 10, flags the composite PSO by PS hash (`Hook_CreateGfxPSO`, `:526`; hash table `kCompositePsHash`, `:511`)
  - `OMSetRenderTargets` — command-list vtable 46 (`Hook_OMSetRT`, `:727`)
  - `DrawInstanced` — vtable 12 (`Hook_DrawInst`, `:871`)
  - `DrawIndexedInstanced` — vtable 13 (`Hook_DrawIdx`, `:862`)
  - `SetPipelineState` — vtable 25 (`Hook_SetPSO`, `:740`)
  - `ClearRenderTargetView` — vtable 48 (`Hook_ClearRTV`, `:748`)
- **Control gate:** key `uiredirect` in `E:\tmp\fh5vr_ctl.txt` (parsed at `:118`; `g_ctl_ui_redirect` — `0`=off, `1`=redirect, `2`=classify-only dry run).
- **Install:** `ui_redirect_install` (`:901`), called once from `on_frame` (`D3D12Component.cpp:156`). The command-list hooks are installed via a throwaway list's vtable (`:945-961`); the per-draw entry point is `BeginDrawRedirect` (`:805`).
- **Diagnostics** logged to `E:\Games\ForzaHorizon5Empress\FH5VR.log` as `[FH5UIR] frame:`, `[FH5UIR] bb-seq:`, `[FH5UIR] RT-tally:` (all emitted from `ui_redirect_on_present`, `:971`).

---

## BREAKTHROUGH — root cause of why the redirect never worked (FIXED)

Prior sessions concluded *"the free-roam HUD can't be separated — only one composite draw (`bb-seq=C3`) ever hits the eye buffer."* **That was a measurement artifact, not reality.**

- **Root cause: RTV-handle aliasing.** The redirect's RTV→resource map (`g_rtv_map`, `:449`) was keyed **only by CPU descriptor handle** (`handle.ptr`). FH5 reuses **one** handle (rtvHeap slot 0 / `GetCPUDescriptorHandleForHeapStart`) for nearly every `CreateRenderTargetView`, and records command lists in **parallel** across threads. So the global handle→resource map is overwritten nondeterministically, and `ResolveRtvResource(handle)` (`:657`) at draw time returns a **stale/wrong resource**.
- **Consequence:** the free-roam HUD draws (into the RGBA8/display buffer) mis-resolved to the **HDR `R11G11B10` scene buffer** → the `IsFullscreenSwapFormat` filter (`:794`) rejected them → they were never classified as HUD.
- **Proven live:** the RT-tally showed **~1829 draws/frame ALL misattributed** to one HDR buffer `0x79D0C870 [1152x864 f26=R11G11B10_FLOAT]`.
- **FIX — thread-id-keyed RTV map.** `g_tid_rtv_map` (`:474`) with `RecordRtvTL` (`:475`) / `ResolveRtvTL` (`:479`), resolved **at bind time** in `Hook_OMSetRT` and stored in `ListBinding::bb_res` (`:696`). `CreateRTV → OMSetRenderTargets → Draw` for one binding always run **adjacent on one recording thread**, so keying by `GetCurrentThreadId()` pairs them race-free.
- **NB — deliberately NOT C++ `thread_local`.** A dynamically-initialised `thread_local` **faults** when first touched on a game render thread that predates the proxy DLL load → first-chance AV → NVIDIA VEH crash. Use a tid-keyed global map under a mutex instead (commented at `:470-472`).

---

## Second bug fixed — the rebind crash

- The redirect's "UI render target" (`g_uir_ui_rt`) was created as `R8G8B8A8_UNORM`, but the live eye-source/backbuffer is `R10G10B10A2` (format 24).
- Rebinding a HUD draw (whose PSO outputs `R10G10B10A2`) to our `R8G8B8A8` RT = a **PSO/RTV format mismatch → GPU device-removal → crash within a few frames**.
- **FIX:** create `g_uir_ui_rt` with `scd.BufferDesc.Format` (the swapchain format) instead of hardcoded RGBA8 — `ui_redirect_install`, `Fh5CameraCbuffer.cpp:931` (`rd.Format = scd.BufferDesc.Format ? ... : DXGI_FORMAT_R8G8B8A8_UNORM`; rationale at `:928-930`).

---

## THE CURRENT WALL — enabling the redirect hard-stalls free-roam (~1 s), and it's the per-draw hook work itself

With resolution fixed (so the redirect now **correctly** targets the free-roam eye-source HUD draws), enabling `uiredirect` hard-stalls FH5's render within **~1 second**. Definitively isolated across **~8 launch cycles**:

- **Not focus-loss.** Ran a background focus-keepalive (`E:\tmp\fh5_keepfocus.ps1` — `AttachThreadInput` + `SetForegroundWindow` every 400 ms on the "Forza Horizon 5" window) so FH5 stayed foreground; enabling `uiredirect` **still stalled within ~1 s**.
  - (Separately, FH5 + SimXR *does* pause on focus loss — UI page `PauseMenuTiled*`, `render3d=0` — which cost several cycles of confusion, but the keepalive ruled it out as the stall cause.)
  - Earlier free-roam rendered continuously (`main_rate~3000`) **without** `uiredirect`.
- **Not the action taken.** The stall happens with the redirect doing **nothing** that frame — diagnostics consistently show `total 0` / `uiDrawsRedir=0` / `bb-seq n=0` (zero eye-source draws classified, zero redirected) yet the render dies. Same across:
  - **skip-mode** (drop the HUD draw),
  - **rebind-mode** (`OMSetRenderTargets` to our UI RT),
  - **format-matched rebind**.
- **It's a true hang.** 0 fps, ~19 s with no `[VR-COPY]` log lines, the game's per-frame producer (`[FH5] main=`) stops. Process stays **alive** (not a crash). State file: `main_rate=0`; `[FH5UI] render3d=0 pages=[PauseMenuTiled*,Hud*,Loading*]`.

### Why it's the per-draw path

The instant `g_ctl_ui_redirect != 0`, `BeginDrawRedirect` (`Fh5CameraCbuffer.cpp:805`) runs **per-draw locked map work on EVERY draw**, across FH5's **multiple parallel command-recording threads**, at free-roam draw volume (**~5000 draws/frame**):

- `GetListBinding` / `g_uir_state_mtx` (`:711`, `:698`)
- `TallyRtDraw` / `g_rt_mtx` (`:687`, `:685`)
- `IsEyeSource` / `g_eye_src_mtx` (`:617`, `:615`)
- plus `IsCompositePso` (`:521`) and `RecordBbSeq` (`:783`) for eye-source draws.

A full 0-fps hang (not a slowdown) most likely means **either**:

- **(a)** a **first-chance exception** in that path escalated into a hang by NVIDIA's vectored exception handler (the documented failure class for this mod — `nvwgf2umx` VEH recursing on first-chance AVs the mod generates); **or**
- **(b)** a **mutex left locked** because a thread holding it hung.

**Not yet pinpointed.**

---

## What we've tried (chronological summary)

- **Fixed the RTV-handle aliasing** (tid-keyed resolution). *[breakthrough]*
- **Removed `thread_local`** (faults on pre-existing game threads → VEH crash) in favor of a tid-keyed global map.
- **Fixed the rebind format mismatch** (UI RT now matches swapchain format 24).
- **Tried 3 redirect actions, all stall:** skip the HUD draw; rebind to UI RT (`R8G8B8A8`, crashed on format); rebind to format-matched UI RT.
- **Added a `uiredirect=2` dry-run** (classify-only, no action) — also destabilizes (`:844`).
- **Switched the classification gate** from `IsFullscreenSwapFormat` (calls `GetDesc` — a `GetDesc` on a transient/stale resolved pointer was a suspected first-chance-AV source) to `IsEyeSource` (a pure pointer-set lookup, no `GetDesc`; gate at `:823`, rationale `:819-822`).
- **Built a focus-keepalive harness** to isolate focus-loss from the stall (proved the stall is **not** focus).
- **~8 full launch + navigate cycles** (each ~5-7 min; boot is intermittently flaky — splash/loading stalls needing retries).

---

## Verification tooling notes

- **Live capture of the eyes:** the OpenXR Simulator (SimXR) preview window is titled **"O"**; capture via `wslsnapit`.
- The flat game window ("Forza Horizon 5") presents the **same eye-source buffer**, so if the redirect cleaned the eyes the flat window would also be clean — captured via `wslsnapit` `PrintWindow` (does not steal focus).
- The redirect's effect on the eyes was **never cleanly verified**, because the render stalls within ~1 s of enabling, before it processes a full HUD frame.

---

## What works today

- **Clean stereo world in VR** (the 6-DOF / projection path).
- **Menu/hub HUD-on-quad** — the redirect *does* cleanly peel the HUD as separable draws in menu/hub scenes; **only free-roam stalls**.
- **The OpenXR quad-layer infrastructure** — head-locked quad in `view_space`, live-tunable via `hudquad` / `hudw` / `hudx` / `hudy` / `hudz` (submission in `D3D12Component::on_frame`, `D3D12Component.cpp:249-288`; quad source switches to `ui_redirect_target()` at `:257-261`).
- **Navigation to free-roam** (Space = "Drive" from the My-Cars hub).

---

## Plan / next steps

1. **PRIMARY (in progress): pinpoint the stall with x64dbg by LAUNCHING FH5 UNDER the debugger** (not attaching late).
   - Launch `ForzaHorizon5.exe` under x64dbg with the SimXR env; set first-chance exceptions to **pass**; navigate to free-roam; enable `uiredirect`; then break into the hang (or catch the escalating first-chance AV) and inspect **all thread call stacks**.
   - Goal: determine whether it's a **first-chance-AV → VEH recursion** (and where the AV originates in the per-draw path) or a **stuck mutex**.
   - *(Earlier ATTACH attempts were blocked by an unreliable x64dbg MCP bridge on port 8888 — `ExecCommand`/`GetModuleList` intermittently refused.)*
2. **Once pinpointed:** fix the specific cause — e.g. eliminate the offending `GetDesc`/deref or lock, or move the per-draw work off the hot path.
3. **Fallback approaches** if per-draw interception proves fundamentally unviable for free-roam:
   - **(a) composite-snapshot** — one `CopyResource` of the eye-source right after the world-composite draw (PS `7783d957`) to get clean-world eyes, and/or a **DXIL rewrite** of the final-combine pixel shader to emit the HUD to a separate RT;
   - **(b) accept baked free-roam HUD** (what the mature FH6 VR mod does) + keep the working menu/hub quad.
