# 07 — The Hard Part: Frame Timing & Synchronization

**What this covers / why it matters.** Of every problem you will hit retrofitting 6DOF VR into a game whose source you do not own, frame timing is the one that decides whether the result is *playable* or *vomit*. Everything else — projection math, HUD reprojection, input — can be wrong and you still get a recognizable image. Get the timing wrong and you get judder, double-vision, a left eye that lags the right by a frame, or a slow drift where the two eyes desynchronize until the world tears apart. This document explains, from first principles, why VR is uniquely unforgiving about pacing; why an **alternate-frame-rendering (AFR)** mod must keep three independent frame counters in lockstep; and then walks the *two real strategies* the reference ports use to do it — Starfield decoding NVIDIA Reflex markers, and Anvil hooking two engine functions. We finish by showing how both collapse into the single `FrameTimeline` abstraction in the shared core, which is the thing you should actually build for a new engine.

---

## 1. Why VR is unforgiving about timing

A flat game can miss a frame and you barely notice — the image holds for an extra 16ms and life goes on. A headset cannot do this. The display is strapped to your head, and your inner ear has an opinion about where your head is. The VR compositor (SteamVR / OpenXR runtime) runs on a hard vsync cadence — 90Hz, 72Hz, 120Hz — and it *expects a fresh, correctly-posed frame on every single tick*. Miss the tick and the runtime reprojects an old frame; pose it for the wrong eye and you get binocular rivalry; pose it for the wrong *moment* and the world swims relative to your motion. The brain reads all three as "I have been poisoned" and makes you sick within minutes.

So the bar is not "good average FPS." The bar is **every frame, on time, posed for the correct eye, derived from a head pose sampled as late as possible.** Three constraints fall out of that:

1. **Low latency.** The HMD pose must be sampled as close to GPU submission as possible (motion-to-photon). Sample it too early and the image lags your head.
2. **Determinism.** The same engine frame must always map to the same eye. No "sometimes the left eye renders twice."
3. **Lockstep.** Because we are faking stereo on a mono engine, the engine's notion of "frame N" and the compositor's notion of "frame N" must never drift apart.

Constraint 3 is the hard one, and it exists *because of how these mods produce stereo at all.*

---

## 2. Stereo by alternate-frame rendering (AFR)

None of these engines render two eyes. They render one camera, one view, one image — a monocular pipeline baked into a game we cannot recompile. Rewriting the renderer to draw twice is off the table.

So the mods cheat with **alternate-frame rendering**:

> **Even engine frame → render the LEFT eye. Odd engine frame → render the RIGHT eye.**

On each frame the mod injects a per-eye projection matrix and a per-eye view matrix (HMD pose composed into the engine's camera, with the basis change covered in guide 06), lets the engine render its single image *as that eye*, grabs the result, and submits it to the correct swapchain texture of the headset. Two consecutive engine frames make one stereo pair. The compositor displays the pair.

This is beautifully cheap — no renderer surgery — and it is the source of every timing headache in this document. The whole illusion rests on one fragile invariant:

> **The engine frame counter's parity must stay locked to the eye the compositor is expecting.**

If frame parity ever slips — the engine renders two "left" frames in a row, or a frame gets dropped between the view injection and the present — the left and right images desynchronize. You see the world from two slightly different times, your eyes fight, and the drift compounds. Detecting and correcting that slip is most of what "frame synchronization" means here.

You can see the parity rule stated bluntly in the shared core, `vrframework/include/spi/FrameTimeline.hpp:51`:

```cpp
// Which eye this frame belongs to under AFR (even=LEFT, odd=RIGHT).
bool is_left_eye_frame() const { return (m_presenter_frame % 2) == 0; }
```

---

## 3. The three frame counters

A flat game has *one* meaningful clock. Our mods need **three**, because the engine's pipeline is itself staged across threads, and each stage advances on its own beat:

| Counter | Meaning | When it advances | Used for |
|---|---|---|---|
| **engine** | Simulation / update frame | When the game's main loop ticks the world | Deciding which eye this frame is, sampling the HMD pose |
| **render** | GPU command recording | When the render thread starts recording draws | Knowing which frame's *commands* are being built (TAA history, view injection) |
| **presenter** | Submit / present | When the frame is handed to the swapchain / compositor | The actual eye cadence the headset sees; drift detection |

These three are *not* equal at any given instant. The engine thread is usually one or two frames ahead of what the GPU is presenting — that's pipelining, and it's good for throughput. The mod's job is to know, for every callback it receives, **which of the three clocks just ticked**, so it can do the right work at the right edge:

- On the **engine** edge: bump parity, run the ImGui overlay frame, update the world camera.
- On the **wait/render** edge: this is the latency-critical moment. Sample the HMD pose *now* (`update_hmd_state`), call `on_wait_rendering` / `on_begin_rendering` so the VR layer can acquire swapchain images and begin the frame.
- On the **present** edge: confirm cadence, and if it has slipped, schedule a corrective skip.

In the shared core these three live as plain fields, `vrframework/include/spi/FrameTimeline.hpp:55`:

```cpp
uint32_t m_engine_frame{ 0 };
uint32_t m_render_frame{ 0 };
uint32_t m_presenter_frame{ 0 };
```

The VR layer keeps the same three under its own names — `m_engine_frame_count`, `m_render_frame_count`, `m_presenter_frame_count` — and both ports write into all three. The interesting difference is *how each engine tells the mod which clock just ticked*. That's the rest of this document.

---

## 4. Strategy A — Starfield: decoding Reflex markers

Bethesda's Creation Engine 2 ships with **NVIDIA Reflex** integrated. Reflex is a latency-reduction SDK: the engine calls into it at well-defined points in the frame — "I'm starting simulation," "I'm about to submit," "present now" — by emitting numbered **markers**. Crucially, those markers are emitted *from the engine's own threads at the engine's own pacing*, and they carry the engine's frame index. That is exactly the timeline signal we need, already wired through the whole pipeline. The Starfield port's insight is: **don't hook the game loop, hook Reflex.**

The single hook is installed in `starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:56`:

```cpp
REL::Relocation<uintptr_t> setReflexMarkerInternalFn{ GameStore::MemoryOffsets::Nvidia::onSetReflexMarkerInternal() };
m_setReflexMarkerInternalHook = safetyhook::create_inline((void*)setReflexMarkerInternalFn.address(), (void*)setReflexMarkerInternal);
```

Every Reflex marker the engine emits now passes through one detour, `setReflexMarkerInternal(rcx, marker, oldFrameIndex)`, at `CreationEngineRendererModule.cpp:319`. The `marker` integer says *which timeline point*, and `oldFrameIndex` is the engine's own frame number. The detour is a small state machine that decodes the marker numbers. Let's walk it.

### 4.1 The markers that matter (6 / 0 / 1 → the engine+wait edge)

```cpp
if ((marker == 6 || marker == 0 || marker == 1) && !engine_notified) {
    engine_notified = true;
    instance->SetWindowSize(0,0);
    vr->on_wait_rendering(oldFrameIndex);
    vr->m_engine_frame_count = oldFrameIndex;
    vr->on_begin_rendering(oldFrameIndex);
    vr->update_hmd_state(oldFrameIndex);
    g_framework->run_imgui_frame(false);
    ...
    cameraModule->UpdateWorldCamera();
}
```
— `CreationEngineRendererModule.cpp:328`

Markers **6, 0, and 1** all sit at the top of the frame (simulation start / input sample / begin). The port treats the *first* of them to arrive in a given frame as **the** start-of-frame edge, and guards with `engine_notified` so that whichever fires first wins and the rest are ignored until the flag resets. At that single edge it does, in order:

1. `SetWindowSize(0,0)` — keep the game window matched to the HMD render resolution.
2. `on_wait_rendering(oldFrameIndex)` — tell the VR layer we are about to wait on the GPU; this is where it can throttle to the compositor.
3. Record the engine frame: `m_engine_frame_count = oldFrameIndex`.
4. `on_begin_rendering` + `update_hmd_state` — **sample the HMD pose**, as late as we can while still being before the view matrices are built.
5. `run_imgui_frame(false)` — advance the overlay.
6. `UpdateWorldCamera()` — push the freshly-sampled pose into the engine camera.

This is the "everything important happens here" edge. Note that the engine's *own* frame index drives it — the mod never invents a frame number, it borrows the engine's, which is what keeps it honest.

The flag is released when marker **1** is seen, so the next frame can fire again:

```cpp
if (marker == 1) {
    engine_notified = false;
}
```
— `CreationEngineRendererModule.cpp:349`

### 4.2 Markers 2 and 4 → render and presenter clocks

```cpp
if (marker == 2) {
    vr->m_render_frame_count = oldFrameIndex;
}
if (marker == 4) {
    vr->m_presenter_frame_count = oldFrameIndex;
}
```
— `CreationEngineRendererModule.cpp:353`

Marker **2** is the render-recording edge → it sets the **render** counter. Marker **4** is the present edge → it sets the **presenter** counter. With markers 6/0/1 driving the engine counter and 2/4 driving the other two, all three clocks in the table from §3 are now sourced directly from Reflex, each carrying the engine's authoritative frame index. No guessing, no separate counting.

### 4.3 Establishing and watching the L/R cadence

When the VR runtime is loaded, the port records whether this frame begins a left-eye pair (even engine frame) and counts frames since the last resync:

```cpp
if(vr->get_runtime()->loaded) {
    frames_since_reset++;
    if(vr->m_engine_frame_count % 2 == 0) {
        sync_marker_started = true;   // even = left-eye frame, a clean pair-start
    } else {
        sync_marker_started = false;
    }
}
```
— `CreationEngineRendererModule.cpp:338`

`sync_marker_started` means "we are inside a cleanly-aligned left→right pair." The comment block at `CreationEngineRendererModule.cpp:362` lays out the model the port expects:

```cpp
/*
 * as we sync on game loop L eye + frame + game loop R eye + frame Enc
 * we don't expect any async frames comes into this loop
 * if we detect async frame we reset sync and let engine to handle it
 */
```

In other words: the steady state is L-eye frame, then R-eye frame, forever. Anything that breaks that ordering is an "async frame," and the port's response is to give up the current alignment and re-establish it.

### 4.4 Drift detection and skip-present recovery

This is the recovery valve — the single most important safety mechanism in the whole file:

```cpp
if(vr->get_runtime()->loaded && marker == 1 && sync_marker_started) {
    sync_marker_started = false;
} else if(frames_since_reset > 100 && marker > 1 && marker < 5
          && sync_marker_started && vr->get_runtime()->loaded) {
    spdlog::info("Detected frame inconsistency, resetting frame sync m={}", marker);
    vr->m_skip_next_present = true;
    frames_since_reset = 0;
    sync_marker_started = false;
}
```
— `CreationEngineRendererModule.cpp:362`

Read the second branch carefully, because it encodes the whole drift heuristic:

- `frames_since_reset > 100` — a warm-up gate. Don't trust cadence judgments until the pipeline has been running a while; transients at startup or after a load screen are normal.
- `marker > 1 && marker < 5` — we are mid-frame (a render/present marker), **not** at a clean frame boundary.
- `sync_marker_started` — we *thought* we were aligned on a left-eye pair.

If a mid-frame marker shows up while we believed we were cleanly pair-aligned, the cadence has slipped — an extra frame snuck in, or one was dropped, and left/right are now out of phase. The fix is surgical: set `m_skip_next_present = true`. The present layer will **drop exactly one frame**, which shifts parity by one and snaps left/right back into alignment. Then reset the warm-up counter and clear the sync flag so we re-establish from scratch.

Why skip a present rather than, say, render an extra eye? Because dropping one frame is the *only* correction that re-phases AFR without producing a visibly wrong eye. A skipped present costs you one compositor reprojection (barely perceptible); a mis-phased eye costs you nausea.

### 4.5 The TAA / NVIDIA history corollary

AFR has a nasty side-effect that lives in this same file and is worth understanding as part of "timing": **temporal effects smear across eyes.** TAA, motion blur, and NVIDIA's denoiser/upscaler history all assume *frame N+1 continues frame N's camera*. Under AFR, frame N+1 is the *other eye*, looking from a different position. Feed the left eye's history into the right eye and you get ghosting that scales with IPD.

The port fixes this by double-buffering the temporal resources per eye. The camera constant block is snapshotted and swapped at `onUpdateConstantBufferView` (`CreationEngineRendererModule.cpp:261`), keyed by frame parity, and the GPU history textures are copied with `SwapBuffer` driven off the *render* frame counter:

```cpp
auto resource = pRenderGraphData->getResourceByIndex(2, (fc - 1) & 1);
instance->SwapBuffer(commandList, resource, 2, (fc - 1) & 1, fc & 1);
```
— `CreationEngineRendererModule.cpp:301`

The detail that matters for *this* document: `fc` is `GameFlow::renderLoopFrameCount()`, and `(fc & 1)` / `((fc-1) & 1)` is exactly the even/odd eye parity from §2. The history fix is *only correct because the frame counters are correct.* If your timeline is wrong, your TAA fix copies the wrong buffer and the ghosting comes back. Frame timing is load-bearing for everything downstream. (The artifact and its fix get a full treatment in guide 09.)

> Historical note: an earlier, abandoned version of this state machine lived in `starfield2vr/src/CreationEngine/CreationEngineGameLoop.cpp` (now fully commented out). It tried to hook `worldTick` and the higher-level Streamline `sl::ReflexMarker` callback (`onSlReflexSetMarker`, `GameLoop.cpp:103`) and used `marker == 3` with a `frame_sync` accumulator instead of the 6/0/1 + drift-guard scheme. It's a useful fossil: it shows the team converged on hooking the *internal* marker function with a warm-up-gated guard only after the higher-level hooks proved too coarse and "sometimes give 2 ticks" (`GameLoop.cpp:48`).

---

## 5. Strategy B — Anvil: two function hooks

Ubisoft's Anvil engine has no Reflex integration to lean on, so the Anvil port (Valhalla here) takes the more conventional route: it finds two functions in the engine's render loop and hooks them directly. The entire timeline driver is 50 lines — `anvilengine2vr/games/valhalla/engine/EngineRendererModule.cpp` — and the contrast with Starfield is instructive precisely because it's *simpler*.

Two hooks are installed in `EngineRendererModule.cpp:8`:

```cpp
auto beginEngineFrameFunct = memory::on_begin_frame_fn_addr();
m_beginEngineFrameHook = safetyhook::create_inline((void*)beginEngineFrameFunct, (void*)&EngineRendererModule::on_begin_engine_frame);
...
auto begin_render_frame_fn_addr = memory::begin_render_frame_fn_addr();
m_beginRenderFrameHook = safetyhook::create_inline((void*)begin_render_frame_fn_addr, (void*)&EngineRendererModule::on_begin_render_frame);
```

These addresses come from the per-game pattern-scan `offsets.h` (Layer 3) — that's how the same engine adapter retargets across Odyssey / Valhalla / Mirage without code changes.

### 5.1 The engine edge

```cpp
uintptr_t EngineRendererModule::on_begin_engine_frame()
{
    SCOPE_PROFILER();
    static auto instance = Get();
    if (g_framework->is_ready()) {
        static auto vr = VR::get();
        vr->m_engine_frame_count++;
        g_framework->enable_engine_thread();
        g_framework->run_imgui_frame(false);
    }
    return instance->m_beginEngineFrameHook.call<uintptr_t>();
}
```
— `EngineRendererModule.cpp:24`

Note the key difference from Starfield: Anvil's engine function carries **no frame index**, so the port *counts its own*: `vr->m_engine_frame_count++`. This counter's parity *is* the eye selector. It also marks the calling thread as the engine thread and advances the ImGui overlay — the same housekeeping Starfield did at its 6/0/1 edge, just triggered by a real function entry instead of a decoded marker.

### 5.2 The render/present edge

```cpp
uintptr_t EngineRendererModule::on_begin_render_frame(struct GlobalContext* context)
{
    static auto instance = Get();
    SCOPE_PROFILER();
    if (g_framework->is_ready()) {
        static auto vr           = VR::get();
        vr->m_render_frame_count = vr->m_engine_frame_count;
        vr->on_begin_rendering(vr->m_render_frame_count);
        vr->update_hmd_state(vr->m_engine_frame_count);
        const auto result           = instance->m_beginRenderFrameHook.call<uintptr_t>(context);
        vr->m_presenter_frame_count = vr->m_render_frame_count;
        return result;
    }
    return instance->m_beginRenderFrameHook.call<uintptr_t>(context);
}
```
— `EngineRendererModule.cpp:37`

This single hook collapses two of Starfield's edges. Before calling the original it copies engine→render, samples the HMD pose (`update_hmd_state`) and calls `on_begin_rendering` — the latency-critical work. After the original returns it copies render→presenter. So Anvil's three counters are derived sequentially from the one self-incremented engine count, all within two hooks.

### 5.3 What Anvil notably does *not* do

Look at what's *missing* compared to Starfield: there is **no `on_wait_rendering`, no warm-up gate, no `sync_marker_started`, and no `m_skip_next_present` drift recovery here.** Anvil's two engine functions are called once per frame, in order, structurally — the engine's own control flow guarantees the cadence, so the port trusts it rather than policing it. Starfield, decoding asynchronous Reflex markers that can interleave from multiple threads, *cannot* trust the ordering and therefore needs the whole drift-detection apparatus.

That is the deepest lesson of the comparison: **how much synchronization machinery you need is a function of how trustworthy your timing signal is.** A clean, single-threaded, frame-indexed hook needs almost none. An asynchronous, multi-source marker stream needs a state machine with a recovery valve.

---

## 6. The two strategies side by side

| Concern | Starfield (Reflex markers) | Anvil (two hooks) |
|---|---|---|
| **Timing signal** | One hook on `setReflexMarkerInternal`, decode marker IDs | Two hooks: `on_begin_engine_frame`, `on_begin_render_frame` |
| **Engine frame index** | Provided by engine (`oldFrameIndex`) | None — port self-increments (`m_engine_frame_count++`) |
| **Engine edge** | markers 6 / 0 / 1 (first wins, guarded) | `on_begin_engine_frame` |
| **Wait edge** | `on_wait_rendering` at the 6/0/1 edge | *(folded into begin-render; no explicit wait)* |
| **Render counter** | marker 2 | `on_begin_render_frame` (= engine count) |
| **Presenter counter** | marker 4 | after original returns (= render count) |
| **HMD pose sample** | at 6/0/1 edge | at `on_begin_render_frame` |
| **Cadence source** | `m_engine_frame_count % 2` | `m_engine_frame_count % 2` |
| **Drift detection** | warm-up (>100) + mid-frame-marker heuristic | none — trusts hook ordering |
| **Recovery** | `m_skip_next_present` (drop one frame) | none |
| **Lines of code** | ~60 (a real state machine) | ~50 (mostly boilerplate) |
| **Why the difference** | async, multi-thread marker stream → must police | structural per-frame calls → can trust |

Both, despite the surface difference, do the *same four things*: bump the engine counter & run ImGui at the engine edge; sample the HMD pose at the render edge; advance render/presenter counters; and (for Starfield) detect drift and skip a present. That shared shape is the whole reason the abstraction in §7 exists.

---

## 7. Generalizing: the `FrameTimeline` abstraction

Both ports re-implement the same logic against different signals. The shared core (`vrframework`) factors it into one engine-agnostic state machine: **the adapter reports abstract timeline *events*; `FrameTimeline` owns the counters, the eye cadence, and the drift recovery.** The header says exactly this, `vrframework/include/spi/FrameTimeline.hpp:7`:

```cpp
// An adapter just reports timeline EVENTS; FrameTimeline owns the counters, the
// even/odd eye cadence, and drift detection, then calls back into VR at the right edges.
```

The event vocabulary is the five logical points every engine has, `FrameTimeline.hpp:21`:

```cpp
enum class Event : uint8_t {
    ENGINE_FRAME_BEGIN,   // simulation/update frame starts
    WAIT_RENDER,          // engine about to wait for the GPU / acquire (Reflex marker ~6)
    RENDER_FRAME_BEGIN,   // GPU recording for this frame starts
    PRESENT_BEGIN,        // submit/present edge
    PRESENT_END,
};
```

The adapter supplies callbacks (`on_wait_rendering`, `on_begin_rendering`, `on_update_hmd_state`, `on_request_imgui`, `FrameTimeline.hpp:29`) that point back at the same `VR::` methods both ports call today. Then it just translates its native signal into `report(Event, frame)`. The implementation, `vrframework/src/spi/FrameTimeline.cpp:9`, is the union of the two strategies:

```cpp
case Event::WAIT_RENDER:
    if (m_cb.on_wait_rendering) m_cb.on_wait_rendering(frame ? frame : m_engine_frame);
    m_engine_frame = frame ? frame : m_engine_frame;
    if (m_cb.on_begin_rendering) m_cb.on_begin_rendering(m_engine_frame);
    if (m_cb.on_update_hmd_state) m_cb.on_update_hmd_state(m_engine_frame);
    m_sync_started = (m_engine_frame % 2) == 0;   // even engine frame begins a left-eye pair
    ++m_frames_since_reset;
    break;
```

Two design choices carry directly over from the ports:

- **`frame ? frame : (m_engine_frame + 1)`** at `FrameTimeline.cpp:12`. If the engine hands you a real index (Starfield's `oldFrameIndex`), use it; otherwise count your own (Anvil). One line absorbs the single biggest difference between the two engines.
- **The drift guard** at `FrameTimeline.cpp:34` is Starfield's `>100` + mid-cadence heuristic, lifted verbatim into the present edge:

```cpp
case Event::PRESENT_BEGIN:
    m_presenter_frame = frame ? frame : m_render_frame;
    if (m_frames_since_reset > 100 && m_sync_started) {
        m_skip_next_present = true;
        m_frames_since_reset = 0;
        m_sync_started = false;
    }
    break;
```

An Anvil-style adapter that trusts its ordering simply never trips this (it reports `PRESENT_BEGIN` cleanly each frame); a Starfield-style adapter reporting async markers gets the recovery for free. One implementation, both behaviors.

---

## 8. Applying this to a brand-new engine

When you sit down in front of an engine nobody has touched, attack the timeline in this order:

1. **Find the most authoritative per-frame signal.** Best case: an instrumentation API the engine already calls every frame with a frame index — Reflex, Streamline, PIX markers, a profiler's frame boundary. Borrow the engine's own index; never invent one if you can avoid it. Worst case: pattern-scan the render loop for a "begin frame" function and self-count, Anvil-style.
2. **Map your signal onto the five `FrameTimeline::Event`s.** You do not need all five. Anvil uses effectively two. Identify, at minimum, the engine-begin edge and the render/present edge.
3. **Find the latency-critical edge** — the last moment before the engine bakes the view/projection matrices for the frame. Sample the HMD pose *there* (`update_hmd_state`). Too early = laggy head; too late = you missed the matrix.
4. **Pick your eye parity off the engine counter** (`% 2`), and decide left=even / right=odd once. Keep it consistent with your view-injection code (guide 06) and your TAA double-buffering (guide 09) — all three must agree on parity or they fight.
5. **Decide whether you need drift recovery.** Single ordered hook per frame? Probably not — trust it like Anvil. Asynchronous or multi-source markers? Add the warm-up-gated drift guard and a one-frame skip-present, like Starfield.
6. **Verify with a counter readout, not your eyes.** Log `engine / render / presenter` each frame. They should advance in lockstep, one apart, with stable parity. A counter that occasionally jumps by two, or a parity that flips mid-session, *is* your judder — long before you can consciously perceive it in the headset.

---

## Key takeaways

- VR demands every frame on time, posed for the correct eye, from a late-sampled pose. Missing this makes players sick — it is a correctness bug, not a perf nicety.
- These mods fake stereo with **AFR**: even engine frame = left eye, odd = right. The whole illusion depends on engine-frame **parity staying locked** to the eye the compositor expects.
- You need **three counters** — engine, render, presenter — because the pipeline is staged across threads and each ticks on its own beat. The job is knowing which one just ticked.
- **Starfield** decodes Reflex markers in one hook: 6/0/1 = engine+wait+pose, 2 = render, 4 = present, plus a warm-up-gated drift guard that drops one present to re-phase. **Anvil** uses two structural hooks, self-counts the engine frame, and trusts the ordering — no drift machinery needed.
- The amount of synchronization machinery you need is proportional to **how untrustworthy your timing signal is.** Clean ordered hook → almost none. Async marker stream → a full state machine with recovery.
- `FrameTimeline` factors both into one engine-agnostic machine: adapters report abstract **events**; the core owns counters, eye cadence, and the skip-present recovery. The `frame ? frame : count` line absorbs the "engine gives an index vs. we count" split; the `>100` guard absorbs drift recovery.
- Frame timing is **load-bearing** for everything downstream — view injection and the TAA history fix are only correct *because* the counters and parity are correct.

**Next:** [08 — Camera & Pose: Composing the HMD Into the Engine View](08-camera-and-pose.md) — how the per-eye view/projection matrices that this timeline schedules are actually built, including the Y-up→Z-up basis change, per-eye eye-to-head offset, decoupled pitch, and head-aim.
