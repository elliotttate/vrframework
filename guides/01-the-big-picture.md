# 01 — The Big Picture: Anatomy of an Engine VR Mod

**What this covers / why it matters.** This is the map you keep open while reading every other document in the series. It answers the question the rest of the guides assume you already understand: *what does it actually mean to "add VR" to a game whose source code you will never see?* The short version is that you are not writing a VR game — you are surgically inserting yourself into a frame loop that is already running, lying to the engine about where the camera is and how big the screen is, and stealing its rendered output before it reaches the monitor so you can hand it to a headset instead. That is a fundamentally different discipline from normal game programming, and it has its own three-layer shape, its own per-frame lifecycle, and its own vocabulary. We'll build all three here, ground every claim in the three real reference mods, and finish with a recommended reading order so you can go deep on whichever sub-problem is biting you. If you only read one document before touching code, read this one.

---

## 1. What "VR-ifying a closed-source engine" really means

A normal VR title is built for VR from the ground up: the renderer knows it has two eyes, the camera rig is a head, input is two tracked controllers, and the engine submits two images to a runtime every frame on purpose. You have none of that. You have:

- A shipping `.exe` you cannot recompile.
- A renderer that draws **one** image, for **one** flat camera, to **one** swapchain.
- No symbols, no headers, no documentation — just machine code and whatever you can reverse-engineer.

"VR-ifying" it means bending that mono pipeline into a stereo one *from the outside*, by intercepting the handful of functions that matter and feeding them altered data. Concretely, you must solve four problems, and almost every line of code in all three reference projects exists to solve one of them:

| # | Problem | What you do about it | Where it lives |
|---|---|---|---|
| 1 | **Get inside the process** | Trick Windows into loading your DLL into the game, then hook DirectX so you can see every frame. | Layer 1 (core) |
| 2 | **Render two eyes from one renderer** | Alternate-Frame Rendering (AFR): make even frames the left eye, odd frames the right, and inject a different camera each time. | Layer 1 + Layer 2 |
| 3 | **Get the timing right** | Synchronise the engine's frame pacing with the headset's "when do you want the next pose / image" cadence. This is the hardest part. | Layer 2 (engine adapter) |
| 4 | **Feed the headset the result** | Grab the engine's rendered texture, copy it per eye, and submit it to OpenVR/OpenXR with the correct projection. | Layer 1 |

Everything else — motion controllers, HUD reprojection, comfort options — is icing on those four.

The key mental shift: **you are a parasite on someone else's frame loop, not the author of a new one.** You react to the engine's events; you do not drive them. The engine ticks when it wants to; your job is to be hooked into the right places so that when it does, you can hijack the camera, capture the image, and submit to the headset before it notices.

---

## 2. The three reference projects (and why there are three)

This series studies three real, shipping mods that solve the same four problems on three unrelated engines. Studying *three* is deliberate: one example teaches you a recipe, three examples teach you the *principle* underneath the recipe, because you can see what changed and what stayed the same.

| Project | Author | Engine | Games | Graphics API | VR runtime | Frame-timing source |
|---|---|---|---|---|---|---|
| **REFramework** | praydog | Capcom RE Engine | Resident Evil 2–Requiem, Monster Hunter, DMC5, SF6, DD2 | D3D11 **and** D3D12 | OpenVR **and** OpenXR | Engine reflection + camera-controller hooks |
| **starfield2vr** | mutars | Bethesda Creation Engine 2 | Starfield | D3D12 only | OpenXR | **Reflex markers** (`setReflexMarkerInternal`, markers 0/1/2/4/6) |
| **anvilengine2vr** | mutars | Ubisoft AnvilNext 2.0 | AC Odyssey / Valhalla / Mirage | D3D12 | OpenXR | **Two frame hooks** (`on_begin_engine_frame`, `on_begin_render_frame`) |

**REFramework is the origin.** It is the oldest, largest, and most feature-rich of the three — it has a Lua scripting layer, a plugin API, a full reflection-driven SDK for RE Engine's type system, and it supports both DirectX versions and both VR runtimes (`E:/Github/REFramework/README.md:23-37`). Because RE Engine exposes a reflection/type system, REFramework can *ask the engine* about its objects by name; the other two engines give you no such gift, so the ports lean entirely on raw pattern-scanning and reverse-engineered struct layouts.

**starfield2vr and anvilengine2vr are forks of REFramework.** Both were lifted from praydog's codebase by mutars (`E:/Github/anvilengine2vr/README.md:76` — "This project is based on REFramework by praydog"). In doing so they proved something important: REFramework's reusable injection/UI/VR machinery is *separable* from its RE-Engine-specific code. That separated core is the private `vrframework` submodule the two ports share.

**vrframework is that core, reconstructed.** This repo (`E:/Github/vrframework`) is a scaffolded reconstruction of the shared core, with the RE-Engine types stripped out and the per-engine contract made explicit as a formal SPI (`IEngineAdapter`, `FrameTimeline`, `StereoView`, `EngineCaps`). Its `README.md` and `PORTING.md` describe the intended architecture and map each file back to the REFramework source it was lifted from. When this series says "the core," it means vrframework; when it says "the adapter," it means the per-engine layer that the ports duplicate and that vrframework's SPI exists to collapse.

> **How to read the three together.** For any sub-problem, ask: *what does REFramework do (the mature original)? How did the Creation-Engine port do it differently (Reflex markers)? How did the Anvil port do it differently (two hooks)?* The differences are never arbitrary — they fall out of what each engine exposes. That triangulation is how you'll figure out a fourth engine you've never seen.

---

## 3. The three-layer mental model

Hold this diagram in your head for the entire series. Every file in every project lands in exactly one of these layers, and the whole point of the architecture is that **higher layers know nothing about engines and lower layers know nothing about VR.**

```
LAYER 3   PER-GAME DATA        offsets / patterns + per-title settings + reclass structs
          (most volatile)      anvil: games/<Title>/   starfield: offsets.h
                               "WHERE in THIS build is the camera struct?"
──────────────────────────────────────────────────────────────────────────────────
LAYER 2   ENGINE ADAPTER       per-engine hooks: frame pacing, projection/view
          (per engine)         injection, HUD, input
                               anvil: EngineCameraModule / EngineRendererModule
                               starfield: CreationEngine* classes
                               "HOW does THIS engine render / set its camera?"
──────────────────────────────────────────────────────────────────────────────────
LAYER 1   UNIVERSAL CORE       injection, d3d hooks, imgui overlay, Mod system,
          (== vrframework)     VR runtime, frame timeline
          (most stable)        "Generic VR machinery that doesn't know what game it's in."
```

### Layer 1 — Universal Core (`vrframework`)

The reusable part. Injection, the DirectX hooks (`D3D11Hook`, `D3D12Hook`), the ImGui debug overlay, the `Mod` + config system, the VR runtimes (OpenVR/OpenXR), pose/projection math, and the frame-timeline state machine. **It contains zero engine types.** Per the vrframework README, this layer is `Framework`/`g_framework`, `Mod` + the `ModValue` widget family, `hooks/`, `vr/` + `mods/VR`, the new `spi/`, and `utility/`+`memory/` (`E:/Github/vrframework/README.md:29-36`). If you swap engines, none of this changes.

### Layer 2 — Engine Adapter

The per-engine glue. This is where you hook *this specific engine's* frame functions, write per-eye matrices into *this engine's* camera memory, reproject *this engine's* HUD, and remap input. In Anvil this is the `EngineCameraModule` / `EngineRendererModule` classes; in Creation Engine 2 it's the `CreationEngine*` classes. vrframework formalises this layer as a single interface, `IEngineAdapter`, with exactly four responsibilities (`E:/Github/vrframework/include/spi/IEngineAdapter.hpp:6-15`):

> 1. install hooks — find + hook the engine's frame/camera/UI functions
> 2. frame pacing — report timeline events → drives VR via FrameTimeline
> 3. stereo injection — write per-eye view+projection into engine memory
> 4. HUD / input reprojection

REFramework expressed these as "a tangle of RE-typed virtuals on the Mod base"; the universalization collapses them into one explicit contract (`IEngineAdapter.hpp:14-15`). A new engine = implement this interface + ship an offsets manifest, and the core never names an engine type.

### Layer 3 — Per-Game Data

The most volatile layer, and the only one that has to change for every patch the game ships. It's pure *data*: the byte offsets and AOB patterns that locate the camera struct, the projection matrix, the frame counter; the per-title comfort settings; and the reverse-engineered ("reclass") struct layouts. In Anvil this lives under `games/<Title>/`; in starfield it's `offsets.h`. When the game updates and your mod breaks, **90% of the time the fix is here** — a moved offset, not a logic change. Keeping it as data (rather than compiled constants) is exactly why vrframework wants an "offsets manifest" instead of a baked `offsets.h` (`E:/Github/vrframework/PORTING.md:12`).

**Why the split pays off:** when AC Valhalla patches, you re-scan Layer 3 and ship. When you port to a *new* engine, you write a new Layer 2 adapter and leave Layer 1 untouched. The blast radius of any change is contained to one layer.

---

## 4. The life of one VR frame

This is the heartbeat of the whole system. Follow one frame from the moment the engine wakes up to the moment a stereo image lands in the headset. The names in parentheses are the real callbacks; you'll meet each in depth in later docs.

```
                         ┌──────────────────────────────────────────┐
   ENGINE TICK  ───────▶ │ adapter hook fires (engine frame begins)  │
   (you don't control)   │  → timeline.report(ENGINE_FRAME_BEGIN)    │
                         └──────────────────────────────────────────┘
                                          │
                                          ▼
   ┌───────────────────────────────────────────────────────────────────────┐
   │ Layer 2 ADAPTER translates engine events → universal VR calls          │
   │                                                                         │
   │   vr->on_wait_rendering(...)   "I'm about to wait on the GPU"           │
   │   vr->on_begin_rendering(...)  "I'm starting to record GPU work"        │
   │   vr->update_hmd_state(...)    "give me the freshest head pose"         │
   └───────────────────────────────────────────────────────────────────────┘
                                          │
                                          ▼
   ┌───────────────────────────────────────────────────────────────────────┐
   │ Which eye is this frame?  (AFR)                                         │
   │   presenter_frame even  → LEFT eye                                      │
   │   presenter_frame odd   → RIGHT eye                                     │
   │                                                                         │
   │ Compose camera:  engine view  ⊕  HMD pose  ⊕  eye-to-head offset        │
   │                  (with Y_UP_TO_Z_UP_BASIS change)                       │
   │ Inject per-eye  view + projection  into engine camera memory           │
   │   (Layer 2: adapter->apply_stereo(StereoView))                         │
   └───────────────────────────────────────────────────────────────────────┘
                                          │
                              ENGINE RENDERS THE SCENE
                          (now drawing from YOUR camera, one eye)
                                          │
                                          ▼
   ┌───────────────────────────────────────────────────────────────────────┐
   │ PRESENT HOOK fires (Layer 1: D3D11Hook / D3D12Hook)                     │
   │   • copy this eye's rendered texture into the VR runtime's swapchain    │
   │   • (every other frame) submit BOTH eyes to the headset                 │
   │   • draw the ImGui overlay                                              │
   │   • optionally SKIP the real present so the flat monitor isn't garbage  │
   └───────────────────────────────────────────────────────────────────────┘
                                          │
                                          ▼
                              HEADSET DISPLAYS STEREO
```

A few things in that loop deserve emphasis right now, because they recur in every later doc:

**Stereo is alternate-frame rendering (AFR).** The engine only knows how to render one camera, so you don't render both eyes per frame — you render one eye *per engine frame* and alternate. Even engine frame = left eye, odd = right eye. The `FrameTimeline` encodes this directly: `is_left_eye_frame()` returns `(m_presenter_frame % 2) == 0` (`E:/Github/vrframework/include/spi/FrameTimeline.hpp:51`). Every other present submits the completed left+right pair to the runtime. This is cheap and engine-agnostic, but it has a sharp cost: it doubles your eye-to-photon latency and it wrecks any rendering technique that assumes "last frame was the same camera." (See §6's TAA note and doc 09.)

**Each frame you inject a camera, not just read one.** You compose the HMD's tracked head pose into the engine's own view matrix using a basis change (`Y_UP_TO_Z_UP_BASIS`) and a per-eye eye-to-head transform, then *write that back* into the engine's camera before it renders. The engine thinks it's rendering its normal third-/first-person camera; it's actually rendering from your eye. This is what `apply_stereo(const StereoView&)` does in the SPI (`IEngineAdapter.hpp:46`).

**Timing is the hard part, and it's a three-counter problem.** The engine has its own pacing; the headset has its own "when do you want a pose / when will you submit" expectations. Bridging them requires tracking three counters — **engine**, **render**, and **presenter** frame numbers — and detecting when they drift apart so you can drop a present and re-align the left/right cadence. The `FrameTimeline` owns exactly these three counters plus a `wants_skip_present()` flag (`FrameTimeline.hpp:42-48`). The two ports feed it from different sources (Reflex markers vs. two hooks), which is the whole subject of doc 06.

---

## 5. Where the code starts: injection in 30 seconds

So you understand the lifecycle, here's the literal entry point, identical in spirit across all three. You drop a **proxy DLL** (a fake `dxgi.dll` or `dinput8.dll`) next to the game exe. Windows loads *yours*, your `DllMain` fires, you spawn a thread, and that thread constructs the framework singleton.

The two mutars ports are almost byte-identical here (`E:/Github/starfield2vr/src/Main.cpp:35-51` and `E:/Github/anvilengine2vr/src/Main.cpp:35-51`):

```cpp
void InitThread(HINSTANCE hModule) {
    Sleep(5000);                                  // let the engine finish booting
    g_framework = std::make_unique<Framework>(hModule);
    // ...
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH)
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitThread, NULL, 0, NULL);
    return TRUE;
}
```

REFramework does the *same shape* but with far more defensive scaffolding, because RE Engine games ship anti-tamper: it forwards the real `dinput8.dll` exports (`E:/Github/REFramework/src/Main.cpp:32-81`), pins a `safetyhook` allocator near the middle of the game module so its trampolines land in range (`Main.cpp:128-138`), initialises a game-identity probe, installs integrity-check bypasses, and only *then* spawns the startup thread that builds `g_framework` (`Main.cpp:99`, `Main.cpp:143-149`).

Two recurring details worth noticing now:

- **Why a thread, why the `Sleep`?** `DllMain` runs under the Windows loader lock — you must not do real work there or you risk deadlock. So all three immediately hand off to a fresh thread. The crude `Sleep(5000)` in the ports just waits for the engine to be alive enough to hook; REFramework instead waits on richer signals. Doc 02 dissects this.
- **`GLM_FORCE_LEFT_HANDED`.** Both ports run a startup self-test verifying the handedness of their math library (`starfield2vr/src/Main.cpp:5-20`). Coordinate-system handedness (left vs. right, Y-up vs. Z-up) is a perennial source of "the world is mirrored / upside down" bugs in VR, which is why they assert it at boot. Doc 07 (camera/matrices) lives and dies on this.

That's the whole bootstrap. From `g_framework = make_unique<Framework>(...)` onward, you're in Layer 1, hooking DirectX and waiting for the first present.

---

## 6. The one artifact that will haunt you: TAA / temporal smear

Worth flagging in the overview because it surprises everyone the first time. Modern engines lean heavily on **temporal** techniques — TAA (temporal anti-aliasing), temporal upscalers (DLSS/FSR), screen-space reflections with history reuse. They all work by blending *this* frame with the *previous* frame, assuming the camera moved only slightly.

Under AFR that assumption is catastrophically false: your previous frame was the **other eye**, half an IPD away. So the temporal accumulator blends the left eye into the right and back, producing a smeared, ghosting, double-vision mess that's worse the more the engine relies on history.

The fix is conceptually simple and recurs across all three projects: **double-buffer the temporal history per eye.** Keep the left eye's past-frame resources separate from the right eye's, and swap which set the engine sees based on the current eye. In the codebases this shows up as `pastProjections`, `m_pastBuffer`, and `SwapBuffer`-style machinery. Doc 09 is entirely about this. For now just internalise the principle: **AFR breaks anything that remembers the last frame, and the cure is to give each eye its own memory of the past.**

---

## 7. Glossary — every term the rest of the series uses

| Term | Meaning |
|---|---|
| **AFR (Alternate-Frame Rendering)** | Render one eye per engine frame, alternating L/R, instead of both eyes per frame. The core stereo trick; even frame = left, odd = right. |
| **AOB (Array-Of-Bytes) / pattern scan** | A byte signature (with wildcards) used to *find* a function or variable in a build with no symbols. The backbone of Layer 3. |
| **Adapter (engine adapter)** | The Layer-2 code that knows one specific engine: where it renders, how it sets its camera, how it takes input. Formalised as `IEngineAdapter`. |
| **Basis change** | A matrix that converts between coordinate conventions (e.g. `Y_UP_TO_Z_UP_BASIS`) so HMD-space and engine-space agree on which way is up/forward. |
| **Decoupled pitch** | Letting the player look up/down with the headset without forcing the game character/camera to pitch the same way — a comfort/aiming feature. |
| **dxgi.dll / dinput8.dll proxy** | A fake system DLL dropped beside the exe so Windows loads your code; it forwards the real exports. The injection method all three use. |
| **Eye-to-head transform** | The fixed offset from the head's centre to each eye (half the IPD, plus the headset's geometry). Applied per eye on top of the head pose. |
| **FrameTimeline** | vrframework's reusable state machine tracking the engine/render/presenter counters, eye cadence, and drift→skip-present recovery. |
| **Frustum / projection matrix** | The matrix describing a camera's field of view and near/far planes. In VR it's *asymmetric* per eye, supplied by the runtime, and must replace the engine's. |
| **HMD** | Head-Mounted Display — the headset; its tracked pose is the camera you inject each frame. |
| **Head aim** | Using the headset's facing direction to aim weapons/interaction instead of the gamepad stick. |
| **Hook** | Redirecting an existing function so your code runs first (or instead). Done via MinHook (REFramework) or safetyhook (the ports). |
| **IPD (Inter-Pupillary Distance)** | The distance between the eyes; sets the stereo separation. |
| **MinHook / safetyhook** | Inline-hooking libraries. REFramework uses MinHook; the ports use safetyhook. |
| **Present hook** | Your hook on DirectX's `Present`/swapchain — the once-per-frame chokepoint where you grab the rendered image and submit to the headset. |
| **Reclass / reclass struct** | A reverse-engineered C++ layout of an engine struct you have no header for (named after the ReClass.NET tool). Lives in Layer 3. |
| **Reflex marker** | An NVIDIA Reflex low-latency signal the engine emits at known frame phases. Creation Engine 2 exposes pacing through these (`setReflexMarkerInternal`, markers 0/1/2/4/6); starfield2vr decodes them for timing. |
| **Runtime (VR runtime)** | OpenVR (SteamVR) or OpenXR — the API you submit eye textures and query poses from. |
| **SPI (Service Provider Interface)** | The contract the core defines and adapters implement (`IEngineAdapter` et al.) — the inverse of an API. |
| **Skip present** | Deliberately not calling the engine's real `Present` so the flat monitor isn't shown a single-eye/garbage frame, and to re-align L/R cadence. |
| **StereoView** | A small POD carrying one eye's final view + projection from core to adapter (`apply_stereo`). |
| **TAA (Temporal Anti-Aliasing)** | History-reusing AA; smears under AFR (see §6). Stand-in here for all temporal effects (DLSS/FSR/SSR). |
| **Three counters** | engine / render / presenter frame indices; their drift is what timing code must detect and correct. |

---

## 8. How the series is organised (recommended reading path)

The documents build on each other roughly in the order of the frame lifecycle in §4: get in, hook D3D, set up VR, sync timing, inject the camera, fix the artifacts, then polish and port.

**Start here, in order:**

1. **01 — The Big Picture** *(this doc)* — the mental model, lifecycle, glossary.
2. **02 — Getting In: Injection & Bootstrap** — the proxy DLL, loader-lock survival, when to start hooking.
3. **D3D hooks & the overlay** — hooking Present/swapchain on D3D11 & D3D12, drawing ImGui.
4. **The VR runtime layer** — OpenVR vs. OpenXR, poses, swapchains, submission.

**The hard middle (timing & camera):**

5. **Frame timing & the three counters** — the deepest sub-problem; Reflex markers vs. two-hook sync, drift recovery, skip-present.
6. **Camera & matrices** — composing HMD pose into the engine view, basis changes, eye-to-head, decoupled pitch, head aim.
7. **TAA / temporal history under AFR** — the smear artifact and the per-eye double-buffer cure.

**Polish & per-game work:**

8. HUD/UI reprojection, motion controllers & input, comfort options, per-game offsets/reclass.

**Finish here:**

16. **Porting checklist for a new engine** — the step-by-step for engine #4, tying every layer together.

> **If you're triaging a specific bug**, jump straight to its doc — the world is mirrored → camera/matrices; ghosting/double-vision → TAA; stutter/judder → frame timing; nothing renders → injection or D3D hooks. But read this doc and doc 02 first regardless; everything else assumes them.

---

## Key takeaways

- **You're a parasite on the engine's frame loop, not its author.** You react to the engine's events and hijack four things: process entry, the camera, the rendered image, and timing.
- **Three layers, contained blast radius.** Layer 1 (universal core = vrframework) knows no engine; Layer 2 (adapter) knows one engine; Layer 3 (data) knows one *build*. Game patches hit Layer 3; new engines hit Layer 2; Layer 1 rarely moves.
- **Stereo = AFR.** One eye per engine frame, even/odd alternating, both submitted every other present. Cheap, engine-agnostic, but it breaks every temporal effect and doubles latency.
- **Each frame you *inject* a camera** — HMD pose composed into the engine view via a basis change + eye-to-head — and then steal the result in the **present hook** to submit to the headset.
- **Timing is the hardest sub-problem:** three counters (engine/render/presenter), drift detection, and skip-present. The ports feed it from Reflex markers (starfield) or two hooks (anvil); vrframework's `FrameTimeline` unifies the machinery.
- **Three projects, one principle.** REFramework is the mature origin (reflection, D3D11+12, OpenVR+OpenXR); the mutars ports are leaner forks proving the core is separable — which is exactly what `vrframework` is.

**Next:** [02 — Getting In: Injection & Bootstrap](02-injection-and-bootstrap.md) — how a few kilobytes of your DLL get loaded by a game you don't control, survive the Windows loader, and wait for the engine to be ready before touching anything.
