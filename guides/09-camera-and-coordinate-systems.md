# 09 — Putting Your Head in the Game: Camera & Coordinate Math

## What this covers / why it matters

Everything else in this series exists to get pixels onto a panel. This document is about *where those pixels live* — the math that takes the headset's tracked pose and folds it into the engine's own view matrix so the world rotates and translates correctly around the player's eyeballs. Get the projection wrong and the image looks stretched; get the *view* math wrong and the world tilts, swims, inverts, or sits at the wrong scale, and the player is reaching for a barf bag within thirty seconds. This is also where the deepest landmines live: every engine has its own opinion about handedness, which axis points up, and what order rotations multiply in, and none of those opinions match OpenVR/OpenXR. We'll derive the full composition `view * BASIS * rotation_offset * hmd_transform * eye * INV_BASIS` term by term, show how Anvil, Creation Engine 2, and the RE Engine each solved the same problems differently, and finish with a recipe for discovering an unknown engine's conventions empirically — because on a closed-source engine, empirical is all you get.

---

## 1. The shape of the problem

A VR runtime hands you a small bundle of data each frame, all expressed in **its** coordinate system (OpenVR/OpenXR: right-handed, +Y up, −Z forward, metres):

- A **HMD pose** — a 4×4 transform of the headset in tracking ("play") space.
- Two **eye-to-head** transforms — the offset from the head origin to each eye (half the IPD, plus a tiny canting on some headsets).
- Two **projections / frustums** — the asymmetric per-eye frustum (covered in doc 08).

The engine, meanwhile, has its *own* camera living in its *own* space, with its own up-axis, its own handedness, its own units (Anvil and Creation both work in metres-ish, but that is luck, not law). Your job each frame is to answer one question:

> Given the engine's current view matrix `V`, produce a new view matrix `V'` that renders the scene as seen from **this eye of the headset**, oriented and positioned by the player's real head — while leaving the engine convinced nothing changed.

Because we render with **alternate-frame rendering** (AFR — even frame = left eye, odd frame = right eye; see doc 06/07), each call only has to produce *one* eye. The eye selector lives inside `get_current_eye_transform()`, which returns the left or right eye matrix depending on the frame parity. That keeps the per-frame math identical for both eyes; only the eye term changes.

---

## 2. The master equation

Here is the actual composition Anvil applies, verbatim, in `onCalcFinalView`:

```cpp
// anvilengine2vr/games/valhalla/engine/EngineCameraModule.cpp:107
const auto eye            = vr->get_current_eye_transform();
const auto hmd_transform  = vr->get_transform(0);
const auto rotation_offset = vr->get_transform_offset();
const auto transform = Y_UP_TO_Z_UP_BASIS * rotation_offset * hmd_transform * eye * INV_UP_TO_Z_UP_BASIS;
...
*in_viewMatrix = *in_viewMatrix * transform;
```

So `V' = V * (BASIS * rotation_offset * hmd_transform * eye * INV_BASIS)`.
([`EngineCameraModule.cpp:106-114`](E:/Github/anvilengine2vr/games/valhalla/engine/EngineCameraModule.cpp))

Read right-to-left, that inner `transform` is a sandwich. Let's unpack every layer.

| Term | What it is | Space it operates in | Why it's there |
|------|-----------|----------------------|----------------|
| `INV_UP_TO_Z_UP_BASIS` | change-of-basis, engine→VR | engine (Y-up) in, VR (Z-up) out | re-express the engine's frame in VR axes so the VR data lines up |
| `eye` | eye-to-head offset | VR | half-IPD shift; the *only* term that differs per eye |
| `hmd_transform` | tracked head pose | VR | the actual head rotation + position |
| `rotation_offset` | recenter / snap-turn | VR | software yaw the player applies |
| `Y_UP_TO_Z_UP_BASIS` | change-of-basis, VR→engine | VR in, engine out | convert the composed VR pose back into engine axes |
| `V *` (outer) | engine's existing view matrix | engine | preserves wherever the game already put the camera |

The two BASIS matrices are the wrapper. Everything between them is "do VR head transform in honest VR coordinates." The wrapper says "...but the engine doesn't speak that language, so translate in and translate back out." This **conjugation pattern** — `B · M · B⁻¹` — is the single most important idea in the whole document. It is how you apply a transform expressed in one coordinate system to data living in another. Memorize it.

---

## 3. Handedness: the silent killer

Before any axis talk, settle handedness, because it poisons *everything* — cross products, rotation direction, winding order, and the sign of the forward vector.

Anvil compiles its entire math layer with **GLM forced left-handed**, and it does not trust the build system to get it right. `Main.cpp` runs an actual runtime assertion on startup:

```cpp
// anvilengine2vr/src/Main.cpp:6-21
bool verifyLeftHandedCoordinates() {
    glm::vec3 forward(0.0f, 0.0f, 1.0f);
    glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0,1,0));
    glm::vec3 result = glm::vec3(rotationY * glm::vec4(forward, 0.0f));
    // In left-handed: +Z rotated 90° about +Y → +X. In right-handed → −X.
    bool isLeftHanded = glm::abs(result.x - 1.0f) < 0.00001f;
    return isLeftHanded;
}
...
#ifdef GLM_FORCE_LEFT_HANDED
    assert(verifyLeftHandedCoordinates());
#endif
```
([`Main.cpp:6-32`](E:/Github/anvilengine2vr/src/Main.cpp))

Why bother asserting something the preprocessor already decided? Because GLM's handedness is a *compile-time* setting bolted on with a macro, and it is extremely easy for one translation unit to include a GLM header before the macro is defined, silently giving you a right-handed `glm::perspective` in a left-handed program. The symptom — everything mirrored, or rotations going the wrong way — is maddening to debug at runtime. A boot-time assert converts a multi-hour mystery into an instant, loud failure. **Steal this technique for any new engine port.** The check is three lines and it has paid for itself many times over.

The lesson generalizes: handedness is a property of the *whole pipeline* (engine convention + your math library + the projection matrix you build + triangle winding). All four must agree. OpenVR/OpenXR are right-handed; Anvil's engine is effectively left-handed; the BASIS change and the hand-built projection matrices are what reconcile them.

---

## 4. Up-axis & the change of basis

OpenVR/OpenXR are **Y-up**. Many engines are too — but the *handedness* differs, and some engines (or some of their subsystems) are **Z-up**. Anvil's view space behaves like a Z-up frame to the VR data, so the port carries an explicit change-of-basis:

```cpp
// EngineCameraModule.cpp:74-79
const glm::mat4 Y_UP_TO_Z_UP_BASIS(1.0f, 0.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f, 1.0f, 0.0f,
                                   0.0f, -1.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f, 0.0f, 1.0f);
const glm::mat4 INV_UP_TO_Z_UP_BASIS = glm::inverse(Y_UP_TO_Z_UP_BASIS);
```

Read it as columns (GLM is column-major). The matrix maps basis vectors like so:

- engine X → X (unchanged)
- engine Y → −Z
- engine Z → Y

i.e. it swaps the Y and Z axes and flips a sign so the chirality stays consistent. The **inverse** is just its transpose (it's a pure rotation/permutation), and the port lets GLM compute it rather than hand-typing it — cheap insurance against a transcription typo.

Creation Engine 2 does the *exact same thing* but names it after the engine's physics library (Havok), and it explicitly keeps the matrix and its transpose as the inverse:

```cpp
// starfield2vr/src/CreationEngine/CreationEngineCameraManager.cpp:22-36
const glm::mat4 permutation_pre = {
    1, 0, 0, 0,
    0, 0, 1, 0,
    0, -1, 0, 0,
    0, 0, 0, 1
};
const glm::mat4 permutation_post = glm::transpose(permutation_pre);
glm::mat4 to_havok_space(const glm::mat4& mat)   { return permutation_pre  * mat * permutation_post; }
glm::mat4 from_havok_space(const glm::mat4& mat) { return permutation_post * mat * permutation_pre;  }
```
([`CreationEngineCameraManager.cpp:21-40`](E:/Github/starfield2vr/src/CreationEngine/CreationEngineCameraManager.cpp))

Notice the numbers are **identical** to Anvil's `Y_UP_TO_Z_UP_BASIS`. Two unrelated engines, two different teams, same 3×3 permutation — because both are reconciling the *same* Y-up VR runtime against a Z-up-ish engine convention. And both wrap the work in the conjugation `pre * M * post`. The only real difference is presentation: Anvil sandwiches the whole VR chain once; Creation has a named helper `to_havok_space()` it sprinkles wherever a VR pose crosses into engine space (e.g. lines 297, 310, 338).

> **Principle → instance → new engine.** The principle: a transform authored in one basis must be conjugated by the change-of-basis to act in another (`B·M·B⁻¹`). The instances: Anvil's `Y_UP_TO_Z_UP_BASIS`, Creation's `to_havok_space`. For a new engine: figure out the engine's up-axis and handedness (Section 11), build the single permutation matrix that carries VR's (+X right, +Y up, −Z fwd) onto the engine's axes, and conjugate every VR pose with it. If the engine is *already* Y-up right-handed, your basis is the identity and these terms vanish — but verify, don't assume.

---

## 5. The eye term — and why it's the only per-eye piece

`get_current_eye_transform()` returns the left or right eye-to-head matrix, chosen by frame parity:

```cpp
// REFramework/src/mods/VR.cpp:2168
Matrix4x4f VR::get_current_eye_transform(bool flip) {
    ...
    if (m_frame_count % 2 == mod_count)
        return get_runtime()->eyes[vr::Eye_Left];
    return get_runtime()->eyes[vr::Eye_Right];
}
```
([`VR.cpp:2168-2182`](E:/Github/REFramework/src/mods/VR.cpp))

This is AFR in one function. Even frames return the left eye, odd frames the right, and the surrounding camera code never has to branch on eye — it just asks for "the current eye." That eye matrix is mostly a translation of about ±IPD/2 along X (plus a possible small per-headset canting rotation). Because it is the rightmost term inside the basis sandwich, it is applied *first*, in VR space, before the head pose rotates it — which is exactly right: your eyes are offset relative to your head, then the whole head rotates.

Get the eye sign backwards and you invert stereo: the brain fuses the image but depth is reversed, near things look far, and it is deeply nauseating *without* looking obviously broken in a screenshot. If "the 3D feels wrong but I can't say why," suspect a swapped eye term first.

---

## 6. The HMD pose and the recenter / snap-turn offset

`hmd_transform = vr->get_transform(0)` is the tracked head pose (index 0 is always the HMD). `rotation_offset = vr->get_transform_offset()` is the **software** orientation the player has layered on top — recenter and snap-turn both live here. Keeping them as a separate matrix term is deliberate: the player's real head motion (`hmd_transform`) and the artificial yaw they dialed in (`rotation_offset`) are different things, and you want to compose them in the right order — offset *outside* the head pose, so snap-turn rotates the whole world around the player rather than around the headset's local axes.

REFramework shows what `rotation_offset` actually is. Recenter snapshots the current head yaw, flattens it to remove pitch/roll, and stores its inverse:

```cpp
// REFramework/src/mods/VR.cpp:2129
void VR::recenter_view() {
    const auto new_rotation_offset =
        glm::normalize(glm::inverse(utility::math::flatten(glm::quat{get_rotation(0)})));
    set_rotation_offset(new_rotation_offset);
}
```
([`VR.cpp:2129-2133`](E:/Github/REFramework/src/mods/VR.cpp))

`flatten` is the same horizon-leveling trick we'll meet again in Section 7 — it projects the rotation's forward vector onto the ground plane so recenter only affects yaw, never tilting the horizon:

```cpp
// REFramework/shared/sdk/REMath.hpp:66
static quat flatten(const quat& q) {
    const auto forward = glm::normalize(glm::quat{q} * Vector3f{0.0f, 0.0f, 1.0f});
    const auto flattened_forward = glm::normalize(Vector3f{forward.x, 0.0f, forward.z});
    return utility::math::to_quat(flattened_forward);
}
```
([`REMath.hpp:66-70`](E:/Github/REFramework/shared/sdk/REMath.hpp))

**Snap-turn** is just incremental recenter: on a button press, pre-multiply `rotation_offset` by a fixed-angle yaw quaternion (e.g. 30°). Because the offset already sits outside the head pose in the chain, the whole virtual world yaws in one discrete step and the player's relative head orientation is preserved. Smooth-turn is the same thing with a small angle every frame.

**Position recenter (standing origin).** Orientation offset handles yaw; the matching translation offset is the "standing origin." Creation subtracts it straight off the HMD translation so the playspace is re-anchored to the game camera:

```cpp
// CreationEngineCameraManager.cpp:290-294
auto hmd_transform   = vr->get_transform(0);
auto standing_origin = vr->get_transform_offset();
hmd_transform[3].x -= standing_origin[3].x;
hmd_transform[3].y -= standing_origin[3].y;
hmd_transform[3].z -= standing_origin[3].z;
```
([`CreationEngineCameraManager.cpp:289-301`](E:/Github/starfield2vr/src/CreationEngine/CreationEngineCameraManager.cpp))

Anvil folds the offset into the matrix multiply (`rotation_offset *` in the chain); Creation does the subtraction by hand on the translation column. Same intent — re-anchor the player — two implementation styles.

---

## 7. Decoupled pitch — keeping the horizon level

This is the single most important comfort feature, and the trickiest bit of matrix surgery in the file.

**The problem.** In a third-person/cinematic engine, the game camera pitches up and down as the player aims or as the camera rig moves. If you naively compose the HMD pose on top of a pitched game camera, the *entire world tilts* with the camera — the horizon rolls away from level while the player's inner ear insists they're sitting upright. That mismatch is a fast track to motion sickness. The fix, called **decoupled pitch**, is to strip the pitch (and roll) out of the engine's camera before adding the head pose, so the horizon stays glued to level and the player controls pitch only with their actual neck.

Anvil's implementation rebuilds the engine view matrix from a flattened forward vector:

```cpp
// EngineCameraModule.cpp:81-88
void removePitchFromZUpMatrix(glm::mat4& transform) {
    // Remove y component and normalize so we have the facing direction
    const auto forward_dir = glm::normalize(Vector3f{ -transform[1].x, 0.0, -transform[1].y });
    auto result = Y_UP_TO_Z_UP_BASIS
                * glm::lookAtRH(Vector3f{}, Vector3f{ forward_dir }, Vector3f(0.0f, 1.0f, 0.0f))
                * INV_UP_TO_Z_UP_BASIS;
    result[3] = transform[3];   // keep the original translation
    transform = result;
}
```
([`EngineCameraModule.cpp:81-88`](E:/Github/anvilengine2vr/games/valhalla/engine/EngineCameraModule.cpp))

Walk through it:

1. **Extract a flattened forward.** It pulls the facing direction out of the matrix's second column, zeroes the vertical component (`0.0` in the middle), and normalizes. That projects the look direction onto the ground plane — exactly the same horizon-leveling move as `flatten` in Section 6, just done on a matrix column instead of a quaternion.
2. **Rebuild a level look-at.** `glm::lookAtRH(origin, forward_dir, up=+Y)` constructs a fresh view rotation that points at the flattened forward with a guaranteed-level up vector. No pitch, no roll can survive this — they were thrown away in step 1.
3. **Conjugate by the basis.** Because the source matrix is in the engine's Z-up frame but `lookAtRH` and the flattened vector are reasoned about in VR Y-up, the whole thing is wrapped in `Y_UP_TO_Z_UP_BASIS * ... * INV_UP_TO_Z_UP_BASIS` — the conjugation pattern again, applying a transform built in one basis to data in another.
4. **Restore translation.** `result[3] = transform[3]` puts the original camera *position* back; only the orientation was leveled.

In `onCalcFinalView` it's applied conditionally, only when the player wants it, *before* the VR transform is multiplied in:

```cpp
// EngineCameraModule.cpp:111-114
if (ModSettings::g_ac_valhalla_settings.decoupledPitch)
    removePitchFromZUpMatrix(*in_viewMatrix);
*in_viewMatrix = *in_viewMatrix * transform;
```

Creation Engine solves the *same* comfort problem in a completely different place — it never touches a view matrix here. It hooks the first-person camera's rotation getter and zeroes the pitch Euler angle directly:

```cpp
// CreationEngineCameraManager.cpp:329-362 (onFPSGetCameraRotation)
havok_rotation.ToEulerAnglesXYZ(pitch, roll, yaw);   // Havok order X->Z->Y
...
if (decoupledPitch && !(...aiming exceptions...)) {
    pitch = 0.0f;                                    // drop pitch
}
havok_rotation.FromEulerAnglesXYZ(pitch, roll, yaw);
*quat_out = RE::NiQuaternion(havok_rotation) * ni_hmd_rotation;  // then add HMD
```
([`CreationEngineCameraManager.cpp:319-363`](E:/Github/starfield2vr/src/CreationEngine/CreationEngineCameraManager.cpp))

| | Anvil | Creation Engine 2 |
|---|---|---|
| Where | render-time view matrix (`onCalcFinalView`) | gameplay camera-rotation getter (`onFPSGetCameraRotation`) |
| Method | rebuild a leveled look-at from flattened forward | decompose to Euler, set `pitch = 0`, recompose |
| Representation | `glm::mat4` | `RE::NiQuaternion` / Euler XYZ |
| Exceptions | none in this path | disabled while aiming-down-sights in some head-tracking modes |

Both arrive at the same result — the horizon stays level, the head still pitches naturally — via the tools their engine made convenient. **The takeaway for a new engine:** decide *where* in the pipeline you can cleanly intercept orientation (a view matrix you can null out, or an Euler/quaternion getter you can edit), then remove pitch/roll there before the HMD pose is added.

---

## 8. Head-aim / the forward vector

Leveling the horizon fixes comfort but creates a new problem: gameplay systems — aim, interaction raycasts, "where is the player looking" — still read the engine's forward vector, which you just flattened. If you do nothing, the player aims with the (now level) game camera, not with their head. Anvil fixes this by hooking the camera's forward-vector getter and rewriting the result to point where the *head* points:

```cpp
// EngineCameraModule.cpp:175-191
glm::vec4* EngineCameraModule::onCameraGetForwardHook(sdk::CameraNode* node, glm::vec4* forward) {
    auto result = instance->m_onCameraGetForwardHook.call<glm::vec4*>(node, forward);
    if (vr->is_hmd_active()) {
        auto hmd_rotation = vr->get_transform(0);
        auto rotation = Y_UP_TO_Z_UP_BASIS * vr->get_transform_offset() * hmd_rotation * INV_UP_TO_Z_UP_BASIS;
        auto original_rotation_quat = node->prevRotation;
        if (ModSettings::g_ac_valhalla_settings.decoupledPitch) {
            auto original_rotation = glm::mat4_cast(glm::normalize(node->prevRotation));
            removePitchFromZUpMatrix(original_rotation);
            original_rotation_quat = glm::normalize(glm::quat_cast(original_rotation));
        }
        *forward = original_rotation_quat * glm::normalize(glm::quat_cast(rotation)) * glm::vec4{0.0, 1.0, 0.0, 0.0};
    }
    return result;
}
```
([`EngineCameraModule.cpp:175-192`](E:/Github/anvilengine2vr/games/valhalla/engine/EngineCameraModule.cpp))

Three things worth noticing:

- The VR rotation is conjugated by the basis (same sandwich, *without* the eye term — a forward vector has no per-eye offset).
- When decoupled pitch is on, it applies `removePitchFromZUpMatrix` to the *engine's* base rotation too, so head-aim is measured from the same leveled reference the camera uses. Consistency between the two hooks matters; if they disagree, aim drifts off where the player is looking.
- The final basis vector is `{0, 1, 0, 0}` — i.e. *+Y is "forward"* in this engine's convention. That is a concrete, load-bearing reminder that "forward" is engine-specific. You will discover yours empirically (Section 11); do not assume −Z.

Creation reaches the same goal through `UpdateMesh` / `onFPSGetCameraRotation`, composing the HMD quaternion onto the camera node's world rotation (`CreationEngineCameraManager.cpp:122-138`). Again: same objective (the player aims with their head), engine-appropriate mechanism.

---

## 9. World scale

VR is metric and absolute: one metre of real head movement *should* move the virtual camera by whatever the engine considers one metre. If the engine's world units aren't metres — or if the game's character is a giant or a mouse — 1:1 head motion feels wrong (the world feels like a dollhouse or a cathedral). The fix is a scalar applied to the translation part of the HMD pose before it enters the engine: multiply `hmd_transform[3].xyz` (and the eye offset) by `world_scale` so a metre of real motion maps to `world_scale` engine units.

Anvil and Creation in these particular titles run effectively at 1:1 (their engines are close enough to metres that no explicit scale term appears in the camera path), but the hook point is obvious: it goes on the translation column of `hmd_transform`/`eye`, right where Creation already subtracts the standing origin (Section 6). On a new engine, expose `world_scale` as a setting from day one — it is the first knob players reach for when the world "feels the wrong size," and retrofitting it means touching every translation in the chain.

---

## 10. Don't forget the viewport, scissor, and HUD

The camera math is necessary but not sufficient. Once you widen/reproject the image per eye, the engine's *scissor* and *viewport* rectangles — computed for the flat game's resolution — are wrong, and you'll get clipped edges or a black bar. Anvil fixes the rects right after calling the original `onCalcFinalView`:

```cpp
// EngineCameraModule.cpp:126-140
if (vr->is_hmd_active()) {
    float offX = out_scissorRect[0] * 2.0f;
    float offY = out_scissorRect[1] * 2.0f;
    out_scissorRect[2] += offX;  out_scissorRect[3] += offY;
    out_scissorRect[0] = 0.0f;   out_scissorRect[1] = 0.0f;
    ...
    out_viewport[2] = out_scissorRect[2];
    out_viewport[3] = out_scissorRect[3];
}
```

And the HUD/UI viewport is rescaled and recentered separately (`onCalcUIViewportHook`, lines 194-210; Creation's equivalent is `onScaleformSetViewPortInternal`, lines 159-206, which also offsets the menu per dominant eye). These are not camera math, but they share the camera's "the engine sized this for one flat screen; resize it for a stereo eye" instinct — keep them in mind or your beautiful head-tracked world will render inside a tiny rectangle.

---

## 11. Discovering an unknown engine's conventions empirically

You will not have a coordinate-system spec for a closed engine. Here is the field procedure these ports were clearly built with:

1. **Find the view-matrix injection point first.** Hook the function that produces the final view (Anvil's `update_views_fn` → `onCalcFinalView`; Creation's `NiCamera` frustum/update path). Don't compose anything yet — just `call` the original and let the game run flat. Confirm you have the right hook by logging the matrix and checking it changes as you move in-game.

2. **Probe handedness with a known rotation.** Use the `verifyLeftHandedCoordinates()` trick from `Main.cpp`: rotate +Z by +90° about +Y and look at the sign of the resulting X. +X means left-handed, −X means right-handed. Do this for *your math library* and, separately, infer the engine's by injecting a tiny known yaw and seeing which way the world turns.

3. **Find the up-axis.** Inject a pure translation along each engine axis in turn (nudge `view[3]` by a metre on X, then Y, then Z) and watch which way the camera slides in-game. The axis that moves you *vertically* is up. If it's Y, you may need no basis change; if it's Z, you need the `Y_UP_TO_Z_UP` permutation.

4. **Find "forward."** Read the engine's forward-vector getter (Anvil's `onCameraGetForwardHook`) and log it while facing a known landmark, or inject the VR head rotation against each candidate basis vector (`{1,0,0}`, `{0,1,0}`, `{0,0,1}` and negatives) until head-yaw turns the aim the correct way. Recall Anvil's answer was the non-obvious `{0,1,0}`.

5. **Build the basis, then conjugate.** Once up-axis and handedness are known, write the single permutation matrix mapping VR (+X, +Y, −Z) onto the engine's axes, let GLM compute its inverse, and drop the VR chain into the `BASIS * (offset * hmd * eye) * INV_BASIS` sandwich. Iterate: head moves should map 1:1, the horizon should stay level, and snap-turn should rotate the world around you, not tilt it.

6. **Validate stereo last.** Cross your eyes (or use the OpenXR simulator's stereo validation): near objects should have the correct parallax. Reversed depth = swapped eye term (Section 5).

Work *one variable at a time*. Changing the basis, the eye sign, and the multiply order simultaneously and then staring at a broken image is how people lose a week. The asserts and the per-axis nudges turn a guessing game into a short, mechanical search.

---

## Key takeaways

- The whole VR camera reduces to one conjugated chain: `V' = V * (BASIS * rotation_offset * hmd_transform * eye * INV_BASIS)`. Learn to read it right-to-left and know what space each term lives in.
- **Conjugation (`B·M·B⁻¹`) is the master pattern.** Anvil's `Y_UP_TO_Z_UP_BASIS` and Creation's `to_havok_space` are the *same* permutation matrix solving the *same* Y-up-VR-vs-Z-up-engine mismatch.
- **Handedness, up-axis, and "forward" are per-engine and non-negotiable.** Assert handedness at boot (the three-line `verifyLeftHandedCoordinates` test); discover the rest empirically by nudging one axis at a time. Anvil's forward is `{0,1,0}`, not the textbook `−Z`.
- **Decoupled pitch is the key comfort feature.** Strip pitch/roll from the engine camera before adding the head pose — by rebuilding a leveled look-at (Anvil) or zeroing the pitch Euler (Creation). Keep the head-aim/forward hook consistent with it or aim drifts.
- **The eye term is the only per-eye piece**, selected by frame parity inside `get_current_eye_transform()` — that's AFR in one function. A swapped eye sign reverses depth and nauseates without looking obviously broken.
- Recenter/snap-turn live in `rotation_offset` (outside the head pose so the world yaws around the player); standing-origin handles position; `world_scale` (expose it early) maps real metres to engine units. And don't forget to fix the scissor/viewport/HUD rects after you reproject.

---

**Next:** `10-input-and-controllers.md` — mapping VR controller poses and buttons into the engine's input system, and attaching the player's hands to the world you just put their head into.
