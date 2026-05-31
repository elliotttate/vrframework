# 16 — Doing It Yourself: A Porting Checklist for a New Engine

**What this covers / why it matters.** Everything in this series so far has been *analysis* — how REFramework, starfield2vr and anvilengine2vr each solved a sub-problem. This is the *synthesis*: a concrete, ordered playbook for bringing up 6DOF VR on an engine you have never touched and have no source for. The organizing principle is **milestones that each produce a visible result**. You do not write the whole adapter and then debug a black screen; you inject, see an overlay, and only then move on. Each milestone below tells you *what to hook*, *how to verify it worked*, and *the failure modes that will actually bite you* — because on a new engine, 80% of the work is debugging the thing you cannot see. We anchor every step to real code in the three reference projects and to the `IEngineAdapter` SPI in this repo, so you always have a worked example to diff against.

The mental model never changes (see guide 01): **Layer 1** is the universal core (this repo, `vrframework`) — you do not rewrite it. **Layer 2** is your engine adapter — the `.cpp` hooks you are about to write. **Layer 3** is per-game data — offsets and per-title settings. This document is almost entirely about Layer 2, with Layer 3 appearing wherever a hook needs an address.

---

## The shape of the work, before you start

Read `vrframework/PORTING.md` once, end to end. It is the map from "scaffold stub" to "working core," and its second table — *New code (no REFramework equivalent)* — is the part you implement per engine:

| SPI file | What you implement | Reference instance |
|---|---|---|
| `include/spi/IEngineAdapter.hpp` | The four responsibilities, as one class | anvil `EngineEntry` + `EngineCameraModule` + `EngineRendererModule` |
| `include/spi/FrameTimeline.hpp` | Report your engine's frame events; timeline owns counters/drift | anvil two-hook; starfield Reflex markers |
| `include/spi/StereoView.hpp` | Write per-eye view+proj where the engine keeps its camera | anvil `onCalcFinalView` / `onCalcProjection` |
| `include/spi/EngineCaps.hpp` | Declare API, AFR-vs-sequential, TAA, basis | per-engine constants |

The four responsibilities are spelled out at the top of `vrframework/include/spi/IEngineAdapter.hpp:5`:

```cpp
//   1. install hooks           find + hook the engine's frame/camera/UI functions
//   2. frame pacing            report timeline events -> drives VR via FrameTimeline
//   3. stereo injection        write per-eye view+projection into engine memory
//   4. HUD / input reprojection
```

A new engine is *exactly* this: implement those four, plus ship an offsets manifest. The core never names an engine type. Keep that interface (`vrframework/include/spi/IEngineAdapter.hpp:26-58`) open in a second pane the whole time — every milestone below maps to one of its methods.

The milestone ordering is deliberate. Each step depends on the previous one being *verified*, not merely *written*. Resist the urge to skip ahead; a bug at milestone 5 is impossible to diagnose if milestone 2 was never actually confirmed.

---

## Milestone 1 — Inject and draw an ImGui overlay

**Goal:** your DLL runs inside the target process and draws an ImGui window over the game. Nothing VR yet. This proves injection, the d3d hook, and the overlay all work — the entire Layer-1 substrate.

**What to do.** Use the proxy-DLL injection pattern the whole series relies on: drop a `dxgi.dll` next to the game's exe. The OS loads it instead of the system one; its `DllMain` spawns a thread, and that thread constructs the framework:

```cpp
// the canonical entry, identical across all three projects
g_framework = std::make_unique<Framework>(hModule);
```

The framework (Layer 1) installs the D3D11/D3D12 present hook, sets up the ImGui context, and hooks window messages so the mouse works in the overlay. You write none of this — it is `vrframework` `Framework` + `hooks/D3D12Hook` + `hooks/WindowsMessageHook`, lifted verbatim per `PORTING.md` (rows 13–16: "None — already engine-agnostic," "Verbatim").

**How to verify.** Launch the game. You should see the framework's default ImGui menu. If your adapter is registered (milestone-0 housekeeping — see `Mods::Mods()` in `anvilengine2vr/games/valhalla/ModConfig.cpp:13-17`), its `on_draw_ui()` collapsing header appears too. That registration is three lines:

```cpp
Mods::Mods() {
    m_mods.emplace_back(VRConfig::get());
    m_mods.emplace_back(VR::get());
    m_mods.emplace_back(EngineEntry::Get());   // <- your adapter
}
```

**Failure modes.**
- *Nothing loads.* Wrong proxy DLL (the game may use `d3d11.dll`, `winmm.dll`, or be DXGI-via-d3d12); 32- vs 64-bit mismatch; or an anti-tamper/anti-cheat that blocks the proxy. Confirm with a DLL-load monitor that your module is even mapped.
- *Loads but no overlay.* The present hook attached to the wrong swapchain, or the game uses an API you did not hook. Add a log line in the present detour; if it never fires, your hook missed.
- *Overlay draws but input is dead.* The window-message hook is not installed or the game eats raw input. The overlay being *visible* but unclickable still counts as passing this milestone — fix input later.

This is the milestone where you confirm the foundation is solid. Do not proceed until the overlay is reliably there on every launch.

---

## Milestone 2 — Hook present, confirm device and queue

**Goal:** from inside the present hook, get a valid pointer to the graphics device and (D3D12) the command queue, and log their addresses. You need these to submit eye textures to the headset later.

**What to do.** The core's present hook already hands you the swapchain. From it, recover the `ID3D12Device` / `ID3D11Device` and, for D3D12, the `ID3D12CommandQueue` used for present. On D3D12 the queue is the awkward one — it is not directly reachable from the swapchain, so the core sniffs it from the `ExecuteCommandLists` / present path. Confirm `EngineCaps::Graphics` matches reality:

```cpp
// vrframework/include/spi/EngineCaps.hpp:18
Graphics graphics{ Graphics::D3D12 };
```

**How to verify.** Log the device and queue pointers once. They should be stable, non-null, and identical frame to frame. If you have RenderDoc or PIX, capturing a frame at this point confirms you are hooked into the real submission path.

**Failure modes.**
- *Null queue on D3D12.* The game creates multiple queues (async compute, copy). Make sure you captured the *direct* queue that feeds present, not a copy queue.
- *Device changes.* Device-reset or a second swapchain (some engines render the main view to one chain and UI to another). Pick the chain that present is actually called on.
- *Wrong API assumption.* If your overlay rendered via D3D11 but the game is D3D12 (or vice versa), you hooked an interop shim. Set `EngineCaps::graphics` to what the present hook actually saw.

---

## Milestone 3 — Read the engine's view and projection

**Goal:** find and hook the engine function that computes the camera's view and projection matrices, and *log them every frame*. This is the single most important reconnaissance step. You are reading first, not writing.

**What to hook.** Every engine has a function that, once per frame, produces the world→view and projection matrices for the main camera. In Anvil there are two, and the names tell the story (`anvilengine2vr/games/valhalla/engine/EngineCameraModule.cpp:11-49`):

```cpp
auto onCalcProjectionFN = memory::calc_projection_fn_addr();   // projection
auto calcFinalViewFn    = memory::update_views_fn_addr();      // world->view
```

How do you *find* them on a new engine? Three converging techniques:
1. **Pattern scan** for a function whose body builds a projection matrix — they have a recognizable shape (a `2/(r-l)`-style series of float stores). Anvil ships these as compiled offsets in `engine/memory/offsets.h` (Layer 3); starfield uses the CommonLibSF address library.
2. **Graphics debugger.** Capture a frame in RenderDoc/PIX, find the constant buffer holding the view-projection, then trace backward to who wrote it.
3. **Known-value scan.** The view matrix's translation row is the camera world position. Move in-game, scan memory for the changing XYZ, and you have found the matrix; backtrack to the function that fills it.

**How to verify.** Log the matrices. Strafe left — the view translation should move right (inverse). Look up — the third basis row should tilt. Cross-check the projection's FOV against the in-game FOV. If the numbers are stable and respond correctly to your movement, you have the right function. This is also where you learn the engine's **handedness and basis**, which you will encode once into `EngineCaps`:

```cpp
// vrframework/include/spi/EngineCaps.hpp:23-27
bool left_handed{ false };       // GLM_FORCE_LEFT_HANDED engine (Anvil = true)
glm::mat4 engine_to_vr_basis{ 1.0f };
```

Anvil's basis is the Y-up↔Z-up swap baked in `EngineCameraModule.cpp:74-77` as `Y_UP_TO_Z_UP_BASIS`. You will derive the equivalent for your engine here.

**Failure modes.**
- *Matrices are garbage.* You hooked a shadow-map or reflection camera, not the main view. There are usually several camera functions per frame; the main one runs once and matches the screen FOV.
- *Right shape, wrong frame timing.* You see last frame's matrix. Note whether the function runs before or after simulation — it matters for milestone 7.
- *Row-major vs column-major confusion.* If translation appears in the wrong place, you have a transpose mismatch between the engine and glm. Decide the convention now and write it down.

---

## Milestone 4 — Get HMD poses from the runtime

**Goal:** initialize the VR runtime (OpenVR or OpenXR) and log the live HMD pose and per-eye projection. Still no rendering into the headset — just prove the data flows.

**What to do.** This is pure Layer 1. The core's `VR` mod owns the runtime (`vrframework/include/vr/VRRuntime.hpp`, `runtimes/OpenVR` / `OpenXR`, all "verbatim" per `PORTING.md`). Your adapter does nothing here except let `VR::get()` initialize. Read back:
- the HMD transform (`vr->get_transform(0)`),
- the per-eye eye-to-head transform (`vr->get_current_eye_transform()`),
- the runtime's per-eye frustum (`vr->get_runtime()->frustums[...]`, used by Anvil's projection rebuild at `EngineCameraModule.cpp:58-71`).

**How to verify.** Put the headset on and move. The logged pose should track your physical head: translation in meters, rotation as expected. The per-eye projections should be asymmetric (VR frustums are off-center) — that asymmetry is *correct*, not a bug. If the runtime fails to start, the overlay should still work (you have not touched it), which is exactly why milestone 1 came first.

**Failure modes.**
- *Runtime won't init.* SteamVR/OpenXR runtime not running, or a loader DLL missing (`openvr_api.dll`, the OpenXR loader). The core logs this; check the log.
- *Pose is frozen.* You read it once instead of per frame, or you are reading before `WaitGetPoses`. Pose must be sampled inside the frame loop (milestone 7 wires the timing).
- *Units feel wrong.* Runtime poses are meters; the engine may be centimeters or arbitrary. You will reconcile world-scale during tweaks (milestone 10); for now just confirm the *direction* of motion is right.

---

## Milestone 5 — Inject one eye

**Goal:** override the engine's view+projection with the **left-eye** VR matrices, on every frame, and render that single eye to the flat monitor. The image should now look like it is coming from inside your head, panning as you move the HMD. Mono, but head-tracked.

**What to do.** In your view hook (milestone 3), instead of *reading* the matrix, *write* it. Compose the HMD pose into the engine view with the basis change and the eye transform. Anvil does exactly this in `onCalcFinalView` (`EngineCameraModule.cpp:106-121`):

```cpp
const auto eye            = vr->get_current_eye_transform();
const auto hmd_transform  = vr->get_transform(0);
const auto rotation_offset= vr->get_transform_offset();
const auto transform = Y_UP_TO_Z_UP_BASIS * rotation_offset * hmd_transform * eye * INV_UP_TO_Z_UP_BASIS;
*in_viewMatrix = *in_viewMatrix * transform;
```

And it replaces the projection with one built from the runtime's frustum (`EngineCameraModule.cpp:64-70`). In the SPI model this composition lives in the core, which hands you a finished `StereoView`; your `apply_stereo` (`IEngineAdapter.hpp:46`) just writes `view[eye]` / `projection[eye]` into engine memory. Either way, the math is the snippet above.

**How to verify.** On the flat screen you should see a head-tracked left-eye image. Turn your head — the view turns. The projection should fill the frame with the VR FOV (wider than the game's default). It will look slightly off-center on a monitor (that is the asymmetric eye frustum); that is fine.

**Failure modes.**
- *World swims / inverts when you turn.* Basis or handedness wrong. This is the #1 bug here. Flip `left_handed`, transpose, or invert the basis until head motion matches. Bisect: zero out rotation and confirm translation alone is correct first.
- *Camera locked to nothing / black.* You overwrote the matrix unconditionally, including during menus or cutscenes. Anvil gates on `vr->is_hmd_active() && !bIsShowingUI` (`EngineCameraModule.cpp:106`). Gate yours too.
- *Stutter or double-vision feel.* You are writing the matrix but the engine recomputes it after your hook. Make sure you hook the *last* function before the matrix is consumed (Anvil's `onCalcFinalView` is named for exactly this).

---

## Milestone 6 — AFR both eyes

**Goal:** render the left eye on even engine frames and the right eye on odd frames (Alternate-Frame Rendering), submit each to the corresponding headset eye, and you have stereo 3D in the HMD.

**What to do.** The cadence is owned by `FrameTimeline` (`vrframework/include/spi/FrameTimeline.hpp:50-51`):

```cpp
bool is_left_eye_frame() const { return (m_presenter_frame % 2) == 0; }
```

Each frame, ask which eye it is, fetch *that* eye's pose/projection, write it (milestone 5's code, now eye-aware via `get_current_eye_transform()`), and submit the resulting render target to the matching headset eye. `EngineCaps::Submission::AFR` (`EngineCaps.hpp:15`) tells the core to drive this cadence. Why AFR and not render-both-in-one-frame? Because you do not control the engine's renderer — you cannot make it draw the scene twice per frame. AFR is the technique that fits a closed engine: you let it render once, you just change *which eye's camera* it renders from, frame by frame.

**How to verify.** In the headset, the world should have correct stereo depth — near objects pop, your IPD feels right. Cross one eye closed, then the other: each should show its own viewpoint. Effective framerate per eye is half the engine's frame rate, so target a high engine FPS.

**Failure modes.**
- *Eyes swapped.* `presenter_frame % 2` maps to the wrong eye. Flip the parity.
- *One eye stale / juddery.* AFR doubles the latency budget and exposes any per-eye state the engine caches across frames — which is the entire next milestone.
- *Vergence wrong (eyes don't fuse).* Eye-to-head transform applied with wrong sign, or IPD/world-scale mismatch. Confirm `get_current_eye_transform()` returns left for even, right for odd.

This is the milestone where the **TAA / temporal history artifact** appears, because alternating eyes makes every temporal effect reproject the *other eye's* history onto this eye — smearing. The fix is to double-buffer the past-frame state per eye. Anvil keeps a two-slot ping-pong of the previous view matrix and swaps it each frame (`EngineCameraModule.cpp:145-156`):

```cpp
copies[frame % 2]       = context->pastViewMatrix;
context->pastViewMatrix = copies[(frame - 1) % 2];
```

That is the `onCopyGfxContext` hook; its whole job is to feed each eye *its own* previous-frame matrix so TAA/Nvidia history reprojects correctly. Declare `EngineCaps::has_taa{ true }` (`EngineCaps.hpp:21`) so the core knows this fix-up is required. If you skip this, the image is *stereo but smeared* — a strong signal you are at exactly this step.

---

## Milestone 7 — Fix timing

**Goal:** the engine, render, and present counters stay aligned, the HMD pose is sampled at the right instant, and the headset stops dropping/duplicating frames. This is the hardest sub-problem in the whole port.

**What to do.** Map your engine's real frame events onto the five logical `FrameTimeline::Event`s (`vrframework/include/spi/FrameTimeline.hpp:21-27`): `ENGINE_FRAME_BEGIN`, `WAIT_RENDER`, `RENDER_FRAME_BEGIN`, `PRESENT_BEGIN`, `PRESENT_END`. The timeline owns the three counters and the drift-recovery "skip a present" logic; you only call `report(...)` from your hooks.

There are two proven ways to source those events, and your engine will resemble one:
- **Two-hook style (Anvil).** Hook "begin engine frame" and "begin render frame." Anvil's `on_begin_engine_frame` bumps the engine count and runs ImGui; `on_begin_render_frame` advances the render count and calls the three VR entry points (`anvilengine2vr/games/valhalla/engine/EngineRendererModule.cpp:24-51`):

```cpp
vr->m_render_frame_count = vr->m_engine_frame_count;
vr->on_begin_rendering(vr->m_render_frame_count);
vr->update_hmd_state(vr->m_engine_frame_count);
const auto result = instance->m_beginRenderFrameHook.call<uintptr_t>(context);
vr->m_presenter_frame_count = vr->m_render_frame_count;
```

- **Reflex-marker style (Starfield).** Creation Engine 2 already emits Reflex markers; the adapter decodes `setReflexMarkerInternal` (markers 0/1/2/4/6) and reports the same events. Same state machine, different signal source.

Either way the timeline calls back into the same three VR functions — that mapping is wired in `FrameTimeline::Callbacks` (`FrameTimeline.hpp:29-34`): `on_wait_rendering`, `on_begin_rendering`, `on_update_hmd_state`.

**How to verify.** The headset's compositor stats should show clean frames — no reprojection spikes, no missed v-syncs. Pose latency should feel tight (turn head, world tracks immediately). Watch the three counters: they should advance together, and `wants_skip_present()` should fire only occasionally to re-align, then clear.

**Failure modes.**
- *Counters drift apart.* You reported an event from the wrong hook, or the engine occasionally skips a frame (loading) and your counts diverge permanently. The timeline's skip-present recovery (`FrameTimeline.hpp:46-48`) exists for this; make sure you actually consult and clear it.
- *Pose sampled too early/late.* `update_hmd_state` must run close to render submission, not at simulation start, or you get visible lag. Anvil calls it in the render hook, not the engine hook.
- *Wrong eye gets the wrong frame's pose.* AFR cadence and the counter that drives it (`presenter_frame`) got out of phase. The eye-to-frame mapping in milestone 6 and the counters here must agree.

Be honest with yourself: this milestone will take longer than all the others combined. It is normal to revisit it repeatedly.

---

## Milestone 8 — HUD

**Goal:** the game's 2D HUD/UI renders at a comfortable size and depth in VR, instead of being stretched across a 200° field where you cannot read the corners.

**What to do.** Two problems: the UI's *viewport/scissor* and its *scale*. Anvil expands the scissor/viewport to the full eye render target in `onCalcFinalView` (`EngineCameraModule.cpp:126-140`) so the UI is not clipped, and then rescales the UI viewport with user-tunable factors in `onCalcUIViewportHook` (`EngineCameraModule.cpp:194-210`):

```cpp
float scaleX = ModSettings::g_ac_valhalla_settings.hudScaleX;
float scaleY = ModSettings::g_ac_valhalla_settings.hudScaleY;
ui_vp->left  = (totalWidth  - (totalWidth  * scaleX)) / 2.f;
ui_vp->right =  totalWidth  * scaleX;
ui_vp->top   = (totalHeight - (totalHeight * scaleY)) / 2.f;
ui_vp->bottom=  totalHeight * scaleY;
```

In the SPI this is `IEngineAdapter::reproject_hud(scale_x, scale_y)` (`IEngineAdapter.hpp:52`). Expose the scale as `ModValue` sliders so the user can dial it in — Anvil wires `m_hud_scaleX`/`m_hud_scaleY` straight to per-game settings (`EngineEntry.cpp:65-71`, `EngineEntry.cpp:100-105`).

**How to verify.** The HUD sits centered, readable, not clipped, and the sliders move it live. Also confirm you correctly *suppressed* the stereo camera override while the UI is up (the `!bIsShowingUI` gate from milestone 5) so menus do not nauseate.

**Failure modes.**
- *HUD clipped at edges.* Scissor not expanded — apply the `EngineCameraModule.cpp:126-140` viewport widening.
- *HUD only on one eye.* It is drawn once per engine frame but you are in AFR, so it lands on whichever eye that frame is. For readability this is often acceptable; for a true world-locked HUD you must redraw per eye, which most ports skip.
- *Menus cause sickness.* You left the head-tracked camera override on during UI. Gate it off.

---

## Milestone 9 — Input

**Goal:** VR controllers (or at least a clean gamepad mapping) drive the game, and the user can summon the overlay without a keyboard.

**What to do.** Two layers. First, **overlay/control input**: Anvil's `EngineEntry::on_pre_imgui_frame` (`EngineEntry.cpp:16-52`) toggles flat-screen mode on Start+Back and opens the menu on a one-second Start long-press — a pattern worth copying because it needs no keyboard. Second, **engine input remap**: the SPI exposes `IEngineAdapter::on_engine_input()` (`IEngineAdapter.hpp:53`) for routing controller state into the engine's input. Note that the XInput seam is already provided by the core; Anvil's `VR::on_xinput_get_state` is even a deliberate no-op (`ModConfig.cpp:7`) — the engine reads the real gamepad, and you augment as needed.

**How to verify.** Toggle the overlay with the controller only. Confirm gameplay inputs reach the game. If you mapped motion controllers, aim should follow the controller, not the head.

**Failure modes.**
- *Inputs double-fire or get eaten.* Your hook returns the modified state but the engine also reads raw input elsewhere. Find the single chokepoint.
- *Head-aim vs controller-aim confusion.* Decide which drives aiming; Anvil's `onCameraGetForwardHook` (`EngineCameraModule.cpp:175-192`) shows how the *forward* vector is recomposed from HMD rotation for head-aim — controller-aim would override this differently.

---

## Milestone 10 — Tweaks

**Goal:** the comfort and fidelity passes that turn "technically VR" into "playable VR."

These are largely `ModValue`-backed toggles surfaced in `on_draw_ui` (`EngineEntry.cpp:55-83`), each calling `update(true)` to push into per-game settings:

- **Decoupled pitch.** Let the headset control pitch independently of the game camera so looking up/down does not fight the engine's auto-pitch. Anvil's `removePitchFromZUpMatrix` (`EngineCameraModule.cpp:81-88`), gated by `decoupledPitch`, strips the engine pitch before composing the HMD pose.
- **World scale / IPD.** Reconcile engine units with real meters until depth feels life-sized.
- **Disable incompatible effects.** Vignette, depth-of-field, motion blur, letterboxing — anything that assumes a flat 2D frame fights VR. The SPI reserves `disable_incompatible_effects()` (`IEngineAdapter.hpp:57`) for this; call it early.
- **Per-title config.** Persist everything through `on_config_load`/`on_config_save` (`EngineEntry.cpp:85-98`) so each game remembers its tuning (Layer 3).

**How to verify.** Play for ten minutes without discomfort. Each toggle should visibly change the experience and survive a restart.

---

## The whole port, on one page

| # | Milestone | Hook / SPI method | Visible result | Worst failure mode |
|---|---|---|---|---|
| 1 | Inject + overlay | proxy DLL → `Framework` | ImGui menu over game | DLL never loads |
| 2 | Present, device/queue | core present hook | logged stable device+queue | null D3D12 queue |
| 3 | Read view/proj | camera calc fn (`calc_projection`, `update_views`) | matrices track movement | hooked shadow camera |
| 4 | HMD poses | `VR` runtime (Layer 1) | pose tracks head | runtime won't init |
| 5 | Inject one eye | view hook / `apply_stereo` | head-tracked mono | world swims (basis) |
| 6 | AFR both eyes | `FrameTimeline::is_left_eye_frame` | stereo depth in HMD | TAA smear |
| 7 | Fix timing | `FrameTimeline::report` + callbacks | clean compositor frames | counters drift |
| 8 | HUD | `reproject_hud` / UI viewport hook | readable centered UI | clipped / nausea |
| 9 | Input | `on_pre_imgui_frame` / `on_engine_input` | controller drives game+overlay | inputs eaten |
| 10 | Tweaks | `ModValue` toggles, config | comfortable, persistent | — |

---

## What is irreducibly hard

Some of this gets easier with practice; some never does. Be realistic.

- **Finding the right functions on a stripped binary.** Pattern scans break on every game update; the offsets manifest (Layer 3) is permanent maintenance. There is no shortcut — a graphics debugger plus patience is the job.
- **Frame timing (milestone 7).** This is the genuinely hard core. The three reference engines each needed a *different* signal (two hooks vs Reflex markers) and still share the same drift-recovery state machine because getting it exactly right is subtle. Expect to spend most of your time here, and expect regressions when the game updates.
- **Temporal effects under AFR (milestone 6).** Every temporal feature — TAA, NVIDIA history, motion vectors, upscalers — assumes frame N+1 continues frame N's eye. AFR violates that. Each one needs its own per-eye double-buffer (the `pastViewMatrix` ping-pong). New engines bring new temporal features and thus new variants of this same bug.
- **The things you cannot move.** A closed engine renders the scene *once* per frame; you cannot make it draw stereo natively, so AFR's halved per-eye framerate is structural. Effects baked deep into the renderer may be untouchable. You are negotiating with a black box, not commanding it.

The encouraging half: milestones 1–4 are largely *mechanical* once you have done them once, and the entire Layer-1 core (`vrframework`) is reused unchanged. The real engineering — and the real reward — is milestones 5–7, where your adapter teaches a game that never imagined VR to render from inside the player's head.

---

## Key takeaways

- **Work in verifiable milestones.** Inject → present → read camera → poses → one eye → AFR → timing → HUD → input → tweaks. Never advance on an unverified step; a milestone-5 bug is undebuggable if milestone-2 was never confirmed.
- **A new engine is four responsibilities.** `install_hooks`, `timeline()`, `apply_stereo`, and HUD/input — exactly `IEngineAdapter`. The core never names your engine; you never rewrite the core.
- **Read before you write.** Milestone 3 (logging the engine's matrices) is the cheapest insurance against the basis/handedness bugs that dominate milestone 5.
- **Budget for timing and temporal artifacts.** Frame pacing and per-eye history double-buffering are where the real time goes. Both are engine-agnostic in shape (`FrameTimeline`, the `pastViewMatrix` ping-pong) and engine-specific in signal.
- **Lean on the worked examples.** Anvil's `EngineCameraModule`/`EngineRendererModule` and Starfield's Reflex-marker adapter are two concrete instances of every step above — diff your adapter against them when stuck.

**Next:** *17 — Lessons, Trade-offs, and What We'd Do Differently* — a retrospective on the three ports, where the abstractions paid off, where they leaked, and what the next engine adapter should inherit.
