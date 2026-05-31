# 04 — Owning the Frame: Hooking D3D11 & D3D12

## What this covers / why it matters

To put a game into VR you have to get *inside* its rendering. You need a guaranteed callback on every frame (to drive the headset's frame loop), a handle to the GPU objects the game already created (device, command queue, swapchain, backbuffers) so you can submit per-eye images to the runtime, and a place to draw your own overlay (the ImGui menu) on top. None of this is exposed by the engine — you have to *intercept the graphics API itself*.

This is the foundation everything else in the guide series stands on. The camera injection (guide 06), the AFR stereo submission (guide 07), and the frame timeline (guide 05) all hang off the `Present` callback you build here. Get this layer wrong and nothing downstream can work; get it right and it is reusable across every game on the same API — which is exactly why it lives in **Layer 1 (universal core)**, not in any engine adapter.

The hard part is that D3D11 and D3D12 don't give you a stable function pointer to hook. The functions you want — `Present`, `ResizeBuffers` — live in a COM **vtable** that only exists at runtime, inside objects the game owns. So the recurring trick across all three projects is: **create a throwaway device and swapchain of your own, read the vtable address out of it, hook that address, then throw your objects away.** The game's real swapchain shares the same vtable, so your hook fires on the game's frames.

We'll work primarily from REFramework's `D3D12Hook` and `D3D11Hook` (the most battle-tested implementation, inherited by both `starfield2vr` and `anvilengine2vr` through the shared core), and contrast with how `starfield2vr` finds the command queue a completely different way.

---

## 1. The core problem: there is no `Present` symbol to hook

In a normal hooking scenario you find a function by name or by pattern and patch its first bytes. Direct3D defeats this. `IDXGISwapChain::Present` is a **virtual method**. The actual code lives in `dxgi.dll` / the user-mode driver, and the only way to reach it is through the swapchain object's vtable:

```
swap_chain  ->  [ vtable* ]  ->  [0] QueryInterface
                                 [1] AddRef
                                 [2] Release
                                 ...
                                 [8]  Present          <-- we want this
                                 [13] ResizeBuffers    <-- and this
                                 [14] ResizeTarget
```

You don't own the game's swapchain, and at injection time it may not even exist yet. But the vtable is **shared by every instance of that COM class in the process**. So if *you* create a swapchain — any swapchain, even a 1×1 invisible one — its vtable pointer is identical to the game's. Read slot 8 from your dummy, install a hook there, and the game's eventual `Present` call lands in your detour.

This "dummy device" pattern is the spine of the whole file.

---

## 2. Stealing the D3D12 vtable: the dummy device + swapchain

`D3D12Hook::hook()` (REFramework/src/D3D12Hook.cpp:153) builds a complete, minimal D3D12 pipeline purely to read two vtables. Walk through what it creates and *why each piece is required*:

1. **A device** — `D3D12CreateDevice(nullptr, …)` (REFramework/src/D3D12Hook.cpp:224). `nullptr` adapter = default GPU. You need a device to create a command queue.
2. **A DXGI factory** — `CreateDXGIFactory` (REFramework/src/D3D12Hook.cpp:258). The factory owns `CreateSwapChainForHwnd`, which is the *other* vtable we want to hook (so we can re-hook when the game recreates its swapchain).
3. **A command queue** — D3D12 swapchains are created *against a command queue*, not a device (this is the big structural difference from D3D11). REFramework/src/D3D12Hook.cpp:272.
4. **A swapchain** — created at 1×1, composition or hidden-window backed (REFramework/src/D3D12Hook.cpp:316).

The swapchain description is deliberately tiny and offscreen (REFramework/src/D3D12Hook.cpp:181):

```cpp
swap_chain_desc1.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
swap_chain_desc1.BufferCount = 2;
swap_chain_desc1.Width  = 1;
swap_chain_desc1.Height = 1;
```

### Why three swapchain-creation attempts?

The dummy swapchain creation is wrapped in a *fallback list* (REFramework/src/D3D12Hook.cpp:316):

```cpp
std::vector<std::function<bool ()>> swapchain_attempts{
    [&]() { return !FAILED(factory->CreateSwapChainForComposition(...)); },
    [&]() { init_dummy_window();
            return !FAILED(factory->CreateSwapChainForHwnd(command_queue, hwnd, ...)); },
    [&]() { return !FAILED(factory->CreateSwapChainForHwnd(command_queue, GetDesktopWindow(), ...)); },
};
```

`CreateSwapChainForComposition` is tried **first on purpose** (REFramework/src/D3D12Hook.cpp:317): other overlays (ReShade, Steam, RTSS) frequently hook `CreateSwapChainForHwnd`, and we do not want our *dummy* creation to wake those overlays up or trip their state machine. Composition swapchains need no window. If that fails (driver/OS quirks), we fall back to creating a real hidden window and finally to the desktop window.

### Unhook your own creation export first

Notice the device creation is bracketed by a temporary un-hook of `D3D12CreateDevice` (REFramework/src/D3D12Hook.cpp:210-231):

```cpp
const auto original_bytes = utility::get_original_bytes(d3d12_create_device);
if (original_bytes) {
    // D3D12CreateDevice is hooked by some other overlay — restore the on-disk bytes,
    // create our dummy device, then put their hook back.
    ...
}
```

If ReShade has hooked `D3D12CreateDevice`, calling it would route *our throwaway device* through *their* pipeline, polluting their state with an object we're about to destroy. So REFramework reads the original bytes off disk, temporarily restores them, creates the device cleanly, and re-applies the other overlay's hook. This kind of good-citizen behavior is the difference between "works on my machine" and "works alongside the seven other DLLs people inject."

---

## 3. The D3D12 wrinkle: finding the command queue offset

Here is the single most D3D12-specific problem in the file, and it's worth understanding deeply because **you cannot submit a frame to OpenVR/OpenXR without the game's command queue.**

In D3D11 the device *is* the submission context. In D3D12 the swapchain presents work that was queued on a `ID3D12CommandQueue`, and **DXGI gives you no API to retrieve it from a swapchain.** REFramework recovers it by brute force: it created the dummy swapchain against a queue *it controls*, so it scans the swapchain object's memory for a pointer equal to that known queue, and records the byte offset (REFramework/src/D3D12Hook.cpp:404):

```cpp
for (auto i = 0; i < 512 * sizeof(void*); i += sizeof(void*)) {
    auto data = *(ID3D12CommandQueue**)((uintptr_t)swap_chain1 + i);
    if (data == command_queue) {
        s_command_queue_offset = i;   // remember where DXGI stashes the queue pointer
        break;
    }
}
```

Later, inside the *real* `present()` detour, that same offset reads the *game's* queue straight out of the *game's* swapchain (REFramework/src/D3D12Hook.cpp:625):

```cpp
d3d12->m_command_queue = *(ID3D12CommandQueue**)((uintptr_t)swap_chain + d3d12->s_command_queue_offset);
```

This is fragile by nature (it depends on DXGI's internal layout), which is why it's discovered at runtime rather than hardcoded.

### `starfield2vr` solves the same problem from the other end

`starfield2vr` does **not** scan the swapchain. Because it has an address library and reclass headers for Starfield's engine, it walks the engine's own renderer objects to the queue. See `CreationEngineDirectX12Module.h:45-87`:

```cpp
struct D3D12Context {
    char pad[8];
    D3D12CommandQueueWrapper *pQueue[3];   // 0 = Direct, 1 = Compute, 2 = Copy
};
...
auto pQueue = pD3D12Context->pQueue[0]; // 0 - Direct, 1 - compute, 2 - copy
return pQueue->pCommandQueue;
```

The hook itself is trivial — `InstallHooks()` just resolves one global pointer via the address library (`CreationEngineDirectX12Module.cpp:10`):

```cpp
static REL::Relocation<RE::CreationRendererPrivate::ServiceLocator**> g_directX12Module{
    GameStore::MemoryOffsets::GlobalDirectX12Module2() };
g_ServiceLocator = g_directX12Module.get();
```

**The comparison that matters:**

| | REFramework (RE Engine) | starfield2vr (Creation Engine 2) |
|---|---|---|
| Where the queue comes from | scanned out of the **DXGI swapchain** at a discovered offset | walked from the **engine's own renderer** (`ServiceLocator → D3D12Context → pQueue[0]`) |
| Knowledge required | none (generic, works on any D3D12 game) | per-game reclass structs + address-library ID |
| Robustness | survives engine updates; breaks if DXGI layout changes | survives DXGI changes; breaks on engine struct shuffles |
| Picks Direct vs Compute queue | takes whatever the swapchain presents on | explicitly selects `pQueue[0]` = the Direct queue |

This is the **Layer 1 vs Layer 3** split in miniature. REFramework's swapchain scan is universal-core code; Starfield's queue walk is per-game data. When you port to a brand-new engine, **start with the swapchain scan** (it needs nothing from the engine), and only graduate to a struct-walk if the scan picks the wrong queue (e.g. the game presents on a compute queue, or a frame-gen interposer sits in the middle).

---

## 4. Two-phase hooking: pointer hook now, vtable hook later

You'd think you'd just hook vtable slot 8 of the dummy and be done. REFramework does something more careful, in **two phases** (the `m_is_phase_1` flag).

**Phase 1 — global pointer hook.** `hook_impl()` (REFramework/src/D3D12Hook.cpp:516) patches the vtable *entry* in memory:

```cpp
auto& present_fn = s_swapchain_vtable[8]; // Present
m_present_hook = std::make_unique<PointerHook>(&present_fn, &D3D12Hook::present);
```

`PointerHook` overwrites the pointer stored at `s_swapchain_vtable[8]`. Because all swapchains share that vtable, this catches the *first* real `Present` on *any* swapchain — even one created before we got control.

**Phase 2 — per-instance vtable hook.** The instant our detour fires for the first time, it tears down the global pointer hook and replaces it with a `VtableHook` bound to *that specific swapchain instance* (REFramework/src/D3D12Hook.cpp:596):

```cpp
// vtable hook the swapchain instead of global hooking
// this seems safer ... if we globally hook the vtable pointers, it causes
// all sorts of weird conflicts with other hooks
d3d12->m_present_hook.reset();
d3d12->m_swapchain_hook = std::make_unique<VtableHook>(swap_chain);
d3d12->m_swapchain_hook->hook_method(8,  (uintptr_t)&D3D12Hook::present);
d3d12->m_swapchain_hook->hook_method(13, (uintptr_t)&D3D12Hook::resize_buffers);
d3d12->m_swapchain_hook->hook_method(14, (uintptr_t)&D3D12Hook::resize_target);
d3d12->m_is_phase_1 = false;
```

Why bother? A *global* vtable-pointer patch mutates state every swapchain shares, which collides badly with Streamline (NVIDIA's DLSS-FG plumbing) and other overlays that also touch that vtable, producing "unexplainable crashes" (the comment at REFramework/src/D3D12Hook.cpp:594 is not joking). A per-instance `VtableHook` copies the vtable for *one object* and is the least intrusive thing you can do. The two-phase dance gets you the *reach* of a global hook with the *safety* of a per-instance one. `ResizeBuffers`/`ResizeTarget` aren't hooked until phase 2 because you don't have a concrete swapchain instance to bind to until then.

D3D11 doesn't need this dance. Its `hook()` (REFramework/src/D3D11Hook.cpp:86) just pointer-hooks slots 8 and 13 directly and stops — fewer overlays fight over the D3D11 swapchain vtable, and there's no Streamline frame-gen interposer to coexist with.

```cpp
auto& present_fn         = (*(void***)swap_chain)[8];
auto& resize_buffers_fn  = (*(void***)swap_chain)[13];
m_present_hook        = std::make_unique<PointerHook>(&present_fn,        &D3D11Hook::present);
m_resize_buffers_hook = std::make_unique<PointerHook>(&resize_buffers_fn, &D3D11Hook::resize_buffers);
```

---

## 5. Inside the `Present` detour: capturing the live objects

Every frame, the game calls `Present`, and now *we* are `Present`. This is where you harvest the objects the rest of the mod needs. From REFramework/src/D3D12Hook.cpp:612:

```cpp
d3d12->m_inside_present = true;
d3d12->m_swap_chain = swap_chain;

{   // the device is queried fresh from the swapchain each frame
    Microsoft::WRL::ComPtr<ID3D12Device4> temp_device{};
    swap_chain->GetDevice(IID_PPV_ARGS(&temp_device));
    d3d12->m_device = temp_device.Get();
}

// the command queue (section 3)
d3d12->m_command_queue = *(ID3D12CommandQueue**)((uintptr_t)swap_chain + d3d12->s_command_queue_offset);
```

After capture, the detour fires the registered callback **before** calling the real Present (REFramework/src/D3D12Hook.cpp:666), then calls the original, then fires the post-present callback (REFramework/src/D3D12Hook.cpp:686):

```cpp
if (d3d12->m_on_present)      d3d12->m_on_present(*d3d12);   // <- the mod's frame logic
...
result = present_fn(swap_chain, sync_interval, flags, r9);   // the real Present
...
if (d3d12->m_on_post_present) d3d12->m_on_post_present(*d3d12);
```

The framework wires those two callbacks to its frame entry points (REFramework/src/REFramework.cpp:768):

```cpp
m_d3d12_hook->on_present([this](D3D12Hook& hook)      { on_frame_d3d12(); });
m_d3d12_hook->on_post_present([this](D3D12Hook& hook) { on_post_present_d3d12(); });
m_d3d12_hook->on_resize_buffers([this](D3D12Hook& hook){ on_reset(); });
m_d3d12_hook->on_resize_target([this](D3D12Hook& hook) { on_reset(); });
```

That `on_present → on_frame_d3d12` edge is the seam the entire VR mod hangs from. `on_resize_*` → `on_reset` is equally important: when the window resizes, every backbuffer-derived resource you created is now stale, so you must release and rebuild them (more in §7).

### Capturing both swapchains

Note the bookkeeping at REFramework/src/D3D12Hook.cpp:628 (and the mirror in D3D11):

```cpp
if (d3d12->m_swapchain_0 == nullptr) {
    d3d12->m_swapchain_0 = swap_chain;
} else if (d3d12->m_swapchain_1 == nullptr && swap_chain != d3d12->m_swapchain_0) {
    d3d12->m_swapchain_1 = swap_chain;
}
```

Many games present on **more than one** swapchain (a launcher window, a video player, the main 3D view). The hook records the first two distinct ones so higher layers can decide which is the "real" game view and ignore the others — complemented by the `WindowFilter` check (REFramework/src/D3D12Hook.cpp:581) that skips windows known not to be the game.

---

## 6. Building the overlay render target

Hooking `Present` gets you a *callback*; it does not get you *pixels on screen*. To draw the ImGui menu you must render into the swapchain's current backbuffer **before** the real Present runs.

The pattern (REFramework/src/REFramework.cpp:1063) is pure D3D12 mechanics, and it's the template you'll reuse on any engine:

```cpp
auto swapchain = m_d3d12_hook->get_swap_chain();
const auto bb_index = swapchain->GetCurrentBackBufferIndex();   // which backbuffer is "live" this frame

// 1. Transition the backbuffer PRESENT -> RENDER_TARGET
barrier.Transition.pResource   = m_d3d12.rts[bb_index].Get();
barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
cmd_ctx->cmd_list->ResourceBarrier(1, &barrier);

// 2. Bind our RTV + descriptor heap and draw ImGui into it
rts[0] = m_d3d12.get_cpu_rtv(device, (D3D12::RTV)bb_index);
cmd_ctx->cmd_list->OMSetRenderTargets(1, rts, FALSE, NULL);
cmd_ctx->cmd_list->SetDescriptorHeaps(1, m_d3d12.srv_desc_heap.GetAddressOf());
ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_ctx->cmd_list.Get());

// 3. Transition RENDER_TARGET -> PRESENT so the real Present is happy
barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
cmd_ctx->cmd_list->ResourceBarrier(1, &barrier);
cmd_ctx->execute();
```

The conceptual checklist — the part you transplant to a new engine:

1. **Get the backbuffer resources.** Call `swapchain->GetBuffer(i, …)` for each of the `BufferCount` buffers and create an RTV for each in your own descriptor heap. Do this once per swapchain (and again after every resize).
2. **Track which backbuffer is current.** D3D12 flip-model rotates buffers; `GetCurrentBackBufferIndex()` tells you which one Present will show. D3D11 hides this — there's effectively one backbuffer view.
3. **Respect resource state with barriers.** The backbuffer arrives in `PRESENT` state; you must transition to `RENDER_TARGET`, draw, then transition back, or the debug layer (and real drivers) will fault.
4. **Use your own command allocator/list/fence.** You can't safely record into the game's command list. REFramework keeps a small ring of `CommandContext`s and `wait(INFINITE)`s on the fence before reusing one (REFramework/src/REFramework.cpp:1076) so the GPU is done with last cycle's commands.

In VR, REFramework actually renders ImGui **twice**: once into a private `RTV::IMGUI` texture for the headset, and once onto the desktop backbuffer (REFramework/src/REFramework.cpp:1091-1121, gated by `VR::get()->is_hmd_active()`). That private texture is what gets composited into the eye buffers — the desktop copy is just a mirror.

D3D12's explicitness (descriptor heaps, barriers, fences, per-buffer RTVs) is exactly why this is more code than D3D11, where ImGui's backend can render into a single RTV you create once from the backbuffer with no barriers.

---

## 7. `ResizeBuffers` / `ResizeTarget`: invalidate everything

When the player alt-tabs, changes resolution, or toggles fullscreen, DXGI calls `ResizeBuffers`. At that moment **every RTV, every cached backbuffer pointer, your ImGui device objects — all stale.** If you don't rebuild them you draw into freed memory and crash.

The D3D12 detour (REFramework/src/D3D12Hook.cpp:697) records the new dimensions and fires `on_resize_buffers`, which the framework wires to `on_reset()`:

```cpp
d3d12->m_display_width  = width;
d3d12->m_display_height = height;
...
if (d3d12->m_on_resize_buffers) d3d12->m_on_resize_buffers(*d3d12);
```

`ResizeTarget` (slot 14) similarly captures the *render* resolution (REFramework/src/D3D12Hook.cpp:823) — note the distinction the hook draws between **display** size (`ResizeBuffers`) and **render** size (`ResizeTarget`), which matter separately for VR.

The D3D11 path additionally nulls the cached swapchains so they get re-captured on the next Present (REFramework/src/D3D11Hook.cpp:229):

```cpp
d3d11->m_swap_chain   = swap_chain;
d3d11->m_swapchain_0  = nullptr;
d3d11->m_swapchain_1  = nullptr;
```

**Rule for a new engine:** treat `ResizeBuffers`/`ResizeTarget` as a hard "release-and-rebuild all GPU resources" signal. Never hold a raw backbuffer pointer across one.

---

## 8. Reentrancy and recursion guards (the subtle, crash-defining part)

Hooking `Present` creates a trap: your detour, or anything it calls (overlays, the driver, your own ImGui submission), can call `Present` *again* before the first call returns. Without protection you get infinite recursion and a hang or stack overflow. Every detour in these files carries a thread-local depth/flag guard.

D3D12 uses a depth counter (REFramework/src/D3D12Hook.cpp:559, used at :637):

```cpp
thread_local int32_t g_present_depth = 0;
...
if (g_present_depth > 0) {
    // We're re-entered. Restore the original bytes so the game can present without
    // looping back into us, then either call through once or just return S_OK.
    auto original_bytes = utility::get_original_bytes(Address{present_fn});
    if (original_bytes) {
        ProtectionOverride protection_override{present_fn, original_bytes->size(), PAGE_EXECUTE_READWRITE};
        memcpy(present_fn, original_bytes->data(), original_bytes->size());
    }
    ...
    return S_OK;
}
```

D3D11 uses a simple `thread_local bool g_inside_d3d11_present` (REFramework/src/D3D11Hook.cpp:121) with the same restore-original-bytes escape hatch, and even caches the `last_d3d11_present_result` to return on reentry (REFramework/src/D3D11Hook.cpp:182). The `g_inside_d3d12_hook` thread-local (REFramework/src/D3D12Hook.cpp:22) plays the same role *during the dummy-device setup*, telling the Streamline and `create_swapchain` detours "this is our own internal call, pass straight through" (REFramework/src/D3D12Hook.cpp:67, :29).

The **restore-original-bytes** trick is the important idea: rather than try to track every path, when recursion is detected the hook *physically un-patches itself* so the nested call runs the real function, guaranteeing forward progress even if the framework's own state machine got confused. It's a safety net, not the happy path.

There is also a **hook monitor mutex** taken at the top of every detour (REFramework/src/D3D12Hook.cpp:566, :702; D3D11 at :125, :217):

```cpp
std::scoped_lock _{g_framework->get_hook_monitor_mutex()};
```

A background "hook monitor" thread periodically checks the hooks are still installed and re-applies them if another overlay clobbered them. That thread and the present thread must not race, hence the shared lock.

---

## 9. The modern-DLSS / Proton gotchas you *will* hit

These are the cases that turn a clean hook into a black screen, and all three projects carry scar tissue for them.

### Frame-generation swapchains (DLSS-FG / FSR3)

With DLSS Frame Generation or FSR3, the "swapchain" the game holds is not DXGI's — it's an **interposer** (`sl.dlss_g.dll`'s `Streamline::interposer::DXGISwapChain`, or AMD's `FrameInterpolationSwapChain`) that *wraps* the real one. REFramework sniffs this during dummy init via RTTI on the class name (REFramework/src/D3D12Hook.cpp:385):

```cpp
if (swapchain_classname.contains("interposer::DXGISwapChain")) {        // DLSS3
    m_using_frame_generation_swapchain = true;
} else if (swapchain_classname.contains("FrameInterpolationSwapChain")) { // FSR3
    m_using_frame_generation_swapchain = true;
}
```

When frame-gen is present, hooking the *interposer's* present makes the overlay flicker, so REFramework digs out the **inner, real** swapchain and targets that instead (REFramework/src/D3D12Hook.cpp:456):

```cpp
if (m_using_frame_generation_swapchain) {
    target_swapchain = (IDXGISwapChain3*)scan_base;   // present on the real one inside the interposer
}
```

It also hooks Streamline's internal `linkSwapchainToCmdQueue` (REFramework/src/D3D12Hook.cpp:97). When DLSS-FG (re)creates its swapchain, this fires; REFramework tears down its hooks (`on_reset()` + `unhook()`) and re-hooks *after* the new swapchain exists (REFramework/src/D3D12Hook.cpp:46-59), so the overlay survives DLSS toggles without waiting for the slow hook-monitor to notice. `starfield2vr`'s reclass header bakes in the same reality — its `DirectX12Module2` queue field is a `union` of a plain `ID3D12CommandQueue*` and a `Nvidia::SlInterposterCommandQueue*` (`CreationEngineDirectX12Module.h:28-34`), because with Streamline active the queue you find is the interposer's async queue, not the raw one.

### Proton / Linux swapchains

On Steam Deck and Linux-via-Proton, DXGI is emulated (DXVK/VKD3D) and the command-queue pointer **isn't stored directly in the swapchain object** — it's one level of indirection away. The simple offset scan (§3) fails, so REFramework runs a *second, two-level* scan: for every pointer in the swapchain, dereference it and scan *that* object for the queue (REFramework/src/D3D12Hook.cpp:425-478):

```cpp
const auto scan_base = *(uintptr_t*)pre_scan_base;     // follow a pointer out of the swapchain
...
auto data = *(ID3D12CommandQueue**)(scan_base + i);    // scan the pointed-at object
if (data == command_queue) {
    if (!m_using_frame_generation_swapchain) m_using_proton_swapchain = true;
    s_command_queue_offset    = i;
    s_proton_swapchain_offset = base;                  // remember the extra hop
}
```

In `present()` the extra hop is replayed (REFramework/src/D3D12Hook.cpp:621):

```cpp
if (d3d12->m_using_proton_swapchain) {
    const auto real_swapchain = *(uintptr_t*)((uintptr_t)swap_chain + d3d12->s_proton_swapchain_offset);
    d3d12->m_command_queue = *(ID3D12CommandQueue**)(real_swapchain + d3d12->s_command_queue_offset);
}
```

Elegantly, the *same* two-level scan doubles as the frame-gen-inner-swapchain finder — both problems are "the thing I want is behind one more pointer."

### Other overlays fighting for the same vtable

Covered above but worth stating as a principle: assume ReShade, RTSS, Steam Overlay, and the Streamline layer are *also* in the process hooking the same exports and vtable slots. The defenses you saw — composition swapchains for the dummy, temporarily restoring on-disk bytes around your own creation calls, per-instance vtable hooks over global ones, and a hook-monitor that re-applies clobbered hooks — exist specifically because of them.

---

## 10. Applying this to a brand-new engine

The reusable recipe, in order:

1. **Decide D3D11 vs D3D12.** Check for `d3d12.dll` (REFramework falls back to D3D11 on Windows 7 / no-D3D12, REFramework/src/REFramework.cpp:757). The whole `D3D12Hook`/`D3D11Hook` split is chosen here.
2. **Build a dummy device + swapchain**, offscreen and minimal, trying composition first. Read vtable slot 8 (`Present`), 13 (`ResizeBuffers`), 14 (`ResizeTarget`) and the factory's `CreateSwapChainForHwnd` (slot 15).
3. **Hook generically first.** Global pointer-hook `Present`; on first fire, swap to a per-instance vtable hook and add the resize hooks. This needs *nothing* from the engine — it's pure Layer 1.
4. **Recover the command queue (D3D12).** Try the swapchain offset scan first; add the two-level scan for Proton/frame-gen. Only if that picks the wrong queue do you drop to a per-engine struct walk like Starfield's `ServiceLocator → D3D12Context → pQueue[0]` — and that's Layer 3 work.
5. **In the `Present` callback**, capture device, queue, swapchain, current backbuffer index; build RTVs from the backbuffers once (rebuild on resize); render your overlay with proper barriers and your own command list/fence.
6. **Guard against reentrancy** with a thread-local depth counter and a restore-original-bytes escape hatch, and serialize with a hook-monitor mutex.
7. **Plan for DLSS-FG, FSR3, and Proton from day one** — sniff the swapchain class name, target the inner swapchain, and re-hook when frame-gen recreates its chain.

Once this layer is solid you have the two things the rest of the mod needs: a guaranteed per-frame callback and live handles to the GPU objects. Everything else is built on top.

---

## Key takeaways

- **There is no `Present` symbol** — you hook a COM vtable slot. The universal trick is to create a throwaway device + swapchain, read the shared vtable, hook it, and discard your objects.
- **D3D12's defining extra problem is the command queue**: DXGI won't hand it to you, so REFramework *scans the swapchain* for it (generic, Layer 1), while starfield2vr *walks engine structs* for it (per-game, Layer 3). Start with the scan.
- **Two-phase hooking** (global pointer hook → per-instance vtable hook) gives broad reach without the cross-overlay crashes a permanent global hook causes. D3D11 needs only the simple pointer hook.
- **The `Present` detour is the seam** the whole VR mod hangs from: capture device/queue/swapchain/backbuffer there, fire `on_present`/`on_post_present`, and handle `ResizeBuffers`/`ResizeTarget` as a full resource-invalidation event.
- **The overlay is real D3D work**: get the current backbuffer, barrier `PRESENT↔RENDER_TARGET`, draw with your own command list/fence and descriptor heap. D3D12 is verbose here; D3D11 is nearly free.
- **Reentrancy will crash you** without thread-local depth guards and a restore-original-bytes safety net.
- **Modern reality is messy**: DLSS-FG/FSR3 interposers and Proton put the queue (and the real swapchain) one or two pointers deeper than expected — design for it up front, don't bolt it on.

**Next:** [05 — Driving the Clock: Frame Pacing & the Frame Timeline](./05-frame-timeline.md), where the `on_present`/`on_post_present` callbacks you just built become the heartbeat that synchronizes the engine, render, and presenter counters against the headset's vsync.
