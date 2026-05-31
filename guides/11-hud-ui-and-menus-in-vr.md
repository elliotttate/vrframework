# 11 — HUD, UI & Menus in VR

## What this covers / why it matters

A game's HUD and menus are written for a flat monitor. They assume a single rectangular framebuffer, a single 2D projection, and a player whose eyes are glued to the screen. None of that survives contact with a headset. Once you stereo-render the world, the 2D UI is the part most likely to make a player physically sick: a health bar pinned to the screen edge sits *outside* the lens's sweet spot, a fullscreen inventory menu fills both eyes with a wall that has no depth, and a dialogue box drawn at native resolution gets cropped to a letterbox. This document is about making 2D UI *usable* in stereo — the techniques the three reference mods use to scale, reproject, and conditionally re-display engine UI, and the trade-offs between leaving UI in screen space versus pushing it into the world.

There are really only three jobs here, and every engine needs all three:

1. **Shrink and re-center the HUD** so it lands inside the comfortable central FOV instead of the screen corners.
2. **Detect when the engine is showing a flat menu** (inventory, map, dialogue) and change the camera/display behaviour for that mode — because a menu and a gameplay HUD want opposite things.
3. **Decide where the UI lives in 3D** — pasted onto the eye texture (screen space) or floating as a quad in the world (world space) — and give the player a way to point at it.

We will look at how Anvil (Assassin's Creed) reshapes the UI viewport, how Creation Engine (Starfield) classifies Scaleform movies to switch modes, and how REFramework builds a true world-space overlay you can laser-point at.

---

## 1. The core problem: the HUD is in the wrong place

On a monitor your eyes dart to the corners constantly — minimap top-right, health bottom-left, ammo bottom-right. In a headset the corners are in your *peripheral* vision, distorted by the lens and often outside the rendered FOV entirely. A HUD authored for a 16:9 screen, blasted unchanged onto a per-eye texture, puts critical information where the player literally cannot read it without rolling their eyes uncomfortably.

The cheap, universal fix is **viewport scaling**: render the same UI but squeeze its rectangle toward the center of the framebuffer. You are not re-authoring the UI; you are telling the engine "draw your UI into a smaller box in the middle." Done right, the whole HUD shrinks uniformly and pulls into the readable zone. Done wrong, you stretch the aspect ratio and everything looks like a funhouse mirror — which is why every implementation below scales width and height **independently** and re-centers by half the slack.

### Anvil: rewriting the UI viewport rectangle

Anvil exposes a function that calculates the UI viewport. The mod hooks it, lets the original run to populate the real viewport struct, then overwrites the rectangle in place:

```cpp
// anvilengine2vr/games/valhalla/engine/EngineCameraModule.cpp:194
void EngineCameraModule::onCalcUIViewportHook()
{
    static auto instance = Get();
    static auto vr       = VR::get();
    instance->m_onCalcUIViewportHook.call<void>();          // let the engine fill ui_vp first
    static auto ui_vp = reinterpret_cast<sdk::Viewport*>(memory::g_ui_viewport_addr());
    if (vr->is_hmd_active()) {
        float totalWidth  = ui_vp->left * 2.f + ui_vp->right;
        float totalHeight = ui_vp->top * 2.f + ui_vp->bottom;
        float scaleX      = ModSettings::g_ac_valhalla_settings.hudScaleX;
        float scaleY      = ModSettings::g_ac_valhalla_settings.hudScaleY;
        ui_vp->left       = (totalWidth - (totalWidth * scaleX)) / 2.f;
        ui_vp->right      = totalWidth * scaleX;
        ui_vp->top        = (totalHeight - (totalHeight * scaleY)) / 2.f;
        ui_vp->bottom     = totalHeight * scaleY;
    }
}
```

Read the arithmetic carefully, because the same shape recurs in every engine:

- `totalWidth = left*2 + right` reconstructs the *full* viewport extent from a partially-offset rectangle. (The engine's `left`/`top` here are margins, not absolute coordinates.)
- `right = totalWidth * scaleX` is the new width — a fraction of the full extent.
- `left = (totalWidth - totalWidth*scaleX) / 2` is the leftover slack split in half, so the shrunken box is **centered**.

Two separate knobs, `hudScaleX` and `hudScaleY` (`EngineCameraModule.cpp:203-204`), let the player dial the HUD box in until it sits inside their headset's comfortable FOV. This is the entire "HUD scale" feature: a hooked viewport calc plus a centered rescale.

There is a second, subtler viewport fix in the same file. When the world view is stereo-rendered, Anvil also has to fix up the **scissor rectangle** so the engine doesn't clip the eye image to the original mono rect:

```cpp
// EngineCameraModule.cpp:126
if (vr->is_hmd_active()) {
    float offX = out_scissorRect[0] * 2.0f;
    float offY = out_scissorRect[1] * 2.0f;
    out_scissorRect[2] += offX;
    out_scissorRect[3] += offY;
    out_scissorRect[0] = 0.0f;
    out_scissorRect[1] = 0.0f;
    ...
    out_viewport[2] = out_scissorRect[2];
    out_viewport[3] = out_scissorRect[3];
}
```

The lesson generalizes: **a flat engine has many places that hard-code "the viewport is the whole screen."** You will be playing whack-a-mole with viewport, scissor, and UI-viewport rectangles. Find each one, let the engine compute it, then rewrite it for stereo.

### Creation Engine: scaling the Scaleform movie viewport

Starfield's UI is **Scaleform / GFx** (Autodesk's Flash-in-a-game-engine; `.swf` and `.gfx` files). Each on-screen menu is a *movie* with its own viewport. The mod hooks the function that sets a movie's viewport and rescales it per-movie:

```cpp
// starfield2vr/src/CreationEngine/CreationEngineCameraManager.cpp:166
void CreationEngineCameraManager::onScaleformSetViewPortInternal(uintptr_t *thisMovie, Scaleform::Gfx::Viewport *viewport) {
    static auto vr = VR::get();
    auto cc = reinterpret_cast<RE::Scaleform::GFx::MovieImpl *>(thisMovie);
    auto file_url = cc->GetMovieDef()->GetFileURL();   // e.g. "Interface/HUDMenu.gfx"
    GameFlow::renderMenu(file_url);                     // classify it (see §2)

    auto backbuffer_size = vr->get_backbuffer_size();
    auto settings = GameFlow::getMenuSettings(file_url);

    if (ModSettings::showFlatScreenDisplay() || !vr->is_hmd_active()) {
        return;                                         // pancake mode: leave UI alone
    }

    auto width_multiplier  = settings.hud_scale;
    auto height_multiplier = settings.hud_scale;
    // per-eye horizontal offset for the dominant eye (see below)
    ...
    auto visible_width  = std::min((int)((float)backbuffer_size[0] * width_multiplier), viewport->bufferWidth);
    auto visible_height = std::min((int)((float)backbuffer_size[1] * height_multiplier), viewport->bufferHeight);
    viewport->width  = visible_width;
    viewport->height = visible_height;
    viewport->left   = (int)(viewport->bufferWidth  - visible_width)  / 2 + offset_left;
    viewport->top    = (int)(viewport->bufferHeight - visible_height) / 2 + offset_top;
}
```

Same center-the-shrunken-box math as Anvil — `(buffer - visible) / 2` is the half-slack — but with two extra ideas that Anvil doesn't have:

1. **The scale is *per-movie*, not global.** `getMenuSettings` looks the movie up in a table and can override the scale for specific menus (more in §2).
2. **There's a per-eye horizontal `offset_left`.** Because Starfield stereo-renders with AFR (left eye on even frames, right on odd), a HUD pasted at the same X in both eyes appears at infinity. Nudging it horizontally by a "perspective" amount on one eye gives the HUD apparent depth — it floats a comfortable arm's length away instead of being painted on infinity. The offset flips sign with the dominant-eye setting:

```cpp
// CreationEngineCameraManager.cpp:191
auto current_eye = vr->get_current_render_eye();
if (ModConstants::dominantEye == 1) {                 // right dominant
    offset_left = (current_eye == VRRuntime::Eye::RIGHT) ? -settings.perspective : 0;
} else {                                              // left dominant
    offset_left = (current_eye == VRRuntime::Eye::LEFT) ?  settings.perspective : 0;
}
```

This is "stereo HUD on the cheap": you fake parallax with a horizontal shift rather than truly projecting the UI into 3D. It works because the HUD is flat and always faces you; you only need *a* disparity, not a correct one.

### Comparison: viewport scaling across the three engines

| Concern | Anvil (`onCalcUIViewportHook`) | Creation Engine (`onScaleformSetViewPort`) | REFramework |
| --- | --- | --- | --- |
| What's hooked | Engine UI-viewport calc | Scaleform `MovieImpl::SetViewport` | The compositor submit (overlay) |
| Granularity | One global UI viewport | Per-movie (per `.swf`/`.gfx`) | One imgui overlay texture |
| Scale knobs | `hudScaleX`, `hudScaleY` (independent) | `hud_scale` (uniform, per-menu override) | Overlay width in meters |
| Stereo depth trick | none (screen-space) | per-eye horizontal `perspective` offset | true world-space quad |
| Centering math | `(total - total*scale)/2` | `(buffer - visible)/2` | n/a (world transform) |

---

## 2. Detecting "are we in a menu?" and switching modes

A HUD and a fullscreen menu want **opposite** camera behaviour:

- **Gameplay HUD** (health, ammo, compass): the world keeps rendering in stereo, your head still aims the camera, the HUD floats over the scene.
- **Fullscreen menu** (inventory, star map, dialogue, pause): the world is frozen or dimmed, head-tracking should *stop* driving the camera (you don't want the menu to swim as you look around), and the flat menu wants to be shown as a stable panel — a quad — rather than smeared across a stereo frame.

So the engine adapter needs a reliable signal: **is a real menu open right now?** This is genuinely hard because the engine has no single "menu open" boolean you can read; it just renders whatever movies/widgets are active this frame. Both Anvil and Starfield solve it by *watching the UI as it renders* and inferring state.

### Anvil: a single "is UI showing" pointer

Anvil is lucky — the engine keeps a global that's non-null while UI is up. The camera module just reads it:

```cpp
// EngineCameraModule.cpp:104
static auto g_current_ui = (uintptr_t**)memory::g_current_ui_pointer_addr();
const bool bIsShowingUI = *g_current_ui;
if (vr->is_hmd_active() && !bIsShowingUI) {
    // ...only inject the HMD pose into the view matrix when NO menu is showing
}
```

The consequence is in `onCalcFinalView` (`EngineCameraModule.cpp:106`): the HMD transform is composed into the view matrix **only when no UI is showing**. The moment a menu opens, head-tracking stops steering the world camera, so the menu doesn't drift while you read it. One pointer dereference gates the entire VR camera. When you have a signal this clean, take it.

### Creation Engine: classifying Scaleform movies by name

Starfield has no such global, so it *builds* one. Every frame, as each Scaleform movie sets its viewport, `renderMenu()` is called with the movie's file URL and classifies it by hashed name into two buckets — "this is a HUD overlay" vs. "this is a real menu":

```cpp
// starfield2vr/src/CreationEngine/models/GameFlow.cpp:25
void renderMenu(std::string_view menuNameHash) {
    switch (djb2Hash(menuNameHash.data())) {
    case "Interface/HUDMenu.gfx"_DJB:
    case "Interface/SpaceshipHudMenu.swf"_DJB:
    case "Interface/ScopeMenu.swf"_DJB:
    case "Interface/DialogueMenu.swf"_DJB:
        // HUD-like overlays: DECREMENT
        gState.uiData.rendered_menus_count[gState.uiData.modulino % 2]--;
        break;
    case "Interface/InventoryMenu.swf"_DJB:
    case "Interface/StarMapMenu.swf"_DJB:
    case "Interface/PauseMenu.swf"_DJB:
    case "Interface/MainMenu.swf"_DJB:
        // real fullscreen menus: INCREMENT
        gState.uiData.rendered_menus_count[gState.uiData.modulino % 2]++;
        break;
    default:
        break;
    }
    gStore.debugData.ui_parts.push_back(menuNameHash);
}
```

This is a clever little accumulator. For the current frame it keeps a running count: each frame, "menu-class" movies push the counter up and "HUD-class" movies push it down. If, on balance, more real menus rendered than HUD overlays, the net count is `>= 0` and we declare "a menu is showing":

```cpp
// GameFlow.cpp:102
bool isShowingMenu() {
    return gState.uiData.rendered_menus_count[(gState.uiData.modulino + 1) % 2] >= 0;
}
```

Two details that make this robust:

- **The `modulino` double-buffer.** `rendered_menus_count` is a two-element array indexed by `modulino % 2`. `resetGameState()` (`GameFlow.cpp:19`) bumps `modulino` and clears the *new* slot at the start of each frame, so `renderMenu` accumulates into the current frame's slot while `isShowingMenu` reads *last* frame's completed slot via `(modulino + 1) % 2`. This avoids reading a half-built count mid-frame — the same double-buffer discipline you see everywhere in AFR VR code, applied to UI state.
- **`resetGameState` runs at a known frame boundary.** It's called from the renderer module exactly when the engine's `CRBeginFrame` render graph starts, which is also where the menu flag is published:

```cpp
// starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:165
if (m_startFramePass == pGraph && before) {
    GameFlow::resetGameState();
    ModSettings::g_internalSettings.showQuadDisplay = GameFlow::isShowingMenu();
}
```

So the whole pipeline is: *watch every movie render → tally HUD vs. menu → publish a stable `isShowingMenu()` once per frame → flip `showQuadDisplay`.* That `showQuadDisplay` flag then drives a cascade of camera decisions elsewhere in the mod — when it's a menu, head-tracking is suppressed (`CreationEngineCameraManager.cpp:283`, `:324`) and the per-movie `perspective` offset is forced to zero so the menu sits flat and centered rather than offset for depth:

```cpp
// GameFlow.cpp:95
auto isMenu = gState.uiData.rendered_menus_count[(gState.uiData.modulino + 1) % 2] >= 0;
if (isMenu) {
    settings.perspective = 0;   // menus are flat panels, no per-eye parallax shift
}
```

### Per-menu overrides: a small data table

`getMenuSettings` also demonstrates the **Layer-3 (per-game data)** pattern from the mental model — a tiny table mapping specific menus to tuned scale/offset values:

```cpp
// GameFlow.cpp:12
const std::unordered_map<uint32_t, MenuSettings> menu_settings = {
    { "Interface/ScopeMenu.swf"_DJB,   { 1.0f, 400 } },   // scope: full-size, big offset
    { "Interface/MonocleMenu.swf"_DJB, { 0.6f, 100 } },   // monocle: shrunk
};
```

The scope overlay needs to be full-screen (you're literally looking through it), so its `hud_scale` is `1.0`; a scanning monocle wants to be smaller. Everything else falls back to the global HUD scale and perspective from settings. When you port to a new engine, expect to grow a table like this: a handful of special-case menus that don't behave like the default.

### Comparison: menu detection

| | Anvil | Creation Engine |
| --- | --- | --- |
| Signal source | Engine global pointer `g_current_ui` | Inferred from per-movie name hashing |
| Cost | one deref | a tally rebuilt every frame |
| Robustness | depends on the engine keeping that global accurate | depends on enumerating every menu name correctly |
| What it gates | HMD pose injection into view matrix | `showQuadDisplay`, head-track suppression, per-eye HUD offset |
| New menus | automatic | must be added to the `switch` (else mis-classified) |

The trade is clear: if your target engine exposes a "UI is up" state, read it. If it doesn't, you can reconstruct it by observing what renders — but you sign up for maintaining a name list, and any menu you forget gets the wrong treatment.

---

## 3. Engine-UI specifics: Scaleform / GFx

Two of these three engines render their UI with **Scaleform GFx**, so it's worth calling out what that means for a porter:

- **UI is movies, not draw calls you'll recognize.** Each menu is a compiled Flash movie (`.swf`) or GFx asset (`.gfx`) loaded by name. The integration point you want is wherever GFx hands the engine a viewport — `MovieImpl::SetViewport` in Starfield's case — because that's the one place you can reshape *every* UI element uniformly without touching ActionScript.
- **Names are your taxonomy.** `GetMovieDef()->GetFileURL()` gives you `"Interface/InventoryMenu.swf"`. Those stable string names are how you tell a HUD from a menu (§2). Hash them once (`djb2`) and switch on the hash for speed.
- **The cursor is its own movie.** Note `CursorMenu.swf` in Starfield's menu list. In flat games the hardware cursor is faked by a UI movie; in VR you typically want to suppress or relocate it, or replace it with a laser pointer (§4).
- **LRG variants.** Starfield ships `_LRG` duplicates of every menu (`HUDMenu_LRG.gfx`, etc.) for high-resolution/large-display modes. Your name table must cover both or half your menus will slip through unclassified — which is exactly why the `switch` in `renderMenu` lists every menu twice.

If your target engine uses a different UI stack (Coherent/UE UMG/custom retained-mode), the *principle* is identical: find the single chokepoint where a UI element is given its rectangle, and the single place where you can ask "which screen am I rendering?"

---

## 4. World-space UI: REFramework's pointer-driven overlay

Everything above keeps the engine's UI in **screen space** — pasted onto the eye texture, faked into depth with a horizontal nudge. The alternative is **world space**: render the UI to an off-screen texture and hang it in the world as a quad you can physically look at and point your controller at. REFramework does this for *its own* menu (the imgui config UI), and it's the cleanest reference for the technique.

It uses an OpenVR **dashboard overlay** — SteamVR draws the quad for you, composites it correctly in stereo, and gives you ray-intersection tests for free:

```cpp
// REFramework/src/mods/vr/OverlayComponent.cpp:14
vr::VROverlay()->CreateOverlay("REFramework", "REFramework", &m_overlay_handle);
vr::VROverlay()->ShowOverlay(m_overlay_handle);
vr::VROverlay()->SetOverlayWidthInMeters(m_overlay_handle, 0.25f);      // physical size
vr::VROverlay()->SetOverlayInputMethod(m_overlay_handle, vr::VROverlayInputMethod_Mouse);
```

The overlay is **attached to the left controller** so it travels with your hand like a wrist menu, with a tunable position/rotation offset:

```cpp
// OverlayComponent.cpp:204
const auto position_offset = vr->m_overlay_position;
const auto rotation_offset = vr->m_overlay_rotation;
left_controller_world_transform = vr->get_transform(controllers[0]) * Matrix4x4f{glm::quat{rotation_offset}};
left_controller_world_transform[3] -= glm::extractMatrixRotation(left_controller_world_transform) * position_offset;
...
vr::VROverlay()->SetOverlayTransformAbsolute(m_overlay_handle, vr::TrackingUniverseStanding, (vr::HmdMatrix34_t*)&steamvr_transform);
```

The genuinely instructive part is **input**. A world-space panel is useless if you can't interact with it, so the overlay translates a controller ray-hit back into imgui mouse coordinates. It fires an intersection test from the controller tip, and on a hit converts the overlay UV into a window-space pixel:

```cpp
// OverlayComponent.cpp:113 (mouse move, GL-space UV -> window pixel)
const auto mouse_point = ImVec2{
    raw_coords[0],
    (rendertarget_height - raw_coords[1])     // flip Y: GL origin is bottom-left
};
io.MousePos = mouse_point;
```

```cpp
// OverlayComponent.cpp:257 — controller tip ray
intersection_params.vSource.v[...]    = tip_world_transform[3][...];     // ray origin
intersection_params.vDirection.v[...] = -tip_world_transform[2][...];    // -Z forward
if (vr::VROverlay()->ComputeOverlayIntersection(m_overlay_handle, &intersection_params, &intersection_results)) {
    ...
    any_intersected = any_intersected && normal.z > 0.0f;               // hit the FRONT, not the back
}
```

Three design decisions here are worth stealing:

- **Both head and controller must aim at the panel** before it becomes interactive (`OverlayComponent.cpp:280` repeats the intersection test from the HMD pose). This prevents the menu from grabbing input while you're just glancing near it.
- **Front-face check.** `normal.z > 0.0f` rejects hits on the back of the quad — you can't click a menu by pointing at its rear.
- **Show a blank texture, don't hide.** When the menu shouldn't be visible it swaps in a blank render target rather than calling `HideOverlay`, because hiding would also disable the intersection tests it needs to detect when you point back at it (`OverlayComponent.cpp:362`).

### Screen-space vs. world-space: the trade-off

| | Screen-space (Anvil/Starfield HUD) | World-space quad (REFramework overlay) |
| --- | --- | --- |
| How | rescale engine's UI viewport, paste on eye texture | render UI to texture, place quad in 3D |
| Depth | faked via per-eye horizontal offset | real, correct stereo depth |
| Interaction | uses existing game input | needs ray-cast → mouse translation |
| Comfort | can swim/feel pasted-on; reads instantly | stable, physical; can be "too far" to read |
| Engine work | must find & reshape every UI viewport | UI must be renderable to an off-screen RT |
| Best for | the game's own busy HUD/menus | your *own* config UI, wrist menus, dialogs |

In practice you mix them. The pragmatic choice both game mods make: **leave the engine's complex HUD in screen space** (re-authoring hundreds of Scaleform widgets into 3D is a non-starter) and **reserve world-space quads for the mod's own UI and for fullscreen menus** that benefit from being a stable panel (`showQuadDisplay`).

---

## 5. Known limitations

These are the rough edges that survive in shipping mods — know them before you promise a player a perfect menu:

- **Letterboxed / cropped dialogs.** When a menu is authored to span the full 16:9 frame and you shrink the viewport to fit the headset FOV, anything anchored to the original screen edges gets cut off or letterboxed. Starfield's per-menu table (`MonocleMenu` at `0.6` scale) is partly a workaround for this — some menus simply can't be scaled uniformly without losing edge content.
- **Cursor placement.** Screen-space cursors (`CursorMenu.swf`) are positioned in flat-screen pixels; once the UI viewport moves, the cursor and the thing it's pointing at can disagree unless you reproject it too.
- **The per-eye offset is a fake.** The horizontal `perspective` nudge gives *a* depth, not a *correct* one; at the screen edges the left/right images don't truly converge, so large HUD elements can ghost. Forcing `perspective = 0` for real menus (§2) sidesteps this for the worst offenders by making them flat panels.
- **Mode-switch latency.** Because Starfield *infers* menu state from a one-frame-delayed tally (`(modulino+1)%2`), there's a frame of lag when opening/closing a menu. Usually invisible, occasionally a flicker as the HUD scale and head-tracking flip.
- **AFR temporal smear.** Anything UI-related that feeds temporal effects (TAA, DLSS history) suffers the same alternate-frame smearing the world does — see the `SwapBuffer`/`pastViewMatrix` double-buffering in the camera/renderer modules, and the dedicated TAA document.

---

## Key takeaways

- **Three jobs, every engine:** scale+center the HUD, detect menu vs. gameplay, and choose screen- vs. world-space. You will implement all three.
- **Viewport scaling is the same math everywhere:** reconstruct the full extent, multiply by a scale, and re-center with half the slack — width and height **independently** so you don't distort aspect ratio.
- **Find the single UI-viewport chokepoint.** Anvil hooks the engine's UI viewport calc; Starfield hooks Scaleform's `SetViewport`. One hook reshapes all UI uniformly.
- **A HUD and a menu want opposite camera behaviour.** Gate HMD pose injection on "is a menu open." Read an engine global if one exists (Anvil); otherwise reconstruct the signal by classifying what renders (Starfield's `renderMenu`/`isShowingMenu` tally, double-buffered by `modulino`).
- **Fake stereo HUD depth with a per-eye horizontal offset** under AFR — cheap and effective for flat, always-facing UI; zero it out for fullscreen menus so they stay flat.
- **Use a real world-space quad for your own UI** (REFramework's controller-attached overlay), and translate controller ray-hits back into mouse coordinates — flip Y, check the front face, require both head and hand to aim.
- **Maintain a per-game menu name table** (Layer 3): a handful of menus always need special scale/offset, and don't forget the `_LRG` variants.

---

**Next:** *12 — Input, Controllers & Interaction in VR* — mapping VR controllers and motion to the engine's input system, the laser-pointer/aim model, and how the HUD-mode detection from this document feeds input routing.
