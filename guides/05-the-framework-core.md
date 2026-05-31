# 05 — The Spine: Framework Singleton, Mods & Overlay

**What this covers / why it matters.** Every feature in these VR mods — the stereo camera, the frame timeline, the HUD projection, the input remap — plugs into one small, boring, *reusable* skeleton: a global `Framework` singleton, a `Mod` base class with a fixed set of lifecycle callbacks, a `Mods` registry that fans those callbacks out, a flat key/value config, and an ImGui overlay. This is Layer 1 (Universal Core, == `vrframework`). It is the part you write **once** and never touch again per-engine. Get its boundaries right and adding a new engine is a matter of writing a Layer-2 adapter; get them wrong — let an engine type leak into the core — and you will fork the core for every game, which is exactly the trap REFramework fell into and `vrframework` was extracted to escape. This doc teaches the skeleton: its lifecycle, its readiness gating, why the base `Mod` must reference **zero** engine types, and how the overlay and config plumb through it.

---

## 1. The mental model: one spine, many ribs

```
                 ┌─────────────────────────────────────────────┐
   DllMain  ───► │  g_framework = make_unique<Framework>(hMod)  │   Layer 1: core
                 │   ├── D3D11Hook / D3D12Hook  (present hook)   │   (this doc)
                 │   ├── WindowsMessageHook     (input)          │
                 │   ├── ImGui context + overlay                │
                 │   └── Mods  ──► [ Mod, Mod, Mod, ... ]        │
                 └─────────────────────────────────────────────┘
                                       │ on_frame / on_draw_ui / on_config_*
                                       ▼
                 VR mod, Camera mod, EngineEntry adapter (Layer 2), ...
```

The `Framework` (REFramework calls it `REFramework`) is a *facilitator*. It does not know what a camera matrix is, what a HUD is, or what engine it is in. It knows: how to hook present, how to pump ImGui, how to read the keyboard, where the config file lives, and a list of `Mod`s to call. Everything game-shaped is a `Mod`.

This is deliberately the same shape as a plugin host. The single most important design decision in the whole codebase is **where the engine knowledge lives**, and the answer is: *not here.*

---

## 2. Lifecycle: from `DllMain` to "ready"

The proxy `dxgi.dll` gets loaded next to the game exe, `DllMain` spawns a thread, and that thread constructs the singleton:

```cpp
g_framework = std::make_unique<Framework>(hModule);
```

(See guide 03 for injection; here we pick up at the constructor.) From there the lifecycle is a **state machine driven by the present hook**, not by the constructor. This matters: when the DLL loads, D3D is not necessarily up yet, the swapchain may not exist, and the game's data structures are certainly not initialized. So almost nothing happens in the constructor — it sets up logging, the ImGui context, MinHook buffers, and reads OS/version info, then returns.

`REFramework::REFramework` — `REFramework/src/REFramework.cpp:255`:

```cpp
REFramework::REFramework(HMODULE reframework_module)
    : m_game_module{GetModuleHandle(0)}
    , m_logger{spdlog::basic_logger_mt("REFramework", ...)} {
    ...
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();      // context now; device objects much later
    ...
}
```

### 2.1 The four readiness gates

Real initialization is spread across **frames**, gated by four booleans you will see checked everywhere. Understanding these is the key to the whole file.

| Flag | Set when | Meaning |
|---|---|---|
| `m_initialized` | present hook fired + D3D device/swapchain valid + window found | Core (hooks, ImGui device objects) is alive |
| `m_game_data_initialized` | engine reflection / address library is up (background thread) | Engine-side data is safe to touch |
| `m_first_frame_d3d_initialize` | one-shot, cleared after first good frame | "run the heavy per-mod D3D init exactly once" |
| `m_mods_fully_initialized` | after `on_initialize_d3d_thread` succeeds | Mods have done their device-thread setup |

The public readiness predicate is the conjunction of the first two:

```cpp
bool is_ready() const { return m_initialized && m_game_data_initialized; }   // REFramework.hpp:70
```

`vrframework` keeps the same predicate but renames the engine gate to make the layering explicit — `m_game_data_initialized` becomes `m_engine_ready`, set by the **adapter** rather than by hard-coded reflection:

```cpp
// vrframework/include/Framework.hpp:33,39
bool is_ready() const { return m_initialized && m_engine_ready; }
void enable_engine_thread() { m_engine_ready = true; }  // the Layer-2 adapter calls this
```

That one rename is the whole thesis of the refactor: *the core no longer knows how "the engine is ready" is determined; it just exposes a switch the adapter flips.*

### 2.2 Why initialization waits 60 frames

Inside `initialize()` there is a deliberate stall — `REFramework.cpp:1905`:

```cpp
if (m_frames_since_init < 60) {
    m_frames_since_init++;
    return false;
}
```

The game's swapchain, device, and window are often not in their final state for the first dozen-or-so presents (overlays, anti-cheat, the engine's own deferred init all churn here). Hooking too early gives you a device that gets torn down and recreated, leaving your ImGui device objects dangling. Waiting ~60 frames is a cheap, robust way to let the engine settle. **What breaks without it:** intermittent crashes on launch that don't reproduce under a debugger (because the debugger changes the timing).

### 2.3 The present-hook loop and `first_frame_initialize`

Each present, the hooked `on_frame_d3d12` (`REFramework.cpp:976`) runs this dance:

1. If `!m_initialized`, call `initialize()` and return (one stage per frame).
2. Null-check the command queue / device — bail if the engine yanked them.
3. Call `first_frame_initialize()` once, which runs every mod's **device-thread** init.
4. If ready, call `m_mods->on_present()`; otherwise render just the ImGui "initializing…" window.

`first_frame_initialize` — `REFramework.cpp:2185` — is the gate that makes "run once, on the render thread, after the engine is up" happen:

```cpp
bool REFramework::first_frame_initialize() {
    const bool is_init_ok = m_error.empty() && m_game_data_initialized;
    if (!is_init_ok || !m_first_frame_d3d_initialize) return is_init_ok;

    m_first_frame_d3d_initialize = false;          // one-shot
    auto e = m_mods->on_initialize_d3d_thread();
    if (e) { m_error = *e; m_game_data_initialized = false; return false; }

    save_config();                                  // write defaults out for the frontend
    m_mods_fully_initialized = true;
    return true;
}
```

Two separate init callbacks exist on purpose:

- **`on_initialize()`** runs on the *game/background* thread when engine data first appears — for hooking engine functions, resolving addresses, building method databases.
- **`on_initialize_d3d_thread()`** runs on the *render* thread on the first good present — for anything that must touch the D3D device (textures, the VR runtime's swapchains, ImGui resources).

Mixing these up is a classic source of "works on my machine" device-context crashes. Keep the split.

---

## 3. The `Mod` base class — and the one rule that matters

A `Mod` is a feature. It overrides a subset of a fixed callback set; the registry calls them. The cleaned `vrframework` base — `vrframework/include/Mod.hpp:193` — is the canonical surface:

```cpp
class Mod {
public:
    virtual std::string_view get_name() const { return "UnknownMod"; }

    virtual std::optional<std::string> on_initialize()            { return std::nullopt; }
    virtual std::optional<std::string> on_initialize_d3d_thread() { return std::nullopt; }

    virtual void on_pre_imgui_frame() {}
    virtual void on_frame()    {}   // BeginRendering — safe for imgui
    virtual void on_present()  {}   // actual present — NOT safe for imgui
    virtual void on_post_frame()   {}
    virtual void on_post_present() {}
    virtual void on_draw_ui()  {}
    virtual void on_device_reset() {}
    virtual bool on_message(HWND, UINT, WPARAM, LPARAM) { return true; }

    virtual void on_config_load(const utility::Config& cfg, bool set_defaults) {}
    virtual void on_config_save(utility::Config& cfg) {}
};
```

That is the **entire** base. Notice what is *not* there.

### 3.1 The cardinal sin: engine types in the base class

REFramework's `Mod` base — `REFramework/src/Mod.hpp:461-487` — has the same lifecycle callbacks, but then keeps going:

```cpp
// REFramework/src/Mod.hpp — the part vrframework deleted
virtual void on_pre_update_transform(RETransform* transform) {};
virtual void on_update_transform(RETransform* transform) {};
virtual void on_pre_update_camera_controller(RopewayPlayerCameraController* controller) {};
virtual void on_pre_camera_get_projection_matrix(REManagedObject* camera, Matrix4x4f* result) {};
virtual void on_gui_draw_element(REComponent* gui_element, void* primitive_context) {};
...
MAKE_LAYER_CALLBACK(Scene, scene);          // sdk::renderer::layer::Scene
MAKE_LAYER_CALLBACK(PostEffect, post_effect);
```

`RETransform`, `RopewayPlayerCameraController`, `REManagedObject`, `sdk::renderer::layer::Scene` — these are **RE Engine types**. They are baked into the base class that *every* mod, on *every* engine, inherits from. The consequence: you cannot compile the core for Starfield or Assassin's Creed without dragging in RE Engine's reflection headers, and you cannot share the core as a submodule without those types polluting it. This is *the* reason the core had to be forked instead of reused.

The `vrframework` header is explicit about the fix, right at the top of the file (`vrframework/include/Mod.hpp:1-6`):

> ```
> // Cleaned: every RE-Engine-typed virtual has been removed so the base class
> // references ZERO engine types. Engine-specific callbacks live behind
> // spi/IEngineAdapter.hpp instead. The ModValue widget family is kept verbatim.
> ```

**The rule:** the base `Mod` may reference only the standard library, ImGui, the config type, and Win32 message params. Anything engine-shaped goes through the `IEngineAdapter` SPI (Layer 2, covered in guide 06). A camera mod that needs `on_camera_get_projection_matrix` gets it from the adapter, not from a virtual on the universal base.

How you apply this to a **new engine**: write your engine's per-frame projection/view/HUD hooks as methods on *your* `IEngineAdapter` implementation. The core `Mod` callbacks (`on_frame`, `on_draw_ui`, …) stay engine-blind; your mod reaches sideways into `g_framework->get_engine_adapter()` for the engine-specific bits.

### 3.2 Callback timing — the three you must not confuse

| Callback | Thread / phase | ImGui-safe? | Use for |
|---|---|---|---|
| `on_pre_imgui_frame` | render thread, before `ImGui::NewFrame` | — | inject VR controller state *as* imgui input |
| `on_frame` | render thread, inside NewFrame/Render | **yes** | per-frame logic + imgui draw data |
| `on_present` | actual `Present` call | **no** | GPU submit, copy to headset |
| `on_post_present` | after `Present` returns | no | end-of-frame bookkeeping |

The split between `on_frame` and `on_present` exists because `run_imgui_frame(from_present)` is called from two places, and game/script code must not run on the present thread. See `REFramework.cpp:844-853`: `on_pre_imgui_frame` and `call_on_frame()` are *guarded* by `!from_present`.

---

## 4. The `Mods` registry — boring on purpose

`Mods` owns a `vector<shared_ptr<Mod>>` and is nothing but a fan-out. The header (`REFramework/src/Mods.hpp`) and its `vrframework` equivalent are a list of `for`-loops:

```cpp
void Mods::on_frame() const {
    for (auto& mod : m_mods) mod->on_frame();       // Mods.cpp:145
}
```

The interesting part is the **constructor**, because that is the *only* place per-game composition lives. REFramework's constructor (`Mods.cpp:26`) is a big conditional tree keyed on `GameIdentity`:

```cpp
Mods::Mods() {
    m_mods.emplace_back(BackBufferRenderer::get());
    m_mods.emplace_back(REFrameworkConfig::get());
    if (sdk::GameIdentity::get().is_reengine_at())
        m_mods.emplace_back(IntegrityCheckBypass::get_shared_instance());
    ...
    m_mods.emplace_back(VR::get());
    if (gi.is_re8() || gi.is_re7())
        m_mods.emplace_back(RE8VR::get());
    ...
    m_mods.emplace_back(std::make_unique<Camera>());
}
```

Compare the Anvil/Valhalla port — `anvilengine2vr/games/valhalla/ModConfig.cpp:13`:

```cpp
Mods::Mods() {
    m_mods.emplace_back(VRConfig::get());
    m_mods.emplace_back(VR::get());
    m_mods.emplace_back(EngineEntry::Get());   // <-- the Layer-2 engine adapter, as a Mod
}
```

Three lines. No `GameIdentity` switch, because the *binary itself* is per-game (Layer 3 = `games/valhalla/`). Note `EngineEntry` — the engine adapter — is registered *as a Mod*. That is the seam: the adapter rides the same lifecycle bus as every feature, so the core never special-cases it.

**Order matters.** `VRConfig` goes first so its `ModValue`s exist (and load from disk) before `VR` reads them in `on_initialize`. The registry calls in vector order; treat the order as a dependency declaration.

### 4.1 Ordering of init vs config load

`Mods::on_initialize` (`Mods.cpp:90`) runs every mod's `on_initialize()`, *then* loads config into every mod:

```cpp
for (auto& mod : m_mods) mod->on_initialize();
utility::Config cfg{ (get_persistent_dir() / REFRAMEWORK_CONFIG_NAME).string() };
for (auto& mod : m_mods) mod->on_config_load(cfg);
```

`on_initialize_d3d_thread` (`Mods.cpp:111`) loads config **again** before *and* after device init, so mods that create options during init still pick up saved values. If your `ModValue`s are constructed lazily, this double-load is what makes "settings stick across restarts" actually work.

---

## 5. The config system — a flat key/value file

The config is intentionally dumb: a flat map of string keys to typed values, serialized to a text file in the persistent dir. There is no schema, no nesting, no migration framework. Each mod owns its keys; the file is the union of everything every mod writes.

Save (`REFramework.cpp:1438`) is a fan-out into one `Config`:

```cpp
void REFramework::save_config() {
    utility::Config cfg{};
    for (auto& mod : m_mods->get_mods()) mod->on_config_save(cfg);
    cfg.save((get_persistent_dir() / REFRAMEWORK_CONFIG_NAME).string());
}
```

Two nuances worth stealing:

- **Default-write on first run.** On the first ready frame, if no config file exists, `save_config()` is called immediately (`REFramework.cpp:1015`) so an external frontend/launcher can read the defaults. `first_frame_initialize` also saves once after device init (`REFramework.cpp:2212`).
- **Deferred save.** UI code never writes the file directly; it sets `m_wants_save_config = true` via `request_save_config()` (`REFramework.hpp:95`), and `run_imgui_frame` flushes it at a safe point off the present thread (`REFramework.cpp:865`). This avoids disk I/O inside `Present`.

### 5.1 `set_defaults` — the one config behavior change in `vrframework`

REFramework's `config_load` silently keeps the current value if a key is missing. `vrframework` adds a `set_defaults` flag so a missing key can be *reset* to its default instead — `vrframework/include/Mod.hpp:49`:

```cpp
void config_load(const utility::Config& cfg, bool set_defaults) override {
    auto v = cfg.get<T>(m_config_name);
    if (v)                 m_value = *v;
    else if (set_defaults) m_value = m_default_value;
}
```

This matters when the double-load (§4.1) would otherwise leave stale values from a prior in-memory state. The signature change ripples to `Mod::on_config_load(cfg, set_defaults)` and is one of the few *intentional* core API divergences from REFramework — note it if you are diffing the two.

---

## 6. `ModValue` — config-backed UI widgets

The `ModValue<T>` family is the glue between config and overlay: a value that *(a)* serializes itself, *(b)* draws its own ImGui widget, and *(c)* draws a read-only value display. It is kept **verbatim** from REFramework into `vrframework` (it references zero engine types, so it was already clean).

The interface (`vrframework/include/Mod.hpp:23`):

```cpp
class IModValue {
public:
    virtual bool draw(std::string_view name) = 0;        // editable widget, returns "changed"
    virtual void draw_value(std::string_view name) = 0;  // read-only text
    virtual void config_load(const utility::Config& cfg, bool set_defaults) = 0;
    virtual void config_save(utility::Config& cfg) = 0;
};
```

Concrete widgets, each wrapping one ImGui control:

| Class | Backing type | Widget | Notes |
|---|---|---|---|
| `ModToggle` | `bool` | `Checkbox` | `toggle()` helper |
| `ModFloat` | `float` | `InputFloat` | |
| `ModSlider` | `float` | `SliderFloat` | carries `m_range` |
| `ModInt32` | `int32_t` | `SliderInt` | |
| `ModCombo` | `int32_t` | `Combo` | index into a string list, clamped |
| `ModKey` | `int32_t` | rebind button | captures next keypress; ESC/Backspace = unbind |

Each is a few lines. `ModSlider` (`vrframework/include/Mod.hpp:114`) is representative:

```cpp
bool draw(std::string_view name) override {
    ImGui::PushID(this);
    auto ret = ImGui::SliderFloat(name.data(), &m_value, m_range.x, m_range.y);
    ImGui::PopID();
    return ret;
}
```

`PushID(this)` keys the widget on the object's address so two sliders with the same label don't collide — a small detail that saves hours of "why does moving slider A move slider B".

`ModKey` is the one with real logic (`REFramework/src/Mod.hpp:329`): when you click it, `m_waiting_for_new_key` flips on, and on the next frame it scans `g_framework->get_keyboard_state()` for the first pressed key, treating ESC/Backspace as "unbind". This is the only widget that reaches into `g_framework` — and it reaches for *input state*, a core concern, not an engine concern. Clean.

### 6.1 Two ways to model settings

There is a subtlety worth calling out. The ported games sometimes use a **plain struct** instead of `ModValue`s. `anvilengine2vr/games/valhalla/ModConfig.h`:

```cpp
struct ACValhallaSettings {
    float hudScaleX{0.4};
    float hudScaleY{0.4 * 1024.0f/1920.0f};
    bool  decoupledPitch{false};
};
extern ACValhallaSettings g_ac_valhalla_settings;
```

This is Layer-3 per-game data accessed directly by the engine adapter, *not* surfaced through the config file or the overlay. Rule of thumb: **use a `ModValue` when the user should see/tune/persist it; use a plain struct/global for tuning constants the developer bakes per-title.** Don't reflexively wrap everything in `ModValue`.

---

## 7. ImGui overlay integration & input plumbing

The overlay is the framework's only UI. Its lifecycle, per present:

```
run_imgui_frame(from_present):                       # REFramework.cpp:826
    consume_input()                                  # snapshot mouse delta
    init_fonts()
    ImGui_ImplWin32_NewFrame()
    if ready && !from_present: mods->on_pre_imgui_frame()   # VR injects controller input here
    ImGui::NewFrame()
    if !from_present: call_on_frame()                # mods->on_frame()
    draw_ui()                                        # the menu + mods->on_draw_ui()
    ImGui::Render()
    if !from_present && m_wants_save_config: save_config()
```

### 7.1 Toggling and focusing the menu

Input arrives through the windows-message hook into `on_message` (`REFramework.cpp:1228`). It maintains `m_last_keys[256]` (mouse buttons + virtual keys), and on the configured menu key (default Insert) toggles the overlay:

```cpp
case WM_KEYDOWN: case WM_SYSKEYDOWN: {
    const auto menu_key = REFrameworkConfig::get()->get_menu_key()->value();
    if (w_param == menu_key && !m_last_keys[w_param]) set_draw_ui(!m_draw_ui);
    m_last_keys[w_param] = true;
    break;
}
case WM_KILLFOCUS:
    std::fill(begin(m_last_keys), end(m_last_keys), false);   // drop stuck keys on focus loss
}
```

`draw_ui()` (`REFramework.cpp:1625`) then does the crucial **input arbitration**: when the menu is open and ImGui wants the keyboard, it tells the DInput hook to swallow input so the game doesn't also receive it:

```cpp
if (io.WantCaptureKeyboard) m_dinput_hook->ignore_input();
else                        m_dinput_hook->acknowledge_input();
```

This — plus the cursor patch (`patch_set_cursor_pos`) — is how the overlay can be moused around while the game underneath is frozen out. **What breaks without it:** typing a font path into the menu also walks your character forward and fires your gun.

### 7.2 Where mods draw

`draw_ui()` renders the framework window, then hands the body to mods *only when ready* (`REFramework.cpp:1700`):

```cpp
if (m_error.empty() && m_game_data_initialized) {
    m_mods->on_draw_ui();                      // each mod draws its section
} else if (!m_game_data_initialized) {
    ImGui::TextWrapped("REFramework is currently initializing...");
}
```

So a mod's `on_draw_ui` is the place it calls `m_my_slider->draw("World Scale")` etc. The readiness gate guarantees a mod's `on_draw_ui` never runs before its `on_initialize`.

### 7.3 D3D11 vs D3D12 overlay

The two backends differ only in *device-object* management, not in the loop above. D3D12 (`REFramework.cpp:976`) juggles **two** ImGui backend data slots (`m_d3d12.imgui_backend_datas[0/1]` — one for the backbuffer, one for the VR eye texture) and renders into its own RTV/SRV descriptor heaps (`REFramework.hpp:300-354`). `vrframework`'s `Framework` keeps both hooks (`get_d3d11_hook`/`get_d3d12_hook`) but trims the heavy D3D12 bookkeeping struct down to render-target dimensions (`vrframework/include/Framework.hpp:112-113`), since the full machinery is engine-independent boilerplate the reconstruction documents rather than duplicates. For a new engine you pick one backend (Starfield and Anvil are D3D12-only) and the loop is identical.

---

## 8. Applying this to a brand-new engine

1. **Take the core as-is.** `Framework`, `Mods`, `Mod`, `ModValue`, config, overlay — copy `vrframework` verbatim. Do not add engine types to any of them.
2. **Write a `Mods()` constructor for your title** (Layer 3): register `VRConfig`, `VR`, and your engine adapter as Mods, in dependency order.
3. **Flip the readiness switch from your adapter.** When your address library / reflection comes up on the background thread, call `g_framework->enable_engine_thread()`. That's what gates `is_ready()`.
4. **Put engine hooks behind the adapter** (`IEngineAdapter`, guide 06), never on the base `Mod`. If you find yourself wanting to add `virtual void on_my_engine_thing()` to `Mod.hpp`, stop — that's the leak that forces a fork.
5. **Surface user-facing settings as `ModValue`s; bake developer constants as plain structs.**

If you hold those five lines, the spine carries every engine without modification — which is precisely the difference between `vrframework` (a shared submodule) and REFramework (a great codebase you have to fork).

---

## Key takeaways

- The `Framework` singleton is a **facilitator**, not an engine integration: hooks + ImGui + input + config + a list of `Mod`s. It knows nothing game-shaped.
- Initialization is a **frame-driven state machine** with four gates (`m_initialized`, engine-ready, first-frame-d3d, mods-fully-initialized). The ~60-frame stall and the `on_initialize` / `on_initialize_d3d_thread` split exist to survive the engine's own startup churn.
- The **one rule** that makes the core reusable: the base `Mod` references **zero** engine types. REFramework leaked `RETransform`/`REManagedObject`/layer types into its base and had to be forked; `vrframework` moved all of that behind `IEngineAdapter`.
- The `Mods` constructor is the *only* per-game composition point. The engine adapter rides the lifecycle bus **as a Mod**.
- Config is a flat key/value file; saves are deferred off the present thread; `ModValue` widgets bind a value to its config key and its ImGui control in one object. `vrframework` adds a `set_defaults` flag so missing keys can reset.
- The overlay arbitrates input (`WantCaptureKeyboard` → DInput `ignore/acknowledge`) so the menu and the game don't fight over the keyboard and cursor.

**Next:** `06 — The Engine Adapter (IEngineAdapter SPI): where engine knowledge actually lives` — how Layer 2 hangs the projection/view/HUD/frame-pacing hooks off the spine without contaminating it.
