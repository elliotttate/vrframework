# 10 — Submitting to the Headset & The TAA/History Problem

**What this covers / why it matters.** You have stereo eye images in the backbuffer — left eye on even engine frames, right eye on odd. Two things still have to happen before the player sees a convincing 3D scene. First, that image has to be *handed to the VR compositor*: copied into a runtime-owned texture, transitioned through the right D3D12 resource states, and submitted per-eye to OpenVR or OpenXR. Second, you have to fight the single nastiest artifact that Alternate-Frame Rendering (AFR) creates: **temporal smearing**. Modern engines reproject the previous frame into the current one (TAA, motion-vector-based denoisers, DLSS, FSR, Nvidia's history buffers). Under AFR the "previous frame" is *the other eye* — so the engine happily blends the left eye into the right and you get a doubled, ghosting, nauseating mess. This document explains how all three mods copy and submit eye textures, and the double-buffer trick (snapshot/restore of past camera matrices and past render resources) that makes temporal effects behave under AFR. If you read one document about why VR mods feel hard, it's this one.

---

## 1. The submission problem in one picture

The engine renders into its own backbuffer. The VR runtime owns a *different* set of textures — a swapchain it allocated, that the compositor will read on its own thread, at its own cadence, to do reprojection and distortion. You cannot just hand the runtime a pointer to the engine's backbuffer:

- The runtime needs the texture in a **specific resource state** (`PIXEL_SHADER_RESOURCE` / `RENDER_TARGET`, never `PRESENT`).
- The runtime needs a texture it **owns and synchronizes** — the engine will overwrite its backbuffer the moment you return control.
- The formats may not match (the engine may render HDR / 10-bit; SteamVR wants 8-bit sRGB).

So submission is fundamentally a **copy**: take the finished eye image out of the engine's backbuffer, copy it into a runtime swapchain image, fence it, and tell the runtime "this image is eye N for this frame."

```
engine backbuffer  ──copy──►  runtime swapchain image  ──Submit/EndFrame──►  compositor
   (PRESENT)                   (PIXEL_SHADER_RESOURCE)                          (reproject + distort)
```

REFramework is the reference implementation for this step, so we start there.

---

## 2. The copy-and-submit loop (REFramework, D3D12)

`D3D12Component::on_frame` is the whole story in one function. Read it top to bottom.

### 2.1 Get the finished eye image

The engine has just rendered an eye into the current backbuffer. REFramework fetches it:

```cpp
const auto backbuffer_index = swapchain->GetCurrentBackBufferIndex();
swapchain->GetBuffer(backbuffer_index, IID_PPV_ARGS(&backbuffer));
```
— `REFramework/src/mods/vr/D3D12Component.cpp:31`

If the backbuffer is not already 8-bit (HDR titles), it copies it into a scratch resource and runs a sprite-batch blit to *convert* it to `R8G8B8A8_UNORM`, because that's what the eye textures and the compositor expect:

```cpp
m_backbuffer_is_8bit = backbuffer_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM;
...
// Copy current backbuffer into our copy so we can use it as an SRV.
m_backbuffer_copy.commands.copy(backbuffer.Get(), m_backbuffer_copy.texture.Get(),
    D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT);
// Convert the backbuffer to 8-bit.
render_srv_to_rtv(command_list, m_backbuffer_copy, m_converted_eye_tex, ...);
```
— `REFramework/src/mods/vr/D3D12Component.cpp:48`, `:54`

Why the intermediate copy? You can't bind the swapchain backbuffer as a shader resource view (SRV) — it's a `PRESENT`-state render target with `DENY_SHADER_RESOURCE`. So REFramework allocates a clone with the SRV flags cleared (`setup()`, `D3D12Component.cpp:243`) and copies into it first. The 8-bit conversion blit is the `SpriteBatch` draw inside `render_srv_to_rtv` (`:352`).

### 2.2 Which eye is this frame?

This is the AFR decision. The engine frame counter's parity tells you the eye:

```cpp
const auto frame_count = vr->m_render_frame_count;
// If m_frame_count is even, we're rendering the left eye.
if (frame_count % 2 == vr->m_left_eye_interval) {   // m_left_eye_interval == 0
    ...
} else {
    ...
}
```
— `REFramework/src/mods/vr/D3D12Component.cpp:62`, `:65`

`m_left_eye_interval` / `m_right_eye_interval` default to `0` / `1` (`VR.hpp:476`). They're variables, not constants, so the mod can *swap which eye renders on even frames* — important when you need to resync after a dropped frame (see §8 on dominant eye).

### 2.3 Copy into the runtime texture and submit

For OpenVR, the per-eye copy is a `CopyResource` wrapped in barriers, then a `Submit`:

```cpp
void copy_left(ID3D12Resource* src) {
    auto& ctx = this->acquire_left();
    ctx.commands.copy(src, ctx.texture.Get(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    ctx.commands.execute();
}
```
— `REFramework/src/mods/vr/D3D12Component.hpp:86`

```cpp
m_openvr.copy_left(eye_texture.Get());
vr::D3D12TextureData_t left { m_openvr.get_left().texture.Get(), command_queue, 0 };
vr::Texture_t left_eye{(void*)&left, vr::TextureType_DirectX12, vr::ColorSpace_Auto};
auto e = vr::VRCompositor()->Submit(vr::Eye_Left, &left_eye, &vr->m_left_bounds);
```
— `REFramework/src/mods/vr/D3D12Component.cpp:74`–`:84`

Three things worth internalising:

1. **`Submit` takes a command queue, not a command list.** OpenVR's D3D12 submission is queue-based: you give it the texture and the queue it was produced on, and the compositor inserts its own fence wait. That's why every copy ends in `ctx.commands.execute()` — the work must be *queued* before `Submit`, but you do **not** block the CPU on it.

2. **`m_left_bounds` / `m_right_bounds`** are `VRTextureBounds_t` (`VR.hpp:338`). Default `{0,0,1,1}` means "use the whole texture." This is the hook for cropping when both eyes share one texture — not used here because each eye gets its own, but it's how a side-by-side single-texture submission would work.

3. **Triple buffering of eye textures.** `left_eye_tex` / `right_eye_tex` are `std::array<TextureContext, 3>` (`D3D12Component.hpp:98`), indexed by `texture_counter % 3`. The compositor may still be reading eye N-1 while you write eye N. Reusing one texture would stomp it; three rotating textures plus a per-texture fence (`commands.wait(INFINITE)` in `acquire_left`, `hpp:73`) guarantee you never write a texture the compositor hasn't released.

### 2.4 OpenXR submission is acquire/wait/copy/release

OpenXR doesn't have a `Submit(eye, texture)` call. You ask the runtime's swapchain for an image, wait on it, copy into it, release it, and then `xrEndFrame` composites both eyes' layers at once:

```cpp
xrAcquireSwapchainImage(swapchain.handle, &acquire_info, &texture_index);
...
xrWaitSwapchainImage(swapchain.handle, &wait_info);     // wait_info.timeout = XR_INFINITE_DURATION
...
texture_ctx->commands.copy(resource, ctx.textures[texture_index].texture,
    D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
texture_ctx->commands.execute();
xrReleaseSwapchainImage(swapchain.handle, &release_info);
```
— `REFramework/src/mods/vr/D3D12Component.cpp:593`, `:616`, `:623`, `:631`

The actual present happens later, once, when both eyes are in:

```cpp
if (frame_count % 2 == vr->m_right_eye_interval) {   // i.e. on the *second* eye
    ...
    auto result = vr->m_openxr->end_frame();
    ...
}
```
— `REFramework/src/mods/vr/D3D12Component.cpp:125`, `:142`

Note the asymmetry: OpenVR `Submit`s each eye *as it's produced* (two separate engine frames), while OpenXR copies each eye as produced but only calls `end_frame` on the right-eye frame, when both swapchain images are ready. Both are valid; OpenXR's model maps more naturally onto AFR because "end frame when the second eye lands" is exactly the AFR cadence.

| Concern | OpenVR | OpenXR |
|---|---|---|
| Hand-off call | `VRCompositor()->Submit(eye, tex, bounds)` per eye | acquire/wait/copy/release + `xrEndFrame` once |
| When it composits | implicit, after both Submits | explicit, on `xrEndFrame` |
| Texture ownership | mod-owned, triple-buffered (`array<,3>`) | runtime-owned swapchain images |
| Target state after copy | `PIXEL_SHADER_RESOURCE` | `RENDER_TARGET` |
| Sync | per-texture fence + queue submit | `xrWaitSwapchainImage` + per-ctx fence |

---

## 3. D3D12 resource states & barriers — the part that crashes you

Every copy in this codebase is bracketed by **resource barriers** that move textures through the exact states the copy engine and the runtime require. Get one wrong and you get a silent black eye, a TDR, or a debug-layer scream.

The canonical helper is starfield's `CopyResource` — a textbook barrier-copy-barrier:

```cpp
inline static void CopyResource(ID3D12GraphicsCommandList* cmdList,
        ID3D12Resource* pSrcResource, ID3D12Resource* pDstResource,
        D3D12_RESOURCE_STATES srcState, D3D12_RESOURCE_STATES dstState) {
    D3D12_RESOURCE_BARRIER barriers[2] = {
        CD3DX12_RESOURCE_BARRIER::Transition(pSrcResource, srcState, D3D12_RESOURCE_STATE_COPY_SOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(pDstResource, dstState, D3D12_RESOURCE_STATE_COPY_DEST) };
    cmdList->ResourceBarrier(2, barriers);
    cmdList->CopyResource(pDstResource, pSrcResource);
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter  = srcState;     // restore
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter  = dstState;     // restore
    cmdList->ResourceBarrier(2, barriers);
}
```
— `starfield2vr/src/CreationEngine/CreationEngineRendererModule.h:153`

The rules this encodes, which you will re-derive painfully if you skip them:

- **A resource has exactly one state per subresource.** `CopyResource` requires the source in `COPY_SOURCE` and the destination in `COPY_DEST`. Anything else is undefined.
- **Restore the original state after copying.** `CopyResource` above transitions *back* to `srcState`/`dstState` so the engine finds its resources exactly as it left them. This is critical because you are stealing the engine's own resources mid-frame (more in §5–6) and it must not notice.
- **You must know the *incoming* state.** That's why the helper takes `srcState`/`dstState` as parameters — there is no API to query a resource's current state at runtime; you have to track it, and on hooked engine resources you have to *know* it from reverse-engineering. Get it wrong and the debug layer reports a state mismatch; in release you get corruption.

REFramework's `render_srv_to_rtv` (`D3D12Component.cpp:352`) shows the manual version: transition `dst` to `RENDER_TARGET`, blit, transition back to `dst_state`. Same discipline, expressed inline.

> **Rule of thumb for a new engine:** wrap every cross-resource copy in a symmetric barrier pair, restore states, and make the "from" state an explicit argument so a wrong assumption is visible at the call site, not buried.

---

## 4. The TAA / Nvidia history problem — why AFR breaks temporal effects

Here is the central villain of VR modding.

Modern renderers are **temporal**. To get clean anti-aliasing and cheap denoising they don't compute each frame from scratch — they *reproject last frame's result* into this frame using motion vectors, then blend. TAA does this for AA. DLSS / FSR do it for upscaling. Many denoisers (SSR, SSAO, RTGI) keep a "history buffer." Nvidia's Reflex/Streamline pipelines carry per-frame history resources.

The blend uses **last frame's camera matrix** (stored as `pastProjections` / a "previous view-projection") to figure out where each pixel *was* last frame, so it can fetch the matching history texel.

Now turn on AFR. Frame N is the **left eye**. Frame N+1 is the **right eye**. When the engine renders the right eye, its "previous frame" is the left eye — rendered from a camera shifted ~63mm to the side. The temporal pass dutifully:

1. takes the left-eye history buffer,
2. reprojects it with the left-eye previous-frame matrix,
3. blends it into the right eye.

The result: each eye is contaminated with the other eye. You see **ghosting/doubling along edges**, a **shimmering disparity** that the human visual system reads as "this is wrong," and within a minute, nausea. Disabling TAA "fixes" it but throws away AA and makes upscalers impossible.

The correct fix is not to disable temporal effects — it's to **give each eye its own history** so the temporal pass reprojects left-into-left and right-into-right. Concretely you must double-buffer two things:

1. **The past camera matrix** the temporal pass reads (so reprojection uses *this eye's* previous frame, two engine-frames ago).
2. **The past render resources** the temporal pass blends (the history textures themselves).

starfield does both. Let's look at each.

---

## 5. Fixing the past *matrices* — `pastProjections` (starfield)

The Creation Engine updates a constant buffer of camera transforms each frame, and as part of that it copies "current → past" so the next frame's temporal pass has the previous view-projection. starfield hooks that function: `onUpdateConstantBufferView`.

```cpp
if (copyCurrentToPast && vr->is_hmd_active() && GameFlow::gStore.internalSettings.nvidiaAndTAAfix) {
    auto key = reinterpret_cast<uintptr_t>(pView) & 0xFFFFFFFFFFFFFFC0;
    if (pastProjections.find(key) != pastProjections.end()) {
        auto temp      = pastProjections[key];
        auto past2past = &pastProjections[key];
        past2past->cameraMatrix           = pView->unk150;   // snapshot what's there now
        past2past->cameraPositionOrJitter = pView->unk190;
        past2past->frameNumber            = pView->unk198;
        if (!resetHistory) {
            pView->unk150 = temp.cameraMatrix;               // restore the value from 2 frames ago
            pView->unk190 = temp.cameraPositionOrJitter;
            pView->unk198 = temp.frameNumber;
        }
    }
    else { /* first time: just snapshot */ }
}
```
— `starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:261`–`:282`

Read carefully — this is a **two-frame delay line** keyed by the constant-buffer view address (`& ...C0` masks the low bits to align to the buffer base, so per-view buffers map to one slot). The native code wrote "the previous frame's matrix" (which under AFR is the *other eye's* matrix). The hook overwrites that with the matrix from **two frames ago** — i.e. the same eye's previous frame. So when the temporal pass reads "previous view-projection," it gets the geometrically correct same-eye history, and reprojection lines up.

Note the `resetHistory` guard: when the engine itself is resetting history (scene cut, camera teleport) the mod stays out of the way and lets the native reset proceed — restoring a stale matrix there would *re-introduce* ghosting across the cut.

This is the cheap half of the fix. Matrices are tiny; snapshotting them is a few struct copies. The expensive half is the history *textures*.

---

## 6. Fixing the past *resources* — `m_pastBuffer` + `SwapBuffer` (starfield)

The matrices tell the temporal pass *where* to sample; the history textures are *what* it samples. Even with correct matrices, if the history texture still holds the other eye's pixels, you ghost. So starfield maintains its own past-resource buffers and swaps them in/out around the temporal pass.

### 6.1 The buffer pool and its validation

```cpp
ComPtr<ID3D12Resource> m_pastBuffer[12][4];   // up to 12 tracked resources, 4 slots each
```
— `starfield2vr/src/CreationEngine/CreationEngineRendererModule.h:192`

`ValidateResource` lazily allocates matching copies and guards against the engine resizing the resource under you:

```cpp
bool ValidateResource(ID3D12Resource* source, ComPtr<ID3D12Resource> pastBuffer[4]) {
    if (source == nullptr) return false;
    D3D12_RESOURCE_DESC desc = source->GetDesc();
    if (pastBuffer[0] != nullptr) {
        D3D12_RESOURCE_DESC desc2 = pastBuffer[0]->GetDesc();
        if (desc.Width != desc2.Width || desc.Height != desc2.Height || desc.Format != desc2.Format) {
            // resolution/format changed -> throw away all 4 copies, reallocate
            pastBuffer[0].Reset(); pastBuffer[1].Reset();
            pastBuffer[2].Reset(); pastBuffer[3].Reset();
        }
    }
    ...
    if (pastBuffer[0] == nullptr || pastBuffer[1] == nullptr) {
        for (int i = 0; i < 4; i++) {
            device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(pastBuffer[i].GetAddressOf()));
        }
    }
    return true;
}
```
— `starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:107`–`:140`

The resolution check matters because the user can change render scale or window size at runtime (and a VR mod changes it constantly — see §9). A mismatched copy means a corrupt blit; resetting and reallocating is the safe response.

### 6.2 The ping-pong: `SwapBuffer`

```cpp
void SwapBuffer(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* originalBuffer,
                int index, int copyFromSlot, int copyToSlot) {
    if (!ValidateResource(originalBuffer, m_pastBuffer[index])) return;
    auto saveBuffer       = m_pastBuffer[index][copyToSlot].Get();
    auto alterFrameBuffer = m_pastBuffer[index][copyFromSlot].Get();
    // 1. stash the engine's current history into our "save" slot for this eye
    CopyResource(cmdList, originalBuffer, saveBuffer,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
    // 2. restore THIS eye's history (from 2 frames ago) into the engine's buffer
    CopyResource(cmdList, alterFrameBuffer, originalBuffer,
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}
```
— `starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:142`–`:151`

`copyFromSlot` / `copyToSlot` are `(fc-1)&1` and `fc&1` — the two parities ping-pong, so each eye reads and writes its own slot. Step 1 saves what the engine just produced (this eye's new history) into the slot the *other* parity will not touch; step 2 overwrites the engine's history buffer with this eye's previous-eye-frame history, so the temporal pass blends same-eye-to-same-eye.

### 6.3 Where it's invoked: around the render graph and at the TAA pass

starfield wires `SwapBuffer` into two hook sites, one per kind of temporal resource:

**(a) Render-graph frame boundary** — for the resource the *frame* pass produces:

```cpp
} else if (!before && m_framePass == pGraph) {
    auto fc = GameFlow::renderLoopFrameCount();
    auto commandList = context->pID3D12CommandList;
    if (vr->is_hmd_active() && ...nvidiaAndTAAfix && ModConstants::enabledResourcesToCopy[1]) {
        auto resource = pRenderGraphData->getResourceByIndex(1, (fc - 1) & 1);
        SwapBuffer(commandList, resource, 1, (fc - 1) & 1, fc & 1);
    }
}
```
— `starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:168`–`:175`

`onRenderGraphRenderStart` calls `RenderGraphStart(..., true)` before the native graph and `RenderGraphStart(..., false)` after (`:101`–`:103`), so the mod can act on either side of the engine's own pass.

**(b) The TAA pass itself** — hooked via vtable slot 7 (`OnTaaVFunc7`, `:82`):

```cpp
uintptr_t CreationEngineRendererModule::onTaaPass(...) {
    auto result = original_fn(pPass, pRenderGraphData, renderPassData);   // let TAA run first
    if (!GameFlow::gStore.internalSettings.nvidiaAndTAAfix) return result;
    ...
    if (vr->is_hmd_active() && ModConstants::enabledResourcesToCopy[2]) {
        auto resource = pRenderGraphData->getResourceByIndex(2, (fc - 1) & 1);
        instance->SwapBuffer(commandList, resource, 2, (fc - 1) & 1, fc & 1);
    }
    if (vr->is_hmd_active() && ModConstants::enabledResourcesToCopy[3]) {
        auto resource = pRenderGraphData->getResourceByIndex(3, (fc - 1) & 1);
        instance->SwapBuffer(commandList, resource, 3, (fc - 1) & 1, fc & 1);
    }
    return result;
}
```
— `starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:286`–`:308`

Which resources get swapped is data-driven by `enabledResourcesToCopy{false, true, true, true, false, ...}` (`CreationEngineConstants.h:14`) — slots 1, 2, 3 are on. This is deliberately tunable per game: every engine has a *different set* of temporal resources, and you discover them by capturing a frame (PIX/Nsight/RenderDoc), finding the buffers that read last-frame's content, and toggling them until the ghosting disappears. There is no shortcut; this is empirical.

> **The mental model:** for *every* temporal resource the engine keeps, you maintain a parallel per-eye copy and ping-pong it around the engine's pass — save the engine's current value, restore this eye's two-frames-ago value. Matrices (§5) and textures (§6) are the same idea applied to two different data types.

---

## 7. Why temporal upscalers (DLSS/TAA) fight VR mods — the general principle

Stepping back from starfield's specifics, here's the durable lesson for a new engine:

- **Any effect that reads frame N-1 is hostile to AFR**, because under AFR frame N-1 is the wrong eye. TAA, FSR2/3, DLSS, XeSS, temporal SSAO/SSR/RTGI, motion blur, and Nvidia history/Reflex resources all qualify.
- **Disabling them is the lazy fix and a bad one** — you lose AA and upscaling, which VR (with its brutal pixel counts) needs *more* than flatscreen does.
- **The real fix is per-eye history.** You either (a) snapshot/restore the past matrices so reprojection targets the correct eye (cheap, always do this), and (b) snapshot/restore the history textures so the blend source is the correct eye (expensive, do it for the resources that actually ghost).
- **You find the resources empirically.** Capture a frame, identify temporal passes, find their history inputs, and toggle copies until artifacts vanish. starfield's `enabledResourcesToCopy` bitset *is* that toggle, frozen into config.

anvil and REFramework sidestep most of this by targeting engines/configs where the dominant temporal contamination is avoidable or where the AFR cadence is tight enough that the previous-frame matrix can simply be re-derived — but the moment you enable DLSS-class upscaling on any engine, you are back in starfield's world. Budget for it.

---

## 8. Dominant-eye handling & resync

Under AFR you submit one *real* eye per engine frame; the other eye in the headset is whatever the runtime reprojects. Which eye is "live" on a given frame is governed by `m_left_eye_interval` / `m_right_eye_interval` (`VR.hpp:476`) — the parity-to-eye mapping. Because they're mutable, the mod can **flip the mapping** to resync when frames slip.

starfield's resync logic lives in the Reflex marker hook:

```cpp
if (vr->m_engine_frame_count % 2 == 0) sync_marker_started = true;
else                                   sync_marker_started = false;
...
} else if (frames_since_reset > 100 && marker > 1 && marker < 5
           && sync_marker_started && vr->get_runtime()->loaded) {
    spdlog::info("Detected frame inconsistency, resetting frame sync m={}", marker);
    vr->m_skip_next_present = true;   // drop one present to re-phase L/R
    frames_since_reset = 0;
    sync_marker_started = false;
}
```
— `starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:340`, `:369`–`:374`

If the engine renders two frames of the same eye in a row (an async frame slipped in), the L/R phase inverts and the player sees the world swap eyes — instant discomfort. The fix is to **skip one present** (`m_skip_next_present`), which re-phases the alternation so even-frames map back to the left eye. REFramework expresses the same idea in `on_frame`: if the runtime "needed a pose update inside present," it returns early without marking `m_submitted`, effectively dropping that submission (`D3D12Component.cpp:160`–`:165`).

The "dominant eye" concept also matters for the *quad/menu* case: when you display a flat UI in the headset, you typically render it once from the dominant eye rather than alternating, so the menu doesn't shimmer. That's what the quad-display flag in §9 controls.

---

## 9. Window / quad presentation & the desktop mirror

Two presentation concerns remain beyond the eyes themselves.

**Quad display for menus.** When the game is in a menu, head-locked alternating stereo of a flat UI looks awful. starfield flips a flag to draw a stabilised quad instead:

```cpp
if (m_startFramePass == pGraph && before) {
    GameFlow::resetGameState();
    ModSettings::g_internalSettings.showQuadDisplay = GameFlow::isShowingMenu();
}
```
— `starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:165`–`:167`

`isShowingMenu()` drives whether the compositor sees a world-stereo layer or a flat quad — the same texture, presented as a billboard in front of the user, no AFR alternation, no ghosting.

**Window sizing.** A VR mod must force the engine's render target to the *headset's* resolution, not the desktop window's. starfield's `SetWindowSize` pushes the HMD dimensions into the engine's display rect and resizes the OS window to match:

```cpp
width  = vr->get_hmd_width();
height = vr->get_hmd_height();
...
ce_rect->cx = width + ce_rect->x;
ce_rect->cy = height + ce_rect->y;
(*m_creationEngineSettings)->displayGameSettings.flags |= 0x100;   // mark dirty
...
SetWindowPos(hWnd, nullptr, 0, 0, nWidth, nHeight, SWP_ASYNCWINDOWPOS);
```
— `starfield2vr/src/CreationEngine/CreationEngineRendererModule.cpp:209`–`:249`

It throttles itself (`(fc - last_synced_frame) < 5*72`, `:198`) so it isn't fighting the engine every frame — resizing a swapchain is expensive and must not happen per-frame.

**Desktop mirror.** Recording/streaming software captures the game window, but under AFR the window alternates eyes and looks like it's flickering between two viewpoints. REFramework's `m_desktop_fix` (`VR.hpp:528`) copies the *previous* backbuffer over the current one on the non-submitting frame so the desktop window shows a single stable eye:

```cpp
if (vr->m_desktop_fix->value()) {
    if (runtime->ready() && m_prev_backbuffer != backbuffer && m_prev_backbuffer != nullptr) {
        auto& copier = m_generic_copiers[frame_count % m_generic_copiers.size()];
        copier.wait(INFINITE);
        copier.copy(m_prev_backbuffer.Get(), backbuffer.Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT);
        copier.execute();
    }
}
```
— `REFramework/src/mods/vr/D3D12Component.cpp:171`–`:178`

Same triple-buffered copier pattern (`m_generic_copiers`, `array<,3>`), same state discipline.

---

## 10. Comparing the three engines

| Step | REFramework (RE Engine) | starfield (Creation Engine 2) | anvil (Ubisoft Anvil) |
|---|---|---|---|
| Eye image source | swapchain backbuffer, optional 8-bit convert | engine render-graph resources | engine backbuffer |
| Submission API | OpenVR `Submit` + OpenXR `xrEndFrame` | (delegates copy/submit to shared core) | OpenXR |
| Copy helper | `TextureContext::commands.copy` + `render_srv_to_rtv` | `CopyResource` (barrier/copy/restore) | shared core |
| Eye texture pool | mod-owned, `array<TextureContext,3>` | runtime/core-owned | runtime swapchain |
| TAA/history fix | (less needed for target configs) | `pastProjections` + `m_pastBuffer`/`SwapBuffer` | — |
| Resync mechanism | early-return drop in `on_frame` | `m_skip_next_present` on marker mismatch | frame-count hooks |
| Frame counters | `m_render_frame_count` (parity = eye) | Reflex markers 0/1/2/4/6 | `on_begin_engine_frame` / `on_begin_render_frame` |

anvil's `EngineRendererModule` is intentionally thin — it drives the frame counters (`vr->m_render_frame_count = vr->m_engine_frame_count`, `valhalla/engine/EngineRendererModule.cpp:43`) and the rendering callbacks (`on_begin_rendering`, `update_hmd_state`, `:44`–`:45`), then lets the shared core do the copy/submit. That's the lineage showing through: the universal copy-and-submit logic was promoted into the private `vrframework` core, and each engine adapter supplies only the hooks unique to it.

---

## 11. Applying this to a brand-new engine

A checklist, in order:

1. **Find the present / backbuffer.** Hook DXGI present (your core already does). Get the current backbuffer; clone it if you need an SRV or a format conversion (REFramework §2.1).
2. **Pick the eye from frame parity.** `frame_count % 2` → left/right, with the mapping in mutable variables so you can resync (§2.2, §8).
3. **Copy into a runtime texture with correct barriers.** Triple-buffer the eye textures, fence per-texture, restore states (§2.3, §3).
4. **Submit per-runtime.** OpenVR: `Submit` each eye with a queue + bounds. OpenXR: acquire/wait/copy/release, `xrEndFrame` on the second eye (§2.3–2.4).
5. **Capture a frame and hunt temporal resources.** Anything reading frame N-1 will ghost. List them (§7).
6. **Snapshot/restore past matrices.** Hook the "copy current camera → past" site; substitute the value from two frames ago, guarded by a reset flag (§5).
7. **Snapshot/restore past textures.** For each ghosting resource, maintain a per-eye copy pool, validate size/format, and ping-pong it around the engine's temporal pass (§6).
8. **Handle menus, window size, and the desktop mirror.** Quad layer for flat UI, force HMD resolution (throttled), stable single-eye mirror for recording (§9).

Do steps 1–4 first; you'll have a working but ghosting headset. Steps 5–7 are what turn "technically VR" into "comfortable VR." Do not skip them.

---

## Key takeaways

- **Submission is a copy.** The engine's backbuffer never goes straight to the compositor — you copy it into a runtime-owned, correctly-stated texture and submit that. Triple-buffer those textures and fence them; the compositor reads asynchronously.
- **Resource states are not optional bookkeeping.** Every cross-resource copy is `barrier → copy → restore`, with the *incoming* state passed explicitly because D3D12 won't tell you what it is. Wrong state = black eye or crash.
- **AFR poisons temporal effects.** TAA, DLSS/FSR, and history-based denoisers reproject "last frame" — which under AFR is the *other eye*. That's the ghosting/doubling that makes VR mods nauseating.
- **The fix is per-eye history, not disabling effects.** Snapshot/restore the past camera matrix (cheap, `pastProjections`) *and* the past history textures (expensive, `m_pastBuffer`/`SwapBuffer`), so each eye reprojects from its own previous frame.
- **You discover the resources empirically** by capturing frames and toggling copies (`enabledResourcesToCopy`) until artifacts vanish — every engine's temporal resource set is different.
- **Don't forget presentation polish:** flat quad for menus, force HMD resolution (throttled), and a stable single-eye desktop mirror so streams don't flicker.

**Next:** `11` — *Input, Haptics & Controller Binding* — getting the player's hands into the game once the world is rendering correctly.
