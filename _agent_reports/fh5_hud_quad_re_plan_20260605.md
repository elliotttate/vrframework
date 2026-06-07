# FH5 HUD quad RE and implementation plan

Scope: offline audit only. FH5 was not launched. Evidence comes from the local game media tree, existing IDA/decompile exports under `E:\ForzaHorizon5_IDA_Decompile`, existing FH5CameraProbe reports, and current `E:\Github\vrframework` source.

## Current conclusion

FH5 does not look like the REFramework/Scaleform path where we can hook a movie viewport by name. The local media tree and decompile evidence point to a native ForzaTech UI stack:

- `media\UI\Fonts.zip` contains native `.vfont` assets.
- `media\UI\Effects.zip` contains UI shader/effect bins such as `NormalEffect.bin.dx12`, `SRGBShader.bin.dx12`, `TintShader.bin.dx12`, and `TextureMaskEffect.bin.dx12`.
- `media\UI\Textures\HiRes\Data_Bound\HUD.zip` contains HUD swatch assets such as `HUD_HorizonLine.swatchbin`, `HUD_Reticle.swatchbin`, and `HUD_Reticle_Glow.swatchbin`.
- The exported code has `AVUI::TRefCountedPtr`, `UIPage`, `UIScene`, `SceneEntryData`, and `XAML...` names.
- The exported code has concrete HUD providers/components: `PlayerMinimapViewComponent`, `HUDSpeedometerComponent`, `HUDStatusSpeedometerProviderComponent`, many `HUDStatus...ProviderComponent` types, plus `CopterHud` / `UIPageCopterHud`.
- No useful `Scaleform`, `GFx`, `Coherent`, `Gameface`, `.swf`, or `.gfx` evidence was found in the local media/decompile search.

So the practical route is draw/pass separation, not movie-viewport reprojection: identify the D3D12 HUD render pass, capture or redirect it into a transparent HUD texture, and submit that texture as an OpenXR quad layer.

## Offline RE findings added 2026-06-05

Status: the static RE pass is complete enough to choose hook families and avoid the dead ends. Exact flat-HUD draw EIDs, alpha behavior, and the final duplicate-HUD suppression point are still capture-only because the offline exports do not contain command-list event order or RTV lifetime.

### UI stack identity

FH5 is using a native ForzaTech AVUI/page/scene stack, not a middleware movie UI path.

- Base class registration exists for `UIPage`: `E:\ForzaHorizon5_IDA_Decompile\pseudocode\fh5_000004.c:2013` / `:2026`.
- Base class allocation/register evidence exists for `UIScene`: `E:\ForzaHorizon5_IDA_Decompile\pseudocode\fh5_000008.c:893`.
- Page factories are built around `AVUI::TRefCountedPtr<UIPage>` and `SceneEntryData const &`: examples in `fh5_000296.c:4194`, `:4223`, `:4260`, `:4306`, `:4337`, `:4355`, and many more.
- UI key/input handlers are explicitly `bool, UIPage::KeyArgs`: examples in `fh5_000296.c:4213`, `:4250`, and `:4287`.
- Element callbacks use `AVUI::FrameworkElement *`: examples in `fh5_000296.c:4494` and `:7526`.
- A transition enum/string is registered as `UISceneTransition`: `E:\ForzaHorizon5_IDA_Decompile\agent_inputpath\cmd_decompiles\cand_sub_140262E70.c:238`.

Conclusion: page/scene hooks are useful for state classification and menu handling, but they are not the right place to extract the final rendered HUD texture.

### Concrete UI and HUD anchors

| Area | Anchor | Evidence | Use |
| --- | --- | --- | --- |
| Base UI page | `UIPage` registration around `0x14004B1C0` | `fh5_000004.c:2013`, `:2026` | Finds common page registration and page-factory patterns. |
| Base UI scene | `UIScene` allocation/register path around `0x1400C3660` | `fh5_000008.c:893` | Finds common scene map and scene active-state paths. |
| Copter HUD page | `nps_register_ui_page_UIPageCopterHud` at `0x14030DF20` | `agent_forcecam\log_names.txt:197`, `names.json:1496` | Known concrete HUD page for registration pattern matching. |
| Copter HUD scene | `nps_register_ui_scene_CopterHud` at `0x14033EAC0` | `agent_forcecam\log_names.txt:201`, `names.json:1512` | Known concrete HUD scene for scene map validation. |
| Gameplay minimap | `PlayerMinimapViewComponent` registration around `0x14016E5D0` | `fh5_000015.c:10684`, `:10699`; `camera_trace_summary.md:240` | High-value gameplay HUD component and minimap update source. |
| Speedometer panel | `HUDSpeedometerComponent` registration around `0x140181220` | `fh5_000016.c:5140`, `:5155` | High-value speedometer state source. |
| Speedometer provider | `HUDStatusSpeedometerProviderComponent` around `0x140231F90` | `fh5_000023.c:20016`, `:20031` | Status feed into the HUD system. |
| HUD prioritizer | `HUDStatusPrioritiserModule` around `0x1401162C0` | `fh5_000012.c:2226`, `:2241` | Candidate for deciding active HUD/status composition. |
| Active inner HUD state | `InnerHudStatusActive` | `fh5_000012.c:16768` | Candidate live state flag/string for "gameplay HUD is active". |
| Notifications/popups | `TextHUDStatus`, `Notifications`, `NetworkProgressHudComponent` | `fh5_000021.c:873`, `fh5_000343.c:10409`, `fh5_000353.c:10004` | Useful for popup/menu classification and avoiding missing transient HUD. |

### HUD status and page-state path

`HUDStatus` is the strongest static signal for classification. Concrete UI panels bind `AVUI::TRefCountedPtr<HUDStatus>`:

- `CopterHud` binds `AVUI::TRefCountedPtr<HUDStatus>` in `fh5_000332.c:192`, `:5616`, and type descriptor `:6449`.
- `LimitedHud` binds `AVUI::TRefCountedPtr<HUDStatus>` in `fh5_000342.c:5546`, `:11592`, and type descriptor `:12462`.
- `Notifications` binds `AVUI::TRefCountedPtr<HUDStatus>` in `fh5_000343.c:10409`, `:16724`, and `fh5_000344.c:391`.
- `NetworkProgressHudComponent` binds `AVUI::TRefCountedPtr<HUDStatus>` and popup invite status in `fh5_000353.c:10004`, `fh5_000354.c:668`, `:687`, `:1897`, and `:1905`.

Use this family to decide which HUD mode is active: gameplay HUD, limited HUD, popup/notification, pause/fullscreen menu, map, or copter/photo-style HUD. Do not use it as the final render extraction point. It describes UI state, not the final D3D12 texture.

### Render-facing resource path

The static render-binding names point at two likely UI draw families:

| Render family | Evidence | Notes |
| --- | --- | --- |
| `ResourceBinding::OverlayRendererPSParameters` | `fh5_000002.c:13860`; ref-count object in `fh5_000066.c:20678` | Pixel-side overlay bindings. Likely relevant to flat UI/HUD draw PSOs. |
| `ResourceBinding::OverlayRendererVSParameters` | `fh5_000002.c:13876`; ref-count object in `fh5_000066.c:20689` | Vertex-side overlay bindings. |
| `ResourceBinding::OverlayRendererSamplerParameters` | `fh5_000002.c:13868`; ref-count object in `fh5_000086.c:15517` | Sampler state family for overlay draws. |
| `ResourceBinding::PrimRendererPixelParameters` | `fh5_000002.c:14169`; ref-count object in `fh5_000074.c:22875` | Primitive UI renderer pixel bindings. |
| `ResourceBinding::PrimRendererVertexParameters` | `fh5_000002.c:14161`; ref-count object in `fh5_000074.c:22897` | Primitive UI renderer vertex bindings. |
| `Tide::TideUITextures` / `RowReference_TideUITextures` | `fh5_000031.c:2878`, `:2913`; row-reference helpers in `fh5_000394.c:7074` through `:7279` | Texture database path for UI resources. |
| `Cms::UITextureData` | `fh5_000033.c:19144`; ref-counts in `fh5_000424.c:12699`, `fh5_000425.c:18407`, `:18435` | UI texture data wrapper. |

These are the best static breadcrumbs for RenderDoc/resource matching. The actual hook should be validated at draw/PSO/RTV level after a capture. The render-binding names alone are not sufficient to skip or redirect draws because the same renderer family can be used by gameplay HUD, menu UI, and possibly non-HUD overlays.

### Asset evidence for RenderDoc labeling

The UI media tree gives names to search for when identifying SRVs/resources:

- `media\UI\Effects.zip`: `NormalEffect.bin.dx12`, `SRGBShader.bin.dx12`, `TintShader.bin.dx12`, `SaturationShader.bin.dx12`, `CircularProgress.bin.dx12`, `TextureMaskEffect.bin.dx12`, plus DX11 variants.
- `media\UI\Fonts.zip`: native `.vfont` assets such as `DG1_ArialBold.vfont`, `DG2_LCD-BOLD.vfont`, and `Horizon_*.vfont`.
- `media\UI\Textures\HiRes\Data_Bound\HUD.zip`: `HUD_HorizonLine.swatchbin`, `HUD_Reticle.swatchbin`, `HUD_Reticle_Glow.swatchbin`, `accolades_tick.swatchbin`, and notification assets.
- `media\UI\Textures\HiRes\Data_Bound\Horizon_Map.zip`: map filter/icon swatch assets under `filters/default/...` and `icons/...`.

Use these names to label SRVs and to separate flat HUD/minimap/menu resources from world-space route-line or chevron resources.

### Debug and suppression knobs

Static config/string evidence found in `E:\ForzaHorizon5_IDA_Decompile\agent_headtrack\dev_gate_dec.txt` and `dev_decompiles.txt`:

| Knob | Evidence | Use |
| --- | --- | --- |
| `nohud` | `dev_gate_dec.txt:143`, `dev_decompiles.txt:3316` | Live debug toggle candidate to prove HUD draw range or suppress duplicate HUD during testing. |
| `fastuirender` | `dev_gate_dec.txt:252`, `dev_decompiles.txt:3425` | UI renderer behavior/perf toggle. Useful for state discovery, not a final VR feature. |
| `fasthud` | `dev_gate_dec.txt:253`, `dev_decompiles.txt:3426` | HUD renderer behavior/perf toggle. Useful for discovery. |
| `uiscenegraphdebug` | `dev_gate_dec.txt:360`, `dev_decompiles.txt:3533` | Possible page/scene graph debug path. |
| `zoomoutminimap` | `dev_gate_dec.txt:362`, `dev_decompiles.txt:3535` | Minimap behavior/debug path. |
| `HideAllOverlays` | `dev_gate_dec.txt:496`, `dev_decompiles.txt:3669` | Strong suppression/debug candidate. Do not assume it only affects HUD until captured. |
| `Options/HUDSafeFrameHorizontal` | `dev_gate_dec.txt:2074`, `dev_decompiles.txt:1963` | Desktop safe-frame fallback or debug shrink/recenter path. |
| `Options/HUDSafeFrameVertical` | `dev_gate_dec.txt:2080`, `dev_decompiles.txt:1969` | Desktop safe-frame fallback or debug shrink/recenter path. |
| `Options/HideInactiveMouseOnHUD` | `dev_gate_dec.txt:2086`, `dev_decompiles.txt:1975` | Cursor/HUD interaction clue for future quad pointer mapping. |

These are probe aids, not the primary architecture. The final VR path should copy or redirect the actual HUD render output to a quad and prevent the baked HUD from landing in the projection layer.

### Hook priority from the offline RE

1. **State/classification hook:** page/scene/HUD-status family. Use this to decide whether the current frame is gameplay HUD, popup, menu, map, copter/photo, or no HUD.
2. **Render extraction hook:** D3D12 draw/PSO/resource boundary for `OverlayRenderer*`, `PrimRenderer*`, `TideUITextures`, or `UITextureData` consumers once RenderDoc identifies the exact flat-HUD events.
3. **Suppression hook:** if FH5 uses a separate UI RT, skip or block only the final UI composite after copying it to the HUD quad. If FH5 draws HUD directly to the final backbuffer, redirect the identified HUD draws into our own transparent target or capture a clean world image before HUD begins.
4. **Debug hook:** `HideAllOverlays` / `nohud` / safe-frame options. Use to validate assumptions and reduce duplicate HUD while testing, but avoid shipping a broad overlay kill unless capture proves it is scoped.

### What cannot be completed offline

The following items require a RenderDoc frame or a live proxy/capture log:

- exact flat-HUD EIDs and PSO IDs for speedometer, minimap, prompt text, and menu UI;
- whether flat HUD draws into a separate alpha-preserving UI RT or directly into the final opaque backbuffer;
- whether `HideAllOverlays`/`nohud` suppress only HUD or also useful non-HUD overlays;
- the draw order boundary for "world complete, HUD begins";
- final duplicate-HUD removal point;
- color/HDR/gamma behavior of the copied HUD texture on an OpenXR quad.

That is now the hard boundary: static RE can tell us where to classify UI state and which renderer families to inspect, but the actual quad texture source must be chosen from capture evidence.

## ★★ 2026-06-06 — CAPTURE RE RESOLVED: it's CASE B (no UI RT). Redirect plan below.

Indexed `ForzaLookingRightCockpit.rdc` (analysis/rdc_lookright). Present = backbuffer `ResourceId::45167` (1152x864 R8G8B8A8_UNORM), eventId 31603. **Only 32 draws write to 45167, all late (30788..31581):**
- **30788 (idx=3 fullscreen): WORLD composite/tonemap into 45167** → after this, 45167 = CLEAN WORLD. PSO `3906`, VS `1092`, PS `3908` (hash `7783d9572effba0fdc5cceecdcd40315`).
- **30983..31581: the UI, drawn DIRECTLY into 45167** (idx=30 HUD quads ×~25, idx=972 minimap, idx=1251/1431/306 text ×2). First UI draw 30983 = VS `14006`/GS `14007`/PS `14008`. Helper textures are tiny (minimap 180², icons, 20² glyph) — no full-screen UI layer.

So there is NO separate UI render target. Chosen fix (user, 2026-06-06): **UI-draw REDIRECT.**

### Redirect implementation plan (mod side; gated by `uiredirect=on`, default off)
- **New: hook the D3D12 COMMAND-LIST vtable** (mod currently only hooks the DEVICE vtable in Fh5CameraCbuffer). Grab the cmd-list vtable from a list the mod already creates (ResourceCopier), or hook CreateCommandList. Hook **`OMSetRenderTargets` (vtable index 46)**.
- **RTV→resource map:** hook DEVICE **`CreateRenderTargetView` (index 20)** to record `rtvHandle.ptr -> resource`. At install, fetch all swapchain backbuffers (`swapchain->GetBuffer(0..N)`) so OMSetRenderTargets can tell "is this RTV the backbuffer?".
- **Redirect logic in OMSetRenderTargets:** if `uiredirect` && the bound RTV resolves to a swapchain backbuffer && we are PAST the world-composite (1st backbuffer bind of the frame), substitute our own **UI RT's RTV handle** (keep/clear the DSV). On the FIRST redirect of the frame, `cmdList->ClearRenderTargetView(uiRtv, {0,0,0,0})` (transparent) on FH5's own list.
- **UI RT:** backbuffer-sized RGBA8 + RTV in a mod-owned heap; lives in RENDER_TARGET state.
- **Result:** 45167 = clean world (→ EYES, no change to the eye copy), UI RT = UI+alpha (→ the quad; `copy_hud` source switches from backbuffer to the UI RT; `hudopaque=off`).
- Frame reset at Present (on_post_present / producer): reset the backbuffer-bind counter.

### HARD PARTS to solve carefully (crash/correctness risk — NVIDIA VEH escalates ANY first-chance AV):
1. **Multi-command-list / multi-thread ordering:** the "1st backbuffer bind = composite, rest = UI" heuristic assumes RECORD order == EXECUTE order. FH5 may record on several lists/threads; ExecuteCommandLists order ≠ record order. SAFER signal: identify the composite by its FULLSCREEN nature (a 3-vertex DrawInstanced, or PSO/PS-hash `7783d957`) and only redirect backbuffer binds whose subsequent draw is NOT that — i.e., may need to also hook DrawInstanced/DrawIndexedInstanced (index 12/13) to confirm phase per-list. Start with the bind-count heuristic behind the knob; validate live; escalate to draw/PSO identification if it mis-splits.
2. **RTV handle validity:** the substituted handle must be a valid RTV descriptor (non-shader-visible RTV heap, mod-owned). 
3. **State/barriers:** the UI RT must be RENDER_TARGET when bound; clear once per frame.
4. **All hook bodies must be AV-proof** (no deref of unmapped memory) — first-chance AVs crash via the NVIDIA overlay VEH ([[fh5-nvidia-veh-crash]]).

## What we already know from previous captures

The existing RenderDoc audit has not identified exact flat-HUD draw EIDs or PSOs yet. It did prove a useful separation:

- Main world/camera draws bind 6912-byte camera CBVs with near around `0.1`, far large, `posW` around `1`, and valid matrices.
- Auxiliary cameras use different content signatures, for example near around `1`, far around `1000`, `posW` around `2`.
- Flat 2D HUD elements stay screen-fixed while the world and route line react to pitch. Route line/chevrons are not flat HUD; they are camera/world consumers and need their own draw-level treatment.

Known main-camera anchor draws from the reports:

- Looking-right cockpit: EID `23620`, rootSig `867`, rootParam `4`; EID `26771`, rootSig `794`, rootParam `0`.
- Cockpit: EIDs `13304`, `23706`, `30432`.
- Chase cam: EIDs `11719`, `18978`, `22328`.

The missing evidence is the exact draw range and render target for flat HUD: minimap, speedometer, text, objective prompts, and menu/fullscreen UI.

## Offline RE tasks

1. Build the FH5 UI symbol map.
   - Start from these strings/classes: `UIPage`, `UIScene`, `AVUI`, `SceneEntryData`, `FrameworkElement`, `HUDStatus`, `HUDSpeedometerComponent`, `HUDStatusSpeedometerProviderComponent`, `PlayerMinimapViewComponent`, `CopterHud`, `UIPageCopterHud`.
   - IDA targets already surfaced by decompile:
     - `PlayerMinimapViewComponent` registration around `0x14016E5D0`.
     - `HUDSpeedometerComponent` registration around `0x140181220`.
     - `HUDStatusSpeedometerProviderComponent` registration around `0x140231F90`.
     - `UIPage` registration around `0x14004B1C0`.
     - `UIScene` allocation/register path around `0x1400C3660`.
     - `nps_register_ui_page_UIPageCopterHud` around `0x14030DF20`.
     - `nps_register_ui_scene_CopterHud` around `0x14033EAC0`.
   - Goal: find the common AVUI render/update boundary and any page/scene active-state list we can read for HUD/menu classification.

2. Trace the safe-frame and HUD options.
   - Settings/options strings found offline:
     - `Options/Speedometer`
     - `Options/DriftHud`
     - `Options/HUDSafeFrameHorizontal`
     - `Options/HUDSafeFrameVertical`
     - `Options/HideInactiveMouseOnHUD`
   - Developer/config toggles found offline:
     - `nohud`
     - `fastuirender`
     - `fasthud`
     - `zoomoutminimap`
     - `uiscenegraphdebug`
     - `HideAllOverlays`
   - Goal: identify whether FH5 has a native HUD-safe-frame rect we can shrink/recenter for desktop fallback and possibly for menu testing. This is secondary to the quad path but useful for debug.

3. Map UI texture/effect assets to render draws.
   - Extract/list `media\UI\Textures\HiRes\Data_Bound\HUD.zip`, `Horizon_Map.zip`, and `Fonts.zip`.
   - Use HUD swatch/texture names to label RenderDoc resources when possible.
   - Goal: correlate flat HUD draw resources/shaders with pixel-history EIDs.

4. Find AVUI render-call choke points.
   - Search callers of D3D12-style UI effect/shader setup, `TideUITextures`, `Cms::UITextureData`, `AVUI::FrameworkElement`, and `UIPage::KeyArgs`.
   - Prefer a named common UI render function over per-component hooks.
   - Goal: a stable hook for "UI pass begins/ends" or "current active page/scene changed".

## RenderDoc/game capture tasks

These are required because offline code/asset evidence cannot prove the D3D12 pass boundary.

1. Capture one stable gameplay frame with:
   - speedometer visible,
   - minimap visible,
   - objective text/prompt visible,
   - route line/chevrons visible,
   - car and shadow visible.

2. Pixel history points:
   - a speedometer digit/needle pixel,
   - a minimap icon/edge pixel,
   - an objective/text pixel,
   - a route-line pixel,
   - a car-body pixel,
   - a car-shadow pixel.

3. For every contributing HUD/UI draw, record:
   - EID,
   - PSO,
   - VS/PS shader hashes or IDs,
   - root signature and root params,
   - SRV texture/resource names where available,
   - RTV resource, format, dimensions,
   - viewport/scissor,
   - draw order relative to known main-camera EIDs and `Present`.

4. Decide which case FH5 uses:
   - Case A: HUD renders to a separate UI render target then composites into the backbuffer.
   - Case B: HUD draws directly into the final backbuffer late in the frame.
   - Case C: menus/fullscreen UI use a separate path from gameplay HUD.

Case A is easiest: copy the UI RT to an OpenXR HUD swapchain and suppress/skip its final composite in the HMD projection image. Case B requires either copying the world eye before HUD draws, then letting HUD draw and copying/capturing only HUD afterwards, or redirecting HUD draws into our own offscreen alpha target. Case C needs separate gameplay-HUD and menu handling.

## Implementation plan in vrframework

### Phase 1 - Add HUD quad infrastructure

Current state:

- `OpenXR::end_frame(extra_layers)` already accepts extra composition layers.
- `D3D12Component::on_frame()` currently calls `vr->m_openxr->end_frame()` with no extra layers.
- `Fh5Adapter::reproject_hud()` is still a TODO.

Add:

- A HUD swapchain manager beside the existing eye swapchains.
- A D3D12 copy path from the chosen HUD texture/RT into the HUD swapchain image.
- A persistent `XrCompositionLayerQuad` builder owned by the D3D12/OpenXR submission path.
- Per-frame submission through `OpenXR::end_frame({hud_quad_layer})` on the right-eye/end-frame path.

Recommended first quad:

- Head-locked or stage-relative quad at about 1.5 to 2.0 meters.
- Width around 1.2 to 1.6 meters, height from HUD texture aspect.
- Alpha-capable RGBA swapchain format.
- Conservative flags first; test runtime alpha blending with a transparent checker HUD texture before trusting live HUD alpha.

### Phase 2 - Capture HUD texture without removing it yet

Implement a debug mode that copies whichever resource RenderDoc identifies as the flat HUD source into the HUD swapchain but does not suppress the game's normal HUD. This proves:

- the resource is the correct HUD,
- the copy/barriers are valid,
- the OpenXR quad appears and is readable,
- color/gamma/alpha are acceptable.

Acceptance: the same speedometer/minimap/objective content appears on the quad.

### Phase 3 - Remove duplicate HUD from the projection layer

Do not accept stereo projection images with a duplicate flat HUD still baked into each eye.

Preferred options by capture result:

- Case A, separate UI RT: copy UI RT to HUD quad, then suppress the final UI composite into the eye image.
- Case B, direct final-backbuffer HUD: add a "world-copy-before-HUD" boundary, submit that clean world image to the eye swapchain, then use later HUD draws to populate the quad.
- Case B fallback: PSO/draw filter and RTV redirection for identified HUD PSOs into a transparent offscreen target.

Acceptance: projection layer has world only; flat HUD exists once as a quad.

### Phase 4 - Classify gameplay HUD versus menus

Use the AVUI/page/scene map to detect:

- normal gameplay HUD,
- pause/fullscreen menus,
- map,
- photo mode,
- popups.

Gameplay HUD should be a smaller comfortable quad. Fullscreen menus should become a larger quad closer to a 2D panel and may pause or damp head-driven camera changes while active.

Acceptance: pause menu/map are readable and usable, not split across both eyes as a baked screen overlay.

### Phase 5 - Input/pointer handling

Gamepad input can initially stay game-native. Mouse/controller pointing needs two modes:

- If the engine still sees desktop-space UI, keep the desktop cursor mapping and let the quad mirror it.
- For VR pointers, intersect controller/head ray with the HUD quad and convert hit UV to desktop/UI coordinates.

Use `HideInactiveMouseOnHUD` and cursor/page state as useful RE anchors.

### Phase 6 - Clean up 3D navigation overlays separately

Do not fold route line/chevrons into the flat HUD plan by accident. They are camera/world consumers. Options:

- Leave them in world stereo if the camera/projection hooks make them correct.
- If they still produce bad stereo, identify their PSOs/EIDs and patch them at draw level.
- Only put them on the HUD quad if we intentionally decide navigation should be flat.

## Risks

- HUD alpha may be lost if FH5 composites into an opaque backbuffer. Need prove whether a separate UI RT preserves alpha.
- Gamma/HDR may differ from the projection layer. UI effect bins include SRGB/tint/saturation paths, so validate color on the quad early.
- TAA/motion history can leave one-eye ghosts in the projection image if HUD/world copies happen after history-sensitive passes.
- Draw filtering by PSO alone may catch non-HUD UI or miss variants. Use resource + viewport + draw order + shader IDs, not PSO alone.
- Menus may render through different scene/page paths than gameplay HUD.

## Immediate next work items

1. Produce the RenderDoc pixel-history table for flat HUD elements.
2. Add a `HudQuadLayer`/HUD swapchain scaffold in `vrframework` using a generated test texture first.
3. Add a `D3D12Component` path to submit that quad through `OpenXR::end_frame(extra_layers)`.
4. After the test quad works, wire the captured HUD resource into the quad copy path.
5. Only after that, add suppression/redirection so the HUD no longer appears baked into each eye.

The first implementation patch should not try to solve HUD extraction and suppression at the same time. Get a known test quad into OpenXR first, then copy the real HUD texture, then remove the duplicate baked HUD.
