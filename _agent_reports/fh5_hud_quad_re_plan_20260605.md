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
