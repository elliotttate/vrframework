# 12 — Input: Gamepads, Motion Controllers & Head Tracking

## What this covers / why it matters

A flat game built for Xbox-style gamepad or keyboard/mouse has *no idea* a VR headset exists. It polls XInput, reads a thumbstick, and turns the player. Your job is to make six tracked devices — two motion controllers and a head — masquerade as that one gamepad, and then to add the things flat games never needed: head-driven aim, decoupled pitch, a menu you can open with your thumbs while both hands are full. This is the layer where a "technically working" port becomes *playable*. Get it wrong and the player can see beautifully into a stereo world but can't walk, shoot, or open a door.

This guide walks the full input pipeline across the three engines: how OpenVR/OpenXR action state becomes an `XINPUT_GAMEPAD`, the two opposite strategies for *delivering* that gamepad to the engine (rewrite the engine's poll vs. plug a virtual pad into Windows), how motion-controller poses get bound to game actions, the head-tracking modes that decide whether your head aims your gun or just looks around, the controller chords that toggle the overlay, and how REFramework's data-driven binding system generalizes all of it.

---

## 1. The core problem: one gamepad, six tracked things

Every flat game's input loop ends at one of a handful of OS entry points. On Windows the dominant one is **XInput** (`XInputGetState`), which fills an `XINPUT_STATE` containing a single `XINPUT_GAMEPAD`: a 16-bit button mask, two trigger bytes, four thumbstick shorts. That is the entire vocabulary the game understands.

VR runtimes speak a completely different language: *actions* (`Trigger`, `Grip`, `Joystick`, `AButton`) bound per-controller-type, plus 6DOF *poses* for each hand and the head. The universal core (Layer 1) exposes this as an action state you query each frame:

```cpp
vr->update_action_states();                       // pump the runtime
bool fire = vr->is_action_active(m_action_trigger, m_right_joystick);
Vector2f move = vr->get_joystick_axis(m_left_joystick);
```

The **engine adapter** (Layer 2) is responsible for the translation: read VR actions, write an `XINPUT_GAMEPAD`, and get that struct into the engine. The single function where the translation happens — by convention across both forks — is:

```cpp
void VR::on_xinput_get_state(uint32_t* retval, uint32_t user_index, XINPUT_STATE* state);
```

It is intentionally an *engine-agnostic* signature (it speaks XInput, which every Windows game understands) but its *body* is per-game, because the button map is per-game. Look at the two extremes: Anvil/Valhalla ships an **empty** body — `void VR::on_xinput_get_state(...) {}` (anvilengine2vr/games/valhalla/ModConfig.cpp:7) — because Valhalla's VR build drives the camera and HUD but leans on passthrough for buttons. Starfield ships a 240-line body that builds the entire pad from scratch (starfield2vr/src/ModConfig.cpp:37). Same SPI hook; wildly different fill.

---

## 2. Two ways to deliver the gamepad

Translating VR actions into an `XINPUT_GAMEPAD` is only half the battle. You then have to make the *game* read your struct instead of (or layered on top of) the real one. The three projects split into two philosophies.

### Strategy A — Hook the engine's poll and edit the struct in place (REFramework, Anvil)

REFramework hooks the game's own call to `XInputGetState`, lets the original run, then *mutates* the returned `XINPUT_STATE` before handing it back. The VR controller state is merged on top of whatever a physical pad reported. This is the cleanest model when the engine politely funnels input through XInput and you can find the call site.

### Strategy B — Synthesize a virtual pad with ViGEm (Starfield)

Starfield's Creation Engine does not give you a friendly XInput choke point to edit. Instead, `CreationEngineInputManager` **creates a virtual Xbox 360 controller** on the Windows side using the ViGEm bus driver, and pushes a full report into it every frame. The engine then reads that virtual pad through its *normal, unmodified* input path — as far as Starfield knows, a real Xbox controller is plugged in.

```cpp
// starfield2vr/src/CreationEngine/CreationEngineInputManager.cpp:27
const auto retval = vigem_connect(client);
...
pad = vigem_target_x360_alloc();
const auto pir = vigem_target_add(client, pad);   // "plug in" event to Windows
```

The push happens off a hook on the engine's own gamepad-poll vfunc, so the timing matches the engine's input cadence exactly:

```cpp
// CreationEngineInputManager.cpp:81
void CreationEngineInputManager::UpdateDeviceState() {
    if(!connected) return;
    XUSB_REPORT report = {0};
    auto vr = VR::get();
    if (!vr->is_hmd_active()) return;
    vr->update_action_states();
    uint32_t retval;
    vr->on_xinput_get_state(&retval, 0, (XINPUT_STATE*)&report);  // fill our report
    vigem_target_x360_update(client, pad, report);                // shove it onto the bus
}
```

Note the cast: `XUSB_REPORT` and `XINPUT_GAMEPAD` share a layout close enough that `on_xinput_get_state` can fill the ViGEm report directly. One translation function feeds both delivery strategies.

| | Hook + edit (REFramework / Anvil) | Virtual pad (Starfield) |
|---|---|---|
| Mechanism | MinHook/safetyhook on `XInputGetState` | ViGEm bus driver, hook on engine poll vfunc |
| Game sees | Its own struct, mutated | A "real" Xbox 360 pad |
| Extra dependency | none | ViGEm bus driver installed |
| Works when | engine routes through XInput you can find | engine input path is opaque/awkward to edit |
| Risk | must find the exact call; anti-cheat may watch it | driver dependency; both real pad *and* virtual pad visible |

**Applying to a new engine:** first try Strategy A — find the engine's `XInputGetState` import or its internal pad-poll and hook it. If the engine reads raw HID, uses DirectInput, or buries the read where you can't reach it, fall back to ViGEm. Starfield is the cautionary tale: a virtual pad is heavier but bulletproof against opaque input paths.

---

## 3. Building the gamepad: the translation in detail

Starfield's `on_xinput_get_state` is the reference implementation for "VR actions → XInput buttons." A few patterns are worth lifting wholesale.

### 3.1 Spoofing connection so the game accepts a pad

A flat game often ignores input from a controller slot it never saw connect. Starfield tracks the lowest user index it has been polled with and forces `ERROR_SUCCESS` so the engine treats that slot as a live, connected pad:

```cpp
// ModConfig.cpp:43
if (now - m_last_xinput_update > std::chrono::seconds(2)) {
    m_lowest_xinput_user_index = user_index;
}
...
m_spoofed_gamepad_connection = true;
if (is_using_controllers_within(std::chrono::minutes(5)))
    *retval = ERROR_SUCCESS;   // "yes, a pad is here"
```

Without this, a player who started the game with no physical controller would find their VR pad ignored — the game polled an empty slot once, got `ERROR_DEVICE_NOT_CONNECTED`, and stopped asking.

### 3.2 Action → button, the literal mapping

The bulk is mechanical OR-ing of button bits when a VR action is active:

```cpp
// ModConfig.cpp:117
if (is_right_a_button_down) pXinputGamepad->wButtons |= XINPUT_GAMEPAD_A;
if (is_left_a_button_down)  pXinputGamepad->wButtons |= XINPUT_GAMEPAD_B;
...
if (is_right_grip_down)    pXinputGamepad->bLeftTrigger  = 255;  // grip → LT
if (is_right_trigger_down) pXinputGamepad->bRightTrigger = 255;  // trigger → RT
```

### 3.3 Axes are *additive*, then clamped

Crucially, joystick axes are *added* to whatever the real pad reported and clamped, not overwritten — so a physical stick and the VR stick can coexist:

```cpp
// ModConfig.cpp:236
pXinputGamepad->sThumbLX = (int16_t)std::clamp<float>(
    (float)pXinputGamepad->sThumbLX + left_joystick_axis.x * 32767.0f,
    -32767.0f, 32767.0f);
```

### 3.4 Swap, layouts, and grip-as-modifier

Two ergonomic ideas recur and you should plan for both:

- **Handedness swap** — `m_swap_controllers->value()` flips which physical hand feeds the left vs. right logical joystick, for left-handed players (ModConfig.cpp:90).
- **Grip-as-shift** — holding grip turns the thumbstick into a D-pad and zeroes the analog axis, so one stick serves two purposes (ModConfig.cpp:244). This is how you cram a 14-button gamepad onto a controller with two buttons and a stick per hand.

The **alternative joy layout** toggle re-routes the shoulder buttons for controllers without capacitive thumbrests:

```cpp
// ModConfig.cpp:193
if (thumbrest_touch_right_down && !alternativeJoyLayout) {
    pXinputGamepad->wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;       // touch capacitive
} else if (is_left_grip_down && is_right_trigger_down && alternativeJoyLayout) {
    pXinputGamepad->wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;        // grip+trigger chord
    pXinputGamepad->bRightTrigger = 0;
}
```

The lesson: expose layout as a *setting*, because no single button map fits Touch, Index, and Vive wands.

---

## 4. Motion-controller poses → game actions, and the binding files

Buttons are only the input half. The *poses* — where each hand is in space — feed the camera and aiming systems (covered in guide 09/10). What concerns input is how the runtime knows that "the right index trigger" should produce the abstract action `Trigger`.

REFramework's `Bindings.cpp` is the canonical answer: a set of embedded JSON manifests, one **action manifest** plus one **binding file per controller type**. The action manifest declares the vocabulary — boolean actions, `vector2` joysticks, `pose`, `skeleton`, and a `vibration` output:

```jsonc
// REFramework/src/mods/vr/Bindings.cpp:24
{ "name": "/actions/default/in/Trigger",  "type": "boolean" },
{ "name": "/actions/default/in/Joystick", "type": "vector2" },
{ "name": "/actions/default/in/Pose",     "type": "pose" },
{ "name": "/actions/default/out/Haptic",  "type": "vibration" }
```

Each controller then gets a `default_bindings` entry pointing at a per-device file (Bindings.cpp:135) — `bindings_oculus_touch.json`, `bindings_knuckles.json`, `binding_vive.json`, etc. Inside, physical inputs map to actions. Touch's `x` button → `abutton`; its `grip` produces both a digital `grip` click and an analog `squeeze` pull:

```jsonc
// Bindings.cpp:228
{ "inputs": { "pull": { "output": "/actions/default/in/squeeze" } },
  "mode": "trigger", "path": "/user/hand/left/input/grip" }
```

The same JSON wires **poses** and **skeleton** so the hands track, and **haptics** so the controller can buzz:

```jsonc
// Bindings.cpp:206
"poses": [
  { "output": "/actions/default/in/pose", "path": "/user/hand/left/pose/raw" },
  { "output": "/actions/default/in/pose", "path": "/user/hand/right/pose/raw" }
]
```

At runtime the core resolves each action name string to a handle once, via a lookup table:

```cpp
// REFramework/src/mods/VR.hpp:390
std::unordered_map<std::string, std::reference_wrapper<vr::VRActionHandle_t>> m_action_handles {
    { "/actions/default/in/Trigger", m_action_trigger },
    { "/actions/default/in/Joystick", m_action_joystick },
    ...
    { "/actions/default/out/Haptic", m_action_haptic },
};
```

After that, the adapter never touches strings — it queries `m_action_trigger` directly. This is the **data-driven binding** pattern: the *what's bound to what* lives in JSON the runtime parses; the *what it means for the game* lives in `on_xinput_get_state`. The two are decoupled, which is why one binding set serves RE2, RE3, Monster Hunter, and (forked) Starfield with only the game-side map changing.

**Applying to a new engine:** copy the action manifest and the five binding files verbatim — they describe controllers, not games. Then write only your `on_xinput_get_state` button map. You almost never need to author new binding JSON unless you add a genuinely new action (note RE-specific extras like `RE3_Dodge`, `RE2_Quickturn` in Bindings.cpp:88 — game-specific actions added to the *shared* manifest).

---

## 5. Head-tracking modes: does your head aim, or just look?

A flat first-person game has exactly one rotation: the camera *is* the aim. In VR that conflates two things that should be separate — where you look and where your body/gun points. Starfield exposes this as explicit, user-facing modes.

`headTrackingType` (an int) and `headTrackingMultiplier` (a float) live in the engine constants and are driven from the menu:

```cpp
// starfield2vr/src/CreationEngine/CreationEngineConstants.h:5
inline float headTrackingMultiplier = 1.0f;
inline int   headTrackingType{0};
```

```cpp
// CreationEngineEntry.cpp:44
if (m_head_tracking_type->draw("Head Tracking Type"))
    ModConstants::headTrackingType = m_head_tracking_type->value();
if (m_pawn_control_rotation->draw("Pawn Control Rotation"))
    GameFlow::gStore.internalSettings.pawnControl = m_pawn_control_rotation->value();
if (m_head_tracking_multiplier->draw("Head Tracking Sensitivity"))
    ModConstants::headTrackingMultiplier = m_head_tracking_multiplier->value();
```

The camera manager reads these to decide how head rotation composes with the game's control rotation. The mode interacts with **aiming down sights** and **decoupled pitch** — type 0 means the head only fully drives aim when *not* aiming down sights, so a steady ADS shot isn't ruined by micro head-jitter:

```cpp
// CreationEngineCameraManager.cpp:119
if (GameFlow::gStore.internalSettings.decoupledPitch
    && !(ModConstants::headTrackingType == 0 && GameFlow::isAimingDownSights())) {
    ...
}
```

```cpp
// CreationEngineCameraManager.cpp:358
if (decoupledPitch
    && !((headTrackingType == 0 && isAimingDownSights()) || headTrackingType == 2)) {
    ...   // type 2 = head fully decoupled from pawn aim ("look, don't aim")
}
```

The **multiplier** is more subtle than a sensitivity slider — it's scaled by the FOV ratio so that head turns feel 1:1 regardless of the game's current zoom:

```cpp
// CreationEngineCameraManager.cpp:265
float get_head_tracking_multiplier() const {
    auto multiplier = (playerCamera->fov + m_fov_adjust) / playerCamera->fov;
    return multiplier * ModConstants::headTrackingMultiplier;
}
```

| Concept | Setting | What it controls |
|---|---|---|
| Head drives aim vs. body | `headTrackingType` (0/1/2) | Whether the head turn feeds the pawn's control rotation, and when it backs off (ADS) |
| Sensitivity | `headTrackingMultiplier` | Gain on head→aim, FOV-compensated |
| Pawn rotation | `pawnControl` | Whether head yaw rotates the player character/pawn at all |
| Decoupled pitch | `decoupledPitch` | Look up/down with the head without pitching the gameplay camera |

**Applying to a new engine:** these are *gameplay* decisions, not rendering ones, and they belong in the engine adapter where it injects the view matrix (guide 10). Expose them as settings from day one — players' comfort thresholds differ enormously, and "head always aims" is nauseating for some and essential for others. Valhalla's adapter exposes the rendering-side counterpart, `decoupledPitch`, through the same `on_draw_ui` pattern (anvilengine2vr/games/valhalla/engine/EngineEntry.cpp:61).

---

## 6. Toggling the menu with a controller chord

When both hands hold controllers there is no spare key to summon the ImGui overlay. Every project solves this with a **chord** — a button combination unlikely to occur in normal play — read on a per-frame hook.

Anvil uses a clean, dedicated `on_pre_imgui_frame` that watches gamepad buttons through ImGui's own input state. A **simultaneous** Start+Back toggles flat-screen mode; a **one-second long-press** of Start toggles the UI:

```cpp
// anvilengine2vr/games/valhalla/engine/EngineEntry.cpp:16
void EngineEntry::on_pre_imgui_frame() {
    const bool start = ImGui::IsKeyDown(ImGuiKey_GamepadStart);
    const bool back  = ImGui::IsKeyDown(ImGuiKey_GamepadBack);

    static bool ui_toggle_active = false;
    if (start && back && !ui_toggle_active) {            // chord: both at once
        ui_toggle_active = true;
        ModSettings::g_internalSettings.forceFlatScreen =
            !ModSettings::g_internalSettings.forceFlatScreen;
        return;
    }
    if (!back || !start) ui_toggle_active = false;       // edge-detect: rearm on release

    if (start) {                                          // long-press path
        if (!menu_button_was_pressed) { /* start timer */ }
        else if (elapsed >= std::chrono::seconds(1)) {
            g_framework->set_draw_ui(!g_framework->is_drawing_ui());
        }
    }
}
```

Starfield does the same thing but *inside* `on_xinput_get_state`, because that's where it already has the VR action state in hand — both joystick clicks together toggle the overlay; grip+joystick-click toggles flat-screen:

```cpp
// ModConfig.cpp:104
bool both_clicks_down = is_left_joystick_click_down && is_right_joystick_click_down;
if (both_clicks_down && !ui_toggle_active) {
    ui_toggle_active = true;
    g_framework->set_draw_ui(!g_framework->is_drawing_ui());
} else if (!is_left_joystick_click_down || !is_right_joystick_click_down) {
    ui_toggle_active = false;
}
if (g_framework->is_drawing_ui()) return;   // swallow game input while menu is open
```

Two patterns to copy:

1. **Edge detection via a `ui_toggle_active` latch** — without it, holding the chord flips the UI on/off every frame at 90 Hz. The latch fires once on press and rearms only on release.
2. **Swallow input while the menu is open** (`if (is_drawing_ui()) return;`) — otherwise your trigger pull both clicks an ImGui button *and* fires your gun.

A subtlety: Anvil reads ImGui's *abstracted* gamepad keys (`ImGuiKey_GamepadStart`), which only works if you've fed your synthesized pad into ImGui's IO. Starfield reads its own VR action state directly. Either is fine; pick based on where your input is already available that frame.

---

## 7. Haptics

Output, not input, but it lives in the same binding manifest (the `vibration` action in §4). The core exposes a single, runtime-agnostic call:

```cpp
// REFramework/src/mods/VR.cpp:4148
void VR::trigger_haptic_vibration(float seconds_from_now, float duration,
                                  float frequency, float amplitude,
                                  vr::VRInputValueHandle_t source) {
    if (!get_runtime()->loaded || !is_using_controllers()) return;
    if (get_runtime()->is_openvr())
        vr::VRInput()->TriggerHapticVibrationAction(
            m_action_haptic, seconds_from_now, duration, frequency, amplitude, source);
    else if (get_runtime()->is_openxr())
        m_openxr->trigger_haptic_vibration(duration, frequency, amplitude,
                                           (VRRuntime::Hand)source);
}
```

Note the `source` parameter selects *which hand* buzzes — the binding file wired both `/user/hand/left/output/haptic` and `/right/output/haptic` to the one `Haptic` action (Bindings.cpp:196), and `source` disambiguates at call time. The hard part of haptics is rarely the API; it's deciding *what game event* should trigger a buzz when the flat game never emitted a rumble signal you can read. In practice you hook the same gameplay events you use for HUD/aim (weapon fire, damage) and call `trigger_haptic_vibration` from there.

---

## 8. How REFramework exposes richer VR bindings

REFramework goes furthest because it's a full SDK, not just an adapter. Beyond the gamepad emulation it:

- **Names actions, not buttons.** Because everything routes through the `m_action_handles` string→handle table (VR.hpp:390), a Lua plugin or another mod can query `is_action_active("/actions/default/in/Block")` without knowing which physical button is bound. The binding lives in JSON; the game logic lives in code; scripts sit on top.
- **Ships game-specific actions in the shared manifest.** `RE3_Dodge`, `RE2_Quickturn`, `RE2_Reset_View`, `MiniMap`, `Block`, `Heal` (Bindings.cpp:88–123) are all first-class actions a per-game module can consume — richer than the 14 XInput bits Starfield is limited to. When a game needs an input that has no gamepad equivalent, you add an action rather than overloading a chord.
- **Exposes input to scripting.** `is_using_controllers`, `trigger_haptic_vibration`, and friends are bound into the Lua API (VR.cpp:743, 753), so community plugins can read VR input and fire haptics without C++.

The progression across the three projects is a useful ladder: Valhalla (passthrough, empty translation) → Starfield (hand-written 14-button XInput map + ViGEm) → REFramework (named action vocabulary + per-game actions + scripting). Start at the bottom; climb only as far as your game needs.

---

## Key takeaways

- **One translation function, `VR::on_xinput_get_state`, is the heart of input.** It speaks engine-agnostic XInput but its body is per-game. Valhalla leaves it empty; Starfield fills 240 lines.
- **Two delivery strategies:** hook-and-edit the engine's XInput call (REFramework/Anvil) when you can find it; synthesize a ViGEm virtual pad (Starfield) when the input path is opaque. Same `XINPUT_GAMEPAD` feeds both.
- **Spoof connection, OR buttons, *add* axes (don't overwrite), and expose handedness/layout as settings** — no single map fits all controllers.
- **Bindings are data.** Copy REFramework's action manifest + five per-controller JSON files verbatim; they describe hardware, not your game. Resolve action strings to handles once via the lookup table, then query handles.
- **Head tracking is a gameplay decision with multiple modes** — `headTrackingType`, FOV-compensated multiplier, pawn control, decoupled pitch — and it backs off during ADS. Expose all of them; comfort is personal.
- **Menu chords need an edge-detect latch and must swallow game input while open**, or the overlay flickers and your trigger fires your gun.
- **Haptics ride the same manifest** via a `vibration` action; the runtime-agnostic `trigger_haptic_vibration(... source)` picks the hand.

---

**Next:** [13 — HUD, Menus & UI Projection in Stereo](./13-hud-and-ui-projection.md) — getting the 2D interface, crosshair, and menus to live believably in a 3D world.
