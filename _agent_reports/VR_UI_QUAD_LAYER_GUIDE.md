# The Ultimate Guide to Building UI/HUD for VR Games Using a Quad Layer

*A cross-engine engineering handbook synthesized from nine real VR mods:*
*JKXR (idTech/OpenGL-ES), OpenJK & OpenJKDF2 (reference ports), crysis_vrmod (CryEngine 3 / D3D10 / OpenXR), farcry_vrmod (CryEngine 1 / D3D9 / OpenVR), anvilengine2vr & starfield2vr (AnvilNext / Creation Engine 2 / D3D12 / OpenXR, shared `vrframework`), fox3/PlayAll (PlayAll engine / D3D11 / OpenXR), and FO2-VR (FlatOut 2 / D3D9 → D3D11 bridge / OpenXR).*

Last updated 2026-06-08. Written as a reference for the FH5 VR mod work, but engine-agnostic.

---

## 0. TL;DR — the one-paragraph version

A flat 2D HUD/menu does not belong *inside* a stereo 3D scene: if you bake it into both eyes you get eye-fighting, depth conflict, blurry text, and a HUD that's painful to read. The fix every mature VR mod converges on is to **lift the UI out of the world and submit it as its own compositor layer** — a single, mono, head- or world-locked textured rectangle that the VR runtime composites *on top of* the stereo world layer at a fixed, comfortable depth. There are exactly two ways to do this (a **native compositor quad layer**, e.g. OpenXR `XrCompositionLayerQuad` / OpenVR `IVROverlay`; or a **baked in-scene quad** drawn into the eye textures), and the hard 80% of the work is never the layer itself — it's **(a) sourcing the UI pixels cleanly into a transparent texture** and **(b) keeping that same UI out of the world eyes.** This guide covers both, with the exact API calls, the alpha/format traps, and the engine-seam hooking patterns from all nine mods.

---

## 1. Why a quad layer at all?

| Problem with HUD baked into the 3D eyes | Why the quad layer fixes it |
|---|---|
| **Depth conflict / eye fighting.** A HUD baked at screen depth has zero parallax, but the world behind it does — your eyes can't fuse the two. | The quad sits at one chosen depth (e.g. 2 m). Both eyes converge on it naturally. |
| **Text shimmer / aliasing.** The HUD is resampled through the per-eye distortion + reprojection every frame. | The compositor samples the quad at native resolution with its own high-quality filter; text stays crisp. |
| **Reprojection smear.** When the world layer is async-reprojected (ATW/ASW), a baked HUD smears with it. | A separate layer is reprojected *independently* (or not at all) — the HUD stays rock-steady. |
| **Wrong scale / FOV.** A HUD authored for a 16:9 monitor wraps uncomfortably across a 100°+ VR FOV. | You pick the angular size of the quad. A 2 m-wide quad at 2 m ≈ 53° — readable, not overwhelming. |
| **Stereoization cost.** Drawing the HUD twice (per eye) doubles UI draw cost. | The quad texture is rendered **once (mono)** and shown to both eyes by the compositor. |

Every single mod surveyed reached this same conclusion. The disagreements are only about *how* to get the pixels and *how* to position the rectangle.

---

## 2. The two architectures (pick one)

### Architecture A — Native compositor quad layer  *(preferred)*
You hand the runtime a **separate swapchain** containing just the UI, plus a pose+size, and the runtime composites it. The world eyes never contain the HUD.

- **OpenXR:** `XrCompositionLayerQuad` appended to `xrEndFrame`'s layer list.
- **OpenVR/SteamVR:** an `IVROverlay` handle the runtime composites every frame.

Used by: **JKXR**, **crysis_vrmod** (OpenXR), **farcry_vrmod** (OpenVR overlay), **anvilengine2vr / starfield2vr** (via `vrframework`), **fox3/PlayAll** (OpenXR).

**Pros:** crispest text, independent reprojection, world eyes are automatically clean, compositor handles stereo. **Cons:** you must produce a clean transparent UI texture and you must have a place to render it that isn't the world.

### Architecture B — Baked in-scene quad
You render the UI into an offscreen texture, then draw it as a **3D textured rectangle into each eye's framebuffer** yourself, before submitting the eyes as a normal projection layer.

Used by: **FO2-VR** (D3D9, no clean way to share a separate layer).

**Pros:** works on *any* engine/API with zero compositor-layer support; one projection layer to submit. **Cons:** HUD is baked into the eyes (no independent reprojection), you pay per-eye draw cost, and you must manage depth/blend yourself. This is the fallback when Architecture A is impossible.

> **Rule of thumb:** Always start with Architecture A. Only fall back to B when you cannot get the UI into a dedicated layer (ancient API, or you can't separate UI from world — see §8). FO2-VR chose B *because* D3D9↔D3D11 sharing forced a CPU readback anyway, so a separate layer bought nothing.

---

## 3. Creating the quad layer

### 3.1 OpenXR `XrCompositionLayerQuad` — field by field

This is the canonical struct. Every OpenXR mod above fills it nearly identically.

```cpp
XrCompositionLayerQuad quad{ XR_TYPE_COMPOSITION_LAYER_QUAD };
quad.next            = nullptr;                 // or &XrCompositionLayerDepthTestFB for depth-tested HUD
quad.layerFlags      = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT; // see §6
quad.space           = space;                   // reference space: VIEW (head-locked), LOCAL, or STAGE — see §5
quad.eyeVisibility   = XR_EYE_VISIBILITY_BOTH;  // MONO: one texture, shown to both eyes — see §7
quad.subImage.swapchain               = hudSwapchain;        // your dedicated UI swapchain
quad.subImage.imageArrayIndex         = 0;
quad.subImage.imageRect.offset        = {0, 0};
quad.subImage.imageRect.extent        = {hudW, hudH};        // texel region to sample
quad.pose.orientation = {0,0,0,1};              // identity = facing -Z (toward the viewer)
quad.pose.position    = {0.0f, 0.0f, -distance};// metres; -Z is "in front" in VIEW/LOCAL space
quad.size             = { widthMeters, heightMeters };       // PHYSICAL size of the rectangle in metres
```

Then in your frame submission:

```cpp
const XrCompositionLayerBaseHeader* layers[2];
int n = 0;
layers[n++] = (const XrCompositionLayerBaseHeader*)&projectionLayer; // the stereo world (drawn first = behind)
layers[n++] = (const XrCompositionLayerBaseHeader*)&quad;            // the HUD (drawn last = on top)

XrFrameEndInfo end{ XR_TYPE_FRAME_END_INFO };
end.displayTime          = predictedDisplayTime;
end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
end.layerCount           = n;
end.layers               = layers;
xrEndFrame(session, &end);
```

**Critical ordering rule:** the layer array is composited **back-to-front in array order.** The world projection layer must come **before** the quad, or the HUD ends up behind the world. (crysis_vrmod, JKXR, fox3 all push world first, quad last.)

#### Field gotchas observed across the mods
- **`size` is in METRES, not pixels.** `subImage.imageRect.extent` is in *texels* (which region of the swapchain to read); `size` is the *physical* width/height of the rectangle in the world. Mixing these up is the #1 beginner bug. JKXR: `size = {6.0f, 5.5f}` (6 m × 5.5 m menu). crysis: `SetHudSize(4.f, 4.f * h/w)`. fox3: `width = m_fHudScale; height = m_fHudScale * (1080/1920)`.
- **Aspect ratio must be applied to `size`, not the texture.** Compute `height = width * (texH/texW)` so the quad isn't stretched. Every mod does `width * aspect`.
- **`pose.position.z` is negative to push the quad away** (in VIEW/LOCAL space, -Z is forward). fox3: `position.z = -m_fHudDistance`.
- **Identity orientation faces the user.** A quad with `orientation = {0,0,0,1}` in VIEW space already faces the eye. You only set orientation when world-locking (see JKXR's yaw-snap below).

### 3.2 OpenVR / SteamVR `IVROverlay` — the managed alternative

OpenVR overlays are *persistent* objects the runtime composites every frame — you don't re-submit them in a layer list. From **farcry_vrmod**:

```cpp
// Create once:
vr::VROverlay()->CreateOverlay("FarCryHud", "FarCry HUD", &m_hudOverlay);
vr::VROverlay()->SetOverlaySortOrder(m_hudOverlay, 1);          // composite order among overlays
vr::VROverlay()->SetOverlayWidthInMeters(m_hudOverlay, 2.0f);   // physical width; height from aspect
vr::VROverlay()->ShowOverlay(m_hudOverlay);

// Per frame — update the texture:
vr::Texture_t tex;
tex.eColorSpace = vr::ColorSpace_Auto;
tex.eType       = vr::TextureType_DirectX;   // or _Vulkan / _DirectX12
tex.handle      = myTextureHandle;
vr::VROverlay()->SetOverlayTexture(m_hudOverlay, &tex);

// Position — pick ONE:
//   head-locked:
vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(
    m_hudOverlay, vr::k_unTrackedDeviceIndex_Hmd, &hmdRelativeMatrix);
//   world-locked:
vr::VROverlay()->SetOverlayTransformAbsolute(
    m_hudOverlay, vr::TrackingUniverseStanding, &absoluteMatrix);
```

**OpenVR vs OpenXR quad — the practical differences:**
| | OpenXR `XrCompositionLayerQuad` | OpenVR `IVROverlay` |
|---|---|---|
| Lifetime | Re-submitted every `xrEndFrame` | Persistent; `ShowOverlay` once |
| Texture update | Acquire/release swapchain image each frame | `SetOverlayTexture` each frame |
| Alpha control | `layerFlags` blend bit | `VROverlayFlags_IgnoreTextureAlpha` (invert: false = respect alpha) |
| Built-in input | None (you raycast yourself) | **Built-in mouse events** via `PollNextOverlayEvent` (huge for menus — see §9) |
| Best for | Modern engines, full layer stacks | Quick mods, dashboard-style UI, free laser-pointer input |

### 3.3 The dedicated UI swapchain (OpenXR)

You need a swapchain **separate from the eye swapchains**, sized to your UI. fox3 uses swapchain index 2 (0/1 = eyes). JKXR reuses `FrameBuffer[0]`. Creation:

```cpp
XrSwapchainCreateInfo ci{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
ci.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
ci.format     = uiFormat;        // MUST match your source — see §6.3
ci.sampleCount= 1;               // no MSAA on a UI layer
ci.width      = uiW;  ci.height = uiH;
ci.faceCount  = 1;  ci.arraySize = 1;  ci.mipCount = 1;
xrCreateSwapchain(session, &ci, &hudSwapchain);
```

Per frame: `xrAcquireSwapchainImage` → `xrWaitSwapchainImage` → render/copy your UI into the returned texture → `xrReleaseSwapchainImage`.

---

## 4. The hard part: sourcing the UI texture

This is where 80% of the engineering goes. You need the HUD pixels in a texture **with a real alpha channel** and **without the 3D world in it.** Five strategies, in rough order of cleanliness:

### Strategy 1 — Native engine integration (cleanest)
If you control the engine (or it has a UI render-target seam), point the UI pass at *your* render target.

- **fox3/PlayAll** creates D3D11 RTs wrapping the OpenXR HUD swapchain images and calls `GetRenderLayer()->SetCurrentRenderTarget(pHudRT)` so the engine's own HUD pass draws straight into the swapchain. Zero copying, zero heuristics. This is the gold standard — and only possible because PlayAll is open-source.
- **JKXR** does the moral equivalent: it gates the whole 2D pass (`SCR_DrawScreenField`) so that when a menu is up, the engine renders **only** the 2D UI into `FrameBuffer[0]`, which is the quad's swapchain.

### Strategy 2 — Render-target swap hook (the workhorse for closed engines)
Hook the engine's known "draw the HUD" function. Inside the hook: save the current RT, set your UI texture as the RT, call the original, then restore.

**FO2-VR** (`MyCameraDrawHud`, hooking `CameraView::DrawHUD` at a known address):
```cpp
void MyCameraDrawHud(uintptr_t this_ptr, ...) {
    dev->GetRenderTarget(0, &s_savedHudRt);     // save
    dev->GetDepthStencilSurface(&s_savedHudDs);
    dev->SetRenderTarget(0, g_uiSurf);          // redirect to UI texture
    callOriginal_DrawHUD(this_ptr, ...);        // HUD draws into g_uiSurf
    dev->SetRenderTarget(0, s_savedHudRt);       // restore
}
```
FO2-VR hooks *three* such functions — main HUD, the render-callback (countdown/"press start"), and the standings overlay — so every UI source lands in the same `g_uiSurf`. **Lesson: real games draw UI from several entry points; find them all.**

This is also exactly the seam your **FH5** work is chasing: the `vf54` UIRenderer is FH5's `DrawHUD` equivalent, and `*(this+0x40)` is its `g_uiSurf`.

### Strategy 3 — Backbuffer capture (legacy / simplest, but dirty)
Copy the whole backbuffer after the game has drawn the HUD. Cheap, but the backbuffer also contains the **world**, so you only get clean UI if you first cleared to transparent and the world went to a *different* target.

- **crysis_vrmod** (`CaptureHUD` → `CopyBackbufferToTexture`): `m_device->CopyResource(target, backbuffer)`, with `ResolveSubresource` if MSAA. Works because Crysis renders the eyes to a stereo buffer and the HUD to a *cleared-transparent* backbuffer.
- **farcry_vrmod**: D3D9 `StretchRect(backBuffer, …, texSurface, …, D3DTEXF_POINT)`.

### Strategy 4 — Resource identification by fingerprint (D3D12, closed engine)
Under D3D12 you can't easily "swap the RT" globally, so you *identify* the UI render target among all RTs and copy it. This is the **starfield2vr / anvilengine2vr / your FH5** world.

Fingerprints used:
- **Shader CRC.** starfield2vr's `D3D12CallBackManager` records `ps_crc/vs_crc/cs_crc` at PSO creation; UI (Scaleform) shaders have stable CRCs you match at draw time.
- **Render-target format + dimensions.** UI RTs are typically full-res `R8G8B8A8_UNORM` (vs the world's `R10G10B10A2`/HDR formats).
- **Menu/asset hash.** starfield2vr hashes the Scaleform movie URL (`"Interface/HUDMenu.gfx"_DJB`) to classify each menu as HUD-type vs full-menu-type and choose depth.
- **Draw-order heuristic.** "First fullscreen draw into the eye-source = world (keep); subsequent UI-shader draws = HUD (redirect)" — the heuristic your FH5 `Fh5CameraCbuffer.cpp` already encodes via `register_eye_source` + `IsEyeSource`.

Once identified, copy with proper barriers (starfield2vr's `CopyResource` helper):
```cpp
D3D12_RESOURCE_BARRIER b[2] = {
  CD3DX12_RESOURCE_BARRIER::Transition(src, srcState, D3D12_RESOURCE_STATE_COPY_SOURCE),
  CD3DX12_RESOURCE_BARRIER::Transition(dst, dstState, D3D12_RESOURCE_STATE_COPY_DEST) };
cmdList->ResourceBarrier(2, b);
cmdList->CopyResource(dst, src);   // then transition both back
```

### Strategy 5 — Cross-API CPU readback (last resort)
When the game's API can't share a handle with your VR-submission API (classic **D3D9 game, D3D11 OpenXR**), you read back through system memory. **FO2-VR**'s three-stage pipeline:
1. D3D9 `GetRenderTargetData(backbuffer → D3DPOOL_SYSTEMMEM offscreen surface)` (GPU→CPU).
2. `LockRect` the sysmem surface, `Map` a `D3D11_USAGE_DYNAMIC` staging texture, `memcpy` row by row.
3. `CopySubresourceRegion` the D3D11 staging texture into the eye swapchain images.

This is slow (a full GPU→CPU→GPU round trip per frame) — only do it when handle sharing is genuinely unavailable. Prefer a **shared keyed-mutex texture** (crysis_vrmod shares a `D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX` texture into D3D11 via `GetSharedHandle`/`OpenSharedResource`) whenever the two APIs are sharing-compatible (D3D10↔D3D11, D3D11↔D3D12).

---

## 5. Positioning & sizing — reference spaces and attachment modes

### 5.1 Reference space = the locking behaviour
| Locking | OpenXR space | OpenVR | Feel | Used by |
|---|---|---|---|---|
| **Head-locked** | `VIEW` space (or LOCAL with per-frame head pose) | `…TrackedDeviceRelative(HMD)` | HUD glued to your face; always centred | starfield2vr (view space + `perspective` depth), fox3 (LOCAL + `-distance`) |
| **World/body-locked, yaw-snapped** | `STAGE`/`LOCAL`, pose set once when menu opens | `…TransformAbsolute(Standing)` | Menu floats in the room; you can look around it | JKXR (snaps to HMD yaw at menu open), crysis/farcry "in front of player" |
| **Hand/weapon-attached** | pose from controller space | controller transform | Scope, binoculars, wrist menu | crysis_vrmod (off-hand binoculars, weapon-hand scope), farcry_vrmod |

**Head-locked is correct for a persistent HUD** (health/speed/minimap). **Yaw-snapped world-locked is correct for full-screen menus** (pause, inventory) so the player can physically look across a large menu. JKXR switches between them by *purpose*: gameplay HUD vs `VR_UseScreenLayer()` menus.

JKXR's yaw-snap (world-lock a menu in front of where you were looking):
```cpp
const XrVector3f yAxis = {0,1,0};
quad.pose.orientation = XrQuaternionf_CreateFromVectorAngle(yAxis, DEG2RAD(vr.hmdorientation_snap[YAW]));
quad.pose.position = {
    head.x - sin(DEG2RAD(snapYaw)) * distance,
    1.0f,                                       // fixed eye height
    head.z - cos(DEG2RAD(snapYaw)) * distance };
```

### 5.2 Distance, size, aspect — sane defaults
- **Distance:** 1.5–2.5 m is the comfort sweet spot. JKXR `2.0 + cvar`; fox3 `m_fHudDistance`; farcry `~2.5 m`. Too close → eye strain & vergence-accommodation conflict; too far → small text.
- **Width:** size the *angular* footprint. A 2 m-wide quad at 2 m subtends ~53°. crysis uses 4 m for menus, 2 m for the head HUD, 0.8 m for binoculars, 0.25 m for a weapon scope — **smaller, closer quads for "instrument" UI; bigger for menus.**
- **Aspect:** always `height = width * texH/texW`. Never stretch.
- **Expose them as cvars/knobs.** Every mod does (`vr_screen_dist`, `hudSettings.hudScale`/`perspective`, `m_fHudScale`, `FO2VR_UI_DISTANCE`). Users' faces and tastes differ.

### 5.3 Asymmetric-FOV centering (subtle but important)
On real HMDs the per-eye FOV is asymmetric. JKXR stores `off_center_fov_x/y = -(angleLeft+angleRight)/2` at eye-buffer prep so the HUD can be nudged to the optical centre rather than the texture centre. If your HUD looks slightly off-centre per eye, this is why.

---

## 6. Transparency, alpha & blending — the part that bites everyone

This is the single most error-prone area (and the source of your FH5 "magenta quad" bug). Three things must all line up.

### 6.1 The layer blend flag
Tell the compositor to honour the texture's alpha:
- **OpenXR:** `quad.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;` (every OpenXR mod sets this).
- **OpenVR:** `SetOverlayFlag(h, VROverlayFlags_IgnoreTextureAlpha, false);` (false = *do* respect alpha). farcry sets it `true` for binoculars/scope where the UI should be fully opaque.

### 6.2 Premultiplied vs straight alpha — know which you have
- If your UI compositor outputs **premultiplied** alpha (RGB already × A — Scaleform does this; fox3's HudRenderer does this), then **do not** also set `XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT`. fox3's feather pass is carefully written to *preserve* premultiplication (`SRC_BLEND=ZERO, DST_BLEND=SRC_ALPHA` on both colour and alpha).
- If your UI is **straight** alpha (plain `D3DBLEND_SRCALPHA/INVSRCALPHA` content, FO2-VR), set the unpremultiplied bit on OpenXR, or premultiply in a small shader before submit.
- Getting this wrong gives **dark fringes** around UI edges (straight treated as premult) or **glowing/washed** edges (premult treated as straight).

### 6.3 Format match — the magenta-quad killer
**The source texture format and the quad swapchain format must be copy-compatible.** Most VR mods composite the UI with a plain `CopyResource`/`CopyTextureRegion`, which does **no format conversion**. If the game's UI/back buffer is `R10G10B10A2_UNORM` (HDR) and you created the HUD swapchain as `R8G8B8A8_UNORM_SRGB`, the copy produces garbage — classic magenta/purple. Fix (from your own FH5 D3D12Component fix): **probe the source format and create the HUD swapchain to match it**:
```cpp
switch (bb_fmt_probe) {
  case R8G8B8A8_UNORM: case R8G8B8A8_UNORM_SRGB: hud_fmt = R8G8B8A8_UNORM_SRGB; break;
  case B8G8R8A8_UNORM: case B8G8R8A8_UNORM_SRGB: hud_fmt = B8G8R8A8_UNORM_SRGB; break;
  case R10G10B10A2_UNORM:                        hud_fmt = R10G10B10A2_UNORM;   break;
  default:                                       hud_fmt = R8G8B8A8_UNORM_SRGB; break;
}
```
> This single class of bug — *purple/magenta HUD on the quad* — is almost always (a) format mismatch on a no-convert copy, or (b) HDR (R10A2 / FP16) content copied to an 8-bit SRGB target. Always match formats, or use a converting blit (a fullscreen shader) instead of `CopyResource`.

### 6.4 Clear-to-transparent + the opaque-alpha trick
Two complementary tricks:
- **Clear the UI RT to transparent black `(0,0,0,0)` before the UI draws** so non-UI pixels are see-through. crysis_vrmod and farcry_vrmod both clear to `ARGB(0,0,0,0)` before the HUD pass.
- **JKXR's inverse trick:** after rendering the UI, it forces the whole framebuffer's **alpha to 1.0** (`glColorMask(0,0,0,1); glClear`) so opaque UI areas fully occlude the world while transparent areas (alpha already 0 from the UI shader) show through. Use this when your UI shader writes meaningful alpha only on drawn pixels.

---

## 7. Stereo vs mono

**The HUD is mono.** You render the UI once and show the same texture to both eyes; the compositor reprojects it per-eye for you. Set `eyeVisibility = XR_EYE_VISIBILITY_BOTH` (OpenXR) or just let the overlay show to both (OpenVR). This is universal across every mod.

Two refinements seen in the wild:
- **Stereo "cinema" mode.** For 3D cutscenes, crysis_vrmod submits **two** quads, `XR_EYE_VISIBILITY_LEFT`/`RIGHT`, reading the left/right halves of a side-by-side texture. farcry uses `VROverlayFlags_SideBySide_Parallel`. Only do this for genuinely stereo content.
- **Per-eye HUD rendering ≠ stereo HUD.** FO2-VR draws the quad into *both* eye framebuffers (because it's baking, Architecture B), but at the *same* head-relative position — so it's still perceptually mono. Drawing twice is a cost of Architecture B, not a stereo feature.

### 7.1 The full-screen-menu trick: black out the world
When a full-screen menu is up you usually want the world *gone*, not showing behind a translucent menu. JKXR's elegant solution: submit a **black projection layer** (point the eye projection views at a `NullFrameBuffer` that's just black) **plus** the UI quad on top. Result: solid black world + crisp menu. It flips into this mode via `VR_UseScreenLayer()` (true when a fullscreen UI, console, loading screen, or cinematic is active).

---

## 8. Keeping the eyes clean (the separation problem)

For Architecture A to look right, the HUD must appear on the quad **and nowhere else.** If the game also draws the HUD into the world eye-texture, you get a doubled, ghosted HUD. Three approaches:

1. **Redirect at the source (best).** The RT-swap hook (§4.2) inherently removes the HUD from the eyes — it never reaches the backbuffer because you pointed it at your UI texture. FO2-VR/JKXR/fox3 are clean by construction.
2. **Suppress the HUD draws in the eye pass.** Under D3D12 you can no-op the UI draws when the bound RT is the eye-source: detect "this is a UI PSO drawing into the eye target" and skip it (this is the FH5 `uiredirect` path). Requires reliable UI-draw identification (§4.4).
3. **Render the world to a different target than the HUD.** crysis/farcry render eyes to a stereo buffer and HUD to the (cleared) backbuffer, so capturing the backbuffer yields HUD-only and the eyes were never contaminated.

> **FH5-specific note (from the project memory):** the AFR/OpenXR path *reroutes* the HUD draws away from the eye-source to per-widget RTs, so they're already absent from the eye — the remaining task is to *find* the UIRenderer's true target (`vf54`'s `*(this+0x40)`) and point the quad at it, then ensure no residual UI composites back into the eye. The UI-PSO-hash tracking approach (Strategy 4) is the most reliable way to locate it under AFR.

---

## 9. Interaction — pointing at the UI

Three mechanisms, increasing in polish:

### 9.1 Free, via OpenVR overlay mouse events
If you used an OpenVR overlay, you get laser-pointer-to-UV **for free.** farcry_vrmod:
```cpp
vr::VROverlay()->SetOverlayMouseScale(m_hudOverlay, &{renderW, renderH});
while (vr::VROverlay()->PollNextOverlayEvent(m_hudOverlay, &e, sizeof(e))) {
  if (e.eventType == vr::VREvent_MouseMove)
     mouse->SetVScreenX(800.f * e.data.mouse.x / W),
     mouse->SetVScreenY(600.f * (1.f - e.data.mouse.y / H));   // note Y flip
  if (e.eventType == vr::VREvent_MouseButtonDown && e.data.mouse.button==VRMouseButton_Left) pressed=true;
}
```
The runtime raycasts the controller against the overlay and hands you UV-mapped mouse coordinates. This alone is a strong reason to consider OpenVR overlays for menu-heavy mods.

### 9.2 Manual ray → angle → UV (OpenXR, simple)
JKXR maps the controller's yaw/pitch *relative to the snapped menu yaw* into 0..1 screen coords, then injects a synthetic mouse event:
```cpp
float cursorX = -sinf(DEG2RAD(controllerYaw - menuYaw)) + 0.5f;
float cursorY = (controllerPitch / 90.0f) + 0.5f;
CL_MouseEvent(int(cursorX*640), int(cursorY*480), 0);   // feed the engine's UI as if it were a mouse
```
Cheap, no real geometry, good enough for a flat menu directly in front.

### 9.3 Proper ray–plane intersection → UV (general)
The correct general method: build a ray from the controller pose, intersect the quad's plane, convert the hit point to the quad's local 2D coords, divide by quad size → UV → pixel. fox3's `VRMenuPointer` / `TryResolveHudNodeHit` does this against the engine's HUD-node hierarchy for real hit-testing. Use this when the quad can be world/hand-locked at arbitrary angles (where the angle approximation in 9.2 breaks down).

**Whatever the mechanism, the endpoint is the same: convert to the UI's native coordinate space and inject the game's existing mouse/cursor event** so the game's own menu logic runs unmodified.

---

## 10. Performance & quality notes

- **Render the UI at native panel resolution, not eye resolution.** The quad doesn't need eye-buffer supersampling; size it to the UI's authored resolution to save fill.
- **No MSAA on the UI swapchain** (`sampleCount = 1`). UI is already crisp; MSAA just costs memory/bandwidth.
- **Double-buffer the UI swapchain** so the compositor reads frame N while you write N+1 (avoids stalls). starfield2vr notes this.
- **Update the quad only when the UI changes** if you can detect it — a static HUD doesn't need a re-copy every frame.
- **Avoid CPU readback** (Strategy 5) unless forced; it's the dominant cost in FO2-VR. Prefer shared textures.
- **Feather the edges** (fox3's HudFeather) for a softer, less "pasted rectangle" look — fade RGB *and* alpha together to keep premultiplication valid.
- **Mind reprojection:** a head-locked quad is rock-steady; a world-locked quad benefits from the compositor's reprojection. Either is better than a baked HUD that smears with the world layer.

---

## 11. Pitfalls checklist (learned the hard way across these mods)

- [ ] **Layer order:** world projection layer **before** the quad in the array, or the HUD hides behind the world.
- [ ] **`size` (metres) vs `imageRect.extent` (texels):** don't confuse them.
- [ ] **Aspect:** `height = width * texH/texW`; never stretch.
- [ ] **Format match on copy:** source fmt == HUD swapchain fmt for `CopyResource`, or use a converting blit. *(This is the magenta-quad bug.)*
- [ ] **HDR source (R10A2/FP16) → 8-bit SRGB quad** = garbage. Match the bit depth or tonemap in a shader.
- [ ] **Premultiplied vs straight alpha:** set/clear `UNPREMULTIPLIED_ALPHA_BIT` to match your UI compositor.
- [ ] **Clear the UI RT to (0,0,0,0)** before the UI pass so empty areas are transparent.
- [ ] **Find ALL UI entry points:** games draw HUD from multiple functions (FO2-VR needed 3 hooks).
- [ ] **Keep the eyes clean:** if you don't redirect at source, you must suppress UI in the eye pass or you'll see a doubled HUD.
- [ ] **Y-axis convention:** texture/UV Y often needs flipping vs screen Y (farcry flips it in the mouse map).
- [ ] **Eye-stamp / per-eye state under AFR:** if your eyes alternate frames, make sure the HUD copy isn't accidentally tied to one eye's frame.
- [ ] **MSAA backbuffer source:** use `ResolveSubresource` (D3D10/11) before copying, not a raw copy.

---

## 12. Decision matrix

| Your situation | Recommended approach |
|---|---|
| Open-source / engine-source available | **Strategy 1** (route UI pass to your RT) + OpenXR quad layer. Cleanest possible. |
| Closed engine, known HUD function, D3D9/11 | **Strategy 2** (RT-swap hook) + OpenXR quad (or OpenVR overlay for free input). |
| Closed engine, D3D12, UI hard to isolate | **Strategy 4** (PSO/CRC/format/menu-hash identification) + suppress-in-eye (§8.2). *This is FH5.* |
| Game API can't share with your XR API (e.g. D3D9→D3D11) | **Strategy 5** readback, and consider **Architecture B** (bake) since you're copying anyway (FO2-VR). |
| Menu-heavy mod, want laser input cheaply | **OpenVR overlay** for the built-in `PollNextOverlayEvent` mouse mapping. |
| Modern engine, full layer stack, best text quality | **OpenXR quad layer**, head-locked HUD + yaw-snapped menus, format-matched swapchain. |

---

## 13. Annotated reference implementations

### 13.1 Minimal OpenXR quad-layer HUD (synthesized from JKXR + crysis + fox3)
```cpp
// --- once, at session init ---
XrSwapchainCreateInfo sci{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
sci.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
sci.format = MatchSourceFormat();        // §6.3 — match the game's UI/backbuffer format!
sci.sampleCount = 1; sci.faceCount = 1; sci.arraySize = 1; sci.mipCount = 1;
sci.width = uiW; sci.height = uiH;
xrCreateSwapchain(session, &sci, &g_hudSwapchain);

// --- every frame, after the game has produced its UI pixels ---
uint32_t idx; xrAcquireSwapchainImage(g_hudSwapchain, nullptr, &idx);
XrSwapchainImageWaitInfo wi{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO }; wi.timeout = XR_INFINITE_DURATION;
xrWaitSwapchainImage(g_hudSwapchain, &wi);
    CopyUiInto(g_hudImages[idx]);        // §4 — your sourcing strategy. Clear to (0,0,0,0) first.
xrReleaseSwapchainImage(g_hudSwapchain, nullptr);

XrCompositionLayerQuad quad{ XR_TYPE_COMPOSITION_LAYER_QUAD };
quad.layerFlags    = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
quad.space         = g_viewSpace;        // head-locked HUD
quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
quad.subImage      = { g_hudSwapchain, {{0,0},{(int)uiW,(int)uiH}}, 0 };
quad.pose          = { {0,0,0,1}, {0, 0, -g_hudDistance} };       // metres, -Z forward
quad.size          = { g_hudWidthM, g_hudWidthM * (float)uiH/uiW }; // aspect-correct

const XrCompositionLayerBaseHeader* layers[] = {
    (XrCompositionLayerBaseHeader*)&g_worldProjectionLayer,  // world first (behind)
    (XrCompositionLayerBaseHeader*)&quad };                  // HUD last (on top)
XrFrameEndInfo end{ XR_TYPE_FRAME_END_INFO };
end.displayTime = g_predictedDisplayTime;
end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
end.layerCount = 2; end.layers = layers;
xrEndFrame(session, &end);
```

### 13.2 Minimal OpenVR overlay HUD with free input (from farcry_vrmod)
```cpp
// once:
vr::VROverlay()->CreateOverlay("game.hud", "Game HUD", &g_hud);
vr::VROverlay()->SetOverlayWidthInMeters(g_hud, 2.0f);
vr::VROverlay()->SetOverlayFlag(g_hud, vr::VROverlayFlags_IgnoreTextureAlpha, false);
vr::HmdVector2_t ms{ (float)W, (float)H }; vr::VROverlay()->SetOverlayMouseScale(g_hud, &ms);
vr::VROverlay()->ShowOverlay(g_hud);

// per frame:
vr::Texture_t t{ myUiTexHandle, vr::TextureType_DirectX, vr::ColorSpace_Auto };
vr::VROverlay()->SetOverlayTexture(g_hud, &t);
vr::HmdMatrix34_t m = headRelative(-distance); // or absolute() for world-lock
vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(g_hud, vr::k_unTrackedDeviceIndex_Hmd, &m);
vr::VREvent_t e;
while (vr::VROverlay()->PollNextOverlayEvent(g_hud, &e, sizeof(e))) handleUiMouse(e); // §9.1
```

---

## 14. Per-mod cheat sheet

| Mod | Engine / API | Layer mechanism | UI sourcing | Lock | Alpha | Input |
|---|---|---|---|---|---|---|
| **JKXR** | idTech3 / GLES / OpenXR | `XrCompositionLayerQuad` (+ black projection for menus) | Engine 2D pass → `FrameBuffer[0]` | Yaw-snapped world | SRC_ALPHA bit + force-alpha=1 trick | yaw/pitch → `CL_MouseEvent` |
| **crysis_vrmod** | CryEngine3 / D3D10 / OpenXR | `XrCompositionLayerQuad` | Backbuffer `CopyResource` (shared keyed-mutex tex to D3D11) | Head / off-hand / weapon | SRC_ALPHA bit, clear (0,0,0,0) | in-world controller aim |
| **farcry_vrmod** | CryEngine1 / D3D9 / **OpenVR** | `IVROverlay` | Backbuffer `StretchRect` | Head / world / binocular / scope | `IgnoreTextureAlpha` toggle | **built-in overlay mouse events** |
| **starfield2vr** | Creation Engine2 / D3D12 / OpenXR | quad via `vrframework` | Scaleform RT identified by menu-hash + shader CRC, `CopyResource` | Head (`perspective` depth) | premult Scaleform | button-mapped |
| **anvilengine2vr** | AnvilNext2 / D3D12 / OpenXR | quad via `vrframework` | Scaleform RT via render-graph hook | Head | premult Scaleform | button-mapped |
| **fox3/PlayAll** | PlayAll / D3D11 / OpenXR | `XrCompositionLayerQuad` (dedicated swapchain idx 2) | **Native**: engine HUD pass → swapchain RT + feather | Head (LOCAL, `-distance`) | premult + feather | `VRMenuPointer` ray→HUD-node hit test |
| **FO2-VR** | FlatOut2 / D3D9→D3D11 / OpenXR | **Architecture B** baked quad in eyes | RT-swap hooks (×3) → `g_uiSurf`; CPU readback to D3D11 | World or head (configurable) | straight `SRCALPHA/INVSRCALPHA`, no z | game menus |
| **OpenJK / OpenJKDF2** | reference ports | *(no VR — baseline for JKXR)* | — | — | — | — |

---

## 15. Closing: the canonical pipeline

Every successful mod, stripped to its essence, is the same five steps:

1. **Render the UI once, mono, into a transparent texture** (your dedicated UI RT) — by routing the engine's UI pass there (best), swapping the RT in a hook, or identifying & copying the UI render target.
2. **Make sure that texture has correct alpha and a matching format** to your submission target.
3. **Submit it as a separate compositor layer** (OpenXR quad / OpenVR overlay), composited *on top of* the stereo world layer.
4. **Position it** head-locked (HUD) or yaw-snapped world-locked (menus), 1.5–2.5 m out, aspect-correct, size by purpose.
5. **Keep it out of the world eyes** (redirect at source, or suppress UI draws in the eye pass) and **wire up pointing** (overlay mouse events, or ray→UV → inject the game's native cursor).

Get those five right and the HUD reads like a crisp floating panel instead of a smeared decal on your eyeballs. Everything else in this document is detail in service of those five steps.
