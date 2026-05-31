# 14 — Fighting the Engine: Compatibility Tweaks & Quirks

## What this covers / why it matters

By the time you reach this document the hard rendering work is done: you have AFR stereo, an injected per-eye projection and view, a working frame timeline, and a headset that no longer makes you sick. And the game *still* looks wrong. A vignette pulses at the edge of your vision. Cutscenes letterbox into a thin slit floating in the void. The desktop window is 1920×1080 but your headset wants 2064×2272 per eye, so the game renders at the wrong aspect and everything is subtly stretched. The screen smears every other frame because temporal anti-aliasing is averaging the left eye into the right. And on a Capcom title the game silently corrupts its own memory because an anti-tamper thread noticed you patched it.

This is the unglamorous half of a VR mod. None of it is intellectually deep, but **all of it is load-bearing** — any one of these quirks can take a technically-correct stereo renderer and make it unusable. This document is the catalogue: which engine effects to kill and why, how to force the game's window and internal resolution to match the HMD, the settings the player must change by hand, and the anti-tamper minefield you have to defuse before any of your patches survive a single frame. Every technique is grounded in real code from the three reference projects.

---

## 1. The taxonomy of "bad effects"

A screen effect is "bad" for VR when it assumes a flat monitor watched by one stationary eye. VR violates both assumptions: there are two eyes with different viewpoints, and the viewport is strapped to a head that moves continuously. Effects fall into three buckets:

| Bucket | Examples | Why it breaks in VR | Fix |
|---|---|---|---|
| **Stereo-incoherent** | TAA, Nvidia history/DLSS history, screen-space reflections | Reuse last frame's data; under AFR last frame was the *other eye*, so they smear | Disable, or double-buffer the history (see doc 12) |
| **Comfort hazards** | Motion blur, depth of field, camera shake, head bob | Add motion or blur the eye can't reconcile with head-tracking → nausea | Disable outright |
| **Framing assumptions** | Letterbox bars, vignette, cinematic aspect crop | Drawn in screen space; in a headset they become a black frame *floating in 3D* | Disable, or stretch to full FOV |

The general principle: **anything that paints in 2D screen space, or that depends on frame N-1, is suspect.** Walk the engine's post-process chain and ask of each pass, "does this assume a flat, monocular, static viewport?" If yes, it's a candidate for the kill list.

---

## 2. Anvil: `EngineTwicks::DisableBadEffects` — patch the machine code

Anvil has no usable settings system to toggle these effects through, so mutars goes straight for the binary. The entry point is one inlined function that runs each tweak and logs the result:

```cpp
// anvilengine2vr/games/valhalla/engine/EngineTwicks.h:17
inline void DisableBadEffects() {
    spdlog::info("Disabling bad effects");
    ENGINE_TWICKS_RUN_AND_LOG(DisableTAA);
//        ENGINE_TWICKS_RUN_AND_LOG(DisableLetterBox);
}
```

Note `DisableLetterBox` is commented out — a recurring theme in this document is that compatibility tweaks are *empirical*. They get written, break something, and get parked behind a comment until someone revisits them. The `ENGINE_TWICKS_RUN_AND_LOG` macro (`EngineTwicks.h:3`) is a tiny but important piece of discipline: every patch reports success or failure to the log, because a tweak that silently fails to apply is worse than one that's off — you'll waste an hour chasing a "rendering bug" that's really a stale offset.

### 2.1 Killing TAA by patching the resolve

`DisableTAA` (`EngineTwicks.cpp:6`) is a textbook code patch:

```cpp
bool DisableTAA() {
    auto                 addr  = memory::disable_taa2_addr();
    std::vector<uint8_t> patch = { 0xC7, 0x46, 0x0C, 0x02, 0x00, 0x00, 0x00,   // mov [rsi+0Ch], 2
                                   0xC7, 0x42, 0x38, 0x00, 0x00, 0x80, 0x3F,   // mov [rdx+38h], 1.0f
                                   0xC6, 0x82, 0xA0, 0x00, 0x00, 0x00, 0x00,   // mov byte [rdx+0A0h], 0
                                   0x90, 0x90, /* … a long run of NOPs … */ };
    return memory::PatchMemory(addr, patch);
}
```

The address is resolved by **pattern, not hardcoded offset**:

```cpp
// games/valhalla/engine/memory/offsets.h:6
inline uintptr_t disable_taa2_addr() {
    static auto pattern = "85 DB 74 41 83 EB 01";
    static auto addr    = FuncRelocation("disable_taa2_addr", pattern, 0x8138cd);
    return addr;
}
```

This is Layer 3 (per-game data) doing its job: the *technique* (overwrite the TAA setup with a forced mode + NOP padding) lives in the engine adapter, but the *where* is a pattern with a known-good fallback offset. When the game patches, the offset rots but the byte pattern usually survives, so the tweak keeps working across updates. The patch itself forces the relevant mode field to `2`, writes `1.0f` into a blend/scale field, clears a flag byte, then NOPs out the rest of the original setup so nothing re-enables it.

### 2.2 Letterbox: a hand-built relative jump

`DisableLetterBox` (`EngineTwicks.cpp:24`) shows the other flavour of code patch — redirecting control flow rather than rewriting a store:

```cpp
auto patch_addr  = (uintptr_t)memory::g_mod + 0x8b9217;
auto target_addr = (uintptr_t)memory::g_mod + 0x8b9260;

uint8_t offset = target_addr - (patch_addr + 2);   // rel8 from end of the 2-byte jmp
offset         = offset & 0xFF;

std::vector<uint8_t> patch = { 0xEB, offset, 0x90, 0x90, 0x90, 0x90, 0x90 };  // jmp short; nop padding
return memory::PatchMemory(patch_addr, patch);
```

`0xEB` is a short `jmp rel8`; the displacement is computed `target - (patch_addr + 2)` because a relative jump is measured from the address of the *next* instruction, and a 2-byte `EB xx` ends two bytes after it starts. The remaining bytes are NOP'd so you don't leave half an instruction behind. This is the single most common shape of compatibility patch you will write: *find the conditional that draws the thing, force the jump that skips it.* The `//TODO signatures` comment above it is the reason it's disabled — it uses raw offsets with no pattern fallback, so it breaks on the first game update. **Never ship a tweak that's only an offset.** Give it a pattern or leave it off.

> **Applying this to a new engine.** Open the game in RenderDoc, find the post-process pass that draws the offending effect, and breakpoint into the CPU code that sets it up. You're looking for one of two things: a *store* that configures the effect (rewrite it, as in `DisableTAA`) or a *branch* that decides whether to draw it (flip it, as in `DisableLetterBox`). Then lift a byte pattern around that site so the patch survives updates.

---

## 3. Starfield: tweak through the engine's own settings system

Where Anvil patches machine code, Starfield does something cleaner: it reaches into the Creation Engine's *own* settings collections and flips the values the game already exposes. This is only possible because Bethesda's engine has a reflectable settings system — and finding it is the clever part.

### 3.1 Locating settings by RTTI vtable scan

`CreationEngineSettings::get_setting` (`CreationEngineSettings.cpp:21`) doesn't call the engine's `GetSetting` API (note the commented-out lines `49`, `52`). Instead it finds every `Setting` object of a given concrete type by scanning the executable for instances of its vtable:

```cpp
auto scan_settings = [&](auto& vtable) {
    auto vt_ptr = utility::rtti::find_vtable(module, vtable);   // resolve mangled name -> vtable addr
    ...
    while (find(begin, end, &offset, instance_ptr)) {           // scan image for pointers to that vtable
        auto setting = reinterpret_cast<RE::Setting*>(begin + offset);
        settings_map[setting->GetKey().data()] = setting;
        offset += sizeof(RE::Setting);
    }
    return settings_map;
};
```

The `vtable` argument is a mangled RTTI type name, one per settings category:

```cpp
case SettingType::kINISetting:           ".?AV?$SettingT@VINISettingCollection@@@@"
case SettingType::kINIPrefSetting:       ".?AVINIPrefSetting@@"
case SettingType::kGameSetting:          ".?AV?$SettingT@VGameSettingCollection@@@@"
case SettingType::kRendererQualitySetting: ".?AVRendererQualitySetting@CreationRenderer@@"
case SettingType::kRendererPerfSetting:    ".?AVRendererQualityPref@CreationRenderer@@"
```

Each map is built once (`static`) and cached. The result is a `name -> Setting*` dictionary you can read or write by string key, with a typed wrapper on top:

```cpp
// CreationEngineSettings.h:34
template <class T>
inline bool set_setting(std::string_view id, SettingType type, T value) {
    auto setting = get_setting(id, type);
    if (setting) return setting->SetValue(value);
    return false;
}
```

So a tweak becomes a one-liner, e.g. forcing the first-person weapon FOV:

```cpp
// GameSettingsComponent.cpp:55
creation_engine_settings->set_setting("fFPGeometryFOV:Camera",
    CreationEngineSettings::SettingType::kINISetting, m_weapon_fov->value());
```

This is strictly nicer than byte-patching: you get the engine's own validation, the values persist through the engine's normal codepaths, and you can drive them live from an ImGui slider (`GameSettingsComponent.cpp:63-72`). The cost is that it only works on engines with a discoverable settings table — Creation Engine has one, Anvil effectively doesn't, which is exactly why the two projects diverge here.

> **Applying this to a new engine.** Before reaching for the disassembler, ask whether the engine has a console-variable or settings system (`r.*` cvars in Unreal, `ini` settings in Creation, `cl_*`/`r_*` in idTech). If it does, locate the registry — often via RTTI, an exported symbol, or a known singleton — and you can disable DOF, motion blur, dynamic resolution and vsync *as data*, no code patch required.

---

## 4. Forcing the window and render resolution to the HMD

A headset has a fixed per-eye render size (e.g. 2064×2272). The game, left alone, renders at whatever the desktop window is — usually 16:9 — and you then resample that into the eye, wasting pixels and distorting aspect. You want the engine's swapchain/back buffer to match the eye target. Both projects do this; Starfield's is the instructive one.

### 4.1 Starfield: `SetWindowSize`

`CreationEngineRendererModule::SetWindowSize` (`CreationEngineRendererModule.cpp:189`) resizes the OS window to the HMD dimensions, which in turn drives the engine's back-buffer size. Several details matter:

```cpp
void CreationEngineRendererModule::SetWindowSize(int width, int height) {
    static std::atomic<bool> inside_change{ false };
    static int               last_synced_frame{ 1000 };
    auto                     fc = GameFlow::renderLoopFrameCount();
    if (inside_change.exchange(true)) return;                 // re-entrancy guard

    if (m_creationEngineSettings == nullptr || *m_creationEngineSettings == nullptr
        || (fc - last_synced_frame) < 5*72) {                 // throttle: at most every ~5s @72Hz
        inside_change.store(false);
        return;
    }
    last_synced_frame = fc;
    inside_change.store(false);

    if (width == 0 || height == 0) {                          // 0,0 means "ask the HMD"
        auto vr = VR::get();
        if (!vr->is_hmd_active()) return;
        width  = vr->get_hmd_width();
        height = vr->get_hmd_height();
        if (width == 0 || height == 0) return;
    }

    auto window = (*m_creationEngineSettings)->pHwindow;
    if ((window->windowCX - window->windowX) == width
     && (window->windowCY - window->windowY) == height) return;   // already correct, no-op

    auto ce_rect = &(*m_creationEngineSettings)->displayGameSettings.displayRect;
    ce_rect->cx = width  + ce_rect->x;
    ce_rect->cy = height + ce_rect->y;
    (*m_creationEngineSettings)->displayGameSettings.flags |= 0x100;   // mark display settings dirty

    auto hWnd = g_framework->get_window();
    RECT rect = { 0, 0, (LONG)width, (LONG)height };
    AdjustWindowRectEx(&rect, GetWindowLong(hWnd, GWL_STYLE),
                       GetMenu(hWnd) != NULL, GetWindowLong(hWnd, GWL_EXSTYLE));
    SetWindowPos(hWnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_ASYNCWINDOWPOS);
}
```

What's worth stealing from this:

- **It writes the engine's *internal* display rect, not just the OS window.** Setting `displayGameSettings.displayRect` and OR-ing in `flags |= 0x100` tells Creation Engine its render target changed; just calling `SetWindowPos` would resize the window but leave the engine rendering at the old resolution. You almost always have to touch *both* the OS window and the engine's notion of resolution.
- **`AdjustWindowRectEx`** converts the desired *client* size (the bit that becomes the swapchain) into an *outer* window size including borders, so the renderable area is exactly the HMD size. Forgetting this gives you an off-by-the-title-bar render every time.
- **It's idempotent and throttled.** The re-entrancy guard, the "already correct → return" check, and the `5*72`-frame throttle exist because this is called every frame from the Reflex-marker hook (`CreationEngineRendererModule.cpp:330`), and resizing a window is expensive and can recurse through `WM_SIZE`. **Resolution sync must be cheap to call constantly and must do nothing when nothing changed.**

The call site ties it into the frame timeline — the window is re-synced from inside the same marker hook that drives VR frame pacing:

```cpp
// CreationEngineRendererModule.cpp:328
if ((marker == 6 || marker == 0 || marker == 1) && !engine_notified) {
    engine_notified = true;
    instance->SetWindowSize(0,0);          // 0,0 -> pull current HMD size
    vr->on_wait_rendering(oldFrameIndex);
    ...
}
```

There's also a debug ImGui path that pokes the raw INI pref directly (`GameSettingsComponent.cpp:69`, `iSize W:Display`) and an OpenXR resolution-scale slider exposed in the overlay (per the README), so the *effective* eye resolution is `HMD size × user scale`.

> **Applying this to a new engine.** Two surfaces need to agree: the **OS window/swapchain** (resize via `SetWindowPos` + `AdjustWindowRectEx`, or by forcing the engine's resolution setting) and the **engine's internal render target** (a settings value, a struct field + dirty flag, or a hook on the engine's own resize path). Sync them from somewhere that runs every frame, but make the function a no-op when already correct.

---

## 5. The settings the player must change by hand

Some things can't (or shouldn't) be forced from code — they're owned by the OS, the GPU driver, or a game menu where overriding them fights the engine. Both projects punt these to the README as a manual checklist. This is a legitimate engineering choice: a documented manual step is more robust than a fragile patch.

| Setting | Starfield README | Anvil README | Why it matters for VR |
|---|---|---|---|
| Display mode | **Windowed** | **Windowed Mode: On** | Exclusive fullscreen owns the swapchain and fights your resize/present hooks |
| VSync | **Off** | **Off** | The HMD runtime paces frames; engine vsync adds a second, conflicting wait and tanks latency |
| Frame cap / gen | Frame Generation **Off** | Frame Cap **Off** | A frame limiter desyncs the engine/render/presenter counters the timeline depends on |
| Depth of Field | **Off** | **Off** | Blurs the eye against head motion → discomfort, and is stereo-incoherent |
| Motion Blur | **Off** | — | Same; smears under head movement |
| HDR | — | **Off** | The compositor expects a known color space; HDR back buffers come through wrong |
| Dynamic Resolution | **Off (recommended)** | — | Changing render size mid-frame fights the fixed per-eye target |

The deep reason most of these appear under "VSync/Frame Cap/Frame Gen off" is the **frame timeline** (doc 11). The mod's three counters — engine, render, presenter — assume one engine frame produces one present. Any limiter, generator, or vsync wait the engine inserts breaks that 1:1 relationship and the AFR eye-alternation drifts. The cheapest fix for an off-by-one-eye stutter is often *"turn off the player's frame cap,"* not a code change.

> **Applying this to a new engine.** Start by assuming vsync, frame cap, frame generation, dynamic resolution, DOF and motion blur are all hostile. Document them as required-off in your README *first*, get a clean stereo image, and only then decide which ones are worth forcing from code.

---

## 6. Known limitations: document what you can't fix

Both READMEs ship an honest "Known Limitations" section, and you should too — an unmodded expectation set is the difference between a bug report and a shrug. Anvil's:

> - Dialogs are rendered in stereo but displayed in **letterbox** format
> - Water and some visual effects are **not visible when looking in the opposite direction** to the character

These map directly to techniques in this document and elsewhere: the letterbox limitation is the still-disabled `DisableLetterBox` patch from §2.2; the "not visible when looking away from the character" artifact is an engine that culls or computes effects relative to the *character's* facing rather than the *head's*, which a head-aim decoupling (doc on camera) only partially solves. Writing the limitation down is what lets a future contributor pick it up.

---

## 7. The anti-tamper minefield (REFramework)

Everything above assumes your patches *stay applied*. On protected titles they don't — the game actively checks its own integrity and either stutters, corrupts memory, or skips content when it notices interference. REFramework's `IntegrityCheckBypass` (`src/mods/IntegrityCheckBypass.cpp`) is the most battle-scarred code in any of the three projects, and the lessons generalise even if the specific patches don't. This matters for VR specifically because *your stereo hooks and effect patches are exactly the kind of modification these systems detect.*

### 7.1 Different games, different defences

The mod is a catalogue of per-title strategies, dispatched off `GameIdentity` (`IntegrityCheckBypass.cpp:42`):

| Game | Symptom of the check | Bypass strategy |
|---|---|---|
| **RE3** | A bool gates the checks | Locate the bool by pattern, force it `true` every frame (`on_frame`, line 268) |
| **RE8** | Checksum routines cause large stutters; break interaction | Pattern-scan the `ja` past the checksum and rewrite it to an unconditional `jmp`; disable manager update timers (`disable_update_timers`, line 285) |
| **RE4** | A conditional jump leaks into a VM cleanup routine → random crashes | Find the conditional jump (with multiple fallbacks) and force it to always jump (line 458) |
| **DD2 / MHWilds** | Per-frame memory scans corrupt the renderer → looks like a `Present` GPU error | Neutralize the scanner function, restore corrupted values via a `createBLAS` hook (`renderer_create_blas_hook`, line 540) |
| **MHRise / SF6** | Reflectable "very cool" methods trip the protection | Find methods by name-hash via the type DB and patch them to `return 0` (line 178) |

### 7.2 The recurring patch shapes

Strip away the per-game specifics and the same handful of moves repeat — these are the primitives of anti-tamper bypass:

- **Flip a branch.** `ja → jmp` (RE8, line 130) or `jnz → jmp` / `jz → jmp` by writing `0xEB`/`0xE9`. Skip the check entirely.
- **Neuter a function.** Overwrite its first byte(s) with `0xC3` (`ret`) or `0xB0 0x00 0xC3` (`mov al,0; ret` — "return false"). Used everywhere for "make this check a no-op" (e.g. line 209, 432).
- **Force a value every frame.** When a check reads a flag, write the benign value from `on_frame` so it never matters what the check computed (RE3, line 270).
- **Nuke heap-allocated code.** Some protections JIT their checks into a heap page; `nuke_heap_allocated_code` (line 557) `memset`s the entire region to `0xC3`.

Crucially, every patch is **pattern-scanned with multiple fallbacks**, because the protected code is *obfuscated and reshuffles on every game update*. The comment at line 60 says it plainly: *"there's an element of randomness per update… we're taking a shot in the dark hoping the obfuscated code stays generally the same."* This is why you'll see three or four alternative patterns per check (e.g. the RE8 `onDamage` variants at lines 72–90) and disassembler-driven fallbacks that walk backwards to find a function start or the nearest branch (RE4, lines 497–526).

### 7.3 Anti-debug, faulty-file detection, and timing

Two further wrinkles are worth flagging because they bite during development, not just at runtime:

- **Anti-debug watcher.** `anti_debug_watcher` (line 580) runs on a background thread (line 639) and continuously checks whether `ntdll!DbgUiRemoteBreakin` has been hooked by the protection; if so it restores the original bytes (and nukes the hook's heap-allocated trampoline). Without this you can't even attach a debugger to a protected title to *develop* the rest of the mod.
- **Faulty-file / unencrypted-pak detection.** `restore_unencrypted_paks` (line 756) NOPs the conditional jump that rejects a PAK whose SHA3-256 hash (RSA-verified against the header) doesn't match — the comment at line 988 explains the whole SHA3+RSA scheme and why NOPing the compare is the only feasible bypass short of Capcom's private key. Related, `scan_patch_files_count` (line 1071) walks the game directory counting `patch_N.pak` files and `patch_version_hook` (line 672) forces the loader's patch-version ceiling up to that count so modded/extra paks actually load. **An engine that validates its own data files will treat your mod's files as corruption** — you have to defeat the file-integrity check, not just the code-integrity check.
- **The timing tax.** Note line 39: in RE8 *"the integrity checks cause a noticeable stutter."* For a flat game a stutter is cosmetic; in VR a stutter is a dropped headset frame and a visible judder. Several of these bypasses exist purely to recover frame-time consistency, which is itself a VR compatibility requirement.

> **Applying this to a new engine.** First, *expect* anti-tamper on any AAA title and budget time for it before stereo even works. Second, your toolkit is small and universal: pattern-scan the check, then flip a branch, `ret` the function, or pin a flag — always with a fallback pattern because protected code mutates per patch. Third, remember integrity comes in two flavours — **code** (your hooks/patches) and **data** (your asset files) — and you may have to defeat both. Fourth, build the anti-debug bypass early; you can't reverse the rest of the engine if you can't attach.

---

## Key takeaways

- **VR-hostile effects fall into three buckets** — stereo-incoherent (TAA, history), comfort hazards (DOF, motion blur), and framing assumptions (letterbox, vignette). Anything that draws in 2D screen space or depends on frame N-1 is suspect.
- **Two ways to kill them.** Patch machine code when the engine has no settings surface (Anvil `DisableTAA`/`DisableLetterBox`: rewrite a store or flip a branch, always pattern-anchored), or flip the engine's own settings when it has a discoverable registry (Starfield's RTTI vtable scan over `Setting` objects).
- **Resolution sync touches two surfaces** — the OS window/swapchain *and* the engine's internal render target — and must be idempotent, throttled, and a no-op when already correct (`SetWindowSize`).
- **Some quirks are README problems, not code problems.** Windowed mode, vsync off, frame cap/gen off, DOF/motion blur/HDR off — most trace back to protecting the frame timeline. Document them; don't fragile-patch them.
- **Write down what you can't fix.** A "Known Limitations" section converts bug reports into understanding and hands the next contributor a to-do list.
- **Anti-tamper is mandatory on AAA titles and uses a tiny universal toolkit** — pattern-scan, then branch-flip / `ret`-the-function / pin-a-flag, with fallback patterns because protected code mutates per update. Defeat both code-integrity and data-integrity checks, and build the anti-debug bypass first.

**Next:** `15-input-and-controllers.md` — mapping VR controllers, motion controls, and head-aim onto engines that only ever expected a gamepad.
