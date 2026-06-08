# FH5 VR — HUD-on-Quad: Status, Findings, Plan (2026-06-07)

## The goal

`FH5VR.dll` is a `dxgi.dll`-proxy VR mod for **Forza Horizon 5** (Empress de-DRM build, ForzaTech/D3D12),
presenting via **OpenXR** (tested against the OpenXR Simulator "SimXR"). Stereo works (per-eye producer hook +
AFR + OpenXR projection layers). **The remaining goal: render the in-game HUD (and menus) onto a flat,
head-locked OpenXR quad layer, while the two eyes show a clean, HUD-free stereo world.** Today the HUD is
"baked" mono into both eyes.

Repo: `E:\Github\vrframework`. Game: `E:\Games\ForzaHorizon5Empress\`. Control file: `E:\tmp\fh5vr_ctl.txt`.
Log: `E:\Games\ForzaHorizon5Empress\FH5VR.log`.

---

## Where we are now

- **Stereo + camera + OpenXR quad infrastructure: built and working.** The mod creates a dedicated HUD
  swapchain and submits a head-locked `XrCompositionLayerQuad` (`hudquad=on`, live knobs `hudw/hudx/hudy/hudz`).
- **The UI-redirect machinery is built** (`Fh5CameraCbuffer.cpp`): hooks `CreateRenderTargetView`,
  `CreateGraphicsPipelineState`, command-list `OMSetRenderTargets`/`SetPipelineState`/`DrawInstanced`/
  `DrawIndexedInstanced`/`ClearRenderTargetView`/`CopyResource`/`CopyTextureRegion`. Control knob `uiredirect`
  (0=off, 1=redirect, 2=classify-only, 3=R10A2-non-eye probe).
- **Crashes are largely mitigated** (see below) — the launcher now auto-retries boot crashes and suppresses
  overlay injectors, so we *can* get to free-roam most launches.
- **We can now RenderDoc-capture the game** in a special no-OpenXR mode (the breakthrough that unblocked the
  investigation).

---

## Problems overcome this session

1. **Overlay-VEH crash storm (the dominant blocker).**
   Root cause: the Empress de-DRM throws first-chance exceptions as normal operation (`EMP.dll+0x21A1C` reads
   `0x76350000` in a tight loop; sometimes storms hundreds/ms). An overlay's **Vectored Exception Handler**
   running ahead of the de-DRM recurses on these → stack-overflow crash. We found **three** injectors live:
   **Xbox Game Bar**, **Discord**, and **NVIDIA `nvspcap64.dll` (ShadowPlay)**. Fixes:
   - Disabled Game DVR (HKCU registry) + the launcher now kills Game Bar + Discord every launch
     (`Launch-FH5VR.ps1`, `-KeepOverlays` to skip). Removing those two correlated with the first crash-free runs.
   - The recursing VEH in the old crash dumps is in **`nvwgf2umx.dll`** = the *core NVIDIA D3D12 driver UMD*,
     which is **unremovable** (stopping `NvContainerLocalSystem` does NOT remove `nvspcap64`; it's driver-loaded).
     So the durable fix is "the mod must not fault" — which we verified (see #2/#3) — plus removing the
     *removable* overlay VEHs so they can't recurse.
   - Added a diagnostic VEH (`DiagVeh` / `[FH5VEH]` in `Fh5CamDriver.cpp`, registered first) that logs
     first-chance faults **only inside our own DLL** (self-range gated, so it never touches/​amplifies the
     de-DRM's exceptions). A clean boot logs exactly ONE first-chance (the de-DRM's, handled).
   - `Launch-FH5VR.ps1` now **auto-retries boot crashes** (the de-DRM intermittently dies at the ~25–35s
     splash; relaunch up to 5×, surviving ~42s = booted).

2. **Use-after-free in the always-on RTV hooks.** `Hook_OMSetRT` called `GetDesc` on a *resolved* (possibly
   freed) cached resource pointer during scene-transition resource churn → a wild vtable call SEH can't catch.
   Fix: compute the fullscreen/dims verdict at **CreateRTV time** (resource provably alive) and cache it in the
   RTV map; the hot path reads only the cached ints.

3. **RTV resolution was resolving NULL for ~95% of draws** (the per-thread RTV map assumed CreateRTV and
   OMSetRenderTargets happen on the same thread; FH5 creates RTVs on one thread and binds on many). Replaced
   with a **global lock-free open-addressing handle→{resource,fs,dims} map** (most-recent-write-per-handle is
   unambiguous since a descriptor holds one view at a time). After this, the world composite is correctly
   detected (`compositeDraws` 0 → ~104/s) and null-resolve dropped to ~48%.

4. **The RenderDoc/PIX-vs-mod wrapper conflict** (the reason no modded GPU capture was possible). Injecting
   RenderDoc wraps the D3D12/DXGI objects; the mod's REFramework `D3D12Hook` then **scans the wrapped dummy
   swapchain** for the command-queue offset and access-violates (same class as the RTTI probe we already skip).
   Fix: **`FH5VR_NO_OPENXR=1`** env → `Framework::hook_d3d12()` is skipped entirely → no dummy swapchain → no
   wrapper conflict → FH5 renders flat and **`renderdoccmd capture` works** (renderdoc.dll resident, overlay
   live). This is what let us finally capture the modded process's render path.

5. **HUD quad copy format bug** (caused the magenta quad). The HUD swapchain was hardcoded
   `R8G8B8A8_UNORM_SRGB`, but free-roam gameplay is **HDR R10G10B10A2 (fmt 24)**; `copy_hud` uses `CopyResource`
   (no conversion), which is invalid R10A2→RGBA8_SRGB. Fix: the HUD swapchain now **format-matches the
   backbuffer** (same mapping the eye swapchains use). `D3D12Component.cpp`.

---

## The key finding (the breakthrough)

Using the no-OpenXR capture mode, we captured the **flat free-roam** frame and analyzed it
(`E:\tmp\fh5_rdc\fh5_noxr_frame68577.rdc`, via `qrenderdoc --python`):

- The swapchain backbuffer (`1152×864 R8G8B8A8`) receives, **in order**: one **3-vertex fullscreen composite**
  (the clean world), then **~200 HUD draws directly into it** — `DrawIndexedInstanced idx=30` glyph batches +
  `DrawInstanced` panels, all distinct UI shaders, all after the composite.
- **⇒ FH5's free-roam HUD is ~200 cleanly separable post-composite draws on the swapchain. The HUD IS
  separable.** This **overturns** the prior multi-session conclusion ("free-roam HUD goes to per-widget RTs,
  no separable layer"). The robust discriminator is **"first fullscreen draw = clean world; every UI-shader
  draw after it = HUD"** (not a fixed PS hash — the composite PS was `2847` here vs `7783d957` in an earlier
  cockpit capture).

### The twist that's still blocking us

That clean path is the **flat (no-OpenXR)** render. In the **live AFR/OpenXR** path, our hooks measure those
~200 HUD draws as **absent from the eye-source** (only the composite hits it); the HUD instead goes to small
per-widget RTs. **Turning on AFR changes how FH5 composites the HUD.** And we can't capture the AFR path
directly (RenderDoc conflicts with the full mod; disabling OpenXR to make it capturable also removes the AFR).
So the AFR HUD routing is only observable via live in-mod instrumentation.

---

## How other VR mods do it (survey: UEVR, REFramework, starfield2vr, anvilengine2vr)

None of them redirect ~200 individual draws. **All use the engine's UI seam:**
- **UEVR**: UE exposes a `RenderTargetManager` → Slate renders into a dedicated `ui_target` texture → its own
  swapchain → `XrCompositionLayerQuad` with `BLEND_TEXTURE_SOURCE_ALPHA`. The cleanest true separation.
- **REFramework**: hooks the per-element GUI draw, flips the engine's `ViewType` Screen→World (+`Overlay`/
  `Detonemap`) so the UI renders as a correct per-eye world quad in the normal pass. No RT redirect.
- **starfield2vr / anvilengine2vr**: hook the UI viewport-set (`Scaleform::MovieImpl::SetViewport` /
  `calc_ui_viewport`) and rescale/offset the UI viewport per-eye (head-locked panel). No RT redirect.

**FH5's equivalent seam is the UIRenderer (`vf54`)**, which (per the FH3 dev-build symbols + the FH5 decompile)
**owns its render target at `*(this+0x40)`** — i.e. FH5's `ui_target`. That is the thing to grab for the quad
and suppress from the eye, mirroring UEVR.

---

## Current blocker / where the last attempt stands

We added a **`vf54` RT probe** (`fh5cb::probe_ui_renderer_rt`): in the UIRenderer hook, scan the UIRenderer
object (`a1`) and `*(a1+0x40)` for a pointer that matches a known RT from our CreateRTV map (pure
pointer-compare via `RtResourceMeta`, AV-safe), and report its dims/format → `[FH5UIRT]` log.

- First (shallow) probe ran clean but **found nothing** at the naive offsets → the UIRenderer's RT is
  **wrapped/indirect** (an engine RenderTarget object, not a direct `ID3D12Resource*` at `+0x40`), or obtained
  via a method (`GetRenderTarget`).
- A deeper **2-level** scan was added, but the last few launches were denied a stable run by the **intermittent
  splash-stall / hub→drive transition crashes**, so we haven't yet captured a `[FH5UIRT]` line with the deeper
  scan. (The `vf54` hook does fire — `calls=2,3,4` seen — so the probe is reachable; we just need a run that
  survives long enough, and the deep 2-level scan may itself be too heavy on the render thread and worth
  trimming.)

---

## Live AFR update from continued in-game testing

This section supersedes the flat-capture-only assumption above where it conflicts.

### Corrected composite identity

The live AFR/SimXR composite PSO is **not** the flat no-OpenXR hash. In-game telemetry shows:

- Live AFR/SimXR eye-source composite PS:
  `9774ba9764c2075905ec5a3338052510`
- Flat no-OpenXR capture composite PS:
  `7783d9572effba0fdc5cceecdcd40315`

`Fh5CameraCbuffer.cpp` now treats both as composite hashes. With the corrected live hash, the in-game dry run
classifies the eye-source composite again, but still finds **no post-composite HUD draw path**:

- `compositeDraws` present
- `uiDrawsRedir=0`
- `uiPsoCreated=0`
- known flat-capture UI PSOs never appear in the live AFR path

So the actionable live conclusion is now tighter: in AFR, the HUD is already baked into the live composite's
inputs, not drawn after the composite into the eye-source.

### Composite SRV tracing

Added diagnostic-only tracing for the live composite draw:

- Device hooks: `CreateShaderResourceView`, `CreateUnorderedAccessView`, `CopyDescriptors`,
  `CopyDescriptorsSimple`
- Command-list hooks: `SetDescriptorHeaps`, `SetGraphicsRootSignature`,
  `SetGraphicsRootDescriptorTable`
- Per-composite dump: `[FH5COMPSRV]`

The first 65k SRV map was too small; 262k resolved enough to prove the path but still reported table-full
drops. The staged build now uses a 1,048,576-slot SRV map, matching the observed shader-visible heap scale
(`heapN=1000000`). That larger map is built and deployed, but the in-game evidence below came from the 262k
map build.

Representative live evidence:

```text
[FH5COMPSRV] roots=20 scanned=480 resolved=255 noHeap=0 noMap=192 rows=27
... heapCpu=0x307A0000 heapGpu=0x45678A01000000 heapN=1000000 ...
0xAD23B4220[1152x864 f26]=9 r0+s10[RT]
0xAD23B7B80[1152x864 f26]=9 r0+s11[RT]
0xAD23AEC10[1152x864 f26]=9 r0+s2[RT]
```

This proves the live composite samples full-screen RTs directly. The likely HUD path is now "draw into one of
the composite-sampled source RTs, then composite into the eye", not post-composite eye-source HUD draws.

### Source-skip A/B

Added `uiredirect=4` plus `srcroot=N`/`srcslot=N` as a non-copy, non-format-changing discriminator. It records
the resource sampled by the selected composite SRV slot, then skips draws into that source RT on the next frame.

Tested in-game:

- `uiredirect=4`, `srcroot=0`, `srcslot=10`
- Target resolved to a 1152x864 f26 RT:
  `srcskip(root=0 slot=10 target=0x4420D930 1152x864 f26 samples=2-3 skipped=40-50)`
- SimXR screenshot: HUD stayed baked into both eyes; the world did not obviously break.

So slot 10 is **not** the HUD-only lever. It is sampled by the composite, but skipping its 40-50 draws/sec did
not remove the HUD.

Attempted to retarget slot 2 in the same run, but the first implementation kept the stale slot-10 target after
the control change. That bug is fixed now by clearing `g_srcskip_target` when the requested root/slot changes.
The follow-up run crashed before source-skip controls were applied (`uiredirect` absent in the control file), so
slot 2 is still untested.

### Live ShaderToggler UI-PSO proof

ReShade/ShaderToggler was temporarily installed to identify the in-game UI shaders, then removed from the game
root. The active VR mod was redeployed afterward; `dxgi.dll` again byte-for-byte matches
`build-fh5\fh5vr\Release\FH5VR.dll`, and `dxgi_real.dll` is the Microsoft DXGI forward target.

ShaderToggler's hashes are **CRC32 over the raw shader bytecode**, not the DXBC container checksum used by the
older RenderDoc hash list. `Fh5CameraCbuffer.cpp` now keeps these two hash domains separate and logs both.

User-verified ShaderToggler pixel-shader hashes now classify live UI PSOs in the AFR/SimXR run:

```text
[FH5UIPSO] matched UI PSO #1 source=gfx ... dxbc=0 toggler=1 psCrc=EC35690D
[FH5UIPSO] matched UI PSO #3 source=gfx ... dxbc=0 toggler=1 psCrc=EF11CCAB
[FH5UIPSO] matched UI PSO #4 source=gfx ... dxbc=0 toggler=1 psCrc=8C2958B4
...
[FH5UIPSO] draws=20496 skipped=0 nullRT=0 fsRT=0 eyeRT=0 uiPsoCreated=31 uiTogglerPso=31
```

Important interpretation: the user's list is correct, but these draws are not the final eye/backbuffer HUD
boundary. They land in small/medium off-screen UI resources, not in the full-screen display/eye source:

```text
targets(distinct=48): 0x67CB2780[288x216 f26]=5656 ...
[FH5PSOHOT] ... [UI][UITOG] PS=...(BC41B4FE) ...
[FH5PSOHOT] ... [UI][UITOG] PS=...(3DD3D4A2) ...
[FH5PSOHOT] ... [UI][UITOG] PS=...(41FA90D1) ...
```

Added `uiredirect=5` as a direct UI-PSO skip mode. In-game A/B:

- `uiredirect=5`: `[FH5UIPSO] draws=20882 skipped=20882 ... targets(distinct=0)`.
- SimXR screenshot: world/cockpit remains, HUD overlay disappears.
- Switch back to `uiredirect=2`: `[FH5UIPSO] skipped=0` and the target list repopulates.
- SimXR screenshot: `HORIZON FESTIVAL MEXICO`, minimap, prompt, and gauge overlay return.

This is strong proof that the ShaderToggler hashes name the UI contribution, and that dropping them gives clean
eyes. It is not yet HUD-on-quad because skipping the source generation also removes the source we would need to
copy to a quad.

### Slot-4 source-skip check

The new UI-PSO logs suggested the 288x216 path might be important, so I tested the composite SRV slot that
exposes a 288x216 RT:

```text
uiredirect=4
srcroot=0
srcslot=4
[FH5COMPSRV] srcskip(root=0 slot=4 target=0x673877A0 288x216 f41 samples=102 skipped=0)
```

Later frames occasionally skipped only ~5 draws/sec. The SimXR screenshot still showed the HUD. Slot 4 is
sampled by the composite, but it is not an active draw-target lever for removing the HUD through the current
source-skip mechanism.

### UEVRJ lineage port / engine-level layer result

I inspected `E:\Github\UEVRJ` for reusable ideas. The useful part was not the OpenXR quad machinery (FH5 already
has a quad layer), but the D3D12 producer/copy lineage model:

- `D3D12Diagnostics` records descriptor reads, resource writes, and copy producer fields.
- `Sn2DescriptorLineage` / `Sn2ResourceTimeline` keep create/copy/access ancestry.

Ported a compact FH5 version into `Fh5CameraCbuffer.cpp`:

- mark RTs written by the ShaderToggler-matched UI PSOs;
- propagate that mark through `CopyResource` / `CopyTextureRegion`;
- annotate composite SRVs whose resource ancestry includes a UI PSO write;
- expose full-screen display-format lineage candidates only through explicit `hudplane=-2`;
- add `uiredirect=6` as a pure pointer-based skip of the latest lineage origin.

Live result:

```text
[FH5UILIN] ... autoSample=0xACFF25750[288x216 f26] autoOrigin=0xACFF25750 ... rows=1
[FH5COMPSRV] ... srcskip(root=0 slot=-1 target=0x0 0x0 f0 samples=0 skipped=84)
[FH5COMPSRV] ... srcskip(root=0 slot=-1 target=0x0 0x0 f0 samples=0 skipped=360)
```

The mode-6 screenshot still showed HUD/minimap/speedometer/banner. That means the identified lineage origin is
real, but it is not sufficient by itself as the engine-level "final UI layer." It is one low-res UI input path,
not the complete assembled HUD surface.

The same run then switched to `uiredirect=5`:

```text
[FH5UIPSO] draws=16326 skipped=16326 ... targets(distinct=0)
[FH5UIPSO] draws=16646 skipped=16646 ... targets(distinct=0)
```

The SimXR screenshot with mode 5 removed the HUD while leaving the world/cockpit. So ShaderToggler works because
it skips the whole shader/PSO class wherever it renders. Our engine-level approach still needs the complete
assembled UI resource, or a draw/blit path that captures all UI PSO outputs into a quad source before suppressing
them.

Also fixed a real redirect setup bug found during this pass: `ui_redirect_install()` used to create the UI RT at
the early boot swapchain (`1600x843 fmt24`) and never update it after FH5 recreated the active swapchain
(`1152x864 fmt28/29`). It now reconfigures the UI RT when width/height/format changes:

```text
[FH5UIR] reconfigured (LOCK-FREE): UI RT 1152x864 fmt=28 hooks(...)=1/1/1/1/1/1/1/1/1/1
```

### Mode 7 UI-PSO mirror capture

Added `uiredirect=7` as a diagnostic capture/replay mode:

- when a ShaderToggler-matched UI PSO draw is seen, bind a same-size/same-format offscreen mirror RT;
- execute the draw once into that mirror;
- restore the game's original RT binding;
- replay the original draw into the game RT so the eye image is unchanged.

The first mirror implementation crashed during a hub/menu transition when it rebound the original DSV against the
mirror RTV. Passing `nullptr` for the DSV made the mirror replay stable. A later attempt to recycle mirror slots
by `clear_frame` correlated with an immediate `nvwgf2umx.dll` crash, so that slot-eviction change was reverted.

Stable mirror build:

```text
deployed FH5VR.dll SHA256 B83619E497EC780D41A14391DEF59A01CA56C53B7E3A92B1616A881845053079
```

Launch-only / splash-menu proof with `hudquad=on`, `uiredirect=7`, `hudplane=-2`:

```text
[FH5UIPSO] draws=2856 ... targets(distinct=1): 0x61EEAC30[288x216 f26]=2856
[FH5UIMIRROR] draws=2856 hits=2856 created=0 failed=0 full=0 rows=1 latest=0x31A5640[288x216 f26] src=0x61EEAC30->rt=0x31A5640[288x216 f26]=2856
[VR-HUDQUAD] quad 1.60x1.20m pos=(0.00,0.00,-1.80) opaque=1 (swapchain 1152x864)
```

Armed transition proof (`Navigate-FH5.ps1 -SkipBootSequence`) with the quad and mirror both on:

```text
RESULT=READY (showcase driver camera, drove from hub, quiet 8s)
[FH5UIPSO] draws=14410 ... targets(distinct=48): 0x61EEAC30[288x216 f26]=3210 ...
[FH5UIMIRROR] draws=4199 hits=4199 created=0 failed=0 full=10211 rows=38 latest=0x31A5640[288x216 f26] ...
[VR-HUDQUAD] quad 1.60x1.20m pos=(0.00,0.00,-1.80) opaque=1 (swapchain 1152x864)
```

This proves the ShaderToggler UI PSO draws can be intercepted and mirrored without removing them from the normal
rendering path. It also proves the complex UI/menu case is not one texture: it is a burst of many UI RTs
(`20x20`, `180/181x181`, `256x256`, etc.) plus the steady `288x216 f26` target. The fixed 128-slot mirror cache
captures the hot rows but saturates under menus (`full` around 9k-11k/sec in this run; earlier transition probes
hit ~13k-15k/sec).

### HUD quad copy safety

The existing HUD quad copier uses `CopyResource`, so it is only safe when source/destination have compatible
shape and format. A prior `hudquad=on` run with mode 7 died in `nvwgf2umx.dll` after `[VR-HUDQUAD]`. That is
consistent with the copier being asked to copy a small UI/lineage source into the 1152x864 HUD swapchain, so
`copy_hud()` now validates the source and destination resource descriptions and skips incompatible copies with a
throttled warning instead of issuing invalid D3D12 commands.

Latest guarded deployed build:

```text
deployed FH5VR.dll SHA256 A6DF992B9381BF74AAC6AF31FB280856598CBACD742BF6D5CEC7B63D0E5AB6ED
```

The latest armed transition did not emit the new incompatible-copy warning, so that run did not prove the guard
caught a bad source. It did prove the guarded build can survive the armed quad+mirror transition. Real-HMD
validation could not be run in this pass because the Meta OpenXR runtime returned
`XR_ERROR_FORM_FACTOR_UNAVAILABLE`; SimXR was used for the D3D/UI path validation.

### Current next discriminator

The UI skip proves clean eyes, mode 6 proves the current lineage-origin resource is incomplete as a HUD quad
source, and mode 7 proves ShaderToggler-matched UI draws can be mirrored before being replayed into the game RT.
The remaining missing piece is no longer "can we intercept the UI?" It is "how do we turn many small mirrored UI
surfaces into one 1152x864 quad texture without `CopyResource`?"

Next useful probe: log the viewport/scissor and draw order for each UI-PSO draw, then build the smallest
D3D12 blit/compositor path that can draw mirrored UI surfaces into our own 1152x864 HUD RT. Start with the steady
`288x216 f26` mirror, then add the extra menu/marker surfaces once placement/order is known.

The pass/fail signal: identify either:

- a stable resource containing the assembled HUD after all UI PSO outputs are composed, or
- the full set of UI PSO outputs plus a blit/composite path into our own HUD RT.

Then copy/blit that resource/RT to the quad and use `uiredirect=5` or an equivalent later-stage suppressor for
clean eyes.

---

## Plan (next steps, in order)

1. **Identify FH5's HUD render target (the `*(this+0x40)` `ui_target`).** Two complementary approaches:
   - (a) Make the `vf54` probe fire **early** (at the splash/menu UIRenderer pass, which is reached far more
     reliably than free-roam) and **lighter** (avoid the heavy blind 2-level scan that may be destabilizing).
     Read `*(a1+0x40)`, follow it as an engine RenderTarget object, and find the `ID3D12Resource` (match
     against the known-RT set). One `[FH5UIRT]` line gives the offset/layout.
   - (b) **Preferred / most reliable: UI-PSO tracking.** Extract the DXBC hashes of the HUD pixel shaders from
     the captures (`PS 6012` panels, `7350/7603/7639/7410` glyph/text — from `fh5_noxr_frame68577.rdc`), match
     them in `Hook_CreateGfxPSO`, then in the AFR run **log the bound RT of any draw using a UI PSO** → reveals
     the HUD's true target under AFR directly, no object-layout RE.

2. **Point the quad at the HUD RT** (format already fixed) → **HUD-on-quad**. Mind the source resource *state*
   for the `CopyResource` barrier (engine RT state at copy time is uncertain — may need a tracked/guessed
   transition, or copy into our own RT first).

3. **If no assembled HUD RT exists, build a quad compositor.** Create our own 1152x864 HUD RT, expose SRVs for
   the mirror slots, and draw/blit the UI mirrors into the HUD RT with viewport/scissor placement. This replaces
   direct `CopyResource` for the small `288x216`, `181x181`, and marker/icon surfaces.

4. **Clean eyes**: once the quad texture is populated, suppress the HUD's contribution to the eye image. The
   simplest version is a mode-7 variant that mirrors but does not replay the original UI PSO draw; the safer
   final version may need a later-stage suppressor if any UI is composited outside the ShaderToggler PSO set.

5. **Keep hardening crashes** if they keep blocking iteration (the splash-stall + hub→drive transition crash
   are the current intermittent offenders; boot-retry covers boot crashes but not these).

### Things to keep as-is
- `uiredirect=3` (R10A2-non-eye redirect) is a **diagnostic only** — proven NOT the HUD (the f24 buffers were
  ruled out via the SimXR preview: HUD stayed in the eyes, quad showed magenta).
- The `vf54` "draw bracket" (`enter/leave_ui_pass`) is effectively dead in AFR (`uipass draws=0` — the
  UIRenderer is decoupled from the D3D12 recording thread); the **RT-extraction** use of `vf54` (above) is the
  right one, not the draw-bracket.

## Key artifacts
- Captures: `E:\tmp\fh5_rdc\fh5_noxr_frame68441/68577.rdc` (race/free-roam), `66882.rdc` (menu/transition).
- Analysis script: `E:\tmp\fh5_rdc\rd_hud2.py` (run via `qrenderdoc --python`) → `hud_analyze2_68577.txt`.
- No-OpenXR capture launch: `renderdoccmd.exe capture --working-dir E:\Games\ForzaHorizon5Empress
  --capture-file <tmpl> ForzaHorizon5.exe` with `FH5VR_NO_OPENXR=1` set.
- Nav-without-state-file helper: `E:\tmp\fh5_key.ps1` (focus FH5 + send a key).
- Memory notes: `fh5-ui-quad-layer.md`, `fh5-nvidia-veh-crash.md` (updated this session).

---

## 2026-06-08 addendum: runtime-tested split-eye solution

The practical solution is now implemented as `uiredirect=18` / `deltaeye` / `worlddelta`:

1. Capture the eye resource immediately after FH5's world composite and before UI draws (`pre_ui_eye_candidate`).
2. Copy that pre-UI texture into the OpenXR projection swapchain, so the normal XR eye layer is world-only.
3. Blit `final eye - pre-UI eye` into the HUD OpenXR quad swapchain, so the quad receives only the HUD delta.
4. Do not skip FH5 UI draws and do not depend on the overlay descriptor path for the primary implementation.

Runtime proof from the successful SimXR run:

```text
[FH5PREUI] captures=73 recreated=1 failed=0 latest=0xB6F14FFA0[1152x864 f28]
[VR-COPY] using pre-UI eye projection source eye=0 src=0xB6F14FFA0[1152x864 f28]
[VR-HUDQUAD] using pre-UI delta source final=0x7BD3B6F0 base=0xB6F14FFA0[1152x864 f28]
[VR-HUDDIFF] wrote one-shot HUD quad diff dump E:\tmp\fh5_hudquad_diff_dump.bmp
```

`E:\tmp\fh5_hudquad_diff_dump.bmp` contains the objective text, festival prompt, minimap, and speed/tach on a
black background, proving the full HUD source is present. A Windows capture of the actual SimXR preview window
with `hudopaque=on` showed the HUD quad rendered over the clean cockpit/world projection. With `hudopaque=off`
the quad disappeared before the latest alpha patch, so the diff shader was updated to make alpha follow the
visible boosted HUD pixels and the quad layer now requests unpremultiplied source-alpha blending.

Update from the later alpha retest: transparent mode now passes in SimXR after fixing the simulator's D3D12 quad
preview path to honor source alpha. The game-side proof from the passing run:

```text
[FH5PREUI] created pre-UI eye copy 0xAD00B0150 1152x864 fmt=28
[VR-COPY] using pre-UI eye projection source eye=0 src=0xAD00B0150[1152x864 f28]
[VR-HUDQUAD] using pre-UI delta source final=0x7640B6C0 base=0xAD00B0150[1152x864 f28]
[VR-HUDDIFF] blit final=1152x864 fmt=28 state=0x0 base=1152x864 fmt=28 state=0x80 -> dst=1152x864 fmt=29
[VR-HUDQUAD] quad 1.60x1.20m pos=(0.00,0.00,-1.80) opaque=0 (swapchain 1152x864)
```

Visual proof:

- `mcp__openxr_simulator.capture_screenshot` showed clean stereo projection: cockpit/world only, no HUD layer.
- `mcp__wslsnapit.take_screenshot(processName=ForzaHorizon5, windowIndex=1)` showed the HUD/minimap/speed UI on
  the quad over the clean cockpit/world projection, without the old opaque black rectangle.
- Latest archived preview screenshot:
  `C:\Users\ellio\Dropbox\Screenshots\screenshot_2026-06-08_05-37-08.png`.

The black-rectangle artifact was not the FH5 mod. It was a simulator preview bug: the D3D12 quad path in
`E:\Github\OpenXR-Simulator\src\runtime.cpp` copied the quad to CPU and painted it with `StretchDIBits(...,
SRCCOPY)`, ignoring `XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT`. The simulator was patched to use a
premultiplied top-down DIB plus `AlphaBlend` for source-alpha quads and to link `msimg32`.

Remaining caveat: FH5 still intermittently hits the known NVIDIA driver stack-overflow crash during neutral boot
or navigation:

```text
Faulting module: nvwgf2umx.dll
Exception code: 0xc00000fd
Fault offset: 0x00000000000f3972
```

This same crash bucket occurs before mode 18 is active, so do not attribute it to the HUD split path without a
post-activation repro. In the passing run, `hudopaque=off,uiredirect=18` stayed live and the process remained
responsive.

---

## 2026-06-08 addendum: phase-locked full-HUD delta

User observation after the successful mode-18 run: the quad was correct, but it flickered because the HUD delta
was copied from whichever AER eye phase was current. The useful requirement is simpler than pre-AER interception:
copy one stable phase's HUD image and submit that same quad texture to both eyes for the frame.

Implemented follow-up modes:

- `uiredirect=23` / `mirrorphase`: phase-locks the ShaderToggler UI-PSO mirror. This proved the mechanics:
  `[VR-HUDQUAD] phase-locked refresh eye=0 ...` followed by
  `[VR-HUDQUAD] reusing phase-locked HUD image phase=0 submitEye=1`. The source dump
  `E:\tmp\fh5_hudquad_src_300.bmp` is clean HUD-only, but partial: objective text plus speed, not minimap/tach.
- `uiredirect=24` / `deltaphase`: phase-locks the earlier full-HUD mode-18 path. The normal XR projection uses
  `pre_ui_eye_candidate` for clean eyes, then the left phase blits `final eye - preUI` into the HUD quad
  swapchain and reuses that copied image for the right-eye submission. Mode 24 forces the left phase for now,
  because the right-phase live switch hit a driver crash immediately after changing phase and the left phase is
  sufficient for the desired stable quad.

Validation status:

- Build passed after adding mode 24.
- The last deployed DLL contains mode 24.
- Fresh mode-24 in-game validation is currently blocked by the same neutral boot/splash NVIDIA stack-overflow
  crash bucket seen with HUD off:

```text
Faulting module: nvwgf2umx.dll
Exception code: 0xc00000fd
Fault offset: 0x00000000000f3972
```

That crash happened before any `uiredirect=24` activation in the latest attempts, so it should not be treated as
evidence against the phase-locked delta path. The correct next runtime discriminator is: get one clean free-roam
boot, then set `hudquad=on`, `hudopaque=off`, `uiredirect=24`, `hudphase=left` and verify the log shows
`phase-locked delta refresh eye=0` plus `reusing phase-locked HUD image phase=0 submitEye=1`.

---

## 2026-06-08 addendum: RenderDoc C++ replay late-UI evidence

Checked the local RenderDoc fork at `E:\Github\renderdoc`. It includes a headless
`util.automation.export_cpp` path that generates Nsight-style D3D12 C++ call streams. It is useful for FH5 as an
offline call/resource map, but not yet as a build-and-run visual replay: this export still leaves FH5
`CreateGraphicsPipeline` chunks unhandled, so the generated call stream contains `SetPipelineState(nullptr)` for
many graphics draws.

Generated artifacts:

- `E:\tmp\fh5_rdc\fh5_cpp_ui_52390_52480\capture_frame.cpp`
- `E:\tmp\fh5_rdc\fh5_cpp_ui_53140_53510\capture_frame.cpp`
- Wrapper script: `E:\tmp\fh5_rdc\fh5_export_cpp_window.py`
- State dumps:
  - `E:\tmp\fh5_rdc\state_68441_52415.json`
  - `E:\tmp\fh5_rdc\state_68441_52462.json`
  - `E:\tmp\fh5_rdc\state_68441_53149.json`
  - `E:\tmp\fh5_rdc\state_68441_53390.json`

Relevant late UI states from `fh5_noxr_frame68441.rdc`:

- EID `52415`: PSO `7408`, RT `816531` (`701x224`, R8G8B8A8 typeless), PS hash
  `fa1c988f87104932ceb57dddb8bac768`; samples `5869`.
- EID `52462`: direct swapchain draw, PSO `6056`, RT `13045` (`1152x864` swapchain), PS hash
  `888c6e5e217bcf0cae1724c262b40823`; samples RT `816532` (`101x98`) via heap 50 slot `87896`.
- EID `53149`: PSO `7525`, offscreen RT `816535` (`180x180`), PS hash
  `970abf864dce4b5bdd403c4c87c48299`.
- EID `53390`: PSO `7480`, offscreen RT `816540` (`20x20`), PS hash
  `fb903e610b47d892d7a563d401d675fd`; samples texture `495504` (`64x64` BC7) via heap 50 slot `2589`.

C++ call-stream shape:

- The offscreen UI targets are rendered through small viewports (`701x224`, repeated `180x180`, and `19.8x19.8`).
- The same command list then switches back to full viewport `1152x864` and RTV heap slot `1709`, which maps to the
  swapchain path seen in the RenderDoc state dumps.
- The final composite draws are `DrawIndexedInstanced(30,1,...)` plus generated-geometry `DrawInstanced(...)`
  runs immediately after each offscreen UI target is transitioned to `PIXEL_SHADER_RESOURCE`.

Implementation implication:

- The user's "last few colour passes" observation is correct. The full HUD is not one texture-binding event; it is
  a late-pass sequence that builds small UI render targets and composites them to the swapchain right before present.
- For native-eye suppression/copy, keying only on the earlier pink-H draw is incomplete. A robust runtime path should
  bracket the late UI region by detecting the switch from offscreen UI RTVs back to the swapchain RTV (`1152x864`,
  heap RTV slot `1709` / resource `13045`) and then copy the stable left-phase quad image once per frame.
- The C++ replay export is therefore useful for finding the exact D3D12 signatures and draw order, but the already
  implemented mode-24 phase-locked full-HUD delta remains the best live path unless we decide to replace it with a
  lower-level D3D12 draw-suppression/copy bracket.

---

## 2026-06-08 addendum: Empress callstack capture and immediate-flush hook

The later RenderDoc callstack capture was done against the Empress install, not the current Steam build:

- Capture launcher: `E:\tmp\fh5_rdc\fh5_callstack_capture.py`
- Launcher target: `E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe`
- Capture: `E:\tmp\fh5_rdc\fh5_callstack_%Y%m%d_%H%M%S_frame1122.rdc`
- Analysis output: `E:\tmp\fh5_rdc\fh5_callstack_analysis_1122.txt`
- RenderDoc state: `HasCallstacks=True`, resolver initialized successfully.

The useful swapchain UI draw callstack was:

```text
ev=90 ID3D12GraphicsCommandList::DrawInstanced()
  forzahorizon5.exe+0x0099143c
  forzahorizon5.exe+0x00df5a2d
  forzahorizon5.exe+0x00e0f56b
  forzahorizon5.exe+0x009a1233
  forzahorizon5.exe+0x009a1030
  forzahorizon5.exe+0x00974f02
  forzahorizon5.exe+0x00c32d9c
  forzahorizon5.exe+0x0064964f
```

With FH5 image base `0x140000000`, this maps to:

- `forzahorizon5.exe+0x00DF5A2D` -> `0x140DF5A2D`, inside Empress `sub_140DF5910`.
- `forzahorizon5.exe+0x00E0F56B` -> `0x140E0F56B`, inside Empress `sub_140E0F240`.

This confirms the decompiler finding that the immediate textured item path (`sub_140E0F240`) feeds the draw flush
(`sub_140DF5910`). It also explains why a hook that only watches `sub_140E15C90` misses some visible UI draws.

Address-provenance check:

- Empress executable: `E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe`, size `182826496`, SHA-256
  `C6024A3ED46BC2A2B08A5F079C3EA72DFC51EE397AB5BEB2AB36459937F11714`.
- Steam executable: `E:\SteamLibrary\steamapps\common\ForzaHorizon5\ForzaHorizon5.exe`, size `176231056`, SHA-256
  `EB71EE4A6AE092B854C58751342C6D06A4D9BC00F089B81E72B073D4867B76B7`.
- `E:\ForzaHorizon5_UI_QuadLayer_Analysis\ui_quad_probe_report.json` declares its database as
  `E:\Games\ForzaHorizon5Empress\ForzaHorizon5.exe`.
- Byte check at the hook RVAs shows normal function prologues in Empress and unrelated bytes at the same RVAs in
  the Steam binary. Example: Empress `RVA 0xDF5910` starts
  `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48`; Steam does not.

Implementation update:

- Added `uiredirect=25` as a phase-locked late final mirror mode that preserves native UI replay while refreshing
  one stable HUD image for both eyes.
- Added late UI PS hashes from the RenderDoc pass to the UI classifier:
  `888c6e5e217bcf0cae1724c262b40823`, `970abf864dce4b5bdd403c4c87c48299`,
  `fb903e610b47d892d7a563d401d675fd`.
- Added `sub_140DF5910` immediate-flush scoping in `Fh5Adapter.cpp`, guarded by the Empress prologue bytes above.
  If the executable layout does not match, the hook is skipped rather than detouring an arbitrary address.
- Added `[FH5OVERLAY] flushes=` and `flushDraws=` counters so live logs can prove whether mode 25 is catching the
  immediate item-style path.

Build/deploy status:

```text
cmake --build E:\Github\vrframework\build-fh5 --config Release --target FH5VR
pwsh E:\Github\vrframework\scripts\Deploy-FH5VR.ps1 -GameDir E:\Games\ForzaHorizon5Empress -BuildDir E:\Github\vrframework\build-fh5 -Config Release
```

Both completed successfully; the rebuilt `FH5VR.dll` was staged as
`E:\Games\ForzaHorizon5Empress\dxgi.dll`.

Short SimXR validation after deploy:

```text
pwsh E:\Github\vrframework\scripts\Launch-FH5VR.ps1 -SimRuntime -HudQuad -UiRedirect 25 -HudPhase left -SkipNavigate
```

The run survived the boot window and was stopped cleanly after log scrape. Relevant proof from
`E:\Games\ForzaHorizon5Empress\FH5VR.log`:

```text
[FH5OVERLAY] sub_140DF5910 immediate-flush hook installed @0x140DF5910 (Empress prologue verified)
[FH5OVERLAY] scopes=84 ... overlayDraws=84 flushes=84 flushDraws=84 ...
[VR-HUDQUAD] phase-locked refresh eye=0 source=0x77C434F0[1600x843 f24]
[VR-HUDQUAD] reusing phase-locked HUD image phase=0 submitEye=1
[VR-HUDQUAD] using final UI mirror source 0x77C451A0[1152x864 f28]
[VR-HUDQUAD] phase-locked refresh eye=0 source=0x77C477E0[1152x864 f28]
[VR-HUDQUAD] reusing phase-locked HUD image phase=0 submitEye=1
```

This proves the new immediate-flush scope is installed on the Empress binary and that mode 25 is refreshing one
left-phase HUD image and reusing it for the right-eye submit. This was a launch-only validation (`-SkipNavigate`),
not a full free-roam visual pass.

Follow-up in-game validation:

- A HUD-off control run reached a stable 3D showcase/driver camera.
- Switching that live process to `uiredirect=25` stayed alive and produced both-eye HUD quad presentation:

```text
[FH5CTL] ... hudquad=1 hudopaque=0 uiredirect=25 hudphase=0 ...
[VR-HUDQUAD] using final UI mirror source 0xB0A74D9C0[1152x864 f26]
[VR-HUDQUAD] phase-locked refresh eye=0 source=0xB0A74D9C0[1152x864 f26]
[VR-HUDQUAD] reusing phase-locked HUD image phase=0 submitEye=1
```

Visual proof:

- OpenXR simulator screenshot showed the HUD/UI visible in both eyes in the cockpit/showcase view.
- Desktop/window screenshot archived at
  `C:\Users\ellio\Dropbox\Screenshots\screenshot_2026-06-08_15-09-19.png`.

Important caveat: mode 25 intentionally preserves FH5's native UI replay while also submitting the phase-locked
quad. It proves the quad source and removes the eye-phase refresh problem for the submitted quad, but it is not yet
the final native-suppression mode.

Native-suppression tests:

- `uiredirect=20` live-switch crashed almost immediately after enabling final-draw suppression. WER bucket:
  `nvwgf2umx.dll`, exception `0xc0000005`, offset `0xF3972`. The earlier full-navigation crashes were the known
  `0xc00000fd` bucket at the same driver offset, but this mode-20 crash is a stronger signal that the discard-RT
  suppression path is unsafe.
- Added `uiredirect=26` as a safer ShaderToggler-style phase-locked PSO mirror + native skip mode. It stayed alive
  in the same 3D scene and logged thousands of mirrored UI PSO draws:

```text
[FH5CTL] ... hudquad=1 hudopaque=0 uiredirect=26 hudphase=0 ...
[FH5UIMIRROR] draws=12204 hits=12078 created=126 ...
[VR-HUDQUAD] using final UI pso-mirror source 0xB266886D0[288x216 f26]
```

Mode 26 successfully suppresses the native HUD without crashing, but the selected quad source is still the small
PSO/atlas mirror rather than the fully composed HUD. The screenshot confirms native HUD removal, but not a complete
quad replacement:
`C:\Users\ellio\Dropbox\Screenshots\screenshot_2026-06-08_15-20-01.png`.

Attempted to re-test `uiredirect=24` as the cleaner full-HUD delta path, but the HUD-off bootstrap crashed before
mode 24 was enabled. WER was the known driver bucket again: `nvwgf2umx.dll`, exception `0xc00000fd`, offset
`0xF3972`. This is not evidence against mode 24; it means the live-switch test did not start.

Current best user-facing mode is therefore `uiredirect=25`: full HUD/UI appears on the quad in both eyes, stable
after live-switch in 3D, but native UI is still replayed underneath. Current best native-suppression direction is
mode 26 plus a composed-PSO-mirror source selector, not mode 20's final-draw discard redirect.
