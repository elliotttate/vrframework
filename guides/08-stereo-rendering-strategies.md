# 08 — Two Eyes From One Engine: Stereo Rendering & Projection

**What this covers / why it matters.** A VR headset needs two images per frame — one per eye, each from a slightly different viewpoint and through a slightly different (asymmetric) frustum. The game engines we are modding render *one* camera. This document is about closing that gap: how to coax a single-camera engine into producing two eyes, why **alternate-frame rendering (AFR)** is the pragmatic answer when you do not own the source, how to build a correct per-eye projection matrix straight from the runtime's frustum bounds, and how to inject that matrix into an engine that never asked for it. Get this wrong and you get double vision, wrong depth, a warped world, or a black eye. Get it right and the engine never knows it is rendering for a headset.

This is the load-bearing chapter. Frame timing (guide 07) decides *when* each eye renders; this chapter decides *what* each eye sees.

---

## 1. The core problem: one camera, two eyes

A flat game has exactly one view matrix and one projection matrix in flight at a time. Stereo VR needs two of each, plus an IPD-correct offset between the eye positions, plus *asymmetric* projection frustums (the runtime decides the FOV per eye; the left and right edges are rarely symmetric on real HMDs). None of that exists in the engine's data model.

You have three structural options:

| Strategy | What it does | Needs | Cost | Used by |
|---|---|---|---|---|
| **Native dual-render** | Engine renders both eyes in one frame (instanced stereo, multiview, or two passes) | Engine source / a stereo-aware render path | ~1.4–1.8× a mono frame | Engines built for VR |
| **Sequential two-pass injection** | Hook the render loop, run the whole frame twice per engine frame with different camera state | A re-entrant render path you can drive twice | ~2× a mono frame, but full rate per eye | Rare in these mods |
| **Alternate-frame rendering (AFR)** | Even engine frame = left eye, odd engine frame = right eye. Inject per-eye camera each frame, submit result to the matching eye | Just the existing single-camera path + a frame counter | Engine runs at 2× headset rate; each eye updates at half engine rate | **anvil, starfield, RE Engine** |

All three reference projects chose **AFR**. The reason is brutally practical: you do not have engine source, you cannot add a multiview render path, and you cannot safely re-enter a closed render pipeline twice per frame without it tripping over its own per-frame state (descriptor heaps, transient allocators, GPU timeline). AFR asks the engine to do exactly what it already does — render one camera — and only changes *which* camera and *where the result goes*. It is the lowest-friction lie you can tell a render loop.

The AFR contract is declared explicitly in the core SPI:

```cpp
// E:/Github/vrframework/include/spi/EngineCaps.hpp:13
enum class Submission : uint8_t {
    SEQUENTIAL,   // both eyes rendered in one engine frame
    AFR,          // alternate-frame: left on even frames, right on odd (Creation 2 / Anvil)
};
```

An adapter sets `submission = Submission::AFR` and the core drives everything else off the frame parity.

---

## 2. AFR in one picture

```
engine frame:   0        1        2        3        4        5
eye rendered:   LEFT     RIGHT    LEFT     RIGHT    LEFT     RIGHT
camera inject:  L view   R view   L view   R view   L view   R view
                + L proj  + R proj + L proj + R proj ...
submit to:      Eye_Left Eye_Right Eye_Left Eye_Right ...
```

Three things have to happen *every engine frame*, keyed on parity:

1. **Pick the eye.** `frame_count % 2` (with an offset constant) selects left or right.
2. **Inject that eye's view + projection** into the engine's camera before it renders.
3. **Submit the finished backbuffer** to the matching compositor eye.

REFramework's D3D12 submit path is the canonical example of step 3 — note it keys off the parity of the render frame counter, exactly mirroring the camera-injection parity:

```cpp
// E:/Github/REFramework/src/mods/vr/D3D12Component.cpp:64
// If m_frame_count is even, we're rendering the left eye.
if (frame_count % 2 == vr->m_left_eye_interval) {
    ... m_openvr.copy_left(eye_texture.Get());
    auto e = vr::VRCompositor()->Submit(vr::Eye_Left, &left_eye, &vr->m_left_bounds);
} else {
    ... m_openvr.copy_right(eye_texture.Get());
    auto e = vr::VRCompositor()->Submit(vr::Eye_Right, &right_eye, &vr->m_right_bounds);
}
```

The `m_left_eye_interval` indirection (rather than a hard-coded `== 0`) exists so the mod can *flip* which parity maps to which eye — useful when frame timing drift makes the engine start a frame "on the wrong foot." Keep that flexibility; you will need it.

The eye that camera injection targets and the eye that submission targets **must agree**. If injection says "this is the left eye" but submission hands the result to `Eye_Right`, you get a stable but nauseating cross-eyed image. The single source of truth is the frame counter, which is why guide 07's frame-timing work is a prerequisite: the counters (`m_engine_frame_count`, `m_render_frame_count`, `m_presenter_frame_count`) are what every parity check reads.

---

## 3. Building the per-eye projection matrix

The view matrix says where the eye is. The **projection matrix** says how the eye sees — and for VR this is *not* a symmetric perspective frustum. The runtime tells you, per eye, the tangent of the frustum's four edge angles at the near plane. Everything flows from those four numbers.

### 3.1 Where the four numbers come from

The core stashes the per-eye frustum bounds where adapters can read them:

```cpp
// E:/Github/vrframework/include/vr/VRRuntime.hpp:57
// Per-eye raw frustum bounds [left, right, top, bottom] — the ports read
// get_runtime()->frustums[eye] to build the projection (see anvil onCalcProjection).
std::array<std::array<float, 4>, 2> frustums{};
```

These come from the VR runtime: OpenVR's `GetProjectionRaw` / OpenXR's `XrFovf` give you the left/right/top/bottom tangents for each eye. They are the *ground truth* FOV the compositor expects. If you substitute the engine's own FOV here, the image will be geometrically wrong at the lens and the world will feel the wrong size.

### 3.2 The Anvil math, line by line

Anvil's `onCalcProjection` hook is the cleanest worked example. It lets the engine compute its normal projection, then — only when the HMD is active and this is the *main world* camera (the `farPlane > 1201.f` guard filters out shadow/UI cameras that have tiny far planes) — overwrites the result with a frustum-bounds projection:

```cpp
// E:/Github/anvilengine2vr/games/valhalla/engine/EngineCameraModule.cpp:58
if (vr->is_hmd_active() && farPlane > 1201.f) {
    auto  frustum = vr->get_runtime()->frustums[0];
    float sum_rl  = (frustum[1] + frustum[0]);
    float sum_tb  = (frustum[2] + frustum[3]);
    float inv_rl  = (1.0f / (frustum[1] - frustum[0]));
    float inv_tb  = (1.0f / (frustum[2] - frustum[3]));
    auto projection = Matrix4x4f {
        2.0f*inv_rl,   0.0f,          0.0f,                                      0.0f,
        0.0f,          2.0f*inv_tb,   0.0f,                                      0.0f,
        sum_rl*inv_rl, sum_tb*inv_tb, farPlane/(nearPlane-farPlane),            -1.0f,
        0.0f,          0.0f,          (nearPlane*farPlane)/(nearPlane-farPlane), 0.0f
    };
    *outProjMatrix = projection;
}
```

`frustum` is `[left, right, top, bottom]` (note the indexing: `[1]`=right, `[0]`=left, `[2]`=top, `[3]`=bottom). Reading it as a standard off-center perspective matrix:

- **`2*inv_rl` = `2 / (right - left)`** — horizontal scale. The wider the horizontal FOV, the smaller this is. This is the `[0][0]` term.
- **`2*inv_tb` = `2 / (top - bottom)`** — vertical scale, the `[1][1]` term.
- **`sum_rl*inv_rl` = `(right + left) / (right - left)`** — the **horizontal skew** that makes the frustum asymmetric. For a symmetric frustum `right == -left`, so this is zero. On a real HMD it is *not* zero, and this term is exactly what nudges each eye's frustum off-center toward the nose. Omit it and both eyes look dead-ahead — the depth cue from convergence breaks.
- **`sum_tb*inv_tb` = `(top + bottom) / (top - bottom)`** — vertical skew (usually small but not zero).
- **`farPlane/(nearPlane-farPlane)`** and **`(nearPlane*farPlane)/(nearPlane-farPlane)`** — the standard depth-remap pair for a **reversed/`-1`** clip convention. The `-1.0f` in the `[2][3]` slot is the perspective divide (w = -z), i.e. a **right-handed, -1..1 (or 0..1) depth** projection in the engine's convention.

Three subtleties that bite:

1. **`nearPlane`/`farPlane` are still the engine's**, not the runtime's. The frustum bounds set the *shape* of the cone (the FOV); the engine's near/far set the *depth range*. Mixing them is correct: you want VR's FOV but the engine's depth precision and far distance so the world clips where the game expects.
2. **The skew terms are the whole point.** A beginner's mistake is to build a symmetric `glm::perspective` from a single FOV value. That renders, looks "fine" on a monitor, and is subtly wrong in the headset — the projected images don't fuse correctly because each eye's off-axis frustum is missing.
3. **The far/near guard (`> 1201.f`)** is a per-engine heuristic to hit only the main scene camera. Every engine has many cameras (shadows, reflections, UI, cubemaps). You only want to rewrite the one the player looks through. Find your engine's equivalent discriminator — far-plane magnitude, a viewport flag, a camera name.

### 3.3 Eyes share the same projection here — by design

Note Anvil reads `frustums[0]` (left eye) for *both* eyes. Because AFR renders one eye per frame and the per-eye difference at the projection level is just the small left/right skew, some ports use the left frustum for both and put the entire stereo separation into the **view** matrix (the eye-to-head translation). This is a legitimate simplification — the dominant stereo cue is the IPD baseline in the view, not the sub-degree frustum asymmetry. A stricter port would index `frustums[eye]`. Either works; just be deliberate about which.

---

## 4. Injecting the matrices into the engine

You have correct matrices. Now you must get them into a render loop that already has its own. The three engines do this in three different places — and the place you pick is dictated entirely by where the engine exposes a hookable, writable copy of the camera.

| Engine | Injection site | Mechanism | What you write |
|---|---|---|---|
| **Anvil** | `onCalcProjection` / `onCalcFinalView` hooks | Modify the hook's **out-parameter** after calling the original | `*outProjMatrix`, `*in_viewMatrix` |
| **Creation 2** | `onUpdateConstantBufferView` hook | Patch the **GPU constant-buffer** struct after the engine fills it | fields inside `RenderPassConstantBufferView` |
| **RE Engine** | reflection-driven camera component | Write via the engine's own type system | camera transform fields |

### 4.1 Anvil: rewrite the out-parameter

Anvil's projection function takes `glm::mat4* outProjMatrix` and fills it. The hook lets the original run, then clobbers the output (Section 3.2). Same pattern for the view: `onCalcFinalView` receives `glm::mat4* in_viewMatrix`, and the hook composes the HMD pose into it *before* the original consumes it:

```cpp
// E:/Github/anvilengine2vr/games/valhalla/engine/EngineCameraModule.cpp:106
if (vr->is_hmd_active() && !bIsShowingUI) {
    const auto eye            = vr->get_current_eye_transform();
    const auto hmd_transform  = vr->get_transform(0);
    const auto rotation_offset= vr->get_transform_offset();
    const auto transform = Y_UP_TO_Z_UP_BASIS * rotation_offset * hmd_transform * eye * INV_UP_TO_Z_UP_BASIS;
    if (ModSettings::g_ac_valhalla_settings.decoupledPitch) {
        removePitchFromZUpMatrix(*in_viewMatrix);
    }
    *in_viewMatrix = *in_viewMatrix * transform;
    ...
}
```

`get_current_eye_transform()` is the eye-to-head offset (the IPD half-baseline) for *whichever eye this AFR frame is*. That single call is what makes frame N the left eye and frame N+1 the right eye — the core flips it based on parity. (The camera/basis math here — `Y_UP_TO_Z_UP_BASIS`, `decoupledPitch` — is the subject of guide 09; here it matters only that the eye offset rides along into the view matrix.) The out-parameter approach is the cleanest when it's available: you're handing the engine a matrix it was about to compute anyway, so nothing downstream is surprised.

### 4.2 Creation 2: patch the constant buffer

Creation 2 (Starfield) does not hand you a tidy out-param. The camera matrices live in a **render-pass constant buffer** that the engine packs and uploads to the GPU. The mod hooks the function that updates that buffer and edits the struct in place. The primary use of this hook in the source is the TAA history fix (Section 6), but it is the same seam through which you would inject per-eye view/projection — you are writing the camera block the GPU will actually read:

```cpp
// E:/Github/starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:252
uintptr_t CreationEngineRendererModule::onUpdateConstantBufferView(
        uint8_t copyCurrentToPast, uint8_t resetHistory, ...,
        RE::RenderPassConstantBufferView* pView)
{
    auto result = original_func(copyCurrentToPast, resetHistory, ..., pView);
    if (copyCurrentToPast && vr->is_hmd_active() && ...) {
        auto key = reinterpret_cast<uintptr_t>(pView) & 0xFFFFFFFFFFFFFFC0;
        ...
        pView->unk150 = temp.cameraMatrix;          // a camera matrix field in the CB
        pView->unk190 = temp.cameraPositionOrJitter;
        pView->unk198 = temp.frameNumber;
    }
    return result;
}
```

The `unk150` / `unk190` / `unk198` names are reverse-engineered offsets into a constant-buffer struct (a reclass header) — Layer 3 per-game data. The `& 0xFFFF...FFC0` masks the pointer down to a 64-byte cache-line boundary so the same logical camera block is keyed consistently across frames. This is the harder injection style: you are writing raw bytes into a GPU-bound buffer at offsets you discovered by inspection, with no type safety. But it is the only option when the engine never surfaces the matrix as a function argument.

**Out-param vs constant-buffer patch — the trade-off:** the out-param style (Anvil) is safer (the engine validates/uses the value normally) but requires the engine to expose the matrix as a hookable argument. The constant-buffer patch (Creation 2) works on *any* engine that uploads a camera CB, but you own the offsets and you bypass whatever the engine would have done with the value. Prefer the out-param when the engine offers one; fall back to the CB patch when it doesn't.

### 4.3 The SPI seam: `StereoView`

The reconstructed core abstracts both styles behind one struct. The core computes the per-eye view and projection; the adapter's *only* job is to copy them into whatever the engine reads:

```cpp
// E:/Github/vrframework/include/spi/StereoView.hpp:12
struct StereoView {
    glm::mat4 view[2];          // per-eye world->view
    glm::mat4 projection[2];    // per-eye projection
    glm::mat4 hmd_transform;    // raw HMD pose (room space)
    glm::mat4 rotation_offset;  // recenter / snap-turn offset
    Eye current_render_eye{Eye::LEFT};
    int presenter_frame{0};     // for AFR ping-pong (which buffer this eye lands in)
};
```

> The adapter's only job is to write these into wherever the engine keeps its camera (a constant buffer, a view matrix out-param, a camera node — engine-specific). This is the seam that lets the same stereo math serve Anvil, Creation 2 and RE Engine unchanged. — `spi/StereoView.hpp:3`

So the porting recipe is: compute `frustums` → core fills `StereoView.view[eye]`/`projection[eye]` → adapter writes them at its one injection site. On a new engine, **finding that injection site is 80% of the work**; the matrix math is copy-paste.

---

## 5. Handedness, matrix layout, and the traps

This is where silent bugs live. The math in Section 3 is correct *for Anvil's conventions*. Three things must line up or the world inverts, mirrors, or turns inside-out:

1. **Row-major vs column-major storage.** GLM is column-major; the literal `Matrix4x4f{...}` in `onCalcProjection` is written so the per-eye `[0][0]`, `[1][1]` scale terms and the `[2][3] = -1` perspective term land in the slots the engine's shaders sample. If your engine multiplies `proj * view * model` with row-vectors, you must transpose. The fastest way to detect a transpose error: the image renders but warps wildly as you turn your head.
2. **Handedness.** `EngineCaps.left_handed` declares it: `bool left_handed{false}; // GLM_FORCE_LEFT_HANDED engine (Anvil = true)` (`spi/EngineCaps.hpp:23`). Anvil is left-handed; the `Y_UP_TO_Z_UP_BASIS` basis change and the `glm::lookAtRH` in `removePitchFromZUpMatrix` (EngineCameraModule.cpp:85) are written against that. The `-1.0f` perspective term encodes the engine's clip convention (w = -z). If your engine uses `+z` forward or `0..1` clip with a `+1` perspective term, the depth term signs flip. Symptom of getting it wrong: depth inverts (near geometry occluded by far) or the whole scene is mirrored left-right.
3. **Near/far depth convention.** `farPlane/(nearPlane-farPlane)` assumes a particular near→far mapping. Some engines use reversed-Z (near=1, far=0) for precision. If yours does, the depth terms change and naively copying Anvil's gives you a z-fighting mess. Check the engine's *existing* projection (let the original run, log its output) and match its `[2][2]`/`[2][3]`/`[3][2]` pattern rather than assuming.

**The diagnostic discipline:** before you write a single matrix, hook the engine's projection function read-only and log the matrix it already produces for a mono frame. That tells you storage order, handedness, and depth convention empirically. Then build your VR projection to match that pattern, swapping only the FOV scale and skew terms for the runtime's frustum values. This is far faster than reasoning about conventions from first principles.

---

## 6. The cost of AFR, and the artifacts you must fix

AFR is cheap to implement and expensive in two specific ways.

### 6.1 Half update rate per eye

Each eye only updates every *other* engine frame. To present 90 Hz per eye, the engine must run at ~180 fps. That is the headline tax: **the engine renders at double the headset's per-eye rate.** On a heavy AAA title this is the hardest performance constraint, and it is why these mods care so much about frame pacing (guide 07) and render-resolution control (`SetWindowSize` in CreationEngineRendererModule.cpp:189 drives the game window to the HMD's render size so you don't waste pixels). There is no software trick that removes this cost; AFR is fundamentally trading GPU throughput for implementation simplicity.

### 6.2 Temporal smearing — the TAA / Nvidia history problem

This is the notorious one. Temporal anti-aliasing and any effect with a frame-to-frame history (TAA, temporal upscalers, motion-vector reprojection, Nvidia's history buffers) assume the *previous frame was the same camera*. Under AFR it wasn't — frame N is the left eye, frame N-1 was the **right** eye. So TAA happily blends the right eye's history into the left eye's image. The result is a sickening ghost/smear, worst at object edges and during head motion.

The fix is to give each eye *its own* history. You double-buffer the temporal resources by eye parity, so when the left eye renders, TAA reads the *left* eye's previous frame, not the right's. Both Creation 2 and Anvil do exactly this, in slightly different ways.

**Creation 2** copies the past-frame GPU resources into per-parity slots and swaps them around the TAA pass:

```cpp
// E:/Github/starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:142
void CreationEngineRendererModule::SwapBuffer(ID3D12GraphicsCommandList* cmdList,
        ID3D12Resource* originalBuffer, int index, int copyFromSlot, int copyToSlot)
{
    if (!ValidateResource(originalBuffer, m_pastBuffer[index])) return;
    auto saveBuffer       = m_pastBuffer[index][copyToSlot].Get();
    auto alterFrameBuffer = m_pastBuffer[index][copyFromSlot].Get();
    CopyResource(cmdList, originalBuffer, saveBuffer, ...);     // stash this eye's frame
    CopyResource(cmdList, alterFrameBuffer, originalBuffer, ...); // restore this eye's prior frame
}
```

It is driven by frame parity at the TAA pass:

```cpp
// E:/Github/starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:300
auto resource = pRenderGraphData->getResourceByIndex(2, (fc - 1) & 1);
instance->SwapBuffer(commandList, resource, 2, (fc - 1) & 1, fc & 1);
```

`(fc - 1) & 1` and `fc & 1` are "previous eye" and "this eye" — the same parity logic as everything else in AFR, now applied to GPU history buffers. `ValidateResource` (line 107) lazily allocates four committed copies matching the source's size/format, re-creating them if the resolution changes.

The matching **constant-buffer** history fix is the `pastProjections` map back in `onUpdateConstantBufferView` (line 261): it remembers each camera block's matrix from the matching-parity previous frame and writes it back so motion vectors reproject against the correct prior camera, not the other eye's. Resource history *and* camera-matrix history both have to be eye-aware or you trade an edge smear for a motion-vector smear.

**Anvil** does the CPU-matrix half of the same idea with a two-slot ping-pong of the view matrix:

```cpp
// E:/Github/anvilengine2vr/games/valhalla/engine/EngineCameraModule.cpp:145
uintptr_t EngineCameraModule::onCopyGfxContext(sdk::GfxContext* context, glm::mat4* viewMatrix) {
    static glm::mat4 copies[2]{ glm::mat4{1.0}, glm::mat4{1.0} };
    auto result = instance->m_onCopyGfxContext.call<uintptr_t>(context, viewMatrix);
    auto frame  = vr->m_presenter_frame_count;
    copies[frame % 2]       = context->pastViewMatrix;
    context->pastViewMatrix = copies[(frame - 1) % 2];   // feed the OTHER parity's past view
    return result;
}
```

`pastViewMatrix` is the engine's "where the camera was last frame" used for motion blur / reprojection. By keying `copies[]` on `m_presenter_frame_count % 2` and feeding back `(frame - 1) % 2`, Anvil ensures the left eye reprojects against the *left* eye's previous frame. Note `presenter_frame` is exactly the field the SPI exposes on `StereoView` "for AFR ping-pong (which buffer this eye lands in)" (`spi/StereoView.hpp:23`) — the abstraction was built around this pattern.

**The principle for a new engine:** any resource or matrix the engine carries from frame to frame is poisoned by AFR. Enumerate them — TAA color history, depth history, motion vectors, the "previous view-projection" constant, exposure/auto-white-balance state — and double-buffer each one by eye parity. The symptom that you missed one is a smear, flicker, or pulsing that only appears in the headset and only while moving. The `EngineCaps.has_taa` flag (`spi/EngineCaps.hpp:21`) exists so the core knows an engine needs this whole machinery; if your engine has no temporal effects you can skip it entirely.

### 6.3 Other AFR fix-ups

Two smaller ones visible in the Anvil source, worth noting because they generalize:

- **Scissor/viewport rounding** (`onCalcFinalView`, EngineCameraModule.cpp:127): VR render targets are often odd sizes; the mod normalizes the scissor rect to start at `(0,0)` and rounds the extents up to even (`roundUp`, line 90) so half-resolution passes don't clip a column of pixels.
- **HUD/UI viewport scaling** (`onCalcUIViewportHook`, line 194): a fullscreen HUD plastered across a VR eye is unreadable; the UI viewport is scaled in toward the center so it sits at a comfortable depth/size. (HUD projection is its own topic — guide 11.)

---

## 7. Applying this to a brand-new engine

The full recipe, in order:

1. **Confirm AFR is viable.** Can the engine sustain ~2× your target eye-rate? If not, AFR will judder no matter how clean your math is. Set `EngineCaps.submission = AFR`.
2. **Find the frame counter** and establish the left/right parity (guide 07). Every later step keys off it.
3. **Find the projection site.** Hook it read-only first; log the matrix for a mono frame to learn storage order, handedness, and depth convention (Section 5).
4. **Find the frustum bounds** from your runtime (`GetProjectionRaw` / `XrFovf`) → `runtime->frustums[eye]`.
5. **Build the per-eye projection** matching the engine's convention, with the runtime's FOV scale + asymmetric skew terms (Section 3.2). Inject it via out-param if available, else patch the camera constant buffer (Section 4).
6. **Find the view site.** Compose the per-eye eye-to-head offset + HMD pose into the engine's view matrix (Section 4.1; basis details in guide 09).
7. **Submit by parity.** Copy the finished backbuffer to the matching compositor eye (Section 2).
8. **Hunt the temporal artifacts.** Enumerate every cross-frame resource/matrix and double-buffer by parity (Section 6.2). Set `EngineCaps.has_taa` accordingly.
9. **Clean up viewport/HUD** (Section 6.3).

If steps 5–7 are correct but the image is still cross-eyed, suspect a parity mismatch between injection and submission. If it's geometrically warped, suspect handedness/transpose. If it smears only while moving, suspect a missed history buffer. Those three failure modes cover the vast majority of stereo bugs.

---

## Key takeaways

- **AFR is the pragmatic choice** for source-less engines: it asks the engine to render one camera, exactly as it already does, and only changes *which* camera and *where the result goes*. All three reference projects use it.
- **Build the projection from the runtime's frustum bounds**, not a symmetric FOV. The off-center **skew terms** (`(right+left)/(right-left)`) are the whole point of stereo projection; omit them and the eyes don't fuse. Keep the *engine's* near/far for depth, use the *runtime's* edges for FOV.
- **Injection site is engine-specific** but falls into two families: rewrite a hookable **out-parameter** (Anvil, safer) or patch the **camera constant buffer** at reverse-engineered offsets (Creation 2, universal). The `StereoView` SPI hides both behind `view[]`/`projection[]`.
- **Handedness, matrix storage, and depth convention must match the engine empirically** — log the engine's own projection first and mirror its pattern; don't reason from first principles.
- **AFR's two costs are non-negotiable:** ~2× engine framerate, and temporal smearing. The smearing is fixed by **double-buffering every cross-frame resource and matrix by eye parity** (`SwapBuffer`, `pastProjections`, the `copies[]` ping-pong). Miss one and you get a movement-only smear in the headset.

**Next:** *09 — The Camera: Composing the HMD Pose Into the Engine View* — the basis change (`Y_UP_TO_Z_UP_BASIS`), per-eye eye-to-head transforms, decoupled pitch, and head-aim, i.e. the *view* half of the matrices this chapter only injected.
